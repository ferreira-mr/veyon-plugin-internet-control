#include <QCoreApplication>
#include <QPluginLoader>
#include <QJsonObject>
#include <QJsonDocument>
#include <cstdio>
#include "PluginInterface.h"

#include <windows.h>

int main(int argc, char** argv) {
    SetDllDirectoryW(L"C:\\Program Files\\Veyon");
    QCoreApplication app(argc, argv);
    QPluginLoader loader("C:/Projetos/plugin-veyon/build/internetcontrol.dll");
    
    QByteArray meta = QJsonDocument(loader.metaData()).toJson();
    fprintf(stderr, "Metadata: %s\n", meta.constData());
    
    QObject* obj = loader.instance();
    fprintf(stderr, "Instance pointer: %p\n", (void*)obj);
    if (!obj) {
        fprintf(stderr, "Error string: %s\n", loader.errorString().toUtf8().constData());
    } else {
        auto iface = qobject_cast<PluginInterface*>(obj);
        fprintf(stderr, "PluginInterface pointer: %p\n", (void*)iface);
        if (iface) {
            fprintf(stderr, "Plugin name: %s\n", iface->name().toUtf8().constData());
        }
    }
    return 0;
}
