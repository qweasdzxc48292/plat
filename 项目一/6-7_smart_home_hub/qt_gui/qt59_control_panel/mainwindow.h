#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QLabel;
class QLineEdit;
class QPushButton;
class QProcess;
class QTimer;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onLedOn();
    void onLedOff();
    void onReadSensor();
    void onPlayMedia();
    void onStopMedia();
    void onRemoteLedOn();
    void onRemoteLedOff();
    void onRefreshVideoStats();
    void onPollTick();

private:
    void buildUi();
    bool writeLed(bool on);
    bool sendMqttLedControl(bool on);
    bool readSensor(int &humi, int &temp);
    void setInfo(const QString &text);

    QLineEdit *m_ledDevEdit;
    QLineEdit *m_dhtDevEdit;
    QLineEdit *m_mediaFileEdit;
    QLineEdit *m_videoStatsEdit;
    QLineEdit *m_mqttHostEdit;
    QLineEdit *m_mqttPortEdit;
    QLineEdit *m_mqttDownTopicEdit;

    QLabel *m_infoLabel;
    QLabel *m_sensorLabel;
    QLabel *m_videoLabel;
    QLabel *m_mediaLabel;

    QProcess *m_mediaProc;
    QTimer *m_pollTimer;
};

#endif // MAINWINDOW_H
