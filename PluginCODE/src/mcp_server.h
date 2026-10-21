#pragma once

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

namespace FooyinMCP {

class FooyinWrapper;
class McpTools;

class McpServer : public QObject {
    Q_OBJECT
public:
    explicit McpServer(FooyinWrapper* wrapper, QObject* parent = nullptr);
    ~McpServer() override;

    bool start(const QString& pipeName);
    void stop();

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    void handleRequest(QLocalSocket* socket, const QJsonObject& request);
    void sendResponse(QLocalSocket* socket, const QJsonValue& id, const QJsonObject& result);
    void sendError(QLocalSocket* socket, const QJsonValue& id, int code, const QString& message);

    QLocalServer* m_server;
    FooyinWrapper* m_wrapper;
    std::unique_ptr<McpTools> m_tools;
};

} // namespace FooyinMCP
