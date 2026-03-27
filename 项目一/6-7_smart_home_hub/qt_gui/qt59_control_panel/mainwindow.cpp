#include "mainwindow.h"

#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <fcntl.h>
#include <unistd.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_ledDevEdit(nullptr),
      m_dhtDevEdit(nullptr),
      m_mediaFileEdit(nullptr),
      m_videoStatsEdit(nullptr),
      m_mqttHostEdit(nullptr),
      m_mqttPortEdit(nullptr),
      m_mqttDownTopicEdit(nullptr),
      m_infoLabel(nullptr),
      m_sensorLabel(nullptr),
      m_videoLabel(nullptr),
      m_mediaLabel(nullptr),
      m_mediaProc(new QProcess(this)),
      m_pollTimer(new QTimer(this))
{
    buildUi();

    connect(m_pollTimer, &QTimer::timeout, this, &MainWindow::onPollTick);
    m_pollTimer->start(2000);

    connect(m_mediaProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
                m_mediaLabel->setText("Media: stopped");
            });
}

MainWindow::~MainWindow()
{
    if (m_mediaProc->state() != QProcess::NotRunning) {
        m_mediaProc->terminate();
        if (!m_mediaProc->waitForFinished(1000)) {
            m_mediaProc->kill();
        }
    }
}

void MainWindow::buildUi()
{
    QWidget *central = new QWidget(this);
    QVBoxLayout *root = new QVBoxLayout(central);

    QHBoxLayout *devRow = new QHBoxLayout();
    devRow->addWidget(new QLabel("LED dev:", this));
    m_ledDevEdit = new QLineEdit("/dev/hub_led", this);
    devRow->addWidget(m_ledDevEdit);
    devRow->addWidget(new QLabel("DHT11 dev:", this));
    m_dhtDevEdit = new QLineEdit("/dev/mydht11", this);
    devRow->addWidget(m_dhtDevEdit);
    root->addLayout(devRow);

    QHBoxLayout *videoRow = new QHBoxLayout();
    videoRow->addWidget(new QLabel("Video stats:", this));
    m_videoStatsEdit = new QLineEdit("/tmp/hub_video_stats.json", this);
    videoRow->addWidget(m_videoStatsEdit);
    QPushButton *videoRefreshBtn = new QPushButton("Refresh Video", this);
    videoRow->addWidget(videoRefreshBtn);
    root->addLayout(videoRow);

    QHBoxLayout *ledRow = new QHBoxLayout();
    QPushButton *ledOnBtn = new QPushButton("LED ON", this);
    QPushButton *ledOffBtn = new QPushButton("LED OFF", this);
    QPushButton *sensorBtn = new QPushButton("Read Sensor", this);
    ledRow->addWidget(ledOnBtn);
    ledRow->addWidget(ledOffBtn);
    ledRow->addWidget(sensorBtn);
    root->addLayout(ledRow);

    QHBoxLayout *mqttRow1 = new QHBoxLayout();
    mqttRow1->addWidget(new QLabel("MQTT host:", this));
    m_mqttHostEdit = new QLineEdit("127.0.0.1", this);
    mqttRow1->addWidget(m_mqttHostEdit);
    mqttRow1->addWidget(new QLabel("port:", this));
    m_mqttPortEdit = new QLineEdit("1883", this);
    mqttRow1->addWidget(m_mqttPortEdit);
    mqttRow1->addWidget(new QLabel("down topic:", this));
    m_mqttDownTopicEdit = new QLineEdit("/iot/down", this);
    mqttRow1->addWidget(m_mqttDownTopicEdit);
    root->addLayout(mqttRow1);

    QHBoxLayout *mqttRow2 = new QHBoxLayout();
    QPushButton *remoteOnBtn = new QPushButton("Remote LED ON", this);
    QPushButton *remoteOffBtn = new QPushButton("Remote LED OFF", this);
    mqttRow2->addWidget(remoteOnBtn);
    mqttRow2->addWidget(remoteOffBtn);
    root->addLayout(mqttRow2);

    QHBoxLayout *mediaRow = new QHBoxLayout();
    mediaRow->addWidget(new QLabel("Media file:", this));
    m_mediaFileEdit = new QLineEdit("/root/media/test.wav", this);
    mediaRow->addWidget(m_mediaFileEdit);
    QPushButton *playBtn = new QPushButton("Play", this);
    QPushButton *stopBtn = new QPushButton("Stop", this);
    mediaRow->addWidget(playBtn);
    mediaRow->addWidget(stopBtn);
    root->addLayout(mediaRow);

    m_infoLabel = new QLabel("Ready", this);
    m_sensorLabel = new QLabel("Sensor: --", this);
    m_videoLabel = new QLabel("Video: --", this);
    m_mediaLabel = new QLabel("Media: stopped", this);
    root->addWidget(m_infoLabel);
    root->addWidget(m_sensorLabel);
    root->addWidget(m_videoLabel);
    root->addWidget(m_mediaLabel);

    setCentralWidget(central);
    setWindowTitle("Smart Home Qt5.9 Control Panel");
    resize(860, 320);

    connect(ledOnBtn, &QPushButton::clicked, this, &MainWindow::onLedOn);
    connect(ledOffBtn, &QPushButton::clicked, this, &MainWindow::onLedOff);
    connect(sensorBtn, &QPushButton::clicked, this, &MainWindow::onReadSensor);
    connect(playBtn, &QPushButton::clicked, this, &MainWindow::onPlayMedia);
    connect(stopBtn, &QPushButton::clicked, this, &MainWindow::onStopMedia);
    connect(remoteOnBtn, &QPushButton::clicked, this, &MainWindow::onRemoteLedOn);
    connect(remoteOffBtn, &QPushButton::clicked, this, &MainWindow::onRemoteLedOff);
    connect(videoRefreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshVideoStats);
}

void MainWindow::setInfo(const QString &text)
{
    m_infoLabel->setText(text);
}

bool MainWindow::writeLed(bool on)
{
    const QString path = m_ledDevEdit->text().trimmed();
    unsigned char buf[2];
    int fd;
    ssize_t n;

    buf[0] = 0;
    buf[1] = on ? 0 : 1; // active-low board

    fd = ::open(path.toLocal8Bit().constData(), O_RDWR);
    if (fd < 0) {
        setInfo(QString("open %1 failed").arg(path));
        return false;
    }
    n = ::write(fd, buf, sizeof(buf));
    ::close(fd);
    if (n != sizeof(buf)) {
        setInfo(QString("write %1 failed").arg(path));
        return false;
    }
    return true;
}

bool MainWindow::readSensor(int &humi, int &temp)
{
    const QString path = m_dhtDevEdit->text().trimmed();
    unsigned char data[2] = {0, 0};
    int fd;
    ssize_t n;

    fd = ::open(path.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        setInfo(QString("open %1 failed").arg(path));
        return false;
    }
    n = ::read(fd, data, sizeof(data));
    ::close(fd);
    if (n != sizeof(data)) {
        setInfo(QString("read %1 failed").arg(path));
        return false;
    }

    humi = static_cast<int>(data[0]);
    temp = static_cast<int>(data[1]);
    return true;
}

bool MainWindow::sendMqttLedControl(bool on)
{
    const QString host = m_mqttHostEdit->text().trimmed();
    const QString port = m_mqttPortEdit->text().trimmed();
    const QString topic = m_mqttDownTopicEdit->text().trimmed();
    const QString payload = QString("{\"method\":\"control\",\"params\":{\"LED1\":%1},\"id\":\"qt-local\"}")
                                .arg(on ? 1 : 0);
    QString program = "mosquitto_pub";
    QStringList args;
    int code;

    if (host.isEmpty() || port.isEmpty() || topic.isEmpty()) {
        setInfo("mqtt host/port/topic invalid");
        return false;
    }

    args << "-h" << host << "-p" << port << "-t" << topic << "-q" << "1" << "-m" << payload;
    code = QProcess::execute(program, args);
    if (code != 0) {
        setInfo("mosquitto_pub failed");
        return false;
    }
    return true;
}

void MainWindow::onLedOn()
{
    if (writeLed(true)) {
        setInfo("LED ON done");
    }
}

void MainWindow::onLedOff()
{
    if (writeLed(false)) {
        setInfo("LED OFF done");
    }
}

void MainWindow::onReadSensor()
{
    int humi = 0;
    int temp = 0;
    if (readSensor(humi, temp)) {
        m_sensorLabel->setText(QString("Sensor: temp=%1 C, humi=%2 %%").arg(temp).arg(humi));
        setInfo("sensor read ok");
    }
}

void MainWindow::onPlayMedia()
{
    const QString file = m_mediaFileEdit->text().trimmed();
    if (file.isEmpty()) {
        setInfo("media file empty");
        return;
    }
    if (!QFileInfo::exists(file)) {
        setInfo(QString("media file not found: %1").arg(file));
        return;
    }

    if (m_mediaProc->state() != QProcess::NotRunning) {
        m_mediaProc->terminate();
        m_mediaProc->waitForFinished(1000);
    }

    m_mediaProc->start("aplay", QStringList() << "-q" << file);
    if (!m_mediaProc->waitForStarted(1000)) {
        setInfo("start aplay failed");
        return;
    }
    m_mediaLabel->setText(QString("Media: playing %1").arg(file));
    setInfo("media play started");
}

void MainWindow::onStopMedia()
{
    if (m_mediaProc->state() == QProcess::NotRunning) {
        m_mediaLabel->setText("Media: stopped");
        setInfo("media already stopped");
        return;
    }

    m_mediaProc->terminate();
    if (!m_mediaProc->waitForFinished(1000)) {
        m_mediaProc->kill();
        m_mediaProc->waitForFinished(1000);
    }
    m_mediaLabel->setText("Media: stopped");
    setInfo("media stopped");
}

void MainWindow::onRemoteLedOn()
{
    if (sendMqttLedControl(true)) {
        setInfo("remote LED ON message sent");
    }
}

void MainWindow::onRemoteLedOff()
{
    if (sendMqttLedControl(false)) {
        setInfo("remote LED OFF message sent");
    }
}

void MainWindow::onRefreshVideoStats()
{
    const QString statsPath = m_videoStatsEdit->text().trimmed();
    QFile f(statsPath);
    QString line;

    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_videoLabel->setText(QString("Video: cannot open %1").arg(statsPath));
        return;
    }
    line = QString::fromUtf8(f.readAll()).trimmed();
    f.close();
    if (line.isEmpty()) {
        m_videoLabel->setText("Video: no stats yet");
    } else {
        m_videoLabel->setText(QString("Video: %1").arg(line));
    }
}

void MainWindow::onPollTick()
{
    onReadSensor();
    onRefreshVideoStats();
}
