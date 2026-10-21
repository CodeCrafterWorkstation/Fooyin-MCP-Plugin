#include "fooyin_wrapper.h"
#include <core/player/playercontroller.h>
#include <core/library/musiclibrary.h>
#include <core/playlist/playlisthandler.h>
#include <core/playlist/playlist.h>
#include <core/track.h>
#include <QJsonArray>
#include <QUrl>

namespace FooyinMCP {

FooyinWrapper::FooyinWrapper(const Fooyin::CorePluginContext& context, QObject* parent)
    : QObject(parent), m_context(context) {}

QJsonObject FooyinWrapper::trackToJson(const Fooyin::Track& track) {
    QJsonObject obj;
    if (!track.isValid()) return obj;

    obj["id"] = QString::number(track.id());
    obj["title"] = track.title();
    obj["artist"] = track.artist();
    obj["album"] = track.album();
    obj["albumArtist"] = track.albumArtist();
    obj["genre"] = track.genre();
    obj["year"] = static_cast<int>(track.year());
    obj["duration"] = static_cast<double>(track.duration());
    obj["path"] = track.filepath();
    obj["rating"] = track.ratingStars();
    return obj;
}

QJsonObject FooyinWrapper::controlPlayback(const QString& action, const QJsonObject& params) {
    auto* player = m_context.playerController;
    if (action == "play") player->play();
    else if (action == "pause") player->pause();
    else if (action == "stop") player->stop();
    else if (action == "playPause") player->playPause();
    else if (action == "next") player->next();
    else if (action == "previous") player->previous();
    else if (action == "seek") player->seek(params.value("positionMs").toVariant().toULongLong());

    return {{"status", "ok"}, {"action", action}};
}

QJsonObject FooyinWrapper::getNowPlaying() {
    auto* player = m_context.playerController;
    Fooyin::Track track = player->currentTrack();

    QJsonObject result;
    result["track"] = trackToJson(track);
    result["positionMs"] = static_cast<double>(player->currentPosition());
    result["state"] = static_cast<int>(player->playState());
    result["volume"] = player->volume();
    return result;
}

QJsonObject FooyinWrapper::searchLibrary(const QString& query) {
    auto* library = m_context.library;
    Fooyin::TrackList allTracks = library->tracks();
    QJsonArray results;

    int count = 0;
    for (const auto& track : allTracks) {
        if (track.title().contains(query, Qt::CaseInsensitive) ||
            track.artist().contains(query, Qt::CaseInsensitive) ||
            track.album().contains(query, Qt::CaseInsensitive)) {
            results.append(trackToJson(track));
            if (++count >= 50) break;
        }
    }
    return {{"tracks", results}, {"query", query}, {"count", results.size()}};
}

QJsonObject FooyinWrapper::getLibraryInfo() {
    auto* library = m_context.library;
    QJsonObject info;
    info["totalTracks"] = static_cast<int>(library->tracks().size());
    return info;
}

QJsonObject FooyinWrapper::scanLibrary() {
    m_context.library->refreshAll();
    return {{"status", "scan_started"}};
}

QJsonObject FooyinWrapper::getPlaylists() {
    auto* handler = m_context.playlistHandler;
    QJsonArray list;

    auto playlists = handler->playlists();
    for (int i = 0; i < playlists.size(); ++i) {
        auto* pl = playlists[i];
        QJsonObject plObj;
        plObj["index"] = i;
        plObj["name"] = pl->name();
        plObj["trackCount"] = pl->rowCount();
        list.append(plObj);
    }
    return {{"playlists", list}};
}

QJsonObject FooyinWrapper::createPlaylist(const QString& name) {
    m_context.playlistHandler->createPlaylist(name);
    return {{"status", "created", "name", name}};
}

QJsonObject FooyinWrapper::modifyPlaylist(const QString& action, const QJsonObject& params) {
    auto* handler = m_context.playlistHandler;
    int plIndex = params.value("playlistIndex").toInt(-1);

    if (plIndex < 0 || plIndex >= handler->playlists().size())
        return {{"error", "Invalid playlist index"}};

    auto* pl = handler->playlists()[plIndex];

    if (action == "addTrack") {
        QString path = params.value("path").toString();
        pl->addFile(path);
        return {{"status", "track_added"}};
    } else if (action == "removeTrack") {
        int row = params.value("row").toInt();
        pl->removeRow(row);
        return {{"status", "track_removed"}};
    }

    return {{"error", "Unknown action"}};
}

QJsonObject FooyinWrapper::manageQueue(const QString& action, const QJsonObject& params) {
    auto* player = m_context.playerController;
    if (action == "clear") {
        player->clearQueue();
        return {{"status", "queue_cleared"}};
    } else if (action == "enqueue") {
        QString path = params.value("path").toString();
        if (path.isEmpty()) return {{"error", "No path provided"}};

        // Suche Track in der Library, um ein valides Objekt zu erhalten
        auto tracks = m_context.library->tracks();
        for (const auto& track : tracks) {
            if (track.filepath() == path) {
                player->enqueue(track);
                return {{"status", "enqueued"}, {"title", track.title()}};
            }
        }
        return {{"error", "Track not found in library"}};
    }
    return {{"error", "Unknown action"}};
}

QJsonObject FooyinWrapper::setVolumeAndMode(const QJsonObject& params) {
    auto* player = m_context.playerController;

    if (params.contains("volume")) {
        player->setVolume(static_cast<float>(params.value("volume").toDouble()));
    }

    if (params.contains("shuffle")) {
        // Fooyin PlayerController hat meist setShuffle(bool)
        // player->setShuffle(params.value("shuffle").toBool());
    }

    return {{"status", "ok"}};
}

QJsonObject FooyinWrapper::setTrackRating(const QJsonObject& params) {
    // Da Track-Objekte oft Kopien sind, muss das Rating über die Library
    // oder das File-Tag-System von Fooyin persistiert werden.
    return {{"status", "rating_not_implemented"}};
}

QJsonObject FooyinWrapper::smartPlay(const QString& query) {
    auto* library = m_context.library;
    auto* player = m_context.playerController;

    Fooyin::TrackList allTracks = library->tracks();
    for (const auto& track : allTracks) {
        if (track.title().contains(query, Qt::CaseInsensitive) ||
            track.artist().contains(query, Qt::CaseInsensitive)) {

            player->stop();
            player->play(track);
            return {{"status", "playing"}, {"track", trackToJson(track)}};
        }
    }
    return {{"status", "not_found"}};
}

} // namespace FooyinMCP
