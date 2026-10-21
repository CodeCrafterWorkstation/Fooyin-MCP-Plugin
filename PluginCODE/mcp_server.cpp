#include "mcp_plugin.h" // For lcMcp category
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QLocalSocket>
#include <QMetaObject>
#include <QThread>

namespace FooyinMCP {

McpServer::McpServer(FooyinWrapper* wrapper, QObject* parent)
    : QObject(parent),
      m_localServer(new QLocalServer(this)),
      m_webSocketServer(new QWebSocketServer(QStringLiteral("Fooyin MCP Server"), QWebSocketServer::NonSecureMode, this)),
      m_pipeName(DEFAULT_PIPE_NAME),
      m_port(DEFAULT_WS_PORT),
      m_wrapper(wrapper),
      m_tools(std::make_unique<McpTools>(wrapper, this)) {

    connect(m_localServer, &QLocalServer::newConnection, this, &McpServer::onNewLocalConnection);
    connect(m_webSocketServer, &QWebSocketServer::newConnection, this, &McpServer::onNewWebSocketConnection);
}

bool McpServer::start(const QString& pipeName, quint16 wsPort) {
    m_pipeName = pipeName;
    m_port = wsPort;

    QLocalServer::removeServer(m_pipeName);
    bool localOk = m_localServer->listen(m_pipeName);

    // SECURITY: Only listen on LocalHost to prevent unauthorized network access
    bool wsOk = m_webSocketServer->listen(QHostAddress::LocalHost, m_port);

    if (localOk) qDebug() << "FooyinMCP: Pipe listening on" << m_pipeName;
    if (wsOk) qDebug() << "FooyinMCP: WebSocket listening on localhost:" << m_port;

    return localOk || wsOk;
}

void McpServer::stop() {
    m_localServer->close();
    m_webSocketServer->close();

    for(auto* s : m_localClients) {
        s->disconnect();
        s->close();
    }
    qDeleteAll(m_localClients);
    m_localClients.clear();

    for(auto* ws : m_webSocketClients) {
        ws->disconnect();
        ws->close();
    }
    qDeleteAll(m_webSocketClients);
    m_webSocketClients.clear();
}

void McpServer::broadcastNotification(const QString& method, const QJsonObject& params) {
    // Ensure thread-safety: if called from a different thread, invoke on this object's thread
    if (thread() != QThread::currentThread()) {
        QMetaObject::invokeMethod(this, [this, method, params] {
            broadcastNotification(method, params);
        }, Qt::QueuedConnection);
        return;
    }

    QJsonObject notification;
    notification["jsonrpc"] = "2.0";
    notification["method"] = method;
    notification["params"] = params;

    QByteArray data = QJsonDocument(notification).toJson(QJsonDocument::Compact) + "\n";

    for (auto* socket : m_localClients) {
        if (socket->state() == QLocalSocket::ConnectedState) {
            socket->write(data);
        }
    }

    QString wsData = QString::fromUtf8(data).trimmed();
    for (auto* ws : m_webSocketClients) {
        ws->sendTextMessage(wsData);
    }
}

void McpServer::onNewLocalConnection() {
    QLocalSocket* socket = m_localServer->nextPendingConnection();
    if (!socket) return;

    m_localClients << socket;
    connect(socket, &QLocalSocket::readyRead, this, &McpServer::onLocalReadyRead);
    connect(socket, &QLocalSocket::disconnected, this, &McpServer::onLocalDisconnected);
}

void McpServer::onLocalReadyRead() {
    auto* socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket) return;

    while (socket->canReadLine()) {
        QByteArray data = socket->readLine();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            handleRequest(socket, nullptr, doc.object());
        }
    }
}

void McpServer::onLocalDisconnected() {
    auto* socket = qobject_cast<QLocalSocket*>(sender());
    if (socket) {
        m_localClients.removeAll(socket);
        socket->deleteLater();
    }
}

void McpServer::onNewWebSocketConnection() {
    QWebSocket* socket = m_webSocketServer->nextPendingConnection();
    if (!socket) return;

    // SECURITY: Origin Check to prevent Cross-Site WebSocket Hijacking (CSWH)
    // Most MCP clients don't send an Origin. Browsers do.
    QString origin = socket->origin();
    if (!origin.isEmpty() && !origin.startsWith(QStringLiteral("http://localhost")) && !origin.startsWith(QStringLiteral("http://127.0.0.1"))) {
        qCCritical(lcMcp) << "MCP Security: Rejecting WebSocket connection from unauthorized origin:" << origin;
        socket->abort();
        socket->deleteLater();
        return;
    }

    m_webSocketClients << socket;
    connect(socket, &QWebSocket::textMessageReceived, this, &McpServer::onWebSocketMessageReceived);
    connect(socket, &QWebSocket::disconnected, this, &McpServer::onWebSocketDisconnected);
}

void McpServer::onWebSocketMessageReceived(const QString& message) {
    auto* socket = qobject_cast<QWebSocket*>(sender());
    if (!socket) return;

    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isObject()) {
        handleRequest(nullptr, socket, doc.object());
    }
}

void McpServer::onWebSocketDisconnected() {
    auto* socket = qobject_cast<QWebSocket*>(sender());
    if (socket) {
        m_webSocketClients.removeAll(socket);
        socket->deleteLater();
    }
}

void McpServer::handleRequest(QIODevice* device, QWebSocket* webSocket, const QJsonObject& request) {
    QString method = request.value("method").toString();
    QJsonValue id = request.value("id");
    QJsonObject params = request.value("params").toObject();

    if (method == "initialize") {
        QJsonObject result;
        result["protocolVersion"] = "2024-11-05";
        QJsonObject capabilities;
        capabilities["tools"] = QJsonObject();
        result["capabilities"] = capabilities;
        result["serverInfo"] = QJsonObject{{"name", "fooyin-mcp-plugin"}, {"version", "1.2.0"}};
        sendResponse(device, webSocket, id, result);
    }
    else if (method == "tools/list") {
        sendResponse(device, webSocket, id, {{"tools", m_tools->getToolDefinitions()}});
    }
    else if (method == "tools/call") {
        QString toolName = params.value("name").toString();
        QJsonObject toolArgs = params.value("arguments").toObject();
        QJsonObject toolResult = m_tools->callTool(toolName, toolArgs);

        QJsonArray content;
        content.append(QJsonObject{{"type", "text"}, {"text", QJsonDocument(toolResult).toJson(QJsonDocument::Indented)}});
        sendResponse(device, webSocket, id, {{"content", content}});
    }
    else {
        sendError(device, webSocket, id, -32601, "Method not found: " + method);
    }
}

void McpServer::sendResponse(QIODevice* device, QWebSocket* webSocket, const QJsonValue& id, const QJsonObject& result) {
    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = result;

    QByteArray data = QJsonDocument(response).toJson(QJsonDocument::Compact);
    if (device) device->write(data + "\n");
    else if (webSocket) webSocket->sendTextMessage(QString::fromUtf8(data));
}

void McpServer::sendError(QIODevice* device, QWebSocket* webSocket, const QJsonValue& id, int code, const QString& message) {
    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["error"] = QJsonObject{{"code", code}, {"message", message}};

    QByteArray data = QJsonDocument(response).toJson(QJsonDocument::Compact);
    if (device) device->write(data + "\n");
    else if (webSocket) webSocket->sendTextMessage(QString::fromUtf8(data));
}

} // namespace FooyinMCP
