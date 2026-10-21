#pragma once

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QtWebSockets/QWebSocketServer>
#include <QtWebSockets/QWebSocket>
#include <QJsonObject>
#include <memory>
#include "fooyin_wrapper.h"
#include "mcp_tools.h"

namespace FooyinMCP {

class McpServer : public QObject {
    Q_OBJECT
public:
    explicit McpServer(FooyinWrapper* wrapper, QObject* parent = nullptr);

    bool start(const QString& pipeName = "\\\\.\\pipe\\fooyin_mcp", quint16 wsPort = 8080);
    void stop();

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
    QWebSocketServer* m_webSocketServer;
    QList<QWebSocket*> m_webSocketClients;

    FooyinWrapper* m_wrapper;
    std::unique_ptr<McpTools> m_tools;
};

} // namespace FooyinMCP
