#include "mcp_server.h"
#include "fooyin_wrapper.h"
#include "mcp_tools.h"
#include <QWebSocket>
#include <QWebSocketServer>
#include <QJsonDocument>
#include <QDebug>

namespace FooyinMCP {

McpServer::McpServer(FooyinWrapper* wrapper, QObject* parent)
    : QObject(parent)
    , m_server(new QWebSocketServer("Fooyin MCP Server", QWebSocketServer::NonSecureMode, this))
    , m_wrapper(wrapper)
    , m_tools(std::make_unique<McpTools>(wrapper, this))
{
    connect(m_server, &QWebSocketServer::newConnection, this, &McpServer::onNewConnection);
}

McpServer::~McpServer()
{
    stop();
}

bool McpServer::start(const QString& pipeName)
{
    // Wir nutzen Port 8080 als Standard für MCP-WebSockets
    return m_server->listen(QHostAddress::LocalHost, 8080);
}

void McpServer::stop()
{
    m_server->close();
}

void McpServer::onNewConnection()
{
    QWebSocket* socket = m_server->nextPendingConnection();
    connect(socket, &QWebSocket::textMessageReceived, this, [this, socket](const QString& message) {
        QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
        if (doc.isObject()) {
            handleRequest(socket, doc.object());
        }
    });
    connect(socket, &QWebSocket::disconnected, socket, &QWebSocket::deleteLater);
}

void McpServer::handleRequest(QWebSocket* socket, const QJsonObject& request)
{
    QString method = request.value("method").toString();
    QJsonValue id = request.value("id");
    QJsonObject params = request.value("params").toObject();

    if (method == "initialize") {
        sendResponse(socket, id, QJsonObject{
            {"protocolVersion", "1.0"},
            {"capabilities", QJsonObject{}},
            {"serverInfo", QJsonObject{{"name", "fooyin-mcp-plugin"}, {"version", "1.0.0"}}}
        });
    } else if (method == "tools/list") {
        sendResponse(socket, id, QJsonObject{{"tools", m_tools->getToolDefinitions()}});
    } else if (method == "tools/call") {
        QString toolName = params.value("name").toString();
        QJsonObject args = params.value("arguments").toObject();
        QJsonObject result = m_tools->callTool(toolName, args);

        QJsonArray content;
        content.append(QJsonObject{{"type", "text"}, {"text", QJsonDocument(result).toJson(QJsonDocument::Compact)}});
        sendResponse(socket, id, QJsonObject{{"content", content}});
    } else {
        sendError(socket, id, -32601, "Method not found");
    }
}

void McpServer::sendResponse(QWebSocket* socket, const QJsonValue& id, const QJsonObject& result)
{
    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = result;
    socket->sendTextMessage(QJsonDocument(response).toJson(QJsonDocument::Compact));
}

void McpServer::sendError(QWebSocket* socket, const QJsonValue& id, int code, const QString& message)
{
    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["error"] = QJsonObject{{"code", code}, {"message", message}};
    socket->sendTextMessage(QJsonDocument(response).toJson(QJsonDocument::Compact));
}

} // namespace FooyinMCP
