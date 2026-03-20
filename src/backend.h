#pragma once

#include <QColor>
#include <QDir>
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/hidraw.h>
#include <unistd.h>

class Backend : public QObject {
    Q_OBJECT

    // Sensors
    Q_PROPERTY(double cpuTemp MEMBER m_cpuTemp NOTIFY statsChanged)
    Q_PROPERTY(double gpuTemp MEMBER m_gpuTemp NOTIFY statsChanged)
    Q_PROPERTY(int cpuFan MEMBER m_cpuFan NOTIFY statsChanged)
    Q_PROPERTY(int gpuFan MEMBER m_gpuFan NOTIFY statsChanged)
    Q_PROPERTY(double gpuPower MEMBER m_gpuPower NOTIFY statsChanged)
    Q_PROPERTY(int batteryPercent MEMBER m_batteryPercent NOTIFY statsChanged)
    Q_PROPERTY(bool batteryCharging MEMBER m_batteryCharging NOTIFY statsChanged)
    Q_PROPERTY(bool acConnected MEMBER m_acConnected NOTIFY statsChanged)
    Q_PROPERTY(double batteryWatts MEMBER m_batteryWatts NOTIFY statsChanged)

    // Profile
    Q_PROPERTY(QString profile READ profile WRITE setProfile NOTIFY profileChanged)

    // TDP
    Q_PROPERTY(int tdpSpl READ tdpSpl WRITE setTdpSpl NOTIFY tdpChanged)
    Q_PROPERTY(int tdpSppt READ tdpSppt WRITE setTdpSppt NOTIFY tdpChanged)
    Q_PROPERTY(int tdpFppt READ tdpFppt WRITE setTdpFppt NOTIFY tdpChanged)

    // LED
    Q_PROPERTY(int ledBrightness READ ledBrightness WRITE setLedBrightness NOTIFY ledChanged)
    Q_PROPERTY(QColor ledColor READ ledColor WRITE setLedColor NOTIFY ledChanged)
    Q_PROPERTY(QString ledMode READ ledMode WRITE setLedMode NOTIFY ledChanged)

    // Charge limit
    Q_PROPERTY(int chargeLimit READ chargeLimit WRITE setChargeLimit NOTIFY chargeLimitChanged)

    // Screen brightness
    Q_PROPERTY(int screenBrightness READ screenBrightness WRITE setScreenBrightness NOTIFY screenBrightnessChanged)

    // Airplane mode
    Q_PROPERTY(bool airplaneMode READ airplaneMode NOTIFY statsChanged)

    // Volume
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY statsChanged)

    // Status bar
    Q_PROPERTY(QString currentTime READ currentTime NOTIFY statsChanged)
    Q_PROPERTY(bool wifiConnected MEMBER m_wifiConnected NOTIFY statsChanged)
    Q_PROPERTY(bool bluetoothConnected MEMBER m_bluetoothConnected NOTIFY statsChanged)

public:
    explicit Backend(QObject *parent = nullptr);

    // Profile
    QString profile() const;
    void setProfile(const QString &p);

    // TDP
    int tdpSpl() const;
    void setTdpSpl(int v);
    int tdpSppt() const;
    void setTdpSppt(int v);
    int tdpFppt() const;
    void setTdpFppt(int v);

    // LED
    int ledBrightness() const;
    void setLedBrightness(int pct);
    QColor ledColor() const;
    void setLedColor(const QColor &c);
    QString ledMode() const;
    void setLedMode(const QString &mode);

    // Charge limit
    int chargeLimit() const;
    void setChargeLimit(int v);

    // Screen
    int screenBrightness() const;
    void setScreenBrightness(int pct);

    // Airplane mode
    bool airplaneMode() const;

    // Volume
    int volume() const;
    void setVolume(int v);

    // Status
    QString currentTime() const;

    // Fan curves: 8 points each, temp in °C, pwm 0-255
    Q_INVOKABLE QVariantList cpuFanCurve() const;
    Q_INVOKABLE QVariantList gpuFanCurve() const;
    Q_INVOKABLE void setCpuFanPoint(int point, int temp, int pwm);
    Q_INVOKABLE void setGpuFanPoint(int point, int temp, int pwm);
    Q_INVOKABLE void applyFanPreset(const QString &preset);

    Q_INVOKABLE void refreshAll();
    Q_INVOKABLE void startPolling();
    Q_INVOKABLE void stopPolling();

    Q_INVOKABLE void cycleLedBrightness(int value);
    Q_INVOKABLE void cycleLedColor();
    Q_INVOKABLE void takeScreenshot();
    Q_INVOKABLE void showKeyboard();
    Q_INVOKABLE void showDesktop();
    Q_INVOKABLE void toggleAirplaneMode();
    Q_INVOKABLE void endTask();
    Q_INVOKABLE void cycleOperatingMode();
    Q_INVOKABLE void suspend();

signals:
    void statsChanged();
    void profileChanged();
    void tdpChanged();
    void ledChanged();
    void chargeLimitChanged();
    void screenBrightnessChanged();
    void fanCurveChanged();

private:
    // Sysfs helpers
    static QString readSysfs(const QString &path);
    static bool writeSysfs(const QString &path, const QString &value);

public:
    static int readSysfsInt(const QString &path, int def = -1);

private:

    // Hwmon discovery
    static QString findHwmon(const QString &name);
    static QString findHidraw();

    // LED HID
    bool sendHidFeature(const char *data, int len);
    void applyLed();
    void loadLedState();
    void saveLedState();

    // LED mode to ID
    static int modeId(const QString &name);
    static QString modeName(int id);

    // Hwmon paths (cached)
    QString m_k10temp;
    QString m_amdgpu;
    QString m_asus;
    QString m_hidraw;

    // Sensor values
    double m_cpuTemp = 0;
    double m_gpuTemp = 0;
    int m_cpuFan = 0;
    int m_gpuFan = 0;
    double m_gpuPower = 0;
    int m_batteryPercent = 0;
    bool m_batteryCharging = false;
    bool m_acConnected = false;
    double m_batteryWatts = 0;
    bool m_wifiConnected = false;
    bool m_bluetoothConnected = false;
    bool m_airplaneMode = false;
    int m_volume = 50;

    // LED state
    int m_ledBrightness = 100;
    QColor m_ledColor{255, 0, 64};
    QString m_ledMode = QStringLiteral("Static");

    // Paths
    static constexpr const char *ARMOURY = "/sys/class/firmware-attributes/asus-armoury/attributes";
    static constexpr const char *PROFILE_PATH = "/sys/firmware/acpi/platform_profile";
    static constexpr const char *LED_BRIGHTNESS = "/sys/class/leds/ally:rgb:joystick_rings/brightness";
    QString m_fanCurveHwmon;

    QString m_stateFile;
    QTimer m_pollTimer;
};
