#include "mcp_plugin.h"
#include "mcp_server.h"
#include "fooyin_wrapper.h"
#include "mcp_logging.h"
#include <QJsonObject>
#include <QString>
#include <memory>
#include <cmath>
#include <core/playlist/playlisthandler.h>
#include <core/playlist/playlist.h>

namespace FooyinMCP {

McpPlugin::McpPlugin(QObject* parent) : QObject(parent) {}

McpPlugin::~McpPlugin() {
    McpPlugin::shutdown();
}

void McpPlugin::initialise(const Fooyin::CorePluginContext& context) {
    shutdown();

    qCDebug(lcMcp) << "Initializing MCP Plugin...";

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

    connect(m_wrapper.get(), &FooyinMCP::FooyinWrapper::playbackStateChanged, this, [this](const QString& state) {
        if (m_server) {
            m_server->broadcastNotification(
                QStringLiteral("notifications/playback_state"),
                QJsonObject{{QStringLiteral("state"), state}}
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

    auto* playlistHandler = context.playlistHandler;
    if (playlistHandler) {
        connect(playlistHandler, &Fooyin::PlaylistHandler::playlistAdded, this, [this](Fooyin::Playlist* playlist) {
            if (m_server && playlist) {
                m_server->broadcastNotification(
                    QStringLiteral("notifications/playlist_added"),
                    QJsonObject{{QStringLiteral("name"), playlist->name()}}
                );
            }
        });
        connect(playlistHandler, &Fooyin::PlaylistHandler::playlistRemoved, this, [this](Fooyin::Playlist* playlist) {
            if (m_server && playlist) {
                m_server->broadcastNotification(
                    QStringLiteral("notifications/playlist_removed"),
                    QJsonObject{{QStringLiteral("name"), playlist->name()}}
                );
            }
        });
        connect(playlistHandler, &Fooyin::PlaylistHandler::playlistUpdated, this, [this](Fooyin::Playlist* playlist) {
            if (m_server && playlist) {
                m_server->broadcastNotification(
                    QStringLiteral("notifications/playlist_updated"),
                    QJsonObject{{QStringLiteral("name"), playlist->name()}, {QStringLiteral("trackCount"), playlist->trackCount()}}
                );
            }
        });
        connect(playlistHandler, &Fooyin::PlaylistHandler::activePlaylistChanged, this, [this](Fooyin::Playlist* playlist) {
            if (m_server && playlist) {
                m_server->broadcastNotification(
                    QStringLiteral("notifications/active_playlist_changed"),
                    QJsonObject{{QStringLiteral("name"), playlist->name()}}
                );
            }
        });
    }

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