#pragma once

#include <QObject>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>

namespace FooyinMCP {

class FooyinWrapper;

class McpTools : public QObject {
    Q_OBJECT
public:
    explicit McpTools(FooyinWrapper* wrapper, QObject* parent = nullptr);

    QJsonArray getToolDefinitions() const;
    QJsonObject callTool(const QString& name, const QJsonObject& arguments);

private:
    struct Tool {
        QString description;
        QJsonObject inputSchema;
        std::function<QJsonObject(const QJsonObject&)> handler;
    };

    QJsonObject validateArguments(const QJsonObject& schema, const QJsonObject& args);
    void registerTools();
    FooyinWrapper* m_wrapper;
    QMap<QString, Tool> m_tools;
};

} // namespace FooyinMCP
