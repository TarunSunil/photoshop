#include "editor/DocumentController.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("LumenForge");
    QGuiApplication::setOrganizationName("LumenForge");

    QQuickStyle::setStyle("Fusion");

    DocumentController documentController;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("documentController", &documentController);
    engine.loadFromModule("LumenForge", "Main");

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return QGuiApplication::exec();
}
