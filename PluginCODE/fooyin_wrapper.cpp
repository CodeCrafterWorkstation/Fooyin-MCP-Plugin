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
#include <QSet>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace FooyinMCP {

    namespace {

        constexpr int kMaxTrackIds = 1000;
// Annahme: Fooyin speichert Ratings normalisiert (0.0 - 1.0). Bei anderer Skala hier anpassen.
        constexpr double kMaxRating = 1.0;

        QJsonObject okResult() {
            return QJsonObject{{QStringLiteral("ok"), true}};
        }

        QJsonObject errorResult(const QString& message) {
            return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), message}};
        }

        bool isIntegral(const QJsonValue& v, double min, double max) {
            if (!v.isDouble()) return false;
            const double d = v.toDouble();
            return std::isfinite(d) && d == std::floor(d) && d >= min && d <= max;
        }

        bool isStringArray(const QJsonValue& v) {
            if (!v.isArray()) return false;
            const QJsonArray arr = v.toArray();
            for (const auto& item : arr) {
                if (!item.isString()) return false;
            }
            return true;
        }

        QStringList toStringList(const QJsonValue& v) {
            QStringList out;
            const QJsonArray arr = v.toArray();
            for (const auto& item : arr) out << item.toString();
            return out;
        }

// Prueft Pfad, loest ihn kanonisch auf (Symlinks, "..") und erlaubt nur bekannte Audio-Endungen.
        bool validateAudioPath(const QString& path, QString& canonicalPath, QString& error) {
            if (path.trimmed().isEmpty()) {
                error = QStringLiteral("Path is empty");
                return false;
            }

            const QFileInfo fi(path);
            if (!fi.exists() || !fi.isFile()) {
                error = QStringLiteral("File not found");
                return false;
            }

            canonicalPath = fi.canonicalFilePath();
            if (canonicalPath.isEmpty()) {
                error = QStringLiteral("Could not resolve file path");
                return false;
            }

            static const QStringList allowedSuffixes{
                    QStringLiteral("mp3"),  QStringLiteral("flac"), QStringLiteral("ogg"),  QStringLiteral("oga"),
                    QStringLiteral("opus"), QStringLiteral("m4a"),  QStringLiteral("mp4"),  QStringLiteral("aac"),
                    QStringLiteral("wav"),  QStringLiteral("wma"),  QStringLiteral("ape"),  QStringLiteral("wv"),
                    QStringLiteral("mpc"),  QStringLiteral("aif"),  QStringLiteral("aiff"), QStringLiteral("alac"),
                    QStringLiteral("dsf"),  QStringLiteral("dff"),  QStringLiteral("tta")};

            if (!allowedSuffixes.contains(QFileInfo(canonicalPath).suffix().toLower())) {
                error = QStringLiteral("Unsupported file type");
                return false;
            }
            return true;
        }

    } // namespace

    FooyinWrapper::FooyinWrapper(const Fooyin::CorePluginContext& context, QObject* parent)
            : QObject(parent), m_context(context) {

        auto* player = m_context.playerController;
        if (player) {
            connect(player, &Fooyin::PlayerController::currentTrackChanged, this, [this](const Fooyin::Track& track) {
                emit nowPlayingChanged(trackToJson(track));
                emitPlaybackStateIfChanged();
            });

            connect(player, &Fooyin::PlayerController::playbackStateChanged, this, [this]() {
                emitPlaybackStateIfChanged();
            });

            m_lastState = getPlaybackState().value(QStringLiteral("state")).toString();
        }
    }

    void FooyinWrapper::emitPlaybackStateIfChanged() {
        const QString state = getPlaybackState().value(QStringLiteral("state")).toString();
        if (state.isEmpty() || state == m_lastState) return;
        m_lastState = state;
        emit playbackStateChanged(state);
    }

    QJsonObject FooyinWrapper::trackToJson(const Fooyin::Track& track) {
        if (!track.isValid()) return QJsonObject();
        QJsonObject obj;
        obj[QStringLiteral("id")] = static_cast<double>(track.id());
        obj[QStringLiteral("title")] = track.title();
        obj[QStringLiteral("artist")] = track.artist();
        obj[QStringLiteral("artists")] = QJsonArray::fromStringList(track.artists());
        obj[QStringLiteral("album")] = track.album();
        obj[QStringLiteral("albumArtists")] = QJsonArray::fromStringList(track.albumArtists());
        obj[QStringLiteral("genres")] = QJsonArray::fromStringList(track.genres());
        obj[QStringLiteral("year")] = static_cast<double>(track.year());
        obj[QStringLiteral("trackNumber")] = track.trackNumber();
        obj[QStringLiteral("rating")] = static_cast<double>(track.rating());
        obj[QStringLiteral("path")] = track.filepath();
        obj[QStringLiteral("duration")] = static_cast<double>(track.duration()); // Millisekunden
        return obj;
    }

    QJsonObject FooyinWrapper::play() {
        if (!m_context.playerController) return errorResult(QStringLiteral("Player not available"));
        m_context.playerController->play();
        return okResult();
    }

    QJsonObject FooyinWrapper::pause() {
        if (!m_context.playerController) return errorResult(QStringLiteral("Player not available"));
        m_context.playerController->pause();
        return okResult();
    }

    QJsonObject FooyinWrapper::stop() {
        if (!m_context.playerController) return errorResult(QStringLiteral("Player not available"));
        m_context.playerController->stop();
        return okResult();
    }

    QJsonObject FooyinWrapper::next() {
        if (!m_context.playerController) return errorResult(QStringLiteral("Player not available"));
        m_context.playerController->next();
        return okResult();
    }

    QJsonObject FooyinWrapper::previous() {
        if (!m_context.playerController) return errorResult(QStringLiteral("Player not available"));
        m_context.playerController->previous();
        return okResult();
    }

    QJsonObject FooyinWrapper::seek(uint64_t ms) {
        if (!m_context.playerController) return errorResult(QStringLiteral("Player not available"));
        m_context.playerController->seek(ms);
        return okResult();
    }

    QJsonObject FooyinWrapper::setPlayMode(const QString& mode) {
        if (!m_context.playerController) return errorResult(QStringLiteral("Player not available"));

        Fooyin::Playlist::PlayModes m = Fooyin::Playlist::Default;
        if (mode == QStringLiteral("default")) m = Fooyin::Playlist::Default;
        else if (mode == QStringLiteral("shuffle")) m = Fooyin::Playlist::ShuffleTracks;
        else if (mode == QStringLiteral("repeatTrack")) m = Fooyin::Playlist::RepeatTrack;
        else if (mode == QStringLiteral("repeatPlaylist")) m = Fooyin::Playlist::RepeatPlaylist;
        else return errorResult(QStringLiteral("Unknown play mode (expected: default, shuffle, repeatTrack, repeatPlaylist)"));

        m_context.playerController->setPlayMode(m);
        return QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("mode"), mode}};
    }

    QJsonObject FooyinWrapper::addTrack(const QString& path, const QString& playlistName, bool createIfMissing) {
        if (!m_context.playlistHandler) return errorResult(QStringLiteral("Playlist handler not available"));

        QString canonicalPath;
        QString pathError;
        if (!validateAudioPath(path, canonicalPath, pathError)) return errorResult(pathError);

        Fooyin::Track track(canonicalPath);
        if (!track.isValid()) return errorResult(QStringLiteral("Invalid track file"));

        Fooyin::Playlist* pl = nullptr;
        if (playlistName.isEmpty()) {
            pl = m_context.playlistHandler->activePlaylist();
            if (!pl) {
                return errorResult(QStringLiteral("No active playlist. Specify 'playlist' (and createIfMissing=true to create it)"));
            }
        } else {
            pl = m_context.playlistHandler->playlistByName(playlistName);
            if (!pl) {
                if (!createIfMissing) {
                    return errorResult(QStringLiteral("Playlist not found: ") + playlistName
                                       + QStringLiteral(" (set createIfMissing=true to create it)"));
                }
                pl = m_context.playlistHandler->createPlaylist(playlistName);
                if (!pl) return errorResult(QStringLiteral("Could not create playlist"));
            }
        }

        // Kein verifizierter Append-Aufruf bekannt: Liste kopieren und ersetzen (O(n) pro Track).
        Fooyin::TrackList currentTracks = pl->tracks();
        currentTracks.push_back(track);
        m_context.playlistHandler->replacePlaylistTracks(pl->id(), currentTracks);

        return QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("playlist"), pl->name()}};
    }

    QJsonObject FooyinWrapper::createPlaylist(const QString& name) {
        if (!m_context.playlistHandler) return errorResult(QStringLiteral("PlaylistHandler not available"));

        const QString trimmed = name.trimmed();
        if (trimmed.isEmpty()) return errorResult(QStringLiteral("Playlist name is empty"));
        if (m_context.playlistHandler->playlistByName(trimmed)) {
            return errorResult(QStringLiteral("Playlist already exists: ") + trimmed);
        }

        auto* pl = m_context.playlistHandler->createPlaylist(trimmed);
        if (!pl) return errorResult(QStringLiteral("Could not create playlist"));

        return QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("name"), pl->name()}};
    }

    QJsonObject FooyinWrapper::listPlaylists() {
        if (!m_context.playlistHandler) return errorResult(QStringLiteral("PlaylistHandler not available"));

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
        if (!m_context.playlistHandler) return errorResult(QStringLiteral("PlaylistHandler not available"));

        auto* pl = m_context.playlistHandler->playlistByName(name);
        if (!pl) return errorResult(QStringLiteral("Playlist not found"));

        QJsonArray arr;
        for (const auto& track : pl->tracks()) {
            arr.append(trackToJson(track));
        }
        return QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("tracks"), arr}};
    }

    QJsonObject FooyinWrapper::removePlaylist(const QString& name) {
        if (!m_context.playlistHandler) return errorResult(QStringLiteral("PlaylistHandler not available"));

        auto* pl = m_context.playlistHandler->playlistByName(name);
        if (!pl) return errorResult(QStringLiteral("Playlist not found"));

        m_context.playlistHandler->removePlaylist(pl->id());
        return okResult();
    }

    QJsonObject FooyinWrapper::sortPlaylist(const QString& name, const QString& sort, const QString& order) {
        if (!m_context.playlistHandler) return errorResult(QStringLiteral("PlaylistHandler not available"));
        if (sort.trimmed().isEmpty()) return errorResult(QStringLiteral("Sort script is empty"));

        const QString orderLower = order.toLower();
        if (!orderLower.isEmpty() && orderLower != QStringLiteral("asc") && orderLower != QStringLiteral("desc")) {
            return errorResult(QStringLiteral("Order must be 'asc' or 'desc'"));
        }

        auto* pl = m_context.playlistHandler->playlistByName(name);
        if (!pl) return errorResult(QStringLiteral("Playlist not found"));

        Fooyin::TrackList tracks = pl->tracks();
        Qt::SortOrder sortOrder = (orderLower == QStringLiteral("desc")) ? Qt::DescendingOrder : Qt::AscendingOrder;

        Fooyin::LibraryInfoMap libraries;
        if (m_context.libraryManager) {
            libraries = m_context.libraryManager->allLibraries();
        }

        Fooyin::TrackSorter sorter(libraries);
        tracks = sorter.calcSortTracks(sort, tracks, sortOrder);

        m_context.playlistHandler->replacePlaylistTracks(pl->id(), tracks);
        return okResult();
    }

    QJsonObject FooyinWrapper::enqueue(const QString& path) {
        if (!m_context.playerController) return errorResult(QStringLiteral("PlayerController not available"));

        QString canonicalPath;
        QString pathError;
        if (!validateAudioPath(path, canonicalPath, pathError)) return errorResult(pathError);

        Fooyin::Track track(canonicalPath);
        if (!track.isValid()) return errorResult(QStringLiteral("Invalid track"));

        m_context.playerController->queueTrack(track);
        return okResult();
    }

    QJsonObject FooyinWrapper::searchLibrary(const QString& query, const QString& sort, const QString& order,
                                             int limit, int offset) {
        if (!m_context.library) return errorResult(QStringLiteral("Library not available"));

        const QString orderLower = order.toLower();
        if (!orderLower.isEmpty() && orderLower != QStringLiteral("asc") && orderLower != QStringLiteral("desc")) {
            return errorResult(QStringLiteral("Order must be 'asc' or 'desc'"));
        }

        limit = std::clamp(limit, 1, MAX_SEARCH_LIMIT);
        offset = std::max(offset, 0);

        // Hinweis: tracks() liefert eine Kopie der gesamten Bibliothek.
        Fooyin::TrackList tracks = m_context.library->tracks();

        Fooyin::TrackQueryFilter filter;
        tracks = filter.filter(query, tracks);

        if (!sort.isEmpty()) {
            Qt::SortOrder sortOrder = (orderLower == QStringLiteral("desc")) ? Qt::DescendingOrder : Qt::AscendingOrder;

            Fooyin::LibraryInfoMap libraries;
            if (m_context.libraryManager) {
                libraries = m_context.libraryManager->allLibraries();
            }

            Fooyin::TrackSorter sorter(libraries);
            tracks = sorter.calcSortTracks(sort, tracks, sortOrder);
        }

        const int total = static_cast<int>(tracks.size());
        QJsonArray arr;
        for (int i = offset; i < total && arr.size() < limit; ++i) {
            arr.append(trackToJson(tracks[static_cast<size_t>(i)]));
        }

        return QJsonObject{
                {QStringLiteral("ok"), true},
                {QStringLiteral("total"), total},
                {QStringLiteral("offset"), offset},
                {QStringLiteral("limit"), limit},
                {QStringLiteral("count"), static_cast<int>(arr.size())},
                {QStringLiteral("truncated"), offset + static_cast<int>(arr.size()) < total},
                {QStringLiteral("tracks"), arr}
        };
    }

    QJsonObject FooyinWrapper::scanLibrary() {
        if (!m_context.library) return errorResult(QStringLiteral("Library not available"));
        m_context.library->refreshAll();
        return okResult();
    }

    QJsonObject FooyinWrapper::updateTrackMetadata(const QJsonArray& trackIds, const QJsonObject& metadata, bool writeToDisk)
    {
        auto* library = m_context.library;
        if (!library) return errorResult(QStringLiteral("Library not available"));
        if (metadata.isEmpty()) return errorResult(QStringLiteral("No metadata fields provided"));
        if (trackIds.isEmpty()) return errorResult(QStringLiteral("No track IDs provided"));
        if (trackIds.size() > kMaxTrackIds) return errorResult(QStringLiteral("Too many track IDs (max 1000)"));

        // --- Validierung (nichts wird veraendert, bevor alles gueltig ist) ---
        static const QStringList stringFields{
                QStringLiteral("title"), QStringLiteral("album"), QStringLiteral("trackNumber"), QStringLiteral("trackTotal"),
                QStringLiteral("discNumber"), QStringLiteral("discTotal"), QStringLiteral("comment"), QStringLiteral("date")};
        static const QStringList listFields{
                QStringLiteral("artists"), QStringLiteral("albumArtists"), QStringLiteral("genres")};

        for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it) {
            const QString key = it.key();
            const QJsonValue val = it.value();

            if (stringFields.contains(key)) {
                if (!val.isString()) return errorResult(QStringLiteral("Field '%1' must be a string").arg(key));
            } else if (listFields.contains(key)) {
                if (!isStringArray(val)) return errorResult(QStringLiteral("Field '%1' must be an array of strings").arg(key));
            } else if (key == QStringLiteral("year")) {
                if (!isIntegral(val, 0, 9999)) return errorResult(QStringLiteral("Field 'year' must be an integer between 0 and 9999"));
            } else if (key == QStringLiteral("rating")) {
                if (!val.isDouble() || val.toDouble() < 0.0 || val.toDouble() > kMaxRating) {
                    return errorResult(QStringLiteral("Field 'rating' must be a number between 0 and 1"));
                }
            } else {
                return errorResult(QStringLiteral("Unknown metadata field: ") + key);
            }
        }

        Fooyin::TrackIds ids;
        for (const auto& val : trackIds) {
            if (!isIntegral(val, 0, 2147483647.0)) {
                return errorResult(QStringLiteral("trackIds must contain non-negative integers"));
            }
            ids.push_back(static_cast<int>(val.toDouble()));
        }

        Fooyin::TrackList tracks = library->tracksForIds(ids);
        if (tracks.empty()) return errorResult(QStringLiteral("Tracks not found"));

        QSet<int> foundIds;
        for (const auto& t : tracks) foundIds.insert(static_cast<int>(t.id()));

        // --- Anwenden ---
        for (auto& track : tracks) {
            if (metadata.contains(QStringLiteral("title"))) track.setTitle(metadata.value(QStringLiteral("title")).toString());
            if (metadata.contains(QStringLiteral("album"))) track.setAlbum(metadata.value(QStringLiteral("album")).toString());
            if (metadata.contains(QStringLiteral("year"))) track.setYear(static_cast<int>(metadata.value(QStringLiteral("year")).toDouble()));
            if (metadata.contains(QStringLiteral("trackNumber"))) track.setTrackNumber(metadata.value(QStringLiteral("trackNumber")).toString());
            if (metadata.contains(QStringLiteral("trackTotal"))) track.setTrackTotal(metadata.value(QStringLiteral("trackTotal")).toString());
            if (metadata.contains(QStringLiteral("discNumber"))) track.setDiscNumber(metadata.value(QStringLiteral("discNumber")).toString());
            if (metadata.contains(QStringLiteral("discTotal"))) track.setDiscTotal(metadata.value(QStringLiteral("discTotal")).toString());
            if (metadata.contains(QStringLiteral("comment"))) track.setComment(metadata.value(QStringLiteral("comment")).toString());
            if (metadata.contains(QStringLiteral("date"))) track.setDate(metadata.value(QStringLiteral("date")).toString());

            if (metadata.contains(QStringLiteral("artists")))
                track.setArtists(toStringList(metadata.value(QStringLiteral("artists"))));
            if (metadata.contains(QStringLiteral("albumArtists")))
                track.setAlbumArtists(toStringList(metadata.value(QStringLiteral("albumArtists"))));
            if (metadata.contains(QStringLiteral("genres")))
                track.setGenres(toStringList(metadata.value(QStringLiteral("genres"))));

            if (metadata.contains(QStringLiteral("rating")))
                track.setRating(static_cast<float>(metadata.value(QStringLiteral("rating")).toDouble()));
        }

        // Fooyin meldet hier keinen Erfolg pro Datei zurueck; das Ergebnis beschreibt nur, was angefordert wurde.
        if (writeToDisk) {
            library->writeTrackMetadata(tracks);
        } else {
            library->updateTrackMetadata(tracks);
        }

        QJsonArray notFound;
        for (int id : ids) {
            if (!foundIds.contains(id)) notFound.append(id);
        }

        QJsonObject res;
        res[QStringLiteral("ok")] = true;
        res[QStringLiteral("updatedCount")] = static_cast<int>(tracks.size());
        res[QStringLiteral("requestedCount")] = static_cast<int>(ids.size());
        res[QStringLiteral("writeToDiskRequested")] = writeToDisk;
        if (!notFound.isEmpty()) res[QStringLiteral("notFound")] = notFound;
        return res;
    }

    QJsonObject FooyinWrapper::getNowPlaying() {
        if (!m_context.playerController) return errorResult(QStringLiteral("Player not available"));

        auto* player = m_context.playerController;
        return QJsonObject{
                {QStringLiteral("ok"), true},
                {QStringLiteral("track"), trackToJson(player->currentTrack())},
                {QStringLiteral("position"), static_cast<double>(player->currentPosition())}
        };
    }

    QJsonObject FooyinWrapper::getPlaybackState() {
        if (!m_context.playerController) return errorResult(QStringLiteral("Player not available"));

        auto* player = m_context.playerController;
        QString stateStr = QStringLiteral("stopped");
        auto state = player->playbackState();

        if (state == Fooyin::PlayerController::PlayingState) stateStr = QStringLiteral("playing");
        else if (state == Fooyin::PlayerController::PausedState) stateStr = QStringLiteral("paused");

        return QJsonObject{
                {QStringLiteral("ok"), true},
                {QStringLiteral("state"), stateStr}
        };
    }

} // namespace FooyinMCP