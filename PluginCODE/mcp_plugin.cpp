#include "mcp_plugin.h"
#include <QJsonObject>
#include <memory>
#include <cmath>

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

namespace FooyinMCP {

McpPlugin::McpPlugin(QObject* parent) : QObject(parent) {}

McpPlugin::~McpPlugin() {
    McpPlugin::shutdown();
}

void McpPlugin::initialise(const Fooyin::CorePluginContext& context) {
    // 1. Guard against double initialization
    shutdown();

    qCDebug(lcMcp) << "Initializing MCP Plugin...";

    // 2. Register MetaTypes for thread-safe Signal/Slot communication
    qRegisterMetaType<Fooyin::PlayState>("Fooyin::PlayState");

    m_wrapper = std::make_unique<FooyinMCP::FooyinWrapper>(context);
    m_server = std::make_unique<FooyinMCP::McpServer>(m_wrapper.get());

    // 3. Connect Wrapper signals to Server broadcasts
    // Using AutoConnection (default) - Qt handles thread safety via the registered metatype

    connect(m_wrapper.get(), &FooyinMCP::FooyinWrapper::nowPlayingChanged, this, [this](const QJsonObject& track) {
        if (m_server) {
            m_server->broadcastNotification(QStringLiteral("notifications/track_changed"),
                                            {{QStringLiteral("track"), track}});
        }
    });

    connect(m_wrapper.get(), &FooyinMCP::FooyinWrapper::playbackStateChanged, this, [this](Fooyin::PlayState state) {
        if (m_server) {
            m_server->broadcastNotification(QStringLiteral("notifications/playback_state"),
                                            {{QStringLiteral("state"), playStateToString(state)}});
        }
    });

    connect(m_wrapper.get(), &FooyinMCP::FooyinWrapper::volumeChanged, this, [this](float volume) {
        if (m_server) {
            // Using double for JSON to avoid float precision artifacts
            double roundedVol = std::round(static_cast<double>(volume) * 100.0) / 100.0;
            m_server->broadcastNotification(QStringLiteral("notifications/volume_changed"),
                                            {{QStringLiteral("volume"), roundedVol}});
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
    if (m_server) {
        m_server->stop();
        m_server.reset();
    }
    m_wrapper.reset();
}

} // namespace FooyinMCP
