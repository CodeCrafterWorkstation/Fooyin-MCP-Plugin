#include "plugin.h"
#include "fooyin_wrapper.h"
#include "mcp_server.h"
#include <QDebug>

McpPlugin::McpPlugin(QObject* parent)
    : QObject(parent)
{
#ifdef Q_OS_WIN
    m_pipeName = R"(\\.\pipe\fooyin_mcp)";
#else
    QString runtimeDir = qgetenv("XDG_RUNTIME_DIR");
    if (runtimeDir.isEmpty()) runtimeDir = "/tmp";
    m_pipeName = runtimeDir + "/fooyin_mcp.sock";
#endif
}

McpPlugin::~McpPlugin() = default;

void McpPlugin::initialise(const Fooyin::CorePluginContext& context)
{
    qDebug() << "[Fooyin MCP] Initializing modular plugin architecture...";

    // 1. Initialisiere die Brücke zu den fooyin-Interna
    m_wrapper = std::make_unique<FooyinMCP::FooyinWrapper>(context);

    // 2. Starte den MCP-Server
    m_server = std::make_unique<FooyinMCP::McpServer>(m_wrapper.get());

    if (m_server->start(m_pipeName)) {
        qDebug() << "[Fooyin MCP] Server listening on:" << m_pipeName;
    } else {
        qCritical() << "[Fooyin MCP] Failed to start server!";
    }
}

void McpPlugin::shutdown()
{
    qDebug() << "[Fooyin MCP] Shutting down...";
    if (m_server) {
        m_server->stop();
    }
}
