#pragma once

#include <core/plugins/plugin.h>
#include <core/plugins/coreplugin.h>
#include <QObject>
#include <memory>

namespace FooyinMCP {
class McpServer;
class FooyinWrapper;
}

class McpPlugin : public QObject, public Fooyin::Plugin, public Fooyin::CorePlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID FOOYIN_PLUGIN_IID FILE "metadata.json")
    Q_INTERFACES(Fooyin::Plugin Fooyin::CorePlugin)

public:
    explicit McpPlugin(QObject* parent = nullptr);
    ~McpPlugin() override;

    // Fooyin::CorePlugin interface
    void initialise(const Fooyin::CorePluginContext& context) override;

    // Fooyin::Plugin interface
    void shutdown() override;

private:
    std::unique_ptr<FooyinMCP::FooyinWrapper> m_wrapper;
    std::unique_ptr<FooyinMCP::McpServer> m_server;
};
