#include "mcp_plugin.h"
#include "mcp_server.h"
#include "fooyin_wrapper.h"
#include "mcp_logging.h"
#include <QJsonObject>
#include <QString>
#include <memory>
#include <cmath>

namespace FooyinMCP {

namespace {
QString playStateToString(Fooyin::PlayState s) {
    switch (s) {
        case Fooyin::PlayState::Playing: return QStringLiteral("playing");
        case Fooyin::PlayState::Paused:  return QStringLiteral("paused");
        case Fooyin::PlayState::Stopped: return QStringLiteral("stopped");
        default: break;
    }
    return QStringLiteral("unknown");
}
} // namespace

McpPlugin::McpPlugin(QObject* parent) : QObject(parent) {}

McpPlugin::~McpPlugin() {
    McpPlugin::shutdown();
}

void McpPlugin::initialise(const Fooyin::CorePluginContext& context) {
    shutdown();

    qCDebug(lcMcp) << "Initializing MCP Plugin...";

    // Ownership: Plugin owns Wrapper and Server
    m_wrapper = std::make_unique<FooyinMCP::FooyinWrapper>(context);
    m_server = std::make_unique<FooyinMCP::McpServer>(m_wrapper.get());

    connect(m_wrapper.get(), &FooyinMCP::FooyinWrapper::nowPlayingChanged, this, [this](const QJsonObject& track) {
        if (m_server) {
            m_server->broadcastNotification(
                QStringLiteral("notifications/track_changed"),
                QJsonObject{{QStringLiteral("track"), track}}
            );
        }
    });

    connect(m_wrapper.get(), &FooyinMCP::FooyinWrapper::playbackStateChanged, this, [this](Fooyin::PlayState state) {
        if (m_server) {
            m_server->broadcastNotification(
                QStringLiteral("notifications/playback_state"),
                QJsonObject{{QStringLiteral("state"), playStateToString(state)}}
            );
        }
    });

    connect(m_wrapper.get(), &FooyinMCP::FooyinWrapper::volumeChanged, this, [this](float volume) {
        if (m_server) {
            double roundedVol = std::round(static_cast<double>(volume) * 100.0) / 100.0;
            m_server->broadcastNotification(
                QStringLiteral("notifications/volume_changed"),
                QJsonObject{{QStringLiteral("volume"), roundedVol}}
            );
        }
    });

    if (m_server->start()) {
        qCDebug(lcMcp) << "Server started. Pipe:" << m_server->pipeName() << "Port:" << m_server->port();
    } else {
        qCCritical(lcMcp) << "Failed to start MCP server!";
        shutdown();
    }
}

void McpPlugin::shutdown() {
    if (m_server) {
        m_server->stop();
        m_server.reset();
    }
    m_wrapper.reset();
}

} // namespace FooyinMCP
