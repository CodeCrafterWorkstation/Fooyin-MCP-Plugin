#include "mcp_server.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>

namespace FooyinMCP {

McpServer::McpServer(FooyinWrapper* wrapper, QObject* parent)
    : QObject(parent), m_server(new QLocalServer(this)), m_wrapper(wrapper),
      m_tools(std::make_unique<McpTools>(wrapper, this)) {
    connect(m_server, &QLocalServer::newConnection, this, &McpServer::onNewConnection);
}

bool McpServer::start(const QString& pipeName) {
    QLocalServer::removeServer(pipeName);
    bool ok = m_server->listen(pipeName);
    if (ok) {
        qDebug() << "MCP Server listening on" << pipeName;
    } else {
        qCritical() << "MCP Server failed to listen on" << pipeName << ":" << m_server->errorString();
    }
    return ok;
}

void McpServer::stop() {
    m_server->close();
}

void McpServer::onNewConnection() {
    QLocalSocket* socket = m_server->nextPendingConnection();
    if (!socket) return;

    connect(socket, &QLocalSocket::readyRead, this, &McpServer::onReadyRead);
    connect(socket, &QLocalSocket::disconnected, this, &McpServer::onDisconnected);
}

void McpServer::onReadyRead() {
    auto* socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket) return;

    while (socket->canReadLine()) {
        QByteArray data = socket->readLine();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            handleRequest(socket, doc.object());
        }
    }
}

void McpServer::onDisconnected() {
    auto* socket = qobject_cast<QLocalSocket*>(sender());
    if (socket) socket->deleteLater();
}

void McpServer::handleRequest(QLocalSocket* socket, const QJsonObject& request) {
    QString method = request.value("method").toString();
    QJsonValue id = request.value("id");
    QJsonObject params = request.value("params").toObject();

    if (method == "initialize") {
        QJsonObject result;
        result["protocolVersion"] = "2024-11-05"; // MCP version
        QJsonObject capabilities;
        capabilities["tools"] = QJsonObject();
        result["capabilities"] = capabilities;
        result["serverInfo"] = QJsonObject{{"name", "fooyin-mcp-plugin"}, {"version", "1.0.0"}};
        sendResponse(socket, id, result);
    }
    else if (method == "tools/list") {
        QJsonArray tools = m_tools->getToolDefinitions();
        sendResponse(socket, id, {{"tools", tools}});
    }
    else if (method == "tools/call") {
        QString toolName = params.value("name").toString();
        QJsonObject toolArgs = params.value("arguments").toObject();
        QJsonObject toolResult = m_tools->callTool(toolName, toolArgs);

        QJsonArray content;
        content.append(QJsonObject{
            {"type", "text"},
            {"text", QJsonDocument(toolResult).toJson(QJsonDocument::Indented)}
        });

        sendResponse(socket, id, {{"content", content}});
    }
    else if (method == "notifications/initialized") {
        // Just consume
    }
    else {
        sendError(socket, id, -32601, "Method not found: " + method);
    }
}

void McpServer::sendResponse(QLocalSocket* socket, const QJsonValue& id, const QJsonObject& result) {
    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = result;
    socket->write(QJsonDocument(response).toJson(QJsonDocument::Compact) + "\n");
}

void McpServer::sendError(QLocalSocket* socket, const QJsonValue& id, int code, const QString& message) {
    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["error"] = QJsonObject{{"code", code}, {"message", message}};
    socket->write(QJsonDocument(response).toJson(QJsonDocument::Compact) + "\n");
}

} // namespace FooyinMCP
