#pragma once

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QtWebSockets/QWebSocketServer>
#include <QtWebSockets/QWebSocket>
#include <QtWebSockets/QWebSocketCorsAuthenticator>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <memory>
#include "fooyin_wrapper.h"
#include "mcp_tools.h"

#include <core/scripting/scriptcontextservice.h>

namespace FooyinMCP {

class McpServer : public QObject {
    Q_OBJECT
public:
    static constexpr const char* DEFAULT_PIPE_NAME = "\\\\.\\pipe\\fooyin_mcp";
    static constexpr quint16 DEFAULT_WS_PORT = 8080;
    static constexpr qint64 MAX_MESSAGE_SIZE = 1024 * 1024; // 1 MiB
    static constexpr int MAX_CONNECTIONS = 20;

    explicit McpServer(FooyinWrapper* wrapper, QObject* parent = nullptr);

    bool start(const QString& pipeName = DEFAULT_PIPE_NAME, quint16 wsPort = DEFAULT_WS_PORT, const QString& token = QString());
    void stop();

    void broadcastNotification(const QString& method, const QJsonObject& params);

    QString pipeName() const { return m_pipeName; }
    quint16 port() const { return m_port; }
    QString token() const { return m_token; }

private slots:
    void onNewLocalConnection();
    void onLocalReadyRead();
    void onLocalDisconnected();

    void onNewWebSocketConnection();
    void onWebSocketMessageReceived(const QString& message);
    void onWebSocketDisconnected();
    void onOriginAuthenticationRequired(QWebSocketCorsAuthenticator* authenticator);

private:
    struct Subscription {
        QString uri;
        Fooyin::ScriptSubscription* scriptSub = nullptr;
    };

    struct ClientState {
        bool authenticated = false;
        QByteArray buffer;
    };

    void handleRequest(QObject* client, QIODevice* device, QWebSocket* webSocket, const QJsonObject& request);
    void sendResponse(QIODevice* device, QWebSocket* webSocket, const QJsonValue& id, const QJsonObject& result);
    void sendError(QIODevice* device, QWebSocket* webSocket, const QJsonValue& id, int code, const QString& message);
    bool isAllowedOrigin(const QString& origin);
    void setupScriptSubscription(QObject* client, Subscription& sub, QIODevice* device, QWebSocket* webSocket);

    QMap<QObject*, QList<Subscription>> m_subscriptions;
    QMap<QObject*, ClientState> m_clientStates;

    QLocalServer* m_localServer;
    QList<QLocalSocket*> m_localClients;

    QWebSocketServer* m_webSocketServer;
    QList<QWebSocket*> m_webSocketClients;

    QString m_pipeName;
    quint16 m_port;
    QString m_token;

    FooyinWrapper* m_wrapper;
    std::unique_ptr<McpTools> m_tools;
};

} // namespace FooyinMCP
