#include "mcp_plugin.h"
#include "mcp_server.h"
#include "fooyin_wrapper.h"
#include "mcp_logging.h"
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QString>
#include <memory>
#include <core/playlist/playlisthandler.h>
#include <core/playlist/playlist.h>

namespace FooyinMCP {

    namespace {

        constexpr int kMinTokenLength = 32; // hex-Zeichen (128 Bit)

// Liest das Zugriffstoken aus der Konfigurationsdatei oder erzeugt ein neues (256 Bit, kryptografisch
// sicher, Datei nur fuer den aktuellen Benutzer lesbar). Gibt einen leeren String zurueck, wenn das
// Token nicht gespeichert werden kann (dann startet der Server den WebSocket nicht).
        QString loadOrCreateToken(QString& tokenFilePath) {
            const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            if (dir.isEmpty() || !QDir().mkpath(dir)) {
                qCWarning(lcMcp) << "Could not determine/create data directory for the token file.";
                return QString();
            }

            tokenFilePath = dir + QStringLiteral("/mcp_token");

            {
                QFile file(tokenFilePath);
                if (file.exists() && file.open(QIODevice::ReadOnly)) {
                    const QString existing = QString::fromUtf8(file.readAll()).trimmed();
                    if (existing.size() >= kMinTokenLength) return existing;
                }
            }

            quint32 words[8];
            QRandomGenerator::system()->fillRange(words);
            const QString token = QString::fromLatin1(
                    QByteArray(reinterpret_cast<const char*>(words), static_cast<int>(sizeof(words))).toHex());

            QFile file(tokenFilePath);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                qCWarning(lcMcp) << "Could not write token file:" << tokenFilePath << file.errorString();
                return QString();
            }
            // Rechte setzen, bevor das Token in die Datei geschrieben wird.
            file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
            file.write(token.toUtf8());
            file.close();
            return token;
        }

    } // namespace

    McpPlugin::McpPlugin(QObject* parent) : QObject(parent) {
        m_playlistUpdateTimer.setSingleShot(true);
        m_playlistUpdateTimer.setInterval(150);
        connect(&m_playlistUpdateTimer, &QTimer::timeout, this, [this]() { flushPlaylistUpdates(); });
    }

    McpPlugin::~McpPlugin() {
        McpPlugin::shutdown();
    }

    void McpPlugin::flushPlaylistUpdates() {
        if (!m_server) {
            m_pendingPlaylistUpdates.clear();
            return;
        }
        const auto pending = m_pendingPlaylistUpdates;
        m_pendingPlaylistUpdates.clear();
        for (auto it = pending.cbegin(); it != pending.cend(); ++it) {
            m_server->broadcastNotification(
                    QStringLiteral("notifications/playlist_updated"),
                    QJsonObject{{QStringLiteral("name"), it.key()}, {QStringLiteral("trackCount"), it.value()}}
            );
        }
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

        auto* playlistHandler = context.playlistHandler;
        m_playlistHandler = playlistHandler;
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
                    m_pendingPlaylistUpdates.insert(playlist->name(), static_cast<int>(playlist->trackCount()));
                    if (!m_playlistUpdateTimer.isActive()) m_playlistUpdateTimer.start();
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

        QString tokenFile;
        const QString token = loadOrCreateToken(tokenFile);

        // pipeName()/port() liefern hier die Standardwerte des Servers.
        if (m_server->start(m_server->pipeName(), m_server->port(), token)) {
            qCDebug(lcMcp) << "Server started. Pipe:" << m_server->pipeName() << "Port:" << m_server->port();
            if (!token.isEmpty()) {
                qCDebug(lcMcp) << "WebSocket clients must connect to ws://127.0.0.1:" << m_server->port()
                               << "/?token=<token> (token file:" << tokenFile << ")";
            }
        } else {
            qCCritical(lcMcp) << "Failed to start MCP server! Pipe:" << m_server->pipeName()
                              << "Port:" << m_server->port() << "- is another instance running?";
            shutdown();
        }
    }

    void McpPlugin::shutdown() {
        m_playlistUpdateTimer.stop();
        m_pendingPlaylistUpdates.clear();

        if (m_playlistHandler) {
            m_playlistHandler->disconnect(this);
            m_playlistHandler.clear();
        }
        if (m_wrapper) {
            m_wrapper->disconnect(this);
        }

        if (m_server) {
            m_server->stop();
            m_server.reset();
        }
        m_wrapper.reset();
    }

} // namespace FooyinMCP