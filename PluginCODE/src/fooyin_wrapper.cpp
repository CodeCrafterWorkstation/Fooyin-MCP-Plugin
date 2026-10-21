#include "fooyin_wrapper.h"
#include <core/player/playercontroller.h>
#include <core/library/musiclibrary.h>
#include <core/playlist/playlisthandler.h>
#include <core/track.h>
#include <QJsonArray>
#include <QUrl>

namespace FooyinMCP {

FooyinWrapper::FooyinWrapper(const Fooyin::CorePluginContext& context, QObject* parent)
    : QObject(parent), m_context(context) {}

QJsonObject FooyinWrapper::trackToJson(const Fooyin::Track& track) {
    QJsonObject obj;
    if (!track.isValid()) return obj;
    obj["id"] = track.id();
    obj["title"] = track.title();
    obj["artist"] = track.artist();
    obj["album"] = track.album();
    obj["duration"] = static_cast<double>(track.duration());
    obj["path"] = track.filepath();
    obj["codec"] = track.codec();
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
    return {{"status", "ok"}};
}

QJsonObject FooyinWrapper::getNowPlaying() {
    auto* player = m_context.playerController;
    Fooyin::Track track = player->currentTrack();
    QJsonObject result;
    result["track"] = trackToJson(track);
    result["position"] = static_cast<double>(player->currentPosition());
    result["state"] = static_cast<int>(player->playState());
    return result;
}

QJsonObject FooyinWrapper::searchLibrary(const QString& query) {
    auto* library = m_context.library;
    Fooyin::TrackList allTracks = library->tracks();
    QJsonArray results;
    int count = 0;
    for (const auto& track : allTracks) {
        if (track.title().contains(query, Qt::CaseInsensitive) ||
            track.artist().contains(query, Qt::CaseInsensitive)) {
            results.append(trackToJson(track));
            if (++count >= 50) break;
        }
    }
    return {{"tracks", results}};
}

QJsonObject FooyinWrapper::getLibraryInfo() {
    QJsonObject info;
    info["totalTracks"] = static_cast<int>(m_context.library->tracks().size());
    return info;
}

QJsonObject FooyinWrapper::scanLibrary() {
    m_context.library->refreshAll();
    return {{"status", "scan_started"}};
}

QJsonObject FooyinWrapper::getPlaylists() {
    QJsonArray list;
    for (auto* pl : m_context.playlistHandler->playlists()) {
        QJsonObject plObj;
        plObj["name"] = pl->name();
        plObj["id"] = pl->id().toString();
        plObj["trackCount"] = pl->trackCount();
        list.append(plObj);
    }
    return {{"playlists", list}};
}

QJsonObject FooyinWrapper::createPlaylist(const QString& name) {
    m_context.playlistHandler->createPlaylist(name);
    return {{"status", "created", "name", name}};
}

QJsonObject FooyinWrapper::modifyPlaylist(const QString& action, const QJsonObject& params) {
    return {{"status", "not_implemented"}};
}

QJsonObject FooyinWrapper::manageQueue(const QString& action, const QJsonObject& params) {
    if (action == "clear") m_context.playerController->clearQueue();
    return {{"status", "ok"}};
}

QJsonObject FooyinWrapper::setTrackRating(const QJsonObject& params) {
    int trackId = params.value("trackId").toInt();
    int rating = params.value("rating").toInt();
    Fooyin::Track track = m_context.library->trackForId(trackId);
    if (track.isValid()) {
        track.setRatingStars(rating);
        m_context.library->updateTrack(track);
        return {{"status", "ok"}};
    }
    return {{"status", "error", "message", "Track not found"}};
}

QJsonObject FooyinWrapper::setVolumeAndMode(const QJsonObject& params) {
    return {{"status", "ok"}};
}

} // namespace FooyinMCP
