#include "mcp_tools.h"
#include "fooyin_wrapper.h"
#include <QJsonDocument>

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

QJsonObject McpTools::validateArguments(const QJsonObject& schema, const QJsonObject& args) {
    // Basic JSON Schema validation for MCP tools
    QString type = schema.value(QStringLiteral("type")).toString();
            if (type == QStringLiteral("object")) {
                QJsonArray required = schema.value(QStringLiteral("required")).toArray();
                for (const auto& req : required) {
                    if (!args.contains(req.toString())) {
                        return QJsonObject{{QStringLiteral("isError"), true}, {QStringLiteral("error"), QStringLiteral("Missing required argument: ") + req.toString()}};
                    }
                }

                QJsonObject properties = schema.value(QStringLiteral("properties")).toObject();
                for (auto it = args.begin(); it != args.end(); ++it) {
                    if (!properties.contains(it.key())) {
                        if (schema.value(QStringLiteral("additionalProperties")).toBool() == false) {
                            return QJsonObject{{QStringLiteral("isError"), true}, {QStringLiteral("error"), QStringLiteral("Unexpected argument: ") + it.key()}};
                        }
                        continue;
                    }

                    QJsonObject propSchema = properties.value(it.key()).toObject();
                    QString propType = propSchema.value(QStringLiteral("type")).toString();
                    QJsonValue val = it.value();

                    if (propType == QStringLiteral("string") && !val.isString()) return QJsonObject{{QStringLiteral("isError"), true}, {QStringLiteral("error"), QStringLiteral("Argument ") + it.key() + QStringLiteral(" must be a string")}};
                    if (propType == QStringLiteral("number") && !val.isDouble()) return QJsonObject{{QStringLiteral("isError"), true}, {QStringLiteral("error"), QStringLiteral("Argument ") + it.key() + QStringLiteral(" must be a number")}};
                    if (propType == QStringLiteral("integer") && !val.isDouble()) return QJsonObject{{QStringLiteral("isError"), true}, {QStringLiteral("error"), QStringLiteral("Argument ") + it.key() + QStringLiteral(" must be an integer")}};
                    if (propType == QStringLiteral("boolean") && !val.isBool()) return QJsonObject{{QStringLiteral("isError"), true}, {QStringLiteral("error"), QStringLiteral("Argument ") + it.key() + QStringLiteral(" must be a boolean")}};


            if (propSchema.contains(QStringLiteral("enum"))) {
                QJsonArray enumVals = propSchema.value(QStringLiteral("enum")).toArray();
                bool found = false;
                for (const auto& ev : enumVals) {
                    if (ev == val) { found = true; break; }
                }
                if (!found) return QJsonObject{{QStringLiteral("isError"), true}, {QStringLiteral("error"), QStringLiteral("Argument ") + it.key() + QStringLiteral(" has invalid value")}};
            }
        }
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

    addTool(QStringLiteral("seek"), QStringLiteral("Seek to position in ms"),
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("position"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("integer")},
                        {QStringLiteral("minimum"), 0}
                    }}
                }},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("position")}},
                {QStringLiteral("additionalProperties"), false}
            },
            [this](const QJsonObject& args) { return m_wrapper->seek(args.value(QStringLiteral("position")).toVariant().toULongLong()); });

    addTool(QStringLiteral("setPlayMode"), QStringLiteral("Set play mode (default, shuffle, repeatTrack, repeatPlaylist)"),
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("mode"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("enum"), QJsonArray{QStringLiteral("default"), QStringLiteral("shuffle"), QStringLiteral("repeatTrack"), QStringLiteral("repeatPlaylist")}}
                    }}
                }},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("mode")}},
                {QStringLiteral("additionalProperties"), false}
            },
            [this](const QJsonObject& args) { return m_wrapper->setPlayMode(args.value(QStringLiteral("mode")).toString()); });

    // --- VOLUME ---
    addTool(QStringLiteral("setVolume"), QStringLiteral("Sets volume (0.0 to 1.0)"),
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("volume"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("number")},
                        {QStringLiteral("minimum"), 0},
                        {QStringLiteral("maximum"), 1}
                    }}
                }},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("volume")}},
                {QStringLiteral("additionalProperties"), false}
            },
            [this](const QJsonObject& args) {
                return m_wrapper->setVolume(static_cast<float>(args.value(QStringLiteral("volume")).toDouble()));
            });

    // --- PLAYLIST / QUEUE ---
    addTool(QStringLiteral("addTrack"), QStringLiteral("Adds a file path to a playlist"),
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("path"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                    {QStringLiteral("playlist"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}
                }},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("path")}},
                {QStringLiteral("additionalProperties"), false}
            },
            [this](const QJsonObject& args) {
                return m_wrapper->addTrack(
                    args.value(QStringLiteral("path")).toString(),
                    args.value(QStringLiteral("playlist")).toString()
                );
            });

    addTool(QStringLiteral("enqueue"), QStringLiteral("Enqueues a file path"),
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("path"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}
                }},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("path")}},
                {QStringLiteral("additionalProperties"), false}
            },
            [this](const QJsonObject& args) { return m_wrapper->enqueue(args.value(QStringLiteral("path")).toString()); });

    addTool(QStringLiteral("createPlaylist"), QStringLiteral("Creates a new playlist with the given name"),
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("name"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}
                }},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("name")}},
                {QStringLiteral("additionalProperties"), false}
            },
            [this](const QJsonObject& args) { return m_wrapper->createPlaylist(args.value(QStringLiteral("name")).toString()); });

    addTool(QStringLiteral("listPlaylists"), QStringLiteral("List all playlists"), emptySchema(),
            [this](const QJsonObject&) { return m_wrapper->listPlaylists(); });

    addTool(QStringLiteral("getPlaylistTracks"), QStringLiteral("Get tracks in a playlist"),
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("name"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}
                }},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("name")}},
                {QStringLiteral("additionalProperties"), false}
            },
            [this](const QJsonObject& args) { return m_wrapper->getPlaylistTracks(args.value(QStringLiteral("name")).toString()); });

    addTool(QStringLiteral("removePlaylist"), QStringLiteral("Remove a playlist"),
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("name"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}
                }},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("name")}},
                {QStringLiteral("additionalProperties"), false}
            },
            [this](const QJsonObject& args) { return m_wrapper->removePlaylist(args.value(QStringLiteral("name")).toString()); });

    addTool(QStringLiteral("sortPlaylist"), QStringLiteral("Sort tracks in a playlist"),
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("name"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("description"), QStringLiteral("Name of the playlist to sort")}
                    }},
                    {QStringLiteral("sort"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("description"), QStringLiteral("Sort script (e.g. %artist% | %album%)")}
                    }},
                    {QStringLiteral("order"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("enum"), QJsonArray{QStringLiteral("asc"), QStringLiteral("desc")}}
                    }}
                }},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("name"), QStringLiteral("sort")}},
                {QStringLiteral("additionalProperties"), false}
            },
            [this](const QJsonObject& args) {
                return m_wrapper->sortPlaylist(
                    args.value(QStringLiteral("name")).toString(),
                    args.value(QStringLiteral("sort")).toString(),
                    args.value(QStringLiteral("order")).toString()
                );
            });

    addTool(QStringLiteral("searchLibrary"), QStringLiteral("Search in library with optional sorting"),
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("query"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("description"), QStringLiteral("Fooyin query expression")}
                    }},
                    {QStringLiteral("sort"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("description"), QStringLiteral("Sort script (e.g. %artist% | %album% | %tracknumber%)")}
                    }},
                    {QStringLiteral("order"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("enum"), QJsonArray{QStringLiteral("asc"), QStringLiteral("desc")}}
                    }}
                }},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("query")}},
                {QStringLiteral("additionalProperties"), false}
            },
            [this](const QJsonObject& args) {
                return m_wrapper->searchLibrary(
                    args.value(QStringLiteral("query")).toString(),
                    args.value(QStringLiteral("sort")).toString(),
                    args.value(QStringLiteral("order")).toString()
                );
            });

    // --- INFO / STATE ---
    addTool(QStringLiteral("getNowPlaying"), QStringLiteral("Returns currently playing track info"), emptySchema(),
            [this](const QJsonObject&) { return m_wrapper->getNowPlaying(); });

    addTool(QStringLiteral("getPlaybackState"), QStringLiteral("Returns current playback state and volume"), emptySchema(),
            [this](const QJsonObject&) { return m_wrapper->getPlaybackState(); });

    addTool(QStringLiteral("scanLibrary"), QStringLiteral("Triggers a library scan"), emptySchema(),
            [this](const QJsonObject&) { return m_wrapper->scanLibrary(); });

    addTool(QStringLiteral("updateTrackMetadata"), QStringLiteral("Updates metadata for tracks"),
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("trackIds"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("array")},
                        {QStringLiteral("items"), QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")}}}
                    }},
                    {QStringLiteral("metadata"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("object")},
                        {QStringLiteral("properties"), QJsonObject{
                            {QStringLiteral("title"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                            {QStringLiteral("album"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                            {QStringLiteral("year"), QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")}}},
                            {QStringLiteral("rating"), QJsonObject{{QStringLiteral("type"), QStringLiteral("number")}}},
                            {QStringLiteral("artists"), QJsonObject{
                                {QStringLiteral("type"), QStringLiteral("array")},
                                {QStringLiteral("items"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}
                            }},
                            {QStringLiteral("albumArtists"), QJsonObject{
                                {QStringLiteral("type"), QStringLiteral("array")},
                                {QStringLiteral("items"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}
                            }},
                            {QStringLiteral("genres"), QJsonObject{
                                {QStringLiteral("type"), QStringLiteral("array")},
                                {QStringLiteral("items"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}
                            }},
                            {QStringLiteral("comment"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}
                        }}
                    }},
                    {QStringLiteral("writeToDisk"), QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}}},
                }},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("trackIds"), QStringLiteral("metadata")}},
                {QStringLiteral("additionalProperties"), false}
            },
            [this](const QJsonObject& args) {
                return m_wrapper->updateTrackMetadata(
                    args.value(QStringLiteral("trackIds")).toArray(),
                    args.value(QStringLiteral("metadata")).toObject(),
                    args.value(QStringLiteral("writeToDisk")).toBool()
                );
            });
}

} // namespace FooyinMCP
