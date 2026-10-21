#pragma once

#include <core/plugins/plugin.h>
#include <core/plugins/coreplugin.h>
#include <QObject>
#include <QLoggingCategory>
#include <memory>

namespace FooyinMCP {

Q_DECLARE_LOGGING_CATEGORY(lcMcp);

class McpServer;
class FooyinWrapper;

} // namespace FooyinMCP

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
