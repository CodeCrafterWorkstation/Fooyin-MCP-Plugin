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
        });

        connect(player, &Fooyin::PlayerController::playStateChanged, this, [this](Fooyin::PlayState state) {
            emit playbackStateChanged(state);
        });

        connect(player, &Fooyin::PlayerController::volumeChanged, this, [this](float volume) {
            emit volumeChanged(volume);
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
    if (!m_context.playerController) return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Player not available")}};
    m_context.playerController->play();
    return {{QStringLiteral("ok"), true}};
}

QJsonObject FooyinWrapper::pause() {
    if (!m_context.playerController) return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Player not available")}};
    m_context.playerController->pause();
    return {{QStringLiteral("ok"), true}};
}

QJsonObject FooyinWrapper::stop() {
    if (!m_context.playerController) return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Player not available")}};
    m_context.playerController->stop();
    return {{QStringLiteral("ok"), true}};
}

QJsonObject FooyinWrapper::next() {
    if (!m_context.playerController) return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Player not available")}};
    m_context.playerController->next();
    return {{QStringLiteral("ok"), true}};
}

QJsonObject FooyinWrapper::previous() {
    if (!m_context.playerController) return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Player not available")}};
    m_context.playerController->previous();
    return {{QStringLiteral("ok"), true}};
}

QJsonObject FooyinWrapper::setVolume(float volume) {
    if (!m_context.playerController) return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Player not available")}};
    float vol = qBound(0.0f, volume, 1.0f);
    m_context.playerController->setVolume(vol);
    return {{QStringLiteral("ok"), true}, {QStringLiteral("volume"), static_cast<double>(vol)}};
}

QJsonObject FooyinWrapper::addTrack(const QString& path) {
    if (!m_context.playlistHandler) return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Playlist handler not available")}};

    QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("File not found")}};
    }

    auto* pl = m_context.playlistHandler->currentPlaylist();
    if (!pl) return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("No active playlist")}};

    pl->addFile(path);
    return {{QStringLiteral("ok"), true}};
}

QJsonObject FooyinWrapper::enqueue(const QString& path) {
    if (!m_context.playerController || !m_context.library)
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Player or Library not available")}};

    QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("File not found")}};
    }

    auto tracks = m_context.library->tracks();
    for (const auto& track : tracks) {
        if (track.filepath() == path) {
            m_context.playerController->enqueue(track);
            return {{QStringLiteral("ok"), true}};
        }
    }
    return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Track not in library")}};
}

QJsonObject FooyinWrapper::scanLibrary() {
    if (!m_context.library) return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Library not available")}};
    m_context.library->refreshAll();
    return {{QStringLiteral("ok"), true}};
}

QJsonObject FooyinWrapper::getNowPlaying() {
    if (!m_context.playerController) return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Player not available")}};
    auto* player = m_context.playerController;
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("track"), trackToJson(player->currentTrack())},
        {QStringLiteral("position"), static_cast<double>(player->currentPosition())}
    };
}

QJsonObject FooyinWrapper::getPlaybackState() {
    if (!m_context.playerController) return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Player not available")}};
    auto* player = m_context.playerController;
    QString state = QStringLiteral("stopped");
    switch (player->playState()) {
        case Fooyin::PlayState::Playing: state = QStringLiteral("playing"); break;
        case Fooyin::PlayState::Paused:  state = QStringLiteral("paused"); break;
        default: break;
    }
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("state"), state},
        {QStringLiteral("volume"), static_cast<double>(player->volume())}
    };
}

} // namespace FooyinMCP
