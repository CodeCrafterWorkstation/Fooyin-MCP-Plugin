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
    
    // Note: In fooyin_core, volume is usually handled via AudioEngine or similar,
    // but PlayerController might not have a direct setVolume.
    // If it's not in PlayerController, we'd need AudioEngine.
    // For now, let's keep the logic or fix based on available headers.
    // Actually, looking at PlayerController.h, setVolume is NOT there.
    // It's likely in AudioEngine.
    return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("setVolume not implemented in PlayerController")}};
}

QJsonObject FooyinWrapper::seek(uint64_t ms) {
    if (!m_context.playerController)
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Player not available")}};
    m_context.playerController->seek(ms);
    return QJsonObject{{QStringLiteral("ok"), true}};
}

QJsonObject FooyinWrapper::setPlayMode(const QString& mode) {
    if (!m_context.playerController)
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Player not available")}};

    Fooyin::Playlist::PlayModes m = Fooyin::Playlist::Default;
    if (mode == QStringLiteral("shuffle")) m = Fooyin::Playlist::ShuffleTracks;
    else if (mode == QStringLiteral("repeatTrack")) m = Fooyin::Playlist::RepeatTrack;
    else if (mode == QStringLiteral("repeatPlaylist")) m = Fooyin::Playlist::RepeatPlaylist;

    m_context.playerController->setPlayMode(m);
    return QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("mode"), mode}};
}

QJsonObject FooyinWrapper::addTrack(const QString& path, const QString& playlistName) {
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

    auto* pl = playlistName.isEmpty() ? m_context.playlistHandler->activePlaylist()
                                      : m_context.playlistHandler->playlistByName(playlistName);

    if (!pl && !playlistName.isEmpty()) {
        pl = m_context.playlistHandler->createPlaylist(playlistName);
    }

    if (!pl) pl = m_context.playlistHandler->playlistByName(QStringLiteral("MCP Playlist"));
    if (!pl) pl = m_context.playlistHandler->createPlaylist(QStringLiteral("MCP Playlist"));

    if (!pl) return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Could not find or create playlist")}};

    Fooyin::TrackList currentTracks = pl->tracks();
    currentTracks.push_back(track);
    m_context.playlistHandler->replacePlaylistTracks(pl->id(), currentTracks);

    return QJsonObject{{QStringLiteral("ok"), true}};
}

QJsonObject FooyinWrapper::createPlaylist(const QString& name) {
    if (!m_context.playlistHandler)
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("PlaylistHandler not available")}};

    m_context.playlistHandler->createPlaylist(name);
    return QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("name"), name}};
}

QJsonObject FooyinWrapper::listPlaylists() {
    if (!m_context.playlistHandler)
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("PlaylistHandler not available")}};

    QJsonArray arr;
    for (auto* pl : m_context.playlistHandler->playlists()) {
        QJsonObject obj;
        obj[QStringLiteral("name")] = pl->name();
        obj[QStringLiteral("trackCount")] = pl->trackCount();
        arr.append(obj);
    }
    return QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("playlists"), arr}};
}

QJsonObject FooyinWrapper::getPlaylistTracks(const QString& name) {
    if (!m_context.playlistHandler)
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("PlaylistHandler not available")}};

    auto* pl = m_context.playlistHandler->playlistByName(name);
    if (!pl) return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Playlist not found")}};

    QJsonArray arr;
    for (const auto& track : pl->tracks()) {
        arr.append(trackToJson(track));
    }
    return QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("tracks"), arr}};
}

QJsonObject FooyinWrapper::removePlaylist(const QString& name) {
    if (!m_context.playlistHandler)
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("PlaylistHandler not available")}};

    auto* pl = m_context.playlistHandler->playlistByName(name);
    if (!pl) return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Playlist not found")}};

    m_context.playlistHandler->removePlaylist(pl->id());
    return QJsonObject{{QStringLiteral("ok"), true}};
}

QJsonObject FooyinWrapper::enqueue(const QString& path) {
    if (!m_context.playerController)
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("PlayerController not available")}};

    Fooyin::Track track(path);
    if (!track.isValid()) return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Invalid track")}};

    m_context.playerController->queueTrack(track);
    return QJsonObject{{QStringLiteral("ok"), true}};
}

QJsonObject FooyinWrapper::searchLibrary(const QString& query) {
    if (!m_context.library)
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Library not available")}};

    // Simplified search: just filter by title/artist in memory for now
    // A real search would use the database query
    QJsonArray arr;
    int count = 0;
    for (const auto& track : m_context.library->tracks()) {
        if (track.title().contains(query, Qt::CaseInsensitive) ||
            track.artist().contains(query, Qt::CaseInsensitive) ||
            track.album().contains(query, Qt::CaseInsensitive)) {
            arr.append(trackToJson(track));
            count++;
            if (count > 100) break; // Limit results
        }
    }
    return QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("tracks"), arr}};
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