#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <core/plugins/coreplugincontext.h>
#include <core/track.h>

namespace FooyinMCP {

class FooyinWrapper : public QObject {
    Q_OBJECT
public:
    explicit FooyinWrapper(const Fooyin::CorePluginContext& context, QObject* parent = nullptr);

Q_SIGNALS:
    void nowPlayingChanged(const QJsonObject& track);
    void playbackStateChanged(int state);
    void volumeChanged(float volume);

public:
    // A. Playback Engine Control
    QJsonObject controlPlayback(const QString& action, const QJsonObject& params);
    QJsonObject getNowPlaying();
    QJsonObject setVolumeAndMode(const QJsonObject& params);
    QJsonObject smartPlay(const QString& query);

    // B. Library & Search
    QJsonObject searchLibrary(const QString& query);
    QJsonObject getLibraryInfo();
    QJsonObject scanLibrary();

    // C. Playlists & Queue
    QJsonObject getPlaylists();
    QJsonObject createPlaylist(const QString& name);
    QJsonObject modifyPlaylist(const QString& action, const QJsonObject& params);
    QJsonObject manageQueue(const QString& action, const QJsonObject& params);

    // D. Metadata & Ratings
    QJsonObject setTrackRating(const QJsonObject& params);

private:
    const Fooyin::CorePluginContext& m_context;
    QJsonObject trackToJson(const Fooyin::Track& track);
};

} // namespace FooyinMCP
