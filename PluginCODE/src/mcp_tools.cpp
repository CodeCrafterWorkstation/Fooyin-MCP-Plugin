#include "mcp_tools.h"
#include "fooyin_wrapper.h"

namespace FooyinMCP {

McpTools::McpTools(FooyinWrapper* wrapper, QObject* parent)
    : QObject(parent), m_wrapper(wrapper) {
    registerTools();
}

void McpTools::registerTools() {
    m_handlers["control_playback"] = [this](const QJsonObject& args) {
        return m_wrapper->controlPlayback(args.value("action").toString(), args);
    };
    m_handlers["get_now_playing"] = [this](const QJsonObject&) {
        return m_wrapper->getNowPlaying();
    };
    m_handlers["search_library"] = [this](const QJsonObject& args) {
        return m_wrapper->searchLibrary(args.value("query").toString());
    };
    m_handlers["get_library_info"] = [this](const QJsonObject&) {
        return m_wrapper->getLibraryInfo();
    };
    m_handlers["create_playlist"] = [this](const QJsonObject& args) {
        return m_wrapper->createPlaylist(args.value("name").toString());
    };
    m_handlers["get_playlists"] = [this](const QJsonObject&) {
        return m_wrapper->getPlaylists();
    };
    m_handlers["manage_queue"] = [this](const QJsonObject& args) {
        return m_wrapper->manageQueue(args.value("action").toString(), args);
    };
    m_handlers["set_track_rating"] = [this](const QJsonObject& args) {
        return m_wrapper->setTrackRating(args);
    };
    m_handlers["scan_library"] = [this](const QJsonObject&) {
        return m_wrapper->scanLibrary();
    };
}

QJsonArray McpTools::getToolDefinitions() const {
    QJsonArray tools;

    // playback
    QJsonObject playback;
    playback["name"] = "control_playback";
    playback["description"] = "Control playback (play, pause, stop, next, previous, seek)";
    QJsonObject playbackInput;
    playbackInput["type"] = "object";
    QJsonObject playbackProps;
    playbackProps["action"] = QJsonObject{{"type", "string"}, {"enum", QJsonArray{"play", "pause", "stop", "next", "previous", "seek"}}};
    playbackProps["positionMs"] = QJsonObject{{"type", "number"}};
    playbackInput["properties"] = playbackProps;
    playbackInput["required"] = QJsonArray{"action"};
    playback["inputSchema"] = playbackInput;
    tools.append(playback);

    // now playing
    tools.append(QJsonObject{
        {"name", "get_now_playing"},
        {"description", "Returns current track metadata and playback status"},
        {"inputSchema", QJsonObject{{"type", "object"}, {"properties", QJsonObject{}}}}
    });

    // search library
    QJsonObject search;
    search["name"] = "search_library";
    search["description"] = "Search music library by title or artist";
    QJsonObject searchInput;
    searchInput["type"] = "object";
    searchInput["properties"] = QJsonObject{{"query", QJsonObject{{"type", "string"}}}};
    searchInput["required"] = QJsonArray{"query"};
    search["inputSchema"] = searchInput;
    tools.append(search);

    // library info
    tools.append(QJsonObject{
        {"name", "get_library_info"},
        {"description", "Returns statistics about the library"},
        {"inputSchema", QJsonObject{{"type", "object"}}}
    });

    // playlists
    tools.append(QJsonObject{
        {"name", "get_playlists"},
        {"description", "Lists all playlists"},
        {"inputSchema", QJsonObject{{"type", "object"}}}
    });

    // create playlist
    QJsonObject createPl;
    createPl["name"] = "create_playlist";
    createPl["description"] = "Create a new playlist";
    createPl["inputSchema"] = QJsonObject{
        {"type", "object"},
        {"properties", QJsonObject{{"name", QJsonObject{{"type", "string"}}}}},
        {"required", QJsonArray{"name"}}
    };
    tools.append(createPl);

    return tools;
}

QJsonObject McpTools::callTool(const QString& name, const QJsonObject& arguments) {
    if (m_handlers.contains(name)) {
        return m_handlers[name](arguments);
    }
    return {{"error", "Tool not found"}};
}

} // namespace FooyinMCP
