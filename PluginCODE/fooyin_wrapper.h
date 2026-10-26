#pragma once

#include <QObject>
#include <QJsonObject>
#include <core/plugins/coreplugin.h>
#include <core/player/playercontroller.h>
#include <core/track.h>

namespace FooyinMCP {

class FooyinWrapper : public QObject {
    Q_OBJECT
public:
    explicit FooyinWrapper(const Fooyin::CorePluginContext& context, QObject* parent = nullptr);

    // Playback Controls
    QJsonObject play();
    QJsonObject pause();
    QJsonObject stop();
    QJsonObject next();
    QJsonObject previous();
    QJsonObject setVolume(float volume);
    QJsonObject seek(uint64_t ms);
    QJsonObject setPlayMode(const QString& mode);

    // Playlist & Library Operations
    QJsonObject addTrack(const QString& path, const QString& playlistName = QString());
    QJsonObject enqueue(const QString& path);
    QJsonObject createPlaylist(const QString& name);
    QJsonObject listPlaylists();
    QJsonObject getPlaylistTracks(const QString& name);
    QJsonObject removePlaylist(const QString& name);
    QJsonObject scanLibrary();
    QJsonObject searchLibrary(const QString& query);

    // Player Status
    QJsonObject getNowPlaying();
    QJsonObject getPlaybackState();

signals:
    void nowPlayingChanged(const QJsonObject& track);
    void playbackStateChanged(const QString& state);
    void volumeChanged(float volume);

private:
    QJsonObject trackToJson(const Fooyin::Track& track);
    Fooyin::CorePluginContext m_context;
};

} // namespace FooyinMCP