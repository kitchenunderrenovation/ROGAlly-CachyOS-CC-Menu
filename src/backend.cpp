#include "backend.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <algorithm>
#include <cmath>

// LED mode IDs matching the HID protocol
enum LedModeId {
    ModeStatic     = 0x00,
    ModeBreathe    = 0x01,
    ModeColorCycle = 0x02,
    ModeRainbow    = 0x03,
    ModeStrobe     = 0x04,
    ModePulse      = 0x0A,
};

static constexpr int SYSFS_BRIGHT_MIN = 13;
static constexpr int SYSFS_BRIGHT_MAX = 255;
static constexpr double COLOR_SCALE_MIN = 0.05;

static bool runHelper(const QStringList &args);

// ---- Construction ----

Backend::Backend(QObject *parent)
    : QObject(parent)
{
    m_k10temp = findHwmon("k10temp");
    m_amdgpu  = findHwmon("amdgpu");
    m_asus    = findHwmon("asus");
    m_hidraw  = findHidraw();
    fprintf(stderr, "Backend: hidraw=%s k10=%s\n",
            qPrintable(m_hidraw), qPrintable(m_k10temp));

    QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty()) runtime = "/tmp";
    m_stateFile = runtime + "/rog-ally-menu-led.state";

    m_fanCurveHwmon = findHwmon("asus_custom_fan_curve");

    loadLedState();

    // Fix hidraw permissions at startup (runs via pkexec, passwordless per polkit policy)
    runHelper({"fix-hidraw"});

    m_pollTimer.setInterval(250);
    connect(&m_pollTimer, &QTimer::timeout, this, [this] {
        // Fast path: only battery power (updates 4x/sec)
        m_batteryWatts = readSysfsInt("/sys/class/power_supply/BAT0/power_now", 0) / 1000000.0;
        m_batteryPercent = readSysfsInt("/sys/class/power_supply/BAT0/capacity", 0);
        emit statsChanged();

        // Slow path: everything else (every 3 seconds)
        static int tick = 0;
        if (++tick >= 12) {
            tick = 0;
            refreshAll();
            emit tdpChanged();
        }
    });
}

// ---- Sysfs helpers ----

QString Backend::readSysfs(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(f.readAll()).trimmed();
}

int Backend::readSysfsInt(const QString &path, int def)
{
    bool ok;
    int v = readSysfs(path).toInt(&ok);
    return ok ? v : def;
}

bool Backend::writeSysfs(const QString &path, const QString &value)
{
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(value.toUtf8());
        return true;
    }
    return false;  // caller should use runHelper() for privileged writes
}

static bool runHelper(const QStringList &args)
{
    QProcess p;
    p.start("pkexec", QStringList{"rog-ally-menu-helper"} + args);
    if (!p.waitForStarted(5000)) {
        fprintf(stderr, "runHelper FAIL start: %s\n", qPrintable(args.join(" ")));
        return false;
    }
    bool ok = p.waitForFinished(10000) && p.exitCode() == 0;
    if (!ok)
        fprintf(stderr, "runHelper FAIL exit=%d: %s\nstderr: %s\n",
                p.exitCode(), qPrintable(args.join(" ")),
                p.readAllStandardError().constData());
    return ok;
}

// ---- Hwmon discovery ----

QString Backend::findHwmon(const QString &name)
{
    QDir dir("/sys/class/hwmon");
    for (const auto &entry : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString path = dir.filePath(entry);
        if (readSysfs(path + "/name") == name)
            return path;
    }
    return {};
}

QString Backend::findHidraw()
{
    QDir dir("/sys/class/leds/ally:rgb:joystick_rings/device/hidraw");
    if (!dir.exists()) return {};
    auto entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (!entries.isEmpty())
        return "/dev/" + entries.first();
    return {};
}

// ---- Sensors ----

void Backend::refreshAll()
{
    if (!m_k10temp.isEmpty())
        m_cpuTemp = readSysfsInt(m_k10temp + "/temp1_input") / 1000.0;
    if (!m_amdgpu.isEmpty()) {
        m_gpuTemp = readSysfsInt(m_amdgpu + "/temp1_input") / 1000.0;
        m_gpuPower = readSysfsInt(m_amdgpu + "/power1_average") / 1000000.0;
    }
    if (!m_asus.isEmpty()) {
        m_cpuFan = readSysfsInt(m_asus + "/fan1_input", 0);
        m_gpuFan = readSysfsInt(m_asus + "/fan2_input", 0);
    }
    m_batteryPercent = readSysfsInt("/sys/class/power_supply/BAT0/capacity", 0);
    m_batteryCharging = readSysfs("/sys/class/power_supply/BAT0/status") == "Charging";
    m_acConnected = readSysfs("/sys/class/power_supply/AC0/online") == "1";
    m_batteryWatts = readSysfsInt("/sys/class/power_supply/BAT0/power_now", 0) / 1000000.0;

    // Wifi status
    m_wifiConnected = false;
    {
        QDir netDir("/sys/class/net");
        for (const auto &iface : netDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (iface.startsWith("wl")) {
                m_wifiConnected = readSysfs("/sys/class/net/" + iface + "/operstate") == "up";
                break;
            }
        }
    }

    // Bluetooth status
    m_bluetoothConnected = false;
    {
        QDir btDir("/sys/class/bluetooth");
        if (btDir.exists())
            m_bluetoothConnected = !btDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty();
    }

    // Airplane mode via rfkill sysfs
    {
        QDir rfkillDir("/sys/class/rfkill");
        for (const auto &entry : rfkillDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QString base = rfkillDir.filePath(entry);
            if (readSysfs(base + "/type") == "wlan") {
                m_airplaneMode = readSysfsInt(base + "/soft", 0) == 1;
                break;
            }
        }
    }

    // Volume: only read on first call (subsequent updates come from setVolume)
    static bool volumeRead = false;
    if (!volumeRead) {
        QProcess volProc;
        volProc.start("wpctl", {"get-volume", "@DEFAULT_AUDIO_SINK@"});
        if (volProc.waitForFinished(500)) {
            auto parts = QString::fromUtf8(volProc.readAllStandardOutput()).trimmed().split(' ');
            if (parts.size() >= 2)
                m_volume = qRound(parts.last().toDouble() * 100);
        }
        volumeRead = true;
    }
}

void Backend::startPolling()
{
    refreshAll();
    emit statsChanged();
    emit tdpChanged();
    emit ledChanged();
    m_pollTimer.start();
}

void Backend::stopPolling()
{
    m_pollTimer.stop();
}

// ---- Profile ----

QString Backend::profile() const
{
    return readSysfs(PROFILE_PATH);
}

void Backend::setProfile(const QString &p)
{
    if (runHelper({"set-profile", p})) {
        emit profileChanged();
        // TDP values change with profile - refresh after a short delay
        QTimer::singleShot(500, this, [this] {
            emit tdpChanged();
        });
    }
}

// ---- TDP ----

static constexpr const char *ARMOURY_PATH = "/sys/class/firmware-attributes/asus-armoury/attributes";

static QString tdpPath(const char *attr) {
    return QString("%1/%2/current_value").arg(ARMOURY_PATH, attr);
}

int Backend::tdpSpl() const { return readSysfsInt(tdpPath("ppt_pl1_spl")); }
int Backend::tdpSppt() const { return readSysfsInt(tdpPath("ppt_pl2_sppt")); }
int Backend::tdpFppt() const { return readSysfsInt(tdpPath("ppt_pl3_fppt")); }

static int tdpMin(const char *attr) {
    return Backend::readSysfsInt(
        QString("%1/%2/min_value").arg(ARMOURY_PATH, attr), 7);
}
static int tdpMax(const char *attr) {
    return Backend::readSysfsInt(
        QString("%1/%2/max_value").arg(ARMOURY_PATH, attr), 30);
}

void Backend::setTdpSpl(int v) {
    v = std::clamp(v, tdpMin("ppt_pl1_spl"), tdpMax("ppt_pl1_spl"));
    if (runHelper({"set-tdp-spl", QString::number(v)})) emit tdpChanged();
}
void Backend::setTdpSppt(int v) {
    v = std::clamp(v, tdpMin("ppt_pl2_sppt"), tdpMax("ppt_pl2_sppt"));
    if (runHelper({"set-tdp-sppt", QString::number(v)})) emit tdpChanged();
}
void Backend::setTdpFppt(int v) {
    v = std::clamp(v, tdpMin("ppt_pl3_fppt"), tdpMax("ppt_pl3_fppt"));
    if (runHelper({"set-tdp-fppt", QString::number(v)})) emit tdpChanged();
}

// ---- LED HID ----

bool Backend::sendHidFeature(const char *data, int len)
{
    if (m_hidraw.isEmpty()) return false;

    int fd = open(m_hidraw.toUtf8().constData(), O_RDWR);
    if (fd < 0) { fprintf(stderr, "HID open FAIL: %s %s\n", qPrintable(m_hidraw), strerror(errno)); return false; }

    char buf[64] = {};
    memcpy(buf, data, std::min(len, 64));
    int ret = ioctl(fd, HIDIOCSFEATURE(64), buf);
    close(fd);
    if (ret < 0) fprintf(stderr, "HID ioctl FAIL: %s\n", strerror(errno));
    return ret >= 0;
}

void Backend::applyLed()
{
    int pct = std::clamp(m_ledBrightness, 0, 100);
    double t = pct / 100.0;

    int r, g, b;
    if (pct == 0) {
        // Fully off: write 0 to sysfs and send black
        writeSysfs(LED_BRIGHTNESS, "0");
        char brightRpt[5] = {0x5A, (char)0xBA, (char)0xC5, (char)0xC4, 0x00};
        sendHidFeature(brightRpt, 5);
        r = g = b = 0;
    } else {
        // Set brightness via both sysfs and HID
        int sysVal = SYSFS_BRIGHT_MIN + (int)((SYSFS_BRIGHT_MAX - SYSFS_BRIGHT_MIN) * t);
        writeSysfs(LED_BRIGHTNESS, QString::number(sysVal));

        int hidBright = (pct >= 75) ? 3 : (pct >= 40) ? 2 : (pct >= 10) ? 1 : 0;
        char brightRpt[5] = {0x5A, (char)0xBA, (char)0xC5, (char)0xC4, (char)hidBright};
        sendHidFeature(brightRpt, 5);

        // Color scaling for dimming
        double cs = COLOR_SCALE_MIN + (1.0 - COLOR_SCALE_MIN) * t;
        r = (int)std::round(m_ledColor.red()   * cs);
        g = (int)std::round(m_ledColor.green() * cs);
        b = (int)std::round(m_ledColor.blue()  * cs);
    }

    int mode = modeId(m_ledMode);

    if (mode == ModeStatic) {
        // Protocol 1: fast static color
        char rpt[16] = {0x5A, (char)0xD1, 0x08, 0x0C,
                        (char)r, (char)g, (char)b,
                        (char)r, (char)g, (char)b,
                        (char)r, (char)g, (char)b,
                        (char)r, (char)g, (char)b};
        sendHidFeature(rpt, 16);
    } else {
        // Protocol 2: init + zone + set + apply
        char init[15] = {0x5A, 'A','S','U','S',' ','T','e','c','h','.','I','n','c','.'};
        sendHidFeature(init, 15);

        char zone[13] = {0x5A, (char)0xB3,
                         0x00, (char)mode,
                         (char)r, (char)g, (char)b,
                         (char)0xEB, 0x00, 0x00,
                         0, 0, 0};
        sendHidFeature(zone, 13);

        char set[2] = {0x5A, (char)0xB5};
        sendHidFeature(set, 2);

        char apply[2] = {0x5A, (char)0xB4};
        sendHidFeature(apply, 2);
    }

    saveLedState();
}

// ---- LED state persistence ----

void Backend::loadLedState()
{
    QFile f(m_stateFile);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    auto parts = QString::fromUtf8(f.readAll()).trimmed().split(' ');
    if (parts.size() >= 3) {
        m_ledColor = QColor(parts[0].toInt(), parts[1].toInt(), parts[2].toInt());
    }
    if (parts.size() >= 4)
        m_ledMode = modeName(parts[3].toInt());
    if (parts.size() >= 5)
        m_ledBrightness = parts[4].toInt();
}

void Backend::saveLedState()
{
    QFile f(m_stateFile);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    f.write(QString("%1 %2 %3 %4 %5")
                .arg(m_ledColor.red()).arg(m_ledColor.green()).arg(m_ledColor.blue())
                .arg(modeId(m_ledMode)).arg(m_ledBrightness).toUtf8());
}

// ---- LED getters/setters ----

int Backend::ledBrightness() const { return m_ledBrightness; }

void Backend::setLedBrightness(int pct) {
    fprintf(stderr, "setLedBrightness: %d%%\n", pct);
    m_ledBrightness = std::clamp(pct, 0, 100);
    applyLed();
    emit ledChanged();
}

QColor Backend::ledColor() const { return m_ledColor; }

void Backend::setLedColor(const QColor &c) {
    fprintf(stderr, "setLedColor: r=%d g=%d b=%d\n", c.red(), c.green(), c.blue());
    m_ledColor = c;
    applyLed();
    emit ledChanged();
}

QString Backend::ledMode() const { return m_ledMode; }

void Backend::setLedMode(const QString &mode) {
    fprintf(stderr, "setLedMode: %s\n", qPrintable(mode));
    m_ledMode = mode;
    applyLed();
    emit ledChanged();
}

// ---- LED mode mapping ----

int Backend::modeId(const QString &name) {
    if (name == "Breathe")     return ModeBreathe;
    if (name == "Color Cycle") return ModeColorCycle;
    if (name == "Rainbow")     return ModeRainbow;
    if (name == "Strobe")      return ModeStrobe;
    if (name == "Pulse")       return ModePulse;
    return ModeStatic;
}

QString Backend::modeName(int id) {
    switch (id) {
    case ModeBreathe:    return "Breathe";
    case ModeColorCycle: return "Color Cycle";
    case ModeRainbow:    return "Rainbow";
    case ModeStrobe:     return "Strobe";
    case ModePulse:      return "Pulse";
    default:             return "Static";
    }
}

// ---- Fan curves ----

QVariantList Backend::cpuFanCurve() const
{
    QVariantList points;
    if (m_fanCurveHwmon.isEmpty()) return points;
    for (int i = 1; i <= 8; i++) {
        QVariantMap pt;
        pt["temp"] = readSysfsInt(m_fanCurveHwmon + QString("/pwm1_auto_point%1_temp").arg(i), 0);
        pt["pwm"]  = readSysfsInt(m_fanCurveHwmon + QString("/pwm1_auto_point%1_pwm").arg(i), 0);
        points.append(pt);
    }
    return points;
}

QVariantList Backend::gpuFanCurve() const
{
    QVariantList points;
    if (m_fanCurveHwmon.isEmpty()) return points;
    for (int i = 1; i <= 8; i++) {
        QVariantMap pt;
        pt["temp"] = readSysfsInt(m_fanCurveHwmon + QString("/pwm2_auto_point%1_temp").arg(i), 0);
        pt["pwm"]  = readSysfsInt(m_fanCurveHwmon + QString("/pwm2_auto_point%1_pwm").arg(i), 0);
        points.append(pt);
    }
    return points;
}

void Backend::setCpuFanPoint(int point, int temp, int pwm)
{
    if (point < 1 || point > 8) return;
    runHelper({"set-fan-curve",
               QString::number(temp),
               m_fanCurveHwmon + QString("/pwm1_auto_point%1_temp").arg(point)});
    runHelper({"set-fan-curve",
               QString::number(pwm),
               m_fanCurveHwmon + QString("/pwm1_auto_point%1_pwm").arg(point)});
    emit fanCurveChanged();
}

void Backend::setGpuFanPoint(int point, int temp, int pwm)
{
    if (point < 1 || point > 8) return;
    runHelper({"set-fan-curve",
               QString::number(temp),
               m_fanCurveHwmon + QString("/pwm2_auto_point%1_temp").arg(point)});
    runHelper({"set-fan-curve",
               QString::number(pwm),
               m_fanCurveHwmon + QString("/pwm2_auto_point%1_pwm").arg(point)});
    emit fanCurveChanged();
}

void Backend::applyFanPreset(const QString &preset)
{
    fprintf(stderr, "applyFanPreset: %s\n", qPrintable(preset));
    struct Point { int temp; int pwm; };
    std::array<Point, 8> cpu, gpu;

    if (preset == "Silent") {
        cpu = {{{40,5},{50,10},{60,30},{65,50},{70,70},{75,100},{80,140},{90,200}}};
        gpu = {{{40,5},{50,10},{60,20},{65,40},{70,60},{75,80},{80,100},{90,140}}};
    } else if (preset == "Balanced") {
        cpu = {{{40,10},{45,30},{55,80},{63,110},{67,140},{70,170},{74,200},{85,254}}};
        gpu = {{{40,10},{45,30},{55,70},{63,90},{67,110},{70,130},{74,150},{85,200}}};
    } else if (preset == "Performance") {
        cpu = {{{35,30},{40,60},{50,100},{55,140},{60,180},{65,210},{70,240},{75,254}}};
        gpu = {{{35,20},{40,50},{50,80},{55,110},{60,140},{65,170},{70,200},{75,254}}};
    } else if (preset == "Aggressive") {
        cpu = {{{30,50},{35,80},{40,120},{45,160},{50,200},{55,230},{60,250},{65,254}}};
        gpu = {{{30,40},{35,70},{40,100},{45,140},{50,180},{55,210},{60,240},{65,254}}};
    } else {
        return;
    }

    if (preset == "Max") {
        runHelper({"set-fan-fullspeed"});
        emit fanCurveChanged();
        return;
    }

    // Restore auto mode first (in case we were in full speed)
    runHelper({"set-fan-auto"});

    QStringList cpuArgs = {"set-fan-preset", "1"};
    QStringList gpuArgs = {"set-fan-preset", "2"};
    for (int i = 0; i < 8; i++) {
        cpuArgs << QString::number(cpu[i].temp) << QString::number(cpu[i].pwm);
        gpuArgs << QString::number(gpu[i].temp) << QString::number(gpu[i].pwm);
    }
    runHelper(cpuArgs);
    runHelper(gpuArgs);
    emit fanCurveChanged();
}

// ---- Charge limit ----

int Backend::chargeLimit() const {
    return readSysfsInt(QString("%1/charge_mode/current_value").arg(ARMOURY_PATH), 100);
}

void Backend::setChargeLimit(int v) {
    v = std::clamp(v, 20, 100);
    if (runHelper({"set-charge-limit", QString::number(v)}))
        emit chargeLimitChanged();
}

// ---- Screen brightness ----

int Backend::screenBrightness() const {
    QDir dir("/sys/class/backlight");
    for (const auto &entry : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString base = dir.filePath(entry);
        int cur = readSysfsInt(base + "/brightness", -1);
        int mx  = readSysfsInt(base + "/max_brightness", -1);
        if (cur >= 0 && mx > 0)
            return std::round(cur * 100.0 / mx);
    }
    return -1;
}

void Backend::setScreenBrightness(int pct) {
    pct = std::clamp(pct, 0, 100);
    QDir dir("/sys/class/backlight");
    for (const auto &entry : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString base = dir.filePath(entry);
        int mx = readSysfsInt(base + "/max_brightness", -1);
        if (mx > 0) {
            int val = std::round(pct * mx / 100.0);
            writeSysfs(base + "/brightness", QString::number(val));
            emit screenBrightnessChanged();
            return;
        }
    }
}

// ---- Airplane mode ----

bool Backend::airplaneMode() const { return m_airplaneMode; }

void Backend::toggleAirplaneMode() {
    if (m_airplaneMode) {
        QProcess::startDetached("rfkill", {"unblock", "all"});
    } else {
        QProcess::startDetached("rfkill", {"block", "all"});
    }
    m_airplaneMode = !m_airplaneMode;
    emit statsChanged();
}

// ---- Volume ----

int Backend::volume() const { return m_volume; }

void Backend::setVolume(int v) {
    v = std::clamp(v, 0, 100);
    m_volume = v;
    QProcess::startDetached("wpctl", {"set-volume", "@DEFAULT_AUDIO_SINK@",
                                       QString::number(v / 100.0, 'f', 2)});
    emit statsChanged();
}

// ---- Status ----

QString Backend::currentTime() const {
    return QDateTime::currentDateTime().toString("h:mm AP");
}

// ---- New actions ----

void Backend::cycleLedBrightness(int value) {
    fprintf(stderr, "cycleLedBrightness: %d%%\n", value);
    setLedBrightness(value);
}

void Backend::cycleLedColor() {
    struct NamedColor { const char *name; int r, g, b; };
    static const NamedColor colors[] = {
        {"Dark Red", 139, 0, 0},
        {"Red",      255, 0, 0},
        {"Blue",     0,   0, 255},
        {"Green",    0,   255, 0},
        {"Orange",   255, 165, 0},
        {"White",    255, 255, 255},
    };
    static constexpr int N = sizeof(colors) / sizeof(colors[0]);

    // Find current color index
    int cur = -1;
    for (int i = 0; i < N; i++) {
        if (m_ledColor.red() == colors[i].r &&
            m_ledColor.green() == colors[i].g &&
            m_ledColor.blue() == colors[i].b) {
            cur = i;
            break;
        }
    }
    int next = (cur + 1) % N;
    setLedColor(QColor(colors[next].r, colors[next].g, colors[next].b));
}

void Backend::takeScreenshot() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (dir.isEmpty()) dir = QDir::homePath() + "/Pictures";
    QDir().mkpath(dir);
    QString file = dir + "/screenshot_" +
                   QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png";
    QProcess::startDetached("grim", {file});
}

void Backend::showKeyboard() {
    // Toggle KDE Plasma virtual keyboard via D-Bus
    QProcess p;
    p.start("dbus-send", {"--session", "--print-reply",
        "--dest=org.kde.KWin", "/VirtualKeyboard",
        "org.freedesktop.DBus.Properties.Get",
        "string:org.kde.kwin.VirtualKeyboard", "string:active"});
    bool active = false;
    if (p.waitForFinished(1000))
        active = p.readAllStandardOutput().contains("true");

    fprintf(stderr, "showKeyboard: active=%d, toggling to %d\n", active, !active);

    if (active) {
        QProcess::execute("dbus-send", {"--session", "--print-reply",
            "--dest=org.kde.KWin", "/VirtualKeyboard",
            "org.freedesktop.DBus.Properties.Set",
            "string:org.kde.kwin.VirtualKeyboard", "string:active",
            "variant:boolean:false"});
    } else {
        QProcess::execute("dbus-send", {"--session", "--print-reply",
            "--dest=org.kde.KWin", "/VirtualKeyboard",
            "org.kde.kwin.VirtualKeyboard.forceActivate"});
    }
}

void Backend::showDesktop() {
    // Toggle KDE show desktop via D-Bus
    QProcess p;
    p.start("dbus-send", {"--session", "--print-reply",
        "--dest=org.kde.KWin", "/KWin",
        "org.freedesktop.DBus.Properties.Get",
        "string:org.kde.KWin", "string:showingDesktop"});
    bool showing = false;
    if (p.waitForFinished(1000))
        showing = p.readAllStandardOutput().contains("true");

    QProcess::startDetached("dbus-send", {"--session", "--type=method_call",
        "--dest=org.kde.KWin", "/KWin",
        "org.kde.KWin.showDesktop", "boolean:" + QString(showing ? "false" : "true")});
}

void Backend::endTask() {
    QProcess::startDetached("ydotool", {"key", "alt+F4"});
}

void Backend::cycleOperatingMode() {
    QString current = profile();
    QString next;
    if (current == "low-power") next = "balanced";
    else if (current == "balanced") next = "performance";
    else next = "low-power";
    setProfile(next);

    // In performance mode, push all TDP values to their hardware max
    if (next == "performance") {
        QTimer::singleShot(600, this, [this] {
            setTdpSpl(tdpMax("ppt_pl1_spl"));
            setTdpSppt(tdpMax("ppt_pl2_sppt"));
            setTdpFppt(tdpMax("ppt_pl3_fppt"));
        });
    }
}

void Backend::suspend() {
    QProcess::startDetached("systemctl", {"suspend"});
}
