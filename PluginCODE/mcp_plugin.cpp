#include "mcp_plugin.h"
#include <QJsonObject>
#include <QString>
#include <QLoggingCategory>
#include <memory>
#include <cmath>

namespace FooyinMCP {

Q_LOGGING_CATEGORY(lcMcp, "fooyin.mcp")

namespace {

QString playStateToString(Fooyin::PlayState s) {
    switch (s) {
        case Fooyin::PlayState::Playing: return QStringLiteral("playing");
        case Fooyin::PlayState::Paused:  return QStringLiteral("paused");
        case Fooyin::PlayState::Stopped: return QStringLiteral("stopped");
    }
    return QStringLiteral("unknown");
}

} // namespace

McpPlugin::McpPlugin(QObject* parent) : QObject(parent) {}

McpPlugin::~McpPlugin() {
    McpPlugin::shutdown();
}

void McpPlugin::initialise(const Fooyin::CorePluginContext& context) {
    // 1. Guard against double initialization
    shutdown();

    qCDebug(lcMcp) << "Initializing MCP Plugin...";

    m_wrapper = std::make_unique<FooyinMCP::FooyinWrapper>(context);
    m_server = std::make_unique<FooyinMCP::McpServer>(m_wrapper.get());

    // 2. Connect Wrapper signals to Server broadcasts
    // Using AutoConnection (default) - Qt handles thread safety for fully defined types.

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
            // Using double for JSON to avoid float precision artifacts
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
        qCCritical(lcMcp) << "Failed to start MCP server! Cleaning up...";
        shutdown();
    }
}

void McpPlugin::shutdown() {
    // Explicit destruction order: Server relies on Wrapper, so kill Server first
    if (m_server) {
        m_server->stop();
        m_server.reset();
    }
    m_wrapper.reset();
}

} // namespace FooyinMCP
