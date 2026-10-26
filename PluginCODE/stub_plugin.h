#pragma once

#include <core/plugins/plugin.h>
#include <QObject>

namespace FooyinMCP {

class StubPlugin : public QObject, public Fooyin::Plugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.fooyin.fooyin.plugin/1.0" FILE "metadata.json")
    Q_INTERFACES(Fooyin::Plugin)

public:
    explicit StubPlugin(QObject* parent = nullptr) : QObject(parent) {}
    ~StubPlugin() override = default;

    void shutdown() override {}
};

} // namespace FooyinMCP
