#include "backend.h"

#include <QDir>
#include <QGuiApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSocketNotifier>
#include <QTimer>

#include <csignal>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

static const char *SOCKET_NAME = "rog-ally-menu";

static bool sendToggle()
{
    QLocalSocket sock;
    sock.connectToServer(SOCKET_NAME);
    if (!sock.waitForConnected(500))
        return false;
    sock.write("toggle");
    sock.flush();
    sock.waitForBytesWritten(500);
    sock.disconnectFromServer();
    return true;
}

// Watch the ROG Ally hidraw device for the CC button (report: 5a a6)
static void setupCCButtonListener(QObject *root)
{
    QDir dir("/sys/class/leds/ally:rgb:joystick_rings/device/hidraw");
    if (!dir.exists()) return;
    auto entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (entries.isEmpty()) return;

    QString devPath = "/dev/" + entries.first();
    int fd = open(devPath.toUtf8().constData(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) return;

    auto *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, root);
    QObject::connect(notifier, &QSocketNotifier::activated, [fd, root] {
        unsigned char buf[64];
        while (read(fd, buf, sizeof(buf)) > 1) {
            if (buf[0] == 0x5a && buf[1] == 0xa6)
                QMetaObject::invokeMethod(root, "toggle", Qt::QueuedConnection);
        }
    });
}

// Find keyboard input devices and watch for F12
static void setupKeyListener(QObject *root)
{
    QDir dir("/dev/input");
    for (const auto &entry : dir.entryList({"event*"}, QDir::System)) {
        QString path = dir.filePath(entry);
        int fd = open(path.toUtf8().constData(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        // Check if this is a keyboard
        char name[256] = {};
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        QString devName(name);

        bool isKeyboard = devName.contains("keyboard", Qt::CaseInsensitive)
                       || devName.contains("Keyboard");

        if (!isKeyboard) {
            close(fd);
            continue;
        }

        // Check it has KEY_F12
        unsigned long keyBits[(KEY_MAX + 1) / (sizeof(long) * 8) + 1] = {};
        ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits);
        bool hasF12 = (keyBits[KEY_F12 / (sizeof(long) * 8)]
                       >> (KEY_F12 % (sizeof(long) * 8))) & 1;
        if (!hasF12) {
            close(fd);
            continue;
        }

        auto *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, root);
        QObject::connect(notifier, &QSocketNotifier::activated, [fd, root] {
            struct input_event ev;
            while (read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
                if (ev.type == EV_KEY && ev.code == KEY_F12 && ev.value == 1)
                    QMetaObject::invokeMethod(root, "toggle", Qt::QueuedConnection);
            }
        });
    }
}

int main(int argc, char *argv[])
{
    bool wantToggle = false;
    bool wantDaemon = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--toggle") == 0) wantToggle = true;
        if (strcmp(argv[i], "--daemon") == 0) wantDaemon = true;
    }

    if (wantToggle && sendToggle())
        return 0;

    QGuiApplication app(argc, argv);
    app.setApplicationName("ROG Ally Menu");
    app.setQuitOnLastWindowClosed(false);

    auto handler = [](int) { QCoreApplication::quit(); };
    signal(SIGINT, handler);
    signal(SIGTERM, handler);
    QTimer sigTimer;
    sigTimer.start(500);
    QObject::connect(&sigTimer, &QTimer::timeout, [] {});

    Backend backend;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/qml/Main.qml"));

    if (engine.rootObjects().isEmpty())
        return -1;

    QObject *root = engine.rootObjects().first();

    // IPC socket
    QLocalServer::removeServer(SOCKET_NAME);
    QLocalServer server;
    server.listen(SOCKET_NAME);
    QObject::connect(&server, &QLocalServer::newConnection, [&] {
        auto *conn = server.nextPendingConnection();
        QObject::connect(conn, &QLocalSocket::readyRead, [root, conn] {
            if (conn->readAll().trimmed() == "toggle")
                QMetaObject::invokeMethod(root, "toggle", Qt::QueuedConnection);
            conn->deleteLater();
        });
    });

    // CC button listener (hidraw)
    setupCCButtonListener(root);

    // F12 key listener
    setupKeyListener(root);

    if (wantToggle)
        QMetaObject::invokeMethod(root, "toggle", Qt::QueuedConnection);

    return app.exec();
}
