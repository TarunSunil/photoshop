#include "editor/DocumentController.hpp"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QDebug>
#include <iostream>
#include <stdexcept>
#ifdef Q_OS_WIN
#  include <windows.h>
#endif

// Show a fatal error in a way that is visible regardless of whether this is
// a Debug console build or a Release WIN32 GUI build (no console window).
// Uses the native Win32 MessageBoxW so there is no dependency on Qt Widgets.
static void fatalError(const QString& title, const QString& text)
{
    // Always write to Qt's log (visible in debuggers, Qt Creator output, etc.)
    qCritical() << title << ":" << text;
#ifdef Q_OS_WIN
    // In a WIN32/Release build there is no console window, so qCritical is
    // invisible.  Pop a native message box so the user can see why it failed.
    const QString msg = title + "\n\n" + text;
    MessageBoxW(nullptr,
                reinterpret_cast<const wchar_t*>(msg.utf16()),
                reinterpret_cast<const wchar_t*>(title.utf16()),
                MB_OK | MB_ICONERROR);
#endif
}

int main(int argc, char* argv[])
{
    std::cout << "MAIN STARTED" << std::endl;

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("LumenForge");
    QGuiApplication::setOrganizationName("LumenForge");
    QGuiApplication::setApplicationVersion("0.1.0");
    QQuickStyle::setStyle("Fusion");

    // DocumentController is constructed here.  Previously InpaintEngine and
    // UpscaleEngine were direct members, so their constructors ran here and
    // tried to initialise Ort::Env — which loads onnxruntime.dll.  If the DLL
    // or any dependency was missing the process terminated silently.  Those
    // engines are now unique_ptrs (lazy-initialised on first AI use), so this
    // constructor is safe.  The try/catch is kept as a safety net.
    qDebug() << "Creating DocumentController";
    DocumentController* docCtrl = nullptr;
    try {
        docCtrl = new DocumentController();
    } catch (const std::exception& e) {
        fatalError("Startup error",
                   QString("DocumentController initialisation failed:\n%1")
                       .arg(QString::fromUtf8(e.what())));
        return -1;
    } catch (...) {
        fatalError("Startup error",
                   "DocumentController initialisation failed with an unknown exception.");
        return -1;
    }
    qDebug() << "DocumentController created";

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("documentController", docCtrl);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        [&]() {
            fatalError("QML error", "Failed to create the root QML object.\n"
                       "Check the Qt Quick module deployment and QML syntax.");
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection
    );

    qDebug() << "Loading QML module";
    engine.loadFromModule("LumenForge", "Main");
    qDebug() << "Root objects:" << engine.rootObjects().size();

    if (engine.rootObjects().isEmpty()) {
        fatalError("QML error", "No root QML object was created.\n"
                   "The LumenForge QML module could not be loaded.");
        delete docCtrl;
        return -1;
    }

    // Transfer ownership to the engine so docCtrl is cleaned up with the engine.
    docCtrl->setParent(engine.rootObjects().first());

    qDebug() << "Entering event loop";
    return QGuiApplication::exec();
}