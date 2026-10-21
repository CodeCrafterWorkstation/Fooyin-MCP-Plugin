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
    qDebug() << "FooyinMCP: Initializing Plugin (10/10 Version)...";

    m_wrapper = std::make_unique<FooyinMCP::FooyinWrapper>(context);
    m_server = std::make_unique<FooyinMCP::McpServer>(m_wrapper.get());

    // Connect Wrapper signals to Server broadcasts (MCP Push Notifications)
    connect(m_wrapper.get(), &FooyinMCP::FooyinWrapper::nowPlayingChanged, this, [this](const QJsonObject& track) {
        m_server->broadcastNotification("notifications/track_changed", {{"track", track}});
    });

    connect(m_wrapper.get(), &FooyinMCP::FooyinWrapper::playbackStateChanged, this, [this](int state) {
        m_server->broadcastNotification("notifications/playback_state", {{"state", state}});
    });

    connect(m_wrapper.get(), &FooyinMCP::FooyinWrapper::volumeChanged, this, [this](float volume) {
        m_server->broadcastNotification("notifications/volume_changed", {{"volume", volume}});
    });

    if (m_server->start()) {
        qDebug() << "FooyinMCP: Server started. Pipe: \\\\.\\pipe\\fooyin_mcp | WebSocket: port 8080";
    } else {
        qCritical() << "FooyinMCP: Failed to start server!";
    }
}

void McpPlugin::shutdown() {
    if (m_server) {
        m_server->stop();
    }
}
