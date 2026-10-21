#pragma once

#include <QObject>
#include <QJsonObject>
#include <core/plugins/coreplugin.h>
#include <core/player/playercontroller.h>

namespace FooyinMCP {

class FooyinWrapper : public QObject {
    Q_OBJECT
public:
    explicit FooyinWrapper(const Fooyin::CorePluginContext& context, QObject* parent = nullptr);

    // Playback
    QJsonObject play();
    QJsonObject pause();
    QJsonObject stop();
    QJsonObject next();
    QJsonObject previous();
    QJsonObject setVolume(float volume);

    // Library / Playlists
    QJsonObject addTrack(const QString& path);
    QJsonObject enqueue(const QString& path);
    QJsonObject scanLibrary();

    // Info
    QJsonObject getNowPlaying();
    QJsonObject getPlaybackState();

signals:
    void nowPlayingChanged(const QJsonObject& track);
    void playbackStateChanged(Fooyin::PlayState state);
    void volumeChanged(float volume);

private:
    QJsonObject trackToJson(const Fooyin::Track& track);
    Fooyin::CorePluginContext m_context;
};

} // namespace FooyinMCP
