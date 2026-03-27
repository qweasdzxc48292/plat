#ifndef CAPTURE_TOOLS_PLUGIN_H
#define CAPTURE_TOOLS_PLUGIN_H

#include <QObject>

#include "../../debug_workbench/idebug_tool_plugin.h"

class QTextEdit;
class QWidget;

class CaptureToolsPlugin : public QObject, public IDebugToolPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IDebugToolPlugin_iid)
    Q_INTERFACES(IDebugToolPlugin)

public:
    CaptureToolsPlugin();
    QString pluginName() const;
    QWidget *createWidget(QWidget *parent = 0);
    void onMessage(const QString &message);

private:
    QTextEdit *m_logView;
};

#endif
