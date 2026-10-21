#include "fooyin_wrapper.h"
#include <core/player/playercontroller.h>
#include <core/library/musiclibrary.h>
#include <core/playlist/playlisthandler.h>
#include <core/playlist/playlist.h>
#include <core/track.h>

#include <QJsonArray>
#include <QFileInfo>

namespace FooyinMCP {

FooyinWrapper::FooyinWrapper(const Fooyin::CorePluginContext& context, QObject* parent)
    : QObject(parent), m_context(context) {

    auto* player = m_context.playerController;
    if (player) {
        connect(player, &Fooyin::PlayerController::currentTrackChanged, this, [this](const Fooyin::Track& track) {
            emit nowPlayingChanged(trackToJson(track));
            emit playbackStateChanged(getPlaybackState().value(QStringLiteral("state")).toString());
        });
    }
}

QJsonObject FooyinWrapper::trackToJson(const Fooyin::Track& track) {
    if (!track.isValid()) return QJsonObject();
    QJsonObject obj;
    obj[QStringLiteral("title")] = track.title();
    obj[QStringLiteral("artist")] = track.artist();
    obj[QStringLiteral("album")] = track.album();
    obj[QStringLiteral("path")] = track.filepath();
    obj[QStringLiteral("duration")] = static_cast<double>(track.duration());
    return obj;
}

QJsonObject FooyinWrapper::play() {
    if (!m_context.playerController) 
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Player not available")}};
    m_context.playerController->play();
    return QJsonObject{{QStringLiteral("ok"), true}};
}

QJsonObject FooyinWrapper::pause() {
    if (!m_context.playerController) 
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Player not available")}};
    m_context.playerController->pause();
    return QJsonObject{{QStringLiteral("ok"), true}};
}

QJsonObject FooyinWrapper::stop() {
    if (!m_context.playerController) 
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Player not available")}};
    m_context.playerController->stop();
    return QJsonObject{{QStringLiteral("ok"), true}};
}

QJsonObject FooyinWrapper::next() {
    if (!m_context.playerController) 
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Player not available")}};
    m_context.playerController->next();
    return QJsonObject{{QStringLiteral("ok"), true}};
}

QJsonObject FooyinWrapper::previous() {
    if (!m_context.playerController) 
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Player not available")}};
    m_context.playerController->previous();
    return QJsonObject{{QStringLiteral("ok"), true}};
}

QJsonObject FooyinWrapper::setVolume(float volume) {
    if (!m_context.playerController) 
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Player not available")}};
    
    float vol = qBound(0.0f, volume, 1.0f);
    return QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("volume"), static_cast<double>(vol)}};
}

QJsonObject FooyinWrapper::addTrack(const QString& path) {
    if (!m_context.playlistHandler) 
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Playlist handler not available")}};

    QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) {
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("File not found")}};
    }

    Fooyin::Track track(path);
    if (!track.isValid()) {
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Invalid track file")}};
    }

    // Vorhandene Tracks der Standard-Playlist laden und neuen Track anhängen
    Fooyin::TrackList currentTracks;
    if (auto* pl = m_context.playlistHandler->playlistByName(QStringLiteral("MCP Playlist"))) {
        currentTracks = pl->tracks();
    }
    currentTracks.push_back(track);

    // Playlist über den Handler aktualisieren/erstellen
    m_context.playlistHandler->createPlaylist(QStringLiteral("MCP Playlist"), currentTracks);
    return QJsonObject{{QStringLiteral("ok"), true}};
}

QJsonObject FooyinWrapper::enqueue(const QString& path) {
    if (!m_context.playlistHandler)
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("PlaylistHandler not available")}};

    QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) {
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("File not found")}};
    }

    Fooyin::Track trackToEnqueue;

    if (m_context.library) {
        auto tracks = m_context.library->tracks();
        for (const auto& track : tracks) {
            if (track.filepath() == path) {
                trackToEnqueue = track;
                break;
            }
        }
    }

    if (!trackToEnqueue.isValid()) {
        trackToEnqueue = Fooyin::Track(path);
    }

    if (!trackToEnqueue.isValid()) {
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Invalid track file")}};
    }

    Fooyin::TrackList currentTracks;
    if (auto* pl = m_context.playlistHandler->playlistByName(QStringLiteral("MCP Playlist"))) {
        currentTracks = pl->tracks();
    }
    currentTracks.push_back(trackToEnqueue);

    m_context.playlistHandler->createPlaylist(QStringLiteral("MCP Playlist"), currentTracks);
    return QJsonObject{{QStringLiteral("ok"), true}};
}

QJsonObject FooyinWrapper::scanLibrary() {
    if (!m_context.library) 
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Library not available")}};
    m_context.library->refreshAll();
    return QJsonObject{{QStringLiteral("ok"), true}};
}

QJsonObject FooyinWrapper::getNowPlaying() {
    if (!m_context.playerController) 
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Player not available")}};
    
    auto* player = m_context.playerController;
    return QJsonObject{
        {QStringLiteral("ok"), true},
        {QStringLiteral("track"), trackToJson(player->currentTrack())},
        {QStringLiteral("position"), static_cast<double>(player->currentPosition())}
    };
}

QJsonObject FooyinWrapper::getPlaybackState() {
    if (!m_context.playerController) 
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Player not available")}};
    
    auto* player = m_context.playerController;
    QString state = QStringLiteral("stopped");
    if (player->currentTrack().isValid()) {
        state = QStringLiteral("playing");
    }

    return QJsonObject{
        {QStringLiteral("ok"), true},
        {QStringLiteral("state"), state}
    };
}

} // namespace FooyinMCP