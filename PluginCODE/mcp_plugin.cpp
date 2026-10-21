#include "mcp_plugin.h"
#include "fooyin_wrapper.h"
#include "mcp_server.h"
#include <QDebug>

McpPlugin::McpPlugin(QObject* parent)
    : QObject(parent) {
}

McpPlugin::~McpPlugin() {
    shutdown();
}

void McpPlugin::initialise(const Fooyin::CorePluginContext& context) {
    qDebug() << "FooyinMCP: Initializing Plugin...";

    m_wrapper = std::make_unique<FooyinMCP::FooyinWrapper>(context);
    m_server = std::make_unique<FooyinMCP::McpServer>(m_wrapper.get());

    if (m_server->start()) {
        qDebug() << "FooyinMCP: Server started on pipe \\\\.\\pipe\\fooyin_mcp";
    } else {
        qCritical() << "FooyinMCP: Failed to start server!";
    }
}

void McpPlugin::shutdown() {
    if (m_server) {
        m_server->stop();
    }
}
