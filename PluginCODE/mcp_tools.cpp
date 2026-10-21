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

    QJsonObject result = m_tools[name].handler(arguments);

    // Ensure that if the wrapper says "ok: false", we mark it as "isError" for the MCP server
    if (result.contains(QStringLiteral("ok")) && !result.value(QStringLiteral("ok")).toBool()) {
        result[QStringLiteral("isError")] = true;
    }

    return result;
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
    addTool(QStringLiteral("addTrack"), QStringLiteral("Adds a file path to the current playlist"),
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("path"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}
                }},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("path")}},
                {QStringLiteral("additionalProperties"), false}
            },
            [this](const QJsonObject& args) { return m_wrapper->addTrack(args.value(QStringLiteral("path")).toString()); });

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

    // --- INFO / STATE ---
    addTool(QStringLiteral("getNowPlaying"), QStringLiteral("Returns currently playing track info"), emptySchema(),
            [this](const QJsonObject&) { return m_wrapper->getNowPlaying(); });

    addTool(QStringLiteral("getPlaybackState"), QStringLiteral("Returns current playback state and volume"), emptySchema(),
            [this](const QJsonObject&) { return m_wrapper->getPlaybackState(); });

    addTool(QStringLiteral("scanLibrary"), QStringLiteral("Triggers a library scan"), emptySchema(),
            [this](const QJsonObject&) { return m_wrapper->scanLibrary(); });
}

} // namespace FooyinMCP
