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

    m_handlers["scan_library"] = [this](const QJsonObject&) {
        return m_wrapper->scanLibrary();
    };

    m_handlers["create_playlist"] = [this](const QJsonObject& args) {
        return m_wrapper->createPlaylist(args.value("name").toString());
    };

    m_handlers["get_playlists"] = [this](const QJsonObject&) {
        return m_wrapper->getPlaylists();
    };

    m_handlers["modify_playlist"] = [this](const QJsonObject& args) {
        return m_wrapper->modifyPlaylist(args.value("action").toString(), args);
    };

    m_handlers["set_volume_mode"] = [this](const QJsonObject& args) {
        return m_wrapper->setVolumeAndMode(args);
    };

    m_handlers["manage_queue"] = [this](const QJsonObject& args) {
        return m_wrapper->manageQueue(args.value("action").toString(), args);
    };

    m_handlers["smart_play"] = [this](const QJsonObject& args) {
        return m_wrapper->smartPlay(args.value("query").toString());
    };
}

QJsonArray McpTools::getToolDefinitions() const {
    QJsonArray tools;

    // 1. Playback
    QJsonObject playback;
    playback["name"] = "control_playback";
    playback["description"] = "Control playback (play, pause, stop, next, prev, seek)";
    QJsonObject pbProps;
    pbProps["action"] = QJsonObject{{"type", "string"}, {"enum", QJsonArray{"play", "pause", "stop", "playPause", "next", "previous", "seek"}}};
    pbProps["positionMs"] = QJsonObject{{"type", "number"}};
    playback["inputSchema"] = QJsonObject{{"type", "object"}, {"properties", pbProps}, {"required", QJsonArray{"action"}}};
    tools.append(playback);

    // 2. Now Playing
    tools.append(QJsonObject{{"name", "get_now_playing"}, {"description", "Get current track metadata and state"}});

    // 3. Search Library
    QJsonObject search;
    search["name"] = "search_library";
    search["description"] = "Search the music library";
    search["inputSchema"] = QJsonObject{{"type", "object"}, {"properties", QJsonObject{{"query", QJsonObject{{"type", "string"}}}}}, {"required", QJsonArray{"query"}}};
    tools.append(search);

    // 4. Playlists
    tools.append(QJsonObject{{"name", "get_playlists"}, {"description", "List all playlists"}});

    QJsonObject modPl;
    modPl["name"] = "modify_playlist";
    modPl["description"] = "Add or remove tracks from a playlist";
    QJsonObject modProps;
    modProps["action"] = QJsonObject{{"type", "string"}, {"enum", QJsonArray{"addTrack", "removeTrack"}}};
    modProps["playlistIndex"] = QJsonObject{{"type", "integer"}};
    modProps["path"] = QJsonObject{{"type", "string"}, {"description", "File path for addTrack"}};
    modProps["row"] = QJsonObject{{"type", "integer"}, {"description", "Row index for removeTrack"}};
    modPl["inputSchema"] = QJsonObject{{"type", "object"}, {"properties", modProps}, {"required", QJsonArray{"action", "playlistIndex"}}};
    tools.append(modPl);

    // 5. Volume
    QJsonObject vol;
    vol["name"] = "set_volume_mode";
    vol["description"] = "Set volume (0.0 to 1.0) and shuffle/repeat modes";
    vol["inputSchema"] = QJsonObject{{"type", "object"}, {"properties", QJsonObject{
        {"volume", QJsonObject{{"type", "number"}}},
        {"shuffle", QJsonObject{{"type", "boolean"}}}
    }}};
    tools.append(vol);

    // 6. Queue Management
    QJsonObject queue;
    queue["name"] = "manage_queue";
    queue["description"] = "Manage the playback queue (add tracks to play next or clear queue)";
    QJsonObject qProps;
    qProps["action"] = QJsonObject{{"type", "string"}, {"enum", QJsonArray{"enqueue", "clear"}}};
    qProps["path"] = QJsonObject{{"type", "string"}, {"description", "Full file path of the track to enqueue"}};
    queue["inputSchema"] = QJsonObject{{"type", "object"}, {"properties", qProps}, {"required", QJsonArray{"action"}}};
    tools.append(queue);

    // 7. Smart Play
    QJsonObject smart;
    smart["name"] = "smart_play";
    smart["description"] = "Search for a track and play it immediately (best match)";
    smart["inputSchema"] = QJsonObject{{"type", "object"}, {"properties", QJsonObject{{"query", QJsonObject{{"type", "string"}}}}}, {"required", QJsonArray{"query"}}};
    tools.append(smart);

    return tools;
}

QJsonObject McpTools::callTool(const QString& name, const QJsonObject& arguments) {
    auto it = m_handlers.find(name);
    if (it != m_handlers.end()) {
        return it->second(arguments);
    }
    return {{"error", "Tool not found"}};
}

} // namespace FooyinMCP
