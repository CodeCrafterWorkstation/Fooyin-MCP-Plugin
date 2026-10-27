#include "mcp_server.h"
#include "mcp_logging.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QLocalSocket>
#include <QMetaObject>
#include <QThread>
#include <QUrl>
#include <QDir>
#include <QStandardPaths>
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

bool McpServer::start(const QString& pipeName, quint16 wsPort, const QString& token) {
    m_pipeName = pipeName;
    m_port = wsPort;
    m_token = token;

    if (m_token.isEmpty()) {
        qCWarning(lcMcp) << "Starting server without authentication token! This is insecure for non-local connections.";
    }

    QString listenPath = m_pipeName;

#ifdef Q_OS_WIN
    if (!listenPath.startsWith(QLatin1String("\\\\.\\pipe\\"))) {
        listenPath = DEFAULT_PIPE_NAME;
    }
    QLocalServer::removeServer(listenPath);
#else
    if (listenPath == QLatin1String(DEFAULT_PIPE_NAME) || !listenPath.startsWith(QLatin1Char('/'))) {
        QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        if (runtimeDir.isEmpty()) {
            runtimeDir = QDir::tempPath();
        }
        listenPath = runtimeDir + QStringLiteral("/fooyin_mcp.sock");
    }
    QFile::remove(listenPath);
#endif
    m_pipeName = listenPath;

    bool localOk = m_localServer->listen(m_pipeName);
    bool wsOk = m_webSocketServer->listen(QHostAddress::LocalHost, m_port);

    if (localOk) qCDebug(lcMcp) << "Pipe listening on" << m_pipeName;
    else qCCritical(lcMcp) << "Pipe error:" << m_localServer->errorString();

    if (wsOk) qCDebug(lcMcp) << "WebSocket listening on localhost:" << m_port;
    else qCCritical(lcMcp) << "WS error:" << m_webSocketServer->errorString();

    return localOk || wsOk;
}

void McpServer::stop() {
    if (m_localServer->isListening()) {
        m_localServer->close();
    }
    if (m_webSocketServer->isListening()) {
        m_webSocketServer->close();
    }

    auto localClients = std::move(m_localClients);
    for(auto* s : localClients) {
        if (s) {
            s->disconnect(this);
            if (s->state() == QLocalSocket::ConnectedState) {
                s->abort();
            }
            s->deleteLater();
        }
    }

    auto wsClients = std::move(m_webSocketClients);
    for(auto* ws : wsClients) {
        if (ws) {
            ws->disconnect(this);
            if (ws->state() == QAbstractSocket::ConnectedState) {
                ws->close();
            }
            ws->deleteLater();
        }
    }

    for (auto& subs : m_subscriptions) {
        for (auto& sub : subs) {
            if (sub.scriptSub) {
                sub.scriptSub->deleteLater();
                sub.scriptSub = nullptr;
            }
        }
    }

    m_clientStates.clear();
    m_subscriptions.clear();
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
    QString wsData = QString::fromUtf8(data).trimmed();

    // Resource update logic
    bool isResourceUpdate = method.startsWith(QStringLiteral("notifications/playlist_"));
    QByteArray resData;
    QString resWsData;
    if (isResourceUpdate) {
        QJsonObject resNotify;
        resNotify[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        resNotify[QStringLiteral("method")] = QStringLiteral("notifications/resources/updated");
        resNotify[QStringLiteral("params")] = QJsonObject{{QStringLiteral("uri"), QStringLiteral("playlists://")}};
        resData = QJsonDocument(resNotify).toJson(QJsonDocument::Compact) + "\n";
        resWsData = QString::fromUtf8(resData).trimmed();
    }

    auto sendToSubscribers = [&](QObject* client, bool ws) {
        if (!m_subscriptions.contains(client)) return;
        for (const auto& sub : m_subscriptions[client]) {
            if (sub.uri == QStringLiteral("playlists://")) {
                if (ws) static_cast<QWebSocket*>(client)->sendTextMessage(resWsData);
                else static_cast<QLocalSocket*>(client)->write(resData);
                break;
            }
        }
    };

    for (auto* socket : m_localClients) {
        if (socket->state() == QLocalSocket::ConnectedState && m_clientStates[socket].authenticated) {
            socket->write(data);
            if (isResourceUpdate) sendToSubscribers(socket, false);
        }
    }

    for (auto* ws : m_webSocketClients) {
        if (ws->state() == QAbstractSocket::ConnectedState && m_clientStates[ws].authenticated) {
            ws->sendTextMessage(wsData);
            if (isResourceUpdate) sendToSubscribers(ws, true);
        }
    }
}

void McpServer::onNewLocalConnection() {
    auto* socket = m_localServer->nextPendingConnection();
    if (!socket) return;

    if (m_localClients.size() >= MAX_CONNECTIONS) {
        socket->abort();
        socket->deleteLater();
        return;
    }

    m_localClients << socket;
    // Require token if set, even for local pipes for consistent security
    m_clientStates[socket].authenticated = m_token.isEmpty();

    connect(socket, &QLocalSocket::readyRead, this, &McpServer::onLocalReadyRead);
    connect(socket, &QLocalSocket::disconnected, this, &McpServer::onLocalDisconnected);
}

void McpServer::onLocalReadyRead() {
    auto* socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket) return;

    while (socket->canReadLine()) {
        QByteArray data = socket->readLine(MAX_MESSAGE_SIZE);
        if (data.isEmpty()) continue;

        if (!data.endsWith('\n')) {
             qCWarning(lcMcp) << "Protocol Error: Line too long or unterminated, closing pipe.";
             socket->abort();
             return;
        }

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError) {
            sendError(socket, nullptr, QJsonValue::Null, -32700, err.errorString());
            continue;
        }
        if (doc.isObject()) handleRequest(socket, socket, nullptr, doc.object());
        else sendError(socket, nullptr, QJsonValue::Null, -32600, QStringLiteral("Invalid Request"));
    }

    if (socket->bytesAvailable() > MAX_MESSAGE_SIZE) {
        qCWarning(lcMcp) << "DoS: Buffer exceeded limit. Aborting.";
        socket->abort();
    }
}

void McpServer::onLocalDisconnected() {
    if (auto* socket = qobject_cast<QLocalSocket*>(sender())) {
        m_localClients.removeAll(socket);
        m_clientStates.remove(socket);
        if (m_subscriptions.contains(socket)) {
            for (auto& sub : m_subscriptions[socket]) {
                if (sub.scriptSub) sub.scriptSub->deleteLater();
            }
            m_subscriptions.remove(socket);
        }
        socket->deleteLater();
    }
}

void McpServer::onNewWebSocketConnection() {
    auto* socket = m_webSocketServer->nextPendingConnection();
    if (!socket) return;

    if (m_webSocketClients.size() >= MAX_CONNECTIONS) {
        socket->abort();
        socket->deleteLater();
        return;
    }

    m_webSocketClients << socket;
    m_clientStates[socket].authenticated = m_token.isEmpty(); // Auto-auth if no token set

    connect(socket, &QWebSocket::textMessageReceived, this, &McpServer::onWebSocketMessageReceived);
    connect(socket, &QWebSocket::disconnected, this, &McpServer::onWebSocketDisconnected);
}

void McpServer::onWebSocketMessageReceived(const QString& message) {
    auto* socket = qobject_cast<QWebSocket*>(sender());
    if (!socket) return;

    if (message.size() > MAX_MESSAGE_SIZE) {
        qCWarning(lcMcp) << "DoS: WebSocket message too large. Aborting.";
        socket->abort();
        return;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError) {
        sendError(nullptr, socket, QJsonValue::Null, -32700, err.errorString());
        return;
    }
    if (doc.isObject()) handleRequest(socket, nullptr, socket, doc.object());
    else sendError(nullptr, socket, QJsonValue::Null, -32600, QStringLiteral("Invalid Request"));
}

void McpServer::onWebSocketDisconnected() {
    if (auto* socket = qobject_cast<QWebSocket*>(sender())) {
        m_webSocketClients.removeAll(socket);
        m_clientStates.remove(socket);
        if (m_subscriptions.contains(socket)) {
            for (auto& sub : m_subscriptions[socket]) {
                if (sub.scriptSub) sub.scriptSub->deleteLater();
            }
            m_subscriptions.remove(socket);
        }
        socket->deleteLater();
    }
}

void McpServer::handleRequest(QObject* client, QIODevice* device, QWebSocket* webSocket, const QJsonObject& request) {
    if (thread() != QThread::currentThread()) {
        QMetaObject::invokeMethod(this, [this, client, device, webSocket, request]() {
            handleRequest(client, device, webSocket, request);
        }, Qt::QueuedConnection);
        return;
    }

    auto& state = m_clientStates[client];
    QJsonValue id = request.value(QStringLiteral("id"));
    const bool isNotification = !request.contains(QStringLiteral("id"));

    // 1. Auth check
    if (!state.authenticated) {
        QString method = request.value(QStringLiteral("method")).toString();
        if (method == QStringLiteral("authenticate")) {
            QString token = request.value(QStringLiteral("params")).toObject().value(QStringLiteral("token")).toString();
            if (!m_token.isEmpty() && token == m_token) {
                state.authenticated = true;
                if (!isNotification) sendResponse(device, webSocket, id, QJsonObject{{QStringLiteral("status"), QStringLiteral("authenticated")}});
                return;
            }
        }
        sendError(device, webSocket, id, -32000, QStringLiteral("Authentication required"));
        if (webSocket) webSocket->close(); else if (auto* s = qobject_cast<QLocalSocket*>(device)) s->abort();
        return;
    }

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
        serverInfo[QStringLiteral("version")] = QStringLiteral("1.3.0");

        QJsonObject capabilities;
        capabilities[QStringLiteral("tools")] = QJsonObject{{QStringLiteral("listChanged"), false}};
        capabilities[QStringLiteral("resources")] = QJsonObject{
            {QStringLiteral("subscribe"), true},
            {QStringLiteral("listChanged"), false}
        };

        QJsonObject result;
        result[QStringLiteral("protocolVersion")] = QStringLiteral("2024-11-05");
        result[QStringLiteral("capabilities")] = capabilities;
        result[QStringLiteral("serverInfo")] = serverInfo;

        sendResponse(device, webSocket, id, result);
    }
    else if (method == QStringLiteral("resources/list")) {
        if (isNotification) return;
        QJsonArray resources;
        QJsonObject playlistRes;
        playlistRes[QStringLiteral("uri")] = QStringLiteral("playlists://");
        playlistRes[QStringLiteral("name")] = QStringLiteral("All Playlists");
        playlistRes[QStringLiteral("description")] = QStringLiteral("Real-time list of all playlists and their metadata");
        resources.append(playlistRes);

        sendResponse(device, webSocket, id, QJsonObject{{QStringLiteral("resources"), resources}});
    }
    else if (method == QStringLiteral("resources/subscribe")) {
        if (isNotification) return;
        QString uri = params.value(QStringLiteral("uri")).toString();

        Subscription sub;
        sub.uri = uri;
        if (uri.startsWith(QStringLiteral("scripts://"))) {
            setupScriptSubscription(client, sub, device, webSocket);
        }
        m_subscriptions[client].append(sub);
        sendResponse(device, webSocket, id, QJsonObject());
    }
    else if (method == QStringLiteral("resources/unsubscribe")) {
        if (isNotification) return;
        QString uri = params.value(QStringLiteral("uri")).toString();
        if (m_subscriptions.contains(client)) {
            auto& subs = m_subscriptions[client];
            auto it = std::find_if(subs.begin(), subs.end(), [&](const Subscription& s){ return s.uri == uri; });
            if (it != subs.end()) {
                if (it->scriptSub) it->scriptSub->deleteLater();
                subs.erase(it);
            }
        }
        sendResponse(device, webSocket, id, QJsonObject());
    }
    else if (method == QStringLiteral("resources/read")) {
        if (isNotification) return;
        QString uri = params.value(QStringLiteral("uri")).toString();
        if (uri == QStringLiteral("playlists://")) {
            QJsonObject playlists = m_wrapper->listPlaylists();
            QJsonArray contents;
            QJsonObject content;
            content[QStringLiteral("uri")] = uri;
            content[QStringLiteral("text")] = QString::fromUtf8(QJsonDocument(playlists).toJson(QJsonDocument::Compact));
            contents.append(content);
            sendResponse(device, webSocket, id, QJsonObject{{QStringLiteral("contents"), contents}});
        } else {
            sendError(device, webSocket, id, -32602, QStringLiteral("Invalid URI"));
        }
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
    else if (method == QStringLiteral("notifications/subscribe")) {
        // Alias for resources/subscribe to support some MCP clients
        if (isNotification) return;
        QString uri = params.value(QStringLiteral("uri")).toString();

        Subscription sub;
        sub.uri = uri;
        if (uri.startsWith(QStringLiteral("scripts://"))) {
            setupScriptSubscription(client, sub, device, webSocket);
        }
        m_subscriptions[client].append(sub);
        sendResponse(device, webSocket, id, QJsonObject());
    }
    else if (method == QStringLiteral("logging/setLevel")) {
        if (!isNotification) sendResponse(device, webSocket, id, QJsonObject());
    }
    else if (!isNotification) {
        sendError(device, webSocket, id, -32601, QStringLiteral("Method not found"));
    }
}

void McpServer::onOriginAuthenticationRequired(QWebSocketCorsAuthenticator* authenticator) {
    QString origin = authenticator->origin();
    if (isAllowedOrigin(origin)) {
        authenticator->setAllowed(true);
    } else {
        qCWarning(lcMcp) << "Blocked WebSocket connection from unauthorized origin:" << origin;
        authenticator->setAllowed(false);
    }
}

bool McpServer::isAllowedOrigin(const QString& origin) {
    if (origin.isEmpty()) return true; // Non-browser clients
    QUrl url(origin);
    QString host = url.host();
    return (host == QStringLiteral("localhost") || host == QStringLiteral("127.0.0.1"));
}

void McpServer::setupScriptSubscription(QObject* client, Subscription& sub, QIODevice* device, QWebSocket* webSocket) {
    auto* scriptContext = m_wrapper->scriptContext();
    if (!scriptContext) return;

    QString scriptName = sub.uri.mid(10); // strip "scripts://"
    sub.scriptSub = scriptContext->subscribe(scriptName, [this, client, device, webSocket, scriptName](const QVariant& value) {
        QJsonObject notification;
        notification[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        notification[QStringLiteral("method")] = QStringLiteral("notifications/resources/updated");
        notification[QStringLiteral("params")] = QJsonObject{
            {QStringLiteral("uri"), QStringLiteral("scripts://") + scriptName},
            {QStringLiteral("value"), QJsonValue::fromVariant(value)}
        };

        QByteArray data = QJsonDocument(notification).toJson(QJsonDocument::Compact) + "\n";
        if (webSocket) {
            if (webSocket->state() == QAbstractSocket::ConnectedState)
                webSocket->sendTextMessage(QString::fromUtf8(data).trimmed());
        } else if (auto* socket = qobject_cast<QLocalSocket*>(device)) {
            if (socket->state() == QLocalSocket::ConnectedState)
                socket->write(data);
        }
    });
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