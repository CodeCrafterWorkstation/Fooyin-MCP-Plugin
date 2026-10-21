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

    // Playlist & Library Operations
    QJsonObject addTrack(const QString& path);
    QJsonObject enqueue(const QString& path);
    QJsonObject scanLibrary();

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