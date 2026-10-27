#include "fooyin_wrapper.h"
#include <core/player/playercontroller.h>
#include <core/library/musiclibrary.h>
#include <core/playlist/playlisthandler.h>
#include <core/playlist/playlist.h>
#include <core/track.h>
#include <core/engine/enginecontroller.h>
#include <core/scripting/trackqueryfilter.h>
#include <core/library/tracksort.h>
#include <core/library/librarymanager.h>

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

    /*
    auto* engine = m_context.engine;
    if (engine) {
        connect(engine, &Fooyin::EngineController::volumeChanged, this, [this](double volume) {
            emit volumeChanged(static_cast<float>(volume));
        });
    }
    */
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
    // Volume API missing in EngineController, stubbing for now
    Q_UNUSED(volume);
    return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Volume control not supported in this version")}};
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

QJsonObject FooyinWrapper::sortPlaylist(const QString& name, const QString& sort, const QString& order) {
    if (!m_context.playlistHandler)
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("PlaylistHandler not available")}};

    auto* pl = m_context.playlistHandler->playlistByName(name);
    if (!pl) return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Playlist not found")}};

    Fooyin::TrackList tracks = pl->tracks();
    Qt::SortOrder sortOrder = (order.toLower() == QStringLiteral("desc")) ? Qt::DescendingOrder : Qt::AscendingOrder;

    Fooyin::LibraryInfoMap libraries;
    if (m_context.libraryManager) {
        libraries = m_context.libraryManager->allLibraries();
    }

    Fooyin::TrackSorter sorter(libraries);
    tracks = sorter.calcSortTracks(sort, tracks, sortOrder);

    m_context.playlistHandler->replacePlaylistTracks(pl->id(), tracks);
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

QJsonObject FooyinWrapper::searchLibrary(const QString& query, const QString& sort, const QString& order) {
    if (!m_context.library)
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Library not available")}};

    Fooyin::TrackList tracks = m_context.library->tracks();

    // 1. Filter using TrackQueryFilter
    Fooyin::TrackQueryFilter filter;
    tracks = filter.filter(query, tracks);

    // 2. Sort if requested
    if (!sort.isEmpty()) {
        Qt::SortOrder sortOrder = (order.toLower() == QStringLiteral("desc")) ? Qt::DescendingOrder : Qt::AscendingOrder;

        Fooyin::LibraryInfoMap libraries;
        if (m_context.libraryManager) {
            libraries = m_context.libraryManager->allLibraries();
        }

        Fooyin::TrackSorter sorter(libraries);
        tracks = sorter.calcSortTracks(sort, tracks, sortOrder);
    }

    QJsonArray arr;
    int count = 0;
    for (const auto& track : tracks) {
        arr.append(trackToJson(track));
        count++;
        if (count > 100) break; // Limit results
    }
    return QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("tracks"), arr}};
}

QJsonObject FooyinWrapper::scanLibrary() {
    if (!m_context.library) 
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Library not available")}};
    m_context.library->refreshAll();
    return QJsonObject{{QStringLiteral("ok"), true}};
}

QJsonObject FooyinWrapper::updateTrackMetadata(const QJsonArray& trackIds, const QJsonObject& metadata, bool writeToDisk)
{
    auto library = m_context.library;
    if (!library) {
        QJsonObject err;
        err[QStringLiteral("error")] = QStringLiteral("Library not available");
        return err;
    }

    Fooyin::TrackIds ids;
    for (const auto& val : trackIds) {
        if (val.isDouble()) ids.push_back(static_cast<int>(val.toDouble()));
    }

    if (ids.empty()) {
        QJsonObject err;
        err[QStringLiteral("error")] = QStringLiteral("No valid track IDs provided");
        return err;
    }

    Fooyin::TrackList tracks = library->tracksForIds(ids);
    if (tracks.empty()) {
        QJsonObject err;
        err[QStringLiteral("error")] = QStringLiteral("Tracks not found");
        return err;
    }

    for (auto& track : tracks) {
        if (metadata.contains("title")) track.setTitle(metadata["title"].toString());
        if (metadata.contains("album")) track.setAlbum(metadata["album"].toString());
        if (metadata.contains("year")) track.setYear(metadata["year"].toInt());
        if (metadata.contains("trackNumber")) track.setTrackNumber(metadata["trackNumber"].toString());
        if (metadata.contains("trackTotal")) track.setTrackTotal(metadata["trackTotal"].toString());
        if (metadata.contains("discNumber")) track.setDiscNumber(metadata["discNumber"].toString());
        if (metadata.contains("discTotal")) track.setDiscTotal(metadata["discTotal"].toString());
        if (metadata.contains("comment")) track.setComment(metadata["comment"].toString());
        if (metadata.contains("date")) track.setDate(metadata["date"].toString());

        if (metadata.contains("artists")) {
            QStringList artists;
            for (const auto& a : metadata["artists"].toArray()) artists << a.toString();
            track.setArtists(artists);
        }
        if (metadata.contains("albumArtists")) {
            QStringList artists;
            for (const auto& a : metadata["albumArtists"].toArray()) artists << a.toString();
            track.setAlbumArtists(artists);
        }
        if (metadata.contains("genres")) {
            QStringList genres;
            for (const auto& g : metadata["genres"].toArray()) genres << g.toString();
            track.setGenres(genres);
        }

        // Special handling for Rating
        if (metadata.contains("rating")) {
            double r = metadata["rating"].toDouble();
            track.setRating(static_cast<float>(r));
        }
    }

    if (writeToDisk) {
        library->writeTrackMetadata(tracks);
    } else {
        library->updateTrackMetadata(tracks);
    }

    QJsonObject res;
    res[QStringLiteral("status")] = QStringLiteral("success");
    res[QStringLiteral("updatedCount")] = static_cast<int>(tracks.size());
    return res;
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