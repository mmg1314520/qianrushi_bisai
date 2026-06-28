#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

#include "controlmodel.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentTime READ currentTime NOTIFY currentTimeChanged)
    Q_PROPERTY(QString location READ location NOTIFY telemetryChanged)
    Q_PROPERTY(QString weather READ weather NOTIFY telemetryChanged)
    Q_PROPERTY(QString weatherType READ weatherType NOTIFY telemetryChanged)
    Q_PROPERTY(int temperature READ temperature NOTIFY telemetryChanged)
    Q_PROPERTY(int humidity READ humidity NOTIFY telemetryChanged)
    Q_PROPERTY(int light READ light NOTIFY telemetryChanged)
    Q_PROPERTY(int soil READ soil NOTIFY telemetryChanged)
    Q_PROPERTY(int co2 READ co2 NOTIFY telemetryChanged)
    Q_PROPERTY(bool temperatureValid READ temperatureValid NOTIFY telemetryChanged)
    Q_PROPERTY(bool humidityValid READ humidityValid NOTIFY telemetryChanged)
    Q_PROPERTY(bool lightValid READ lightValid NOTIFY telemetryChanged)
    Q_PROPERTY(bool soilValid READ soilValid NOTIFY telemetryChanged)
    Q_PROPERTY(bool co2Valid READ co2Valid NOTIFY telemetryChanged)
    Q_PROPERTY(QString cropAdvice READ cropAdvice NOTIFY adviceChanged)
    Q_PROPERTY(QString controlAdvice READ controlAdvice NOTIFY adviceChanged)
    Q_PROPERTY(QString trendAdvice READ trendAdvice NOTIFY adviceChanged)
    Q_PROPERTY(QVariantList temperatureHistory READ temperatureHistory NOTIFY historyChanged)
    Q_PROPERTY(QVariantList humidityHistory READ humidityHistory NOTIFY historyChanged)
    Q_PROPERTY(QVariantList lightHistory READ lightHistory NOTIFY historyChanged)
    Q_PROPERTY(QVariantList soilHistory READ soilHistory NOTIFY historyChanged)
    Q_PROPERTY(QVariantList co2History READ co2History NOTIFY historyChanged)
    Q_PROPERTY(int historyCapacity READ historyCapacity CONSTANT)
    Q_PROPERTY(bool autoMode READ autoMode NOTIFY autoModeChanged)
    Q_PROPERTY(QString modeLabel READ modeLabel NOTIFY autoModeChanged)
    Q_PROPERTY(QStringList portNames READ portNames NOTIFY portNamesChanged)
    Q_PROPERTY(QString selectedPortName READ selectedPortName WRITE setSelectedPortName NOTIFY selectedPortNameChanged)
    Q_PROPERTY(int baudRate READ baudRate WRITE setBaudRate NOTIFY baudRateChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectionStateChanged)
    Q_PROPERTY(bool mockEnabled READ mockEnabled WRITE setMockEnabled NOTIFY mockEnabledChanged)
    Q_PROPERTY(QString connectionStatus READ connectionStatus NOTIFY connectionStateChanged)
    Q_PROPERTY(QString serialLog READ serialLog NOTIFY serialLogChanged)
    Q_PROPERTY(QString cameraIp READ cameraIp WRITE setCameraIp NOTIFY videoStateChanged)
    Q_PROPERTY(QString rtspUrl READ rtspUrl NOTIFY videoStateChanged)
    Q_PROPERTY(bool udpOnline READ udpOnline NOTIFY videoStateChanged)
    Q_PROPERTY(QString udpStatus READ udpStatus NOTIFY videoStateChanged)
    Q_PROPERTY(QString k230Status READ k230Status NOTIFY videoStateChanged)
    Q_PROPERTY(QString pestStatus READ pestStatus NOTIFY videoStateChanged)
    Q_PROPERTY(QString witherStatus READ witherStatus NOTIFY videoStateChanged)
    Q_PROPERTY(QString videoAdvice READ videoAdvice NOTIFY videoStateChanged)
    Q_PROPERTY(ControlModel *controlModel READ controlModel CONSTANT)

public:
    explicit AppController(QObject *parent = nullptr);

    QString currentTime() const;
    QString location() const;
    QString weather() const;
    QString weatherType() const;
    int temperature() const;
    int humidity() const;
    int light() const;
    int soil() const;
    int co2() const;
    bool temperatureValid() const;
    bool humidityValid() const;
    bool lightValid() const;
    bool soilValid() const;
    bool co2Valid() const;
    QString cropAdvice() const;
    QString controlAdvice() const;
    QString trendAdvice() const;
    QVariantList temperatureHistory() const;
    QVariantList humidityHistory() const;
    QVariantList lightHistory() const;
    QVariantList soilHistory() const;
    QVariantList co2History() const;
    int historyCapacity() const;
    bool autoMode() const;
    QString modeLabel() const;
    QStringList portNames() const;
    QString selectedPortName() const;
    void setSelectedPortName(const QString &portName);
    int baudRate() const;
    void setBaudRate(int baudRate);
    bool connected() const;
    bool mockEnabled() const;
    void setMockEnabled(bool enabled);
    QString connectionStatus() const;
    QString serialLog() const;
    QString cameraIp() const;
    void setCameraIp(const QString &cameraIp);
    QString rtspUrl() const;
    bool udpOnline() const;
    QString udpStatus() const;
    QString k230Status() const;
    QString pestStatus() const;
    QString witherStatus() const;
    QString videoAdvice() const;
    ControlModel *controlModel();

    Q_INVOKABLE void refreshPorts();
    Q_INVOKABLE void connectSerial();
    Q_INVOKABLE void disconnectSerial();
    Q_INVOKABLE void toggleAutoMode();
    Q_INVOKABLE void toggleControl(int row);
    Q_INVOKABLE void clearSerialLog();
    Q_INVOKABLE void restartUdpListener();

signals:
    void currentTimeChanged();
    void telemetryChanged();
    void adviceChanged();
    void autoModeChanged();
    void portNamesChanged();
    void selectedPortNameChanged();
    void baudRateChanged();
    void connectionStateChanged();
    void mockEnabledChanged();
    void serialLogChanged();
    void historyChanged();
    void videoStateChanged();

private:
    struct TelemetryValue {
        int value = 0;
        bool valid = false;
    };

    void updateCurrentTime();
    void setupMockData();
    void pushMockTelemetry();
    void refreshAdviceTexts();
    void setWeather(const QString &weatherText);
    void sendModeCommand();
    void sendControlCommand(const QString &device, int state);
    void sendJson(const QJsonObject &object);
    void closeSerialPort();
    void pollSerialPort();
    void processIncomingData();
    void processJsonObject(const QJsonObject &object);
    void applyTelemetryObject(const QJsonObject &object);
    void applyControlsObject(const QJsonObject &object);
    int parseControlState(const QString &key, const QJsonValue &value) const;
    void updateControlState(const QString &key, int state, bool fromRemote);
    bool updateTelemetryValue(TelemetryValue &target, const QJsonValue &value);
    void appendSerialLog(const QString &channel, const QString &text);
    void resetHistory();
    void appendHistorySample();
    void appendHistoryValue(QVariantList &history, const TelemetryValue &value);
    double historyDrift(const QVariantList &history) const;
    void setupUdpListener();
    void processPendingDatagrams();
    void processUdpStatusDatagram(const QByteArray &payload, const QString &senderIp);
    QString buildRtspUrl(const QString &cameraIp) const;
    QString prettifyPestStatus(const QString &pest) const;
    QString prettifyWitherStatus(const QString &wither) const;
    void refreshVideoAdvice();
    void markUdpOffline();

    QString m_currentTime;
    QString m_location;
    QString m_weather;
    QString m_weatherType = QStringLiteral("neutral");
    TelemetryValue m_temperature;
    TelemetryValue m_humidity;
    TelemetryValue m_light;
    TelemetryValue m_soil;
    TelemetryValue m_co2;
    QString m_cropAdvice;
    QString m_controlAdvice;
    QString m_trendAdvice;
    QString m_externalCropAdvice;
    QString m_externalControlAdvice;
    QVariantList m_temperatureHistory;
    QVariantList m_humidityHistory;
    QVariantList m_lightHistory;
    QVariantList m_soilHistory;
    QVariantList m_co2History;
    int m_historyCapacity = 30;
    bool m_autoMode = false;
    QStringList m_portNames;
    QString m_selectedPortName;
    int m_baudRate = 9600;
    bool m_mockEnabled = true;
    QString m_lastError;
    QString m_serialLog;
    QStringList m_serialLogLines;
    QString m_cameraIp;
    QString m_rtspUrl;
    bool m_udpOnline = false;
    QString m_udpStatus = QStringLiteral("正在监听 K230 的 UDP 广播端口 9000。");
    QString m_k230Status = QStringLiteral("等待 K230 广播上线。");
    QString m_pestStatus = QStringLiteral("等待虫害识别结果");
    QString m_witherStatus = QStringLiteral("等待植株状态结果");
    QString m_videoAdvice = QStringLiteral("等待 K230 的 RTSP 画面与 UDP 检测状态接入。");
    int m_pestNoneStreak = 0;

    ControlModel m_controlModel;
    QByteArray m_serialBuffer;
    QTimer m_clockTimer;
    QTimer m_mockTimer;
    QTimer m_serialPollTimer;
    QTimer m_udpWatchdogTimer;
    class QUdpSocket *m_udpSocket = nullptr;

#ifdef Q_OS_WIN
    HANDLE m_serialHandle = INVALID_HANDLE_VALUE;
#endif
};
