#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QVector>

class QPluginLoader;
class QPushButton;
class QTabWidget;
class QTextEdit;
class QTimer;
class QWidget;
class IDebugToolPlugin;
class QObject;

struct LoadedPluginItem {
    QPluginLoader *loader;
    IDebugToolPlugin *plugin;
    QObject *instance;
    QWidget *widget;
    QString fileName;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void reloadPlugins();
    void emitHeartbeat();

private:
    void setupUi();
    void appendLog(const QString &message);
    void loadPlugins(const QString &path);
    void unloadPlugins();

    QTabWidget *m_tabWidget;
    QTextEdit *m_logView;
    QPushButton *m_reloadButton;
    QTimer *m_heartbeatTimer;
    QString m_pluginDir;
    QVector<LoadedPluginItem> m_plugins;
};

#endif
