#include "mcp_tools.h"
#include "fooyin_wrapper.h"
#include <QJsonDocument>
#include <cmath>

namespace FooyinMCP {

    McpTools::McpTools(FooyinWrapper* wrapper, QObject* parent)
            : QObject(parent), m_wrapper(wrapper) {
        registerTools();
    }

    QJsonArray McpTools::getToolDefinitions() const {
        QJsonArray tools;
        for (auto it = m_tools.begin(); it != m_tools.end(); ++it) {
            QJsonObject tool;
            tool[QStringLiteral("name")] = it.key();
            tool[QStringLiteral("description")] = it.value().description;
            tool[QStringLiteral("inputSchema")] = it.value().inputSchema;
            tools.append(tool);
        }
        return tools;
    }

    QJsonObject McpTools::callTool(const QString& name, const QJsonObject& arguments) {
        if (!m_tools.contains(name)) {
            return QJsonObject{{QStringLiteral("isError"), true}, {QStringLiteral("error"), QStringLiteral("Tool not found")}};
        }

        const auto& tool = m_tools[name];
        QJsonObject validation = validateArguments(tool.inputSchema, arguments);
        if (validation.value(QStringLiteral("isError")).toBool()) {
            return validation;
        }

        QJsonObject result = tool.handler(arguments);

        // Ensure that if the wrapper says "ok: false", we mark it as "isError" for the MCP server
        if (result.contains(QStringLiteral("ok")) && !result.value(QStringLiteral("ok")).toBool()) {
            result[QStringLiteral("isError")] = true;
        }

        return result;
    }

    namespace {

        QString typeName(const QJsonValue& v) {
            switch (v.type()) {
                case QJsonValue::Null: return QStringLiteral("null");
                case QJsonValue::Bool: return QStringLiteral("boolean");
                case QJsonValue::Double: return QStringLiteral("number");
                case QJsonValue::String: return QStringLiteral("string");
                case QJsonValue::Array: return QStringLiteral("array");
                case QJsonValue::Object: return QStringLiteral("object");
                default: return QStringLiteral("undefined");
            }
        }

        QString labelFor(const QString& path) {
            return path.isEmpty() ? QStringLiteral("Arguments") : QStringLiteral("Argument '") + path + QStringLiteral("'");
        }

// Liefert eine Fehlermeldung oder einen leeren String, wenn der Wert zum Schema passt.
// Unterstuetzt: type, enum, minimum/maximum, minLength/maxLength, minItems/maxItems, items,
// properties/required/additionalProperties. additionalProperties ist standardmaessig FALSE
// (strenger als JSON Schema), damit Clients keine stillschweigend ignorierten Felder senden.
        QString validateValue(const QJsonObject& schema, const QJsonValue& val, const QString& path) {
            const QString type = schema.value(QStringLiteral("type")).toString();

            if (type == QStringLiteral("string")) {
                if (!val.isString()) return labelFor(path) + QStringLiteral(" must be a string");
                const int len = val.toString().size();
                if (schema.contains(QStringLiteral("minLength")) && len < schema.value(QStringLiteral("minLength")).toInt())
                    return labelFor(path) + QStringLiteral(" is too short");
                if (schema.contains(QStringLiteral("maxLength")) && len > schema.value(QStringLiteral("maxLength")).toInt())
                    return labelFor(path) + QStringLiteral(" is too long");
            } else if (type == QStringLiteral("number") || type == QStringLiteral("integer")) {
                if (!val.isDouble()) {
                    return labelFor(path) + (type == QStringLiteral("integer") ? QStringLiteral(" must be an integer")
                                                                               : QStringLiteral(" must be a number"));
                }
                const double d = val.toDouble();
                if (!std::isfinite(d)) return labelFor(path) + QStringLiteral(" must be a finite number");
                if (type == QStringLiteral("integer") && (d != std::floor(d) || std::fabs(d) > 9007199254740992.0))
                    return labelFor(path) + QStringLiteral(" must be an integer");
                if (schema.contains(QStringLiteral("minimum")) && d < schema.value(QStringLiteral("minimum")).toDouble())
                    return labelFor(path) + QStringLiteral(" is below the minimum");
                if (schema.contains(QStringLiteral("maximum")) && d > schema.value(QStringLiteral("maximum")).toDouble())
                    return labelFor(path) + QStringLiteral(" is above the maximum");
            } else if (type == QStringLiteral("boolean")) {
                if (!val.isBool()) return labelFor(path) + QStringLiteral(" must be a boolean");
            } else if (type == QStringLiteral("array")) {
                if (!val.isArray()) return labelFor(path) + QStringLiteral(" must be an array");
                const QJsonArray arr = val.toArray();
                if (schema.contains(QStringLiteral("minItems")) && arr.size() < schema.value(QStringLiteral("minItems")).toInt())
                    return labelFor(path) + QStringLiteral(" has too few items");
                if (schema.contains(QStringLiteral("maxItems")) && arr.size() > schema.value(QStringLiteral("maxItems")).toInt())
                    return labelFor(path) + QStringLiteral(" has too many items");
                if (schema.contains(QStringLiteral("items"))) {
                    const QJsonObject itemSchema = schema.value(QStringLiteral("items")).toObject();
                    for (int i = 0; i < arr.size(); ++i) {
                        const QString err = validateValue(itemSchema, arr.at(i), path + QStringLiteral("[") + QString::number(i) + QStringLiteral("]"));
                        if (!err.isEmpty()) return err;
                    }
                }
            } else if (type == QStringLiteral("object")) {
                if (!val.isObject()) return labelFor(path) + QStringLiteral(" must be an object (got ") + typeName(val) + QStringLiteral(")");
                const QJsonObject obj = val.toObject();
                const QString prefix = path.isEmpty() ? QString() : path + QStringLiteral(".");

                const QJsonArray required = schema.value(QStringLiteral("required")).toArray();
                for (const auto& req : required) {
                    if (!obj.contains(req.toString())) return QStringLiteral("Missing required argument: ") + prefix + req.toString();
                }

                const QJsonObject properties = schema.value(QStringLiteral("properties")).toObject();
                const bool allowAdditional = schema.value(QStringLiteral("additionalProperties")).toBool(false);
                for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
                    if (!properties.contains(it.key())) {
                        if (!allowAdditional) return QStringLiteral("Unexpected argument: ") + prefix + it.key();
                        continue;
                    }
                    const QString err = validateValue(properties.value(it.key()).toObject(), it.value(), prefix + it.key());
                    if (!err.isEmpty()) return err;
                }
            }

            if (schema.contains(QStringLiteral("enum"))) {
                const QJsonArray enumVals = schema.value(QStringLiteral("enum")).toArray();
                bool found = false;
                for (const auto& ev : enumVals) {
                    if (ev == val) { found = true; break; }
                }
                if (!found) return labelFor(path) + QStringLiteral(" has an invalid value");
            }

            return QString();
        }

    } // namespace

    QJsonObject McpTools::validateArguments(const QJsonObject& schema, const QJsonObject& args) {
        const QString error = validateValue(schema, args, QString());
        if (!error.isEmpty()) {
            return QJsonObject{{QStringLiteral("isError"), true}, {QStringLiteral("error"), error}};
        }
        return QJsonObject{{QStringLiteral("isError"), false}};
    }

    void McpTools::registerTools() {
        auto addTool = [this](const QString& name, const QString& desc, const QJsonObject& schema, std::function<QJsonObject(const QJsonObject&)> handler) {
            m_tools.insert(name, {desc, schema, handler});
        };

        // Helper for empty schemas
        auto emptySchema = []() {
            return QJsonObject{{QStringLiteral("type"), QStringLiteral("object")}, {QStringLiteral("additionalProperties"), false}};
        };

        // Hilfsfunktionen fuer Schemas
        auto str = [](const QString& desc, int minLen = -1, int maxLen = -1) {
            QJsonObject o{{QStringLiteral("type"), QStringLiteral("string")}, {QStringLiteral("description"), desc}};
            if (minLen >= 0) o[QStringLiteral("minLength")] = minLen;
            if (maxLen >= 0) o[QStringLiteral("maxLength")] = maxLen;
            return o;
        };
        auto strArray = [](const QString& desc) {
            return QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("array")},
                    {QStringLiteral("description"), desc},
                    {QStringLiteral("items"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}
            };
        };
        auto objectSchema = [](const QJsonObject& properties, const QJsonArray& required) {
            QJsonObject o{
                    {QStringLiteral("type"), QStringLiteral("object")},
                    {QStringLiteral("properties"), properties},
                    {QStringLiteral("additionalProperties"), false}
            };
            if (!required.isEmpty()) o[QStringLiteral("required")] = required;
            return o;
        };

        // --- PLAYBACK ---
        addTool(QStringLiteral("play"), QStringLiteral("Starts playback"), emptySchema(),
                [this](const QJsonObject&) { return m_wrapper->play(); });

        addTool(QStringLiteral("pause"), QStringLiteral("Pauses playback"), emptySchema(),
                [this](const QJsonObject&) { return m_wrapper->pause(); });

        addTool(QStringLiteral("stop"), QStringLiteral("Stops playback"), emptySchema(),
                [this](const QJsonObject&) { return m_wrapper->stop(); });

        addTool(QStringLiteral("next"), QStringLiteral("Next track"), emptySchema(),
                [this](const QJsonObject&) { return m_wrapper->next(); });

        addTool(QStringLiteral("previous"), QStringLiteral("Previous track"), emptySchema(),
                [this](const QJsonObject&) { return m_wrapper->previous(); });

        addTool(QStringLiteral("seek"), QStringLiteral("Seek to position in milliseconds"),
                objectSchema(
                        QJsonObject{{QStringLiteral("position"), QJsonObject{
                                {QStringLiteral("type"), QStringLiteral("integer")},
                                {QStringLiteral("minimum"), 0},
                                {QStringLiteral("description"), QStringLiteral("Position in milliseconds")}
                        }}},
                        QJsonArray{QStringLiteral("position")}),
                [this](const QJsonObject& args) {
                    return m_wrapper->seek(static_cast<uint64_t>(args.value(QStringLiteral("position")).toDouble()));
                });

        addTool(QStringLiteral("setPlayMode"), QStringLiteral("Set play mode"),
                objectSchema(
                        QJsonObject{{QStringLiteral("mode"), QJsonObject{
                                {QStringLiteral("type"), QStringLiteral("string")},
                                {QStringLiteral("enum"), QJsonArray{QStringLiteral("default"), QStringLiteral("shuffle"),
                                                                    QStringLiteral("repeatTrack"), QStringLiteral("repeatPlaylist")}}
                        }}},
                        QJsonArray{QStringLiteral("mode")}),
                [this](const QJsonObject& args) { return m_wrapper->setPlayMode(args.value(QStringLiteral("mode")).toString()); });

        // Hinweis: Es gibt bewusst kein setVolume-Tool, solange Fooyin keine Lautstaerke-API fuer Plugins anbietet.

        // --- PLAYLIST / QUEUE ---
        addTool(QStringLiteral("addTrack"),
                QStringLiteral("Adds an audio file to a playlist. Uses the active playlist if 'playlist' is omitted. "
                               "A missing playlist is only created when createIfMissing is true."),
                objectSchema(
                        QJsonObject{
                                {QStringLiteral("path"), str(QStringLiteral("Absolute path to an audio file"), 1, 4096)},
                                {QStringLiteral("playlist"), str(QStringLiteral("Target playlist name (optional)"), 1, 256)},
                                {QStringLiteral("createIfMissing"), QJsonObject{
                                        {QStringLiteral("type"), QStringLiteral("boolean")},
                                        {QStringLiteral("description"), QStringLiteral("Create the playlist if it does not exist (default false)")}
                                }}
                        },
                        QJsonArray{QStringLiteral("path")}),
                [this](const QJsonObject& args) {
                    return m_wrapper->addTrack(
                            args.value(QStringLiteral("path")).toString(),
                            args.value(QStringLiteral("playlist")).toString(),
                            args.value(QStringLiteral("createIfMissing")).toBool(false)
                    );
                });

        addTool(QStringLiteral("enqueue"), QStringLiteral("Adds an audio file to the playback queue"),
                objectSchema(
                        QJsonObject{{QStringLiteral("path"), str(QStringLiteral("Absolute path to an audio file"), 1, 4096)}},
                        QJsonArray{QStringLiteral("path")}),
                [this](const QJsonObject& args) { return m_wrapper->enqueue(args.value(QStringLiteral("path")).toString()); });

        addTool(QStringLiteral("createPlaylist"), QStringLiteral("Creates a new playlist (fails if the name already exists)"),
                objectSchema(
                        QJsonObject{{QStringLiteral("name"), str(QStringLiteral("Playlist name"), 1, 256)}},
                        QJsonArray{QStringLiteral("name")}),
                [this](const QJsonObject& args) { return m_wrapper->createPlaylist(args.value(QStringLiteral("name")).toString()); });

        addTool(QStringLiteral("listPlaylists"), QStringLiteral("List all playlists"), emptySchema(),
                [this](const QJsonObject&) { return m_wrapper->listPlaylists(); });

        addTool(QStringLiteral("getPlaylistTracks"), QStringLiteral("Get tracks in a playlist (includes track ids)"),
                objectSchema(
                        QJsonObject{{QStringLiteral("name"), str(QStringLiteral("Playlist name"), 1, 256)}},
                        QJsonArray{QStringLiteral("name")}),
                [this](const QJsonObject& args) { return m_wrapper->getPlaylistTracks(args.value(QStringLiteral("name")).toString()); });

        addTool(QStringLiteral("removePlaylist"), QStringLiteral("Remove a playlist"),
                objectSchema(
                        QJsonObject{{QStringLiteral("name"), str(QStringLiteral("Playlist name"), 1, 256)}},
                        QJsonArray{QStringLiteral("name")}),
                [this](const QJsonObject& args) { return m_wrapper->removePlaylist(args.value(QStringLiteral("name")).toString()); });

        const QJsonObject orderProp{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("enum"), QJsonArray{QStringLiteral("asc"), QStringLiteral("desc")}}
        };

        addTool(QStringLiteral("sortPlaylist"), QStringLiteral("Sort tracks in a playlist"),
                objectSchema(
                        QJsonObject{
                                {QStringLiteral("name"), str(QStringLiteral("Name of the playlist to sort"), 1, 256)},
                                {QStringLiteral("sort"), str(QStringLiteral("Sort script (e.g. %artist% | %album%)"), 1, 1024)},
                                {QStringLiteral("order"), orderProp}
                        },
                        QJsonArray{QStringLiteral("name"), QStringLiteral("sort")}),
                [this](const QJsonObject& args) {
                    return m_wrapper->sortPlaylist(
                            args.value(QStringLiteral("name")).toString(),
                            args.value(QStringLiteral("sort")).toString(),
                            args.value(QStringLiteral("order")).toString()
                    );
                });

        addTool(QStringLiteral("searchLibrary"),
                QStringLiteral("Search the library with an optional sort. Returns tracks (with ids), 'total' matches and "
                               "'truncated' if more results exist beyond limit/offset."),
                objectSchema(
                        QJsonObject{
                                {QStringLiteral("query"), str(QStringLiteral("Fooyin query expression"), 0, 1024)},
                                {QStringLiteral("sort"), str(QStringLiteral("Sort script (e.g. %artist% | %album% | %tracknumber%)"), 0, 1024)},
                                {QStringLiteral("order"), orderProp},
                                {QStringLiteral("limit"), QJsonObject{
                                        {QStringLiteral("type"), QStringLiteral("integer")},
                                        {QStringLiteral("minimum"), 1},
                                        {QStringLiteral("maximum"), FooyinWrapper::MAX_SEARCH_LIMIT},
                                        {QStringLiteral("description"), QStringLiteral("Max results (default 100, max 500)")}
                                }},
                                {QStringLiteral("offset"), QJsonObject{
                                        {QStringLiteral("type"), QStringLiteral("integer")},
                                        {QStringLiteral("minimum"), 0},
                                        {QStringLiteral("description"), QStringLiteral("Number of results to skip (default 0)")}
                                }}
                        },
                        QJsonArray{QStringLiteral("query")}),
                [this](const QJsonObject& args) {
                    return m_wrapper->searchLibrary(
                            args.value(QStringLiteral("query")).toString(),
                            args.value(QStringLiteral("sort")).toString(),
                            args.value(QStringLiteral("order")).toString(),
                            args.value(QStringLiteral("limit")).toInt(FooyinWrapper::DEFAULT_SEARCH_LIMIT),
                            args.value(QStringLiteral("offset")).toInt(0)
                    );
                });

        // --- INFO / STATE ---
        addTool(QStringLiteral("getNowPlaying"), QStringLiteral("Returns currently playing track info (position in ms)"), emptySchema(),
                [this](const QJsonObject&) { return m_wrapper->getNowPlaying(); });

        addTool(QStringLiteral("getPlaybackState"), QStringLiteral("Returns current playback state (playing, paused or stopped)"), emptySchema(),
                [this](const QJsonObject&) { return m_wrapper->getPlaybackState(); });

        addTool(QStringLiteral("scanLibrary"), QStringLiteral("Triggers a library scan"), emptySchema(),
                [this](const QJsonObject&) { return m_wrapper->scanLibrary(); });

        const QJsonObject metadataProps{
                {QStringLiteral("title"), str(QStringLiteral("Track title"), 0, 1024)},
                {QStringLiteral("album"), str(QStringLiteral("Album title"), 0, 1024)},
                {QStringLiteral("year"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("integer")},
                        {QStringLiteral("minimum"), 0},
                        {QStringLiteral("maximum"), 9999}
                }},
                {QStringLiteral("date"), str(QStringLiteral("Date string, e.g. 2024-05-17"), 0, 64)},
                {QStringLiteral("trackNumber"), str(QStringLiteral("Track number"), 0, 16)},
                {QStringLiteral("trackTotal"), str(QStringLiteral("Total tracks"), 0, 16)},
                {QStringLiteral("discNumber"), str(QStringLiteral("Disc number"), 0, 16)},
                {QStringLiteral("discTotal"), str(QStringLiteral("Total discs"), 0, 16)},
                {QStringLiteral("rating"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("number")},
                        {QStringLiteral("minimum"), 0},
                        {QStringLiteral("maximum"), 1},
                        {QStringLiteral("description"), QStringLiteral("Rating normalised to 0.0 - 1.0")}
                }},
                {QStringLiteral("artists"), strArray(QStringLiteral("Track artists"))},
                {QStringLiteral("albumArtists"), strArray(QStringLiteral("Album artists"))},
                {QStringLiteral("genres"), strArray(QStringLiteral("Genres"))},
                {QStringLiteral("comment"), str(QStringLiteral("Comment"), 0, 4096)}
        };

        addTool(QStringLiteral("updateTrackMetadata"),
                QStringLiteral("Updates metadata of tracks by id (ids come from searchLibrary / getPlaylistTracks). "
                               "Changes only touch the library database unless writeToDisk is true, which writes the tags "
                               "into the audio files."),
                objectSchema(
                        QJsonObject{
                                {QStringLiteral("trackIds"), QJsonObject{
                                        {QStringLiteral("type"), QStringLiteral("array")},
                                        {QStringLiteral("minItems"), 1},
                                        {QStringLiteral("maxItems"), 1000},
                                        {QStringLiteral("items"), QJsonObject{
                                                {QStringLiteral("type"), QStringLiteral("integer")},
                                                {QStringLiteral("minimum"), 0}
                                        }}
                                }},
                                {QStringLiteral("metadata"), QJsonObject{
                                        {QStringLiteral("type"), QStringLiteral("object")},
                                        {QStringLiteral("properties"), metadataProps},
                                        {QStringLiteral("additionalProperties"), false}
                                }},
                                {QStringLiteral("writeToDisk"), QJsonObject{
                                        {QStringLiteral("type"), QStringLiteral("boolean")},
                                        {QStringLiteral("description"), QStringLiteral("Write tags into the files (default false)")}
                                }}
                        },
                        QJsonArray{QStringLiteral("trackIds"), QStringLiteral("metadata")}),
                [this](const QJsonObject& args) {
                    return m_wrapper->updateTrackMetadata(
                            args.value(QStringLiteral("trackIds")).toArray(),
                            args.value(QStringLiteral("metadata")).toObject(),
                            args.value(QStringLiteral("writeToDisk")).toBool(false)
                    );
                });
    }

} // namespace FooyinMCP