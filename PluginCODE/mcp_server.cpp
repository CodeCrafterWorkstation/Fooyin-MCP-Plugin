#include "mcp_server.h"
#include "mcp_logging.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QLocalSocket>
#include <QMetaObject>
#include <QThread>
#include <QUrl>
#include <utility>

namespace FooyinMCP {

McpServer::McpServer(FooyinWrapper* wrapper, QObject* parent)
    : QObject(parent),
      m_localServer(new QLocalServer(this)),
      m_webSocketServer(new QWebSocketServer(QStringLiteral("Fooyin MCP Server"), QWebSocketServer::NonSecureMode, this)),
      m_pipeName(DEFAULT_PIPE_NAME),
      m_port(DEFAULT_WS_PORT),
      m_wrapper(wrapper),
      m_tools(std::make_unique<McpTools>(wrapper, this)) {

    m_localServer->setSocketOptions(QLocalServer::UserAccessOption);

    connect(m_localServer, &QLocalServer::newConnection, this, &McpServer::onNewLocalConnection);
    connect(m_webSocketServer, &QWebSocketServer::newConnection, this, &McpServer::onNewWebSocketConnection);
    connect(m_webSocketServer, &QWebSocketServer::originAuthenticationRequired, this, &McpServer::onOriginAuthenticationRequired);
}

bool McpServer::start(const QString& pipeName, quint16 wsPort) {
    m_pipeName = pipeName;
    m_port = wsPort;

    QLocalServer::removeServer(m_pipeName);
    bool localOk = m_localServer->listen(m_pipeName);
    bool wsOk = m_webSocketServer->listen(QHostAddress::LocalHost, m_port);

    if (localOk) qCDebug(lcMcp) << "Pipe listening on" << m_pipeName;
    else qCCritical(lcMcp) << "Pipe error:" << m_localServer->errorString();

    if (wsOk) qCDebug(lcMcp) << "WebSocket listening on localhost:" << m_port;
    else qCCritical(lcMcp) << "WS error:" << m_webSocketServer->errorString();

    return localOk || wsOk;
}

void McpServer::stop() {
    m_localServer->close();
    m_webSocketServer->close();

    auto localClients = std::move(m_localClients);
    for(auto* s : localClients) {
        s->disconnect(this);
        s->abort();
        s->deleteLater();
    }

    auto wsClients = std::move(m_webSocketClients);
    for(auto* ws : wsClients) {
        ws->disconnect(this);
        ws->close();
        ws->deleteLater();
    }
}

void McpServer::broadcastNotification(const QString& method, const QJsonObject& params) {
    if (thread() != QThread::currentThread()) {
        QMetaObject::invokeMethod(this, [this, method, params] {
            broadcastNotification(method, params);
        }, Qt::QueuedConnection);
        return;
    }

    QJsonObject notification;
    notification[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    notification[QStringLiteral("method")] = method;
    notification[QStringLiteral("params")] = params;

    QByteArray data = QJsonDocument(notification).toJson(QJsonDocument::Compact) + "\n";
    for (auto* socket : m_localClients) {
        if (socket->state() == QLocalSocket::ConnectedState) socket->write(data);
    }

    QString wsData = QString::fromUtf8(data).trimmed();
    for (auto* ws : m_webSocketClients) {
        if (ws->state() == QAbstractSocket::ConnectedState) ws->sendTextMessage(wsData);
    }
}

void McpServer::onNewLocalConnection() {
    if (m_localClients.size() >= MAX_CONNECTIONS) {
        if (auto* s = m_localServer->nextPendingConnection()) {
            s->abort();
            s->deleteLater();
        }
        return;
    }
    if (auto* socket = m_localServer->nextPendingConnection()) {
        m_localClients << socket;
        connect(socket, &QLocalSocket::readyRead, this, &McpServer::onLocalReadyRead);
        connect(socket, &QLocalSocket::disconnected, this, &McpServer::onLocalDisconnected);
    }
}

void McpServer::onLocalReadyRead() {
    auto* socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket) return;

    while (socket->canReadLine()) {
        QByteArray data = socket->readLine(MAX_MESSAGE_SIZE);
        if (data.isEmpty()) continue;

        if (!data.endsWith('\n')) {
             qCWarning(lcMcp) << "Protocol Error: Unterminated line, closing pipe.";
             socket->abort();
             return;
        }

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError) {
            sendError(socket, nullptr, QJsonValue::Null, -32700, err.errorString());
            continue;
        }
        if (doc.isObject()) handleRequest(socket, nullptr, doc.object());
        else sendError(socket, nullptr, QJsonValue::Null, -32600, QStringLiteral("Invalid Request"));
    }

    if (socket->bytesAvailable() > MAX_MESSAGE_SIZE) {
        qCWarning(lcMcp) << "DoS: Line buffer exceeded limit without newline. Aborting.";
        socket->abort();
    }
}

void McpServer::onLocalDisconnected() {
    if (auto* socket = qobject_cast<QLocalSocket*>(sender())) {
        m_localClients.removeAll(socket);
        socket->deleteLater();
    }
}

void McpServer::onNewWebSocketConnection() {
    if (auto* socket = m_webSocketServer->nextPendingConnection()) {
        if (m_webSocketClients.size() >= MAX_CONNECTIONS || !isAllowedOrigin(socket->origin())) {
            socket->abort();
            socket->deleteLater();
            return;
        }
        m_webSocketClients << socket;
        connect(socket, &QWebSocket::textMessageReceived, this, &McpServer::onWebSocketMessageReceived);
        connect(socket, &QWebSocket::disconnected, this, &McpServer::onWebSocketDisconnected);
    }
}

void McpServer::onOriginAuthenticationRequired(QWebSocketCorsAuthenticator* authenticator) {
    authenticator->setAllowed(isAllowedOrigin(authenticator->origin()));
}

bool McpServer::isAllowedOrigin(const QString& origin) {
    if (origin.isEmpty()) return true;

    QUrl url(origin);
    QString host = url.host().toLower();
    if (host.startsWith('[') && host.endsWith(']')) host = host.mid(1, host.length() - 2);

    return host == QStringLiteral("localhost") || host == QStringLiteral("127.0.0.1") || host == QStringLiteral("::1");
}

void McpServer::onWebSocketMessageReceived(const QString& message) {
    auto* socket = qobject_cast<QWebSocket*>(sender());
    if (!socket) return;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError) {
        sendError(nullptr, socket, QJsonValue::Null, -32700, err.errorString());
        return;
    }
    if (doc.isObject()) handleRequest(nullptr, socket, doc.object());
    else sendError(nullptr, socket, QJsonValue::Null, -32600, QStringLiteral("Invalid Request"));
}

void McpServer::onWebSocketDisconnected() {
    if (auto* socket = qobject_cast<QWebSocket*>(sender())) {
        m_webSocketClients.removeAll(socket);
        socket->deleteLater();
    }
}

void McpServer::handleRequest(QIODevice* device, QWebSocket* webSocket, const QJsonObject& request) {
    QJsonValue id = request.value(QStringLiteral("id"));
    const bool isNotification = !request.contains(QStringLiteral("id"));

    if (!isNotification && !id.isString() && !id.isDouble() && !id.isNull()) {
        sendError(device, webSocket, QJsonValue::Null, -32600, QStringLiteral("Invalid ID type"));
        return;
    }

    if (request.value(QStringLiteral("jsonrpc")).toString() != QStringLiteral("2.0")) {
        if (!isNotification) sendError(device, webSocket, id, -32600, QStringLiteral("Invalid JSON-RPC version"));
        return;
    }

    QString method = request.value(QStringLiteral("method")).toString();
    QJsonObject params = request.value(QStringLiteral("params")).toObject();

    if (method == QStringLiteral("initialize")) {
        if (isNotification) return;
        QJsonObject serverInfo;
        serverInfo[QStringLiteral("name")] = QStringLiteral("fooyin-mcp-plugin");
        serverInfo[QStringLiteral("version")] = QStringLiteral("1.2.1");

        QJsonObject capabilities;
        capabilities[QStringLiteral("tools")] = QJsonObject{{QStringLiteral("listChanged"), false}};

        QJsonObject result;
        result[QStringLiteral("protocolVersion")] = QStringLiteral("2024-11-05");
        result[QStringLiteral("capabilities")] = capabilities;
        result[QStringLiteral("serverInfo")] = serverInfo;

        sendResponse(device, webSocket, id, result);
    }
    else if (method == QStringLiteral("ping")) {
        if (!isNotification) sendResponse(device, webSocket, id, QJsonObject());
    }
    else if (method == QStringLiteral("tools/list")) {
        if (!isNotification) sendResponse(device, webSocket, id, QJsonObject{{QStringLiteral("tools"), m_tools->getToolDefinitions()}});
    }
    else if (method == QStringLiteral("tools/call")) {
        if (isNotification) return;
        QJsonObject toolResult = m_tools->callTool(params.value(QStringLiteral("name")).toString(), params.value(QStringLiteral("arguments")).toObject());

        if (toolResult.value(QStringLiteral("isError")).toBool()) {
            QJsonObject item;
            item[QStringLiteral("type")] = QStringLiteral("text");
            item[QStringLiteral("text")] = toolResult.value(QStringLiteral("error")).toString();

            QJsonObject errorData;
            errorData[QStringLiteral("content")] = QJsonArray{item};
            errorData[QStringLiteral("isError")] = true;

            sendResponse(device, webSocket, id, errorData);
            return;
        }

        QJsonObject item;
        item[QStringLiteral("type")] = QStringLiteral("text");
        item[QStringLiteral("text")] = QString::fromUtf8(QJsonDocument(toolResult).toJson(QJsonDocument::Compact));

        QJsonObject responseObj;
        responseObj[QStringLiteral("content")] = QJsonArray{item};

        sendResponse(device, webSocket, id, responseObj);
    }
    else if (!isNotification) {
        sendError(device, webSocket, id, -32601, QStringLiteral("Method not found"));
    }
}

void McpServer::sendResponse(QIODevice* device, QWebSocket* webSocket, const QJsonValue& id, const QJsonObject& result) {
    if (!id.isString() && !id.isDouble() && !id.isNull()) return;

    QJsonObject response;
    response[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    response[QStringLiteral("id")] = id;
    response[QStringLiteral("result")] = result;

    QByteArray data = QJsonDocument(response).toJson(QJsonDocument::Compact);
    if (device) device->write(data + "\n");
    else if (webSocket) webSocket->sendTextMessage(QString::fromUtf8(data));
}

void McpServer::sendError(QIODevice* device, QWebSocket* webSocket, const QJsonValue& id, int code, const QString& message) {
    QJsonObject errorObj;
    errorObj[QStringLiteral("code")] = code;
    errorObj[QStringLiteral("message")] = message;

    QJsonObject response;
    response[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    response[QStringLiteral("id")] = id;
    response[QStringLiteral("error")] = errorObj;

    QByteArray data = QJsonDocument(response).toJson(QJsonDocument::Compact);
    if (device) device->write(data + "\n");
    else if (webSocket) webSocket->sendTextMessage(QString::fromUtf8(data));
}

} // namespace FooyinMCP