#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <cstdint>
#include <core/plugins/coreplugin.h>
#include <core/player/playercontroller.h>
#include <core/track.h>

namespace FooyinMCP {

    class FooyinWrapper : public QObject {
        Q_OBJECT
    public:
        static constexpr int DEFAULT_SEARCH_LIMIT = 100;
        static constexpr int MAX_SEARCH_LIMIT = 500;

        explicit FooyinWrapper(const Fooyin::CorePluginContext& context, QObject* parent = nullptr);

        // Alle Methoden liefern {"ok": bool, "error": string (nur bei ok=false), ...}.

        // Playback Controls
        QJsonObject play();
        QJsonObject pause();
        QJsonObject stop();
        QJsonObject next();
        QJsonObject previous();
        QJsonObject seek(uint64_t ms);
        QJsonObject setPlayMode(const QString& mode);

        // Playlist & Library Operations
        QJsonObject addTrack(const QString& path, const QString& playlistName = QString(), bool createIfMissing = false);
        QJsonObject enqueue(const QString& path);
        QJsonObject createPlaylist(const QString& name);
        QJsonObject listPlaylists();
        QJsonObject getPlaylistTracks(const QString& name);
        QJsonObject removePlaylist(const QString& name);
        QJsonObject sortPlaylist(const QString& name, const QString& sort, const QString& order = QString());
        QJsonObject scanLibrary();
        QJsonObject searchLibrary(const QString& query, const QString& sort = QString(), const QString& order = QString(),
                                  int limit = DEFAULT_SEARCH_LIMIT, int offset = 0);
        QJsonObject updateTrackMetadata(const QJsonArray& trackIds, const QJsonObject& metadata, bool writeToDisk);

        Fooyin::ScriptContextService* scriptContext() const { return m_context.scriptContext; }

        // Player Status
        QJsonObject getNowPlaying();
        QJsonObject getPlaybackState();

        signals:
                void nowPlayingChanged(const QJsonObject& track);
        void playbackStateChanged(const QString& state);

    private:
        QJsonObject trackToJson(const Fooyin::Track& track);
        void emitPlaybackStateIfChanged();

        Fooyin::CorePluginContext m_context;
        QString m_lastState;
    };

} // namespace FooyinMCP