#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>
#include <map>

namespace FooyinMCP {

class FooyinWrapper;

class McpTools : public QObject {
    Q_OBJECT
public:
    explicit McpTools(FooyinWrapper* wrapper, QObject* parent = nullptr);

    QJsonArray getToolDefinitions() const;
    QJsonObject callTool(const QString& name, const QJsonObject& arguments);

private:
    FooyinWrapper* m_wrapper;

    using ToolHandler = std::function<QJsonObject(const QJsonObject&)>;
    std::map<QString, ToolHandler> m_handlers;

    void registerTools();
};

} // namespace FooyinMCP
