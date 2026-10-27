#pragma once

#include <core/plugins/plugin.h>
#include <core/plugins/coreplugin.h>
#include <QObject>
#include <QHash>
#include <QLoggingCategory>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <memory>
#include "mcp_logging.h"

namespace FooyinMCP {

    class McpServer;
    class FooyinWrapper;

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
        void flushPlaylistUpdates();

        std::unique_ptr<FooyinWrapper> m_wrapper;
        std::unique_ptr<McpServer> m_server;
        QPointer<QObject> m_playlistHandler; // Fooyin::PlaylistHandler

        // Fasst viele playlistUpdated-Signale (z. B. bei Massenaenderungen) zu einer Benachrichtigung zusammen.
        QTimer m_playlistUpdateTimer;
        QHash<QString, int> m_pendingPlaylistUpdates;
    };

} // namespace FooyinMCP