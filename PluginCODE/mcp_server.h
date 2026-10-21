#pragma once

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QtWebSockets/QWebSocketServer>
#include <QtWebSockets/QWebSocket>
#include <QJsonObject>
#include <QList>
#include <memory>
#include "fooyin_wrapper.h"
#include "mcp_tools.h"

namespace FooyinMCP {

class McpServer : public QObject {
    Q_OBJECT
public:
    static constexpr const char* DEFAULT_PIPE_NAME = "\\\\.\\pipe\\fooyin_mcp";
    static constexpr quint16 DEFAULT_WS_PORT = 8080;

    explicit McpServer(FooyinWrapper* wrapper, QObject* parent = nullptr);

    bool start(const QString& pipeName = DEFAULT_PIPE_NAME, quint16 wsPort = DEFAULT_WS_PORT);
    void stop();

    // Broadcast a notification to all connected clients (MCP 10/10 Feature)
    // Marked thread-safe via QMetaObject::invokeMethod if called from other threads
    void broadcastNotification(const QString& method, const QJsonObject& params);

    QString pipeName() const { return m_pipeName; }
    quint16 port() const { return m_port; }

private slots:
    // Pipe slots
    void onNewLocalConnection();
    void onLocalReadyRead();
    void onLocalDisconnected();

    // WebSocket slots
    void onNewWebSocketConnection();
    void onWebSocketMessageReceived(const QString& message);
    void onWebSocketDisconnected();

private:
    void handleRequest(QIODevice* device, QWebSocket* webSocket, const QJsonObject& request);
    void sendResponse(QIODevice* device, QWebSocket* webSocket, const QJsonValue& id, const QJsonObject& result);
    void sendError(QIODevice* device, QWebSocket* webSocket, const QJsonValue& id, int code, const QString& message);

    QLocalServer* m_localServer;
    QList<QLocalSocket*> m_localClients;

    QWebSocketServer* m_webSocketServer;
    QList<QWebSocket*> m_webSocketClients;

    QString m_pipeName;
    quint16 m_port;

    FooyinWrapper* m_wrapper; // Weak pointer, lifetime managed by McpPlugin
    std::unique_ptr<McpTools> m_tools;
};

} // namespace FooyinMCP
