#ifndef LOGIC_VIEW_PLUGIN_H
#define LOGIC_VIEW_PLUGIN_H

#include <QObject>
#include <QString>

#include "../../debug_workbench/idebug_tool_plugin.h"

class QLabel;
class QWidget;

class LogicViewPlugin : public QObject, public IDebugToolPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IDebugToolPlugin_iid)
    Q_INTERFACES(IDebugToolPlugin)

public:
    LogicViewPlugin();
    QString pluginName() const;
    QWidget *createWidget(QWidget *parent = 0);
    void onMessage(const QString &message);

private:
    QLabel *m_statusLabel;
};

#endif
