#include "appcontroller.h"

#include <algorithm>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QHostAddress>
#include <QNetworkDatagram>
#include <QRandomGenerator>
#include <QTime>
#include <QUdpSocket>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

QString weatherTypeFromText(const QString &weather)
{
    const QString text = weather.trimmed().toLower();
    if (text.contains(QStringLiteral("rain"))) {
        return QStringLiteral("rainy");
    }
    if (text.contains(QStringLiteral("cloud")) || text.contains(QStringLiteral("overcast"))) {
        return QStringLiteral("cloudy");
    }
    if (text.contains(QStringLiteral("sun"))) {
        return QStringLiteral("sunny");
    }
    return QStringLiteral("neutral");
}

QString weatherLabelForMock(int light)
{
    if (light < 25) {
        return QStringLiteral("Rain");
    }
    if (light < 55) {
        return QStringLiteral("Cloudy");
    }
    return QStringLiteral("Sunny");
}

#ifdef Q_OS_WIN
QString windowsErrorMessage(DWORD errorCode)
{
    wchar_t *buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr);

    if (length == 0 || buffer == nullptr) {
        return QStringLiteral("Windows error %1").arg(errorCode);
    }

    QString message = QString::fromWCharArray(buffer, static_cast<int>(length)).trimmed();
    LocalFree(buffer);
    return message;
}
#endif

} // namespace

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    m_location = QStringLiteral("Smart Greenhouse A");
    setWeather(QStringLiteral("Cloudy"));

    connect(&m_clockTimer, &QTimer::timeout, this, &AppController::updateCurrentTime);
    m_clockTimer.start(1000);
    updateCurrentTime();

    connect(&m_mockTimer, &QTimer::timeout, this, &AppController::pushMockTelemetry);
    m_mockTimer.setInterval(1600);

    connect(&m_serialPollTimer, &QTimer::timeout, this, &AppController::pollSerialPort);
    m_serialPollTimer.setInterval(80);

    m_udpWatchdogTimer.setInterval(8000);
    m_udpWatchdogTimer.setSingleShot(true);
    connect(&m_udpWatchdogTimer, &QTimer::timeout, this, &AppController::markUdpOffline);

    refreshPorts();
    m_controlModel.setAutoMode(m_autoMode);
    setupUdpListener();
    setupMockData();
    pushMockTelemetry();
    m_mockTimer.start();
}

QString AppController::currentTime() const
{
    return m_currentTime;
}

QString AppController::location() const
{
    return m_location;
}

QString AppController::weather() const
{
    return m_weather;
}

QString AppController::weatherType() const
{
    return m_weatherType;
}

int AppController::temperature() const
{
    return m_temperature.value;
}

int AppController::humidity() const
{
    return m_humidity.value;
}

int AppController::light() const
{
    return m_light.value;
}

int AppController::soil() const
{
    return m_soil.value;
}

int AppController::co2() const
{
    return m_co2.value;
}

bool AppController::temperatureValid() const
{
    return m_temperature.valid;
}

bool AppController::humidityValid() const
{
    return m_humidity.valid;
}

bool AppController::lightValid() const
{
    return m_light.valid;
}

bool AppController::soilValid() const
{
    return m_soil.valid;
}

bool AppController::co2Valid() const
{
    return m_co2.valid;
}

QString AppController::cropAdvice() const
{
    return m_cropAdvice;
}

QString AppController::controlAdvice() const
{
    return m_controlAdvice;
}

QString AppController::trendAdvice() const
{
    return m_trendAdvice;
}

QVariantList AppController::temperatureHistory() const
{
    return m_temperatureHistory;
}

QVariantList AppController::humidityHistory() const
{
    return m_humidityHistory;
}

QVariantList AppController::lightHistory() const
{
    return m_lightHistory;
}

QVariantList AppController::soilHistory() const
{
    return m_soilHistory;
}

QVariantList AppController::co2History() const
{
    return m_co2History;
}

int AppController::historyCapacity() const
{
    return m_historyCapacity;
}

bool AppController::autoMode() const
{
    return m_autoMode;
}

QString AppController::modeLabel() const
{
    return m_autoMode ? QStringLiteral("AUTO") : QStringLiteral("MANUAL");
}

QStringList AppController::portNames() const
{
    return m_portNames;
}

QString AppController::selectedPortName() const
{
    return m_selectedPortName;
}

void AppController::setSelectedPortName(const QString &portName)
{
    if (m_selectedPortName == portName) {
        return;
    }

    m_selectedPortName = portName;
    emit selectedPortNameChanged();
}

int AppController::baudRate() const
{
    return m_baudRate;
}

void AppController::setBaudRate(int baudRate)
{
    if (m_baudRate == baudRate || baudRate <= 0) {
        return;
    }

    m_baudRate = baudRate;
    emit baudRateChanged();
}

bool AppController::connected() const
{
#ifdef Q_OS_WIN
    return m_serialHandle != INVALID_HANDLE_VALUE;
#else
    return false;
#endif
}

bool AppController::mockEnabled() const
{
    return m_mockEnabled;
}

void AppController::setMockEnabled(bool enabled)
{
    if (m_mockEnabled == enabled) {
        return;
    }

    m_mockEnabled = enabled;
    if (m_mockEnabled) {
        setupMockData();
        pushMockTelemetry();
        m_mockTimer.start();
    } else {
        m_mockTimer.stop();
    }

    refreshAdviceTexts();
    emit mockEnabledChanged();
    emit connectionStateChanged();
}

QString AppController::connectionStatus() const
{
    if (connected()) {
        return QStringLiteral("Connected: %1 @ %2").arg(m_selectedPortName).arg(m_baudRate);
    }
    if (!m_lastError.isEmpty()) {
        return QStringLiteral("Disconnected: %1").arg(m_lastError);
    }
    if (m_mockEnabled) {
        return QStringLiteral("Mock data active for preview.");
    }
    return QStringLiteral("Waiting for serial connection.");
}

QString AppController::serialLog() const
{
    return m_serialLog;
}

QString AppController::cameraIp() const
{
    return m_cameraIp;
}

void AppController::setCameraIp(const QString &cameraIp)
{
    const QString trimmed = cameraIp.trimmed();
    const QString nextRtspUrl = buildRtspUrl(trimmed);
    if (m_cameraIp == trimmed && m_rtspUrl == nextRtspUrl) {
        return;
    }

    m_cameraIp = trimmed;
    m_rtspUrl = nextRtspUrl;
    if (m_cameraIp.isEmpty()) {
        m_k230Status = QStringLiteral("等待 K230 广播上线。");
    }
    refreshVideoAdvice();
    emit videoStateChanged();
}

QString AppController::rtspUrl() const
{
    return m_rtspUrl;
}

bool AppController::udpOnline() const
{
    return m_udpOnline;
}

QString AppController::udpStatus() const
{
    return m_udpStatus;
}

QString AppController::k230Status() const
{
    return m_k230Status;
}

QString AppController::pestStatus() const
{
    return m_pestStatus;
}

QString AppController::witherStatus() const
{
    return m_witherStatus;
}

QString AppController::videoAdvice() const
{
    return m_videoAdvice;
}

ControlModel *AppController::controlModel()
{
    return &m_controlModel;
}

void AppController::refreshPorts()
{
    QStringList names;
#ifdef Q_OS_WIN
    for (int index = 1; index <= 32; ++index) {
        const QString name = QStringLiteral("COM%1").arg(index);
        wchar_t target[512] = {0};
        if (QueryDosDeviceW(reinterpret_cast<LPCWSTR>(name.utf16()), target, 512) != 0) {
            names.push_back(name);
        }
    }
#endif

    if (names == m_portNames) {
        return;
    }

    m_portNames = names;
    if (!m_portNames.contains(m_selectedPortName)) {
        m_selectedPortName = m_portNames.isEmpty() ? QString() : m_portNames.first();
        emit selectedPortNameChanged();
    }
    emit portNamesChanged();
}

void AppController::connectSerial()
{
    refreshPorts();
    m_lastError.clear();

    if (m_selectedPortName.isEmpty()) {
        m_lastError = QStringLiteral("No serial port available.");
        appendSerialLog(QStringLiteral("SYS"), QStringLiteral("No serial port available."));
        emit connectionStateChanged();
        return;
    }

#ifdef Q_OS_WIN
    closeSerialPort();

    const QString nativeName = QStringLiteral("\\\\.\\%1").arg(m_selectedPortName);
    m_serialHandle = CreateFileW(reinterpret_cast<LPCWSTR>(nativeName.utf16()),
                                 GENERIC_READ | GENERIC_WRITE,
                                 0,
                                 nullptr,
                                 OPEN_EXISTING,
                                 0,
                                 nullptr);

    if (m_serialHandle == INVALID_HANDLE_VALUE) {
        m_lastError = windowsErrorMessage(GetLastError());
        appendSerialLog(QStringLiteral("SYS"), QStringLiteral("Open failed: %1").arg(m_lastError));
        emit connectionStateChanged();
        return;
    }

    DCB dcb = {};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(m_serialHandle, &dcb)) {
        m_lastError = windowsErrorMessage(GetLastError());
        appendSerialLog(QStringLiteral("SYS"), QStringLiteral("Read serial config failed: %1").arg(m_lastError));
        closeSerialPort();
        emit connectionStateChanged();
        return;
    }

    dcb.BaudRate = static_cast<DWORD>(m_baudRate);
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fAbortOnError = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    if (!SetCommState(m_serialHandle, &dcb)) {
        m_lastError = windowsErrorMessage(GetLastError());
        appendSerialLog(QStringLiteral("SYS"), QStringLiteral("Apply serial config failed: %1").arg(m_lastError));
        closeSerialPort();
        emit connectionStateChanged();
        return;
    }

    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(m_serialHandle, &timeouts);
    SetupComm(m_serialHandle, 4096, 4096);
    PurgeComm(m_serialHandle, PURGE_RXCLEAR | PURGE_TXCLEAR);
#else
    m_lastError = QStringLiteral("Windows serial support is not available in this build.");
    appendSerialLog(QStringLiteral("SYS"), QStringLiteral("This build does not include Windows serial support."));
    emit connectionStateChanged();
    return;
#endif

    if (m_mockEnabled) {
        setMockEnabled(false);
    }

    m_serialBuffer.clear();
    m_serialPollTimer.start();
    appendSerialLog(QStringLiteral("SYS"), QStringLiteral("Connected to %1 @ %2").arg(m_selectedPortName).arg(m_baudRate));
    emit connectionStateChanged();
    sendModeCommand();
}

void AppController::disconnectSerial()
{
    if (connected()) {
        appendSerialLog(QStringLiteral("SYS"), QStringLiteral("Disconnected from %1").arg(m_selectedPortName));
    }

    closeSerialPort();
    m_lastError.clear();
    emit connectionStateChanged();
}

void AppController::toggleAutoMode()
{
    m_autoMode = !m_autoMode;
    m_controlModel.setAutoMode(m_autoMode);
    refreshAdviceTexts();
    sendModeCommand();
    emit autoModeChanged();
}

void AppController::toggleControl(int row)
{
    if (m_autoMode) {
        return;
    }

    if (!m_controlModel.cycleState(row)) {
        return;
    }

    refreshAdviceTexts();
    sendControlCommand(m_controlModel.keyAt(row), m_controlModel.stateAt(row));
}

void AppController::clearSerialLog()
{
    if (m_serialLog.isEmpty()) {
        return;
    }

    m_serialLog.clear();
    m_serialLogLines.clear();
    emit serialLogChanged();
}

void AppController::restartUdpListener()
{
    setupUdpListener();
}

void AppController::updateCurrentTime()
{
    const QString newTime = QDateTime::currentDateTime().toString(QStringLiteral("M/d hh:mm"));
    if (newTime == m_currentTime) {
        return;
    }

    m_currentTime = newTime;
    emit currentTimeChanged();
}

void AppController::setupMockData()
{
    m_temperature = {27, true};
    m_humidity = {63, true};
    m_light = {58, true};
    m_soil = {41, true};
    m_co2 = {780, true};
    m_location = QStringLiteral("Smart Greenhouse A");
    setWeather(QStringLiteral("Cloudy"));
    resetHistory();
    appendHistorySample();
    refreshAdviceTexts();
    emit telemetryChanged();
}

void AppController::pushMockTelemetry()
{
    if (!m_mockEnabled) {
        return;
    }

    auto nextValue = [](int current, int minimum, int maximum, int step) {
        const int delta = QRandomGenerator::global()->bounded(step * 2 + 1) - step;
        return std::clamp(current + delta, minimum, maximum);
    };

    m_temperature.value = nextValue(m_temperature.value, 18, 37, 2);
    m_humidity.value = nextValue(m_humidity.value, 35, 88, 3);
    m_light.value = nextValue(m_light.value, 8, 96, 5);
    m_soil.value = nextValue(m_soil.value, 18, 82, 4);
    m_co2.value = nextValue(m_co2.value, 420, 1800, 90);

    const QString nextWeather = weatherLabelForMock(m_light.value);
    if (nextWeather != m_weather) {
        setWeather(nextWeather);
    }

    if (m_autoMode) {
        updateControlState(QStringLiteral("pump"), m_soil.value < 35 ? 1 : 0, false);
        updateControlState(QStringLiteral("fan"), (m_temperature.value > 30 || m_co2.value > 1300) ? 1 : 0, false);
        updateControlState(QStringLiteral("window"), m_temperature.value > 32 ? 2 : (m_temperature.value > 28 ? 1 : 0), false);
        updateControlState(QStringLiteral("pestLamp"), m_light.value < 18 ? 1 : 0, false);
        updateControlState(QStringLiteral("buzzer"), m_co2.value > 1550 ? 1 : 0, false);
    }

    appendHistorySample();
    refreshAdviceTexts();
    emit telemetryChanged();
}

void AppController::refreshAdviceTexts()
{
    QString nextCropAdvice = m_externalCropAdvice;
    if (nextCropAdvice.isEmpty()) {
        if (m_soil.valid && m_soil.value < 35) {
            nextCropAdvice = QStringLiteral("Soil moisture is low. Consider irrigation soon.");
        } else if (m_soil.valid && m_soil.value > 78) {
            nextCropAdvice = QStringLiteral("Soil moisture is high. Reduce irrigation duration.");
        } else if (m_light.valid && m_light.value < 30) {
            nextCropAdvice = QStringLiteral("Light is weak. Consider supplemental lighting.");
        } else if (m_co2.valid && m_co2.value > 1400) {
            nextCropAdvice = QStringLiteral("CO2 is high. Increase ventilation and observe changes.");
        } else {
            nextCropAdvice = QStringLiteral("Environment is stable for continued crop growth.");
        }
    }

    QString nextControlAdvice = m_externalControlAdvice;
    if (nextControlAdvice.isEmpty()) {
        const QString activeAdvice = m_controlModel.activeAdvice();
        if (m_autoMode) {
            nextControlAdvice = activeAdvice.isEmpty()
                ? QStringLiteral("Auto mode is active. Waiting for the next control decision.")
                : QStringLiteral("Auto mode active: %1").arg(activeAdvice);
        } else {
            nextControlAdvice = activeAdvice.isEmpty()
                ? QStringLiteral("Manual mode active. Devices can be switched from the Qt panel.")
                : activeAdvice;
        }
    }

    const double temperatureTrend = historyDrift(m_temperatureHistory);
    const double humidityTrend = historyDrift(m_humidityHistory);
    const double lightTrend = historyDrift(m_lightHistory);
    const double soilTrend = historyDrift(m_soilHistory);
    const double co2Trend = historyDrift(m_co2History);

    QString nextTrendAdvice;
    if (m_temperatureHistory.size() < 4 || m_co2History.size() < 4) {
        nextTrendAdvice = QStringLiteral("Trend window is still warming up. Keep receiving serial data.");
    } else if (soilTrend <= -6.0 && humidityTrend <= -4.0) {
        nextTrendAdvice = QStringLiteral("Soil and air moisture are both dropping. Irrigation may be needed soon.");
    } else if (temperatureTrend >= 3.0 && co2Trend >= 120.0) {
        nextTrendAdvice = QStringLiteral("Temperature and CO2 are rising together. Ventilation may be falling behind.");
    } else if (lightTrend <= -10.0 && temperatureTrend <= -2.0) {
        nextTrendAdvice = QStringLiteral("Light and temperature are both weakening. Photosynthesis may slow down.");
    } else if (co2Trend >= 180.0) {
        nextTrendAdvice = QStringLiteral("CO2 is climbing quickly. Check fan and window linkage first.");
    } else if (soilTrend >= 4.0 && temperatureTrend >= -1.0 && temperatureTrend <= 2.0 && lightTrend >= 6.0) {
        nextTrendAdvice = QStringLiteral("Light and soil moisture are improving. Growth conditions look positive.");
    } else {
        nextTrendAdvice = QStringLiteral("The five curves are stable overall. Continue observing the next cycle.");
    }

    bool adviceTextChanged = false;
    if (nextCropAdvice != m_cropAdvice) {
        m_cropAdvice = nextCropAdvice;
        adviceTextChanged = true;
    }
    if (nextControlAdvice != m_controlAdvice) {
        m_controlAdvice = nextControlAdvice;
        adviceTextChanged = true;
    }
    if (nextTrendAdvice != m_trendAdvice) {
        m_trendAdvice = nextTrendAdvice;
        adviceTextChanged = true;
    }
    if (adviceTextChanged) {
        emit adviceChanged();
    }

    refreshVideoAdvice();
}

void AppController::setupUdpListener()
{
    m_udpWatchdogTimer.stop();
    m_udpOnline = false;

    if (m_udpSocket == nullptr) {
        m_udpSocket = new QUdpSocket(this);
        connect(m_udpSocket, &QUdpSocket::readyRead, this, &AppController::processPendingDatagrams);
    } else {
        m_udpSocket->close();
    }

    const bool bound = m_udpSocket->bind(QHostAddress::AnyIPv4,
                                         9000,
                                         QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);

    if (!bound) {
        m_udpOnline = false;
        m_udpStatus = QStringLiteral("UDP 监听启动失败：%1").arg(m_udpSocket->errorString());
        m_k230Status = QStringLiteral("K230 广播监听不可用。");
        refreshVideoAdvice();
        appendSerialLog(QStringLiteral("SYS"), m_udpStatus);
        emit videoStateChanged();
        return;
    }

    m_udpStatus = QStringLiteral("正在监听 K230 的 UDP 广播端口 9000。");
    m_k230Status = m_cameraIp.isEmpty()
        ? QStringLiteral("等待 K230 广播上线。")
        : QStringLiteral("等待来自 %1 的下一条 K230 广播。").arg(m_cameraIp);
    refreshVideoAdvice();
    appendSerialLog(QStringLiteral("SYS"), m_udpStatus);
    emit videoStateChanged();
}

void AppController::processPendingDatagrams()
{
    if (m_udpSocket == nullptr) {
        return;
    }

    while (m_udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        processUdpStatusDatagram(datagram.data(), datagram.senderAddress().toString());
    }
}

void AppController::processUdpStatusDatagram(const QByteArray &payload, const QString &senderIp)
{
    constexpr int pestNoneConfirmFrames = 3;

    const QString cleanIp = senderIp.startsWith(QStringLiteral("::ffff:"))
        ? senderIp.mid(7)
        : senderIp;

    appendSerialLog(QStringLiteral("UDP"), QStringLiteral("%1  %2").arg(cleanIp, QString::fromUtf8(payload).trimmed()));

    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject()) {
        return;
    }

    const QJsonObject object = document.object();
    const QString rawPest = object.value(QStringLiteral("pest")).toString().trimmed();
    const QString nextPest = prettifyPestStatus(rawPest);
    const QString nextWither = prettifyWitherStatus(object.value(QStringLiteral("wither")).toString());
    const QString announcedRtspUrl = object.value(QStringLiteral("rtsp_url")).toString().trimmed();
    const QString nextRtspUrl = !announcedRtspUrl.isEmpty()
        ? announcedRtspUrl
        : buildRtspUrl(cleanIp);

    bool changed = false;

    if (!cleanIp.isEmpty() && cleanIp != m_cameraIp) {
        m_cameraIp = cleanIp;
        changed = true;
    }

    if (m_rtspUrl != nextRtspUrl) {
        m_rtspUrl = nextRtspUrl;
        changed = true;
    }

    const QString pestUpper = rawPest.toUpper();
    const bool pestMissing = pestUpper == QStringLiteral("NONE_DETECTED");
    const bool pestPayloadPresent = !rawPest.isEmpty();
    if (pestPayloadPresent) {
        if (pestMissing) {
            ++m_pestNoneStreak;
            if (m_pestNoneStreak >= pestNoneConfirmFrames && nextPest != m_pestStatus) {
                m_pestStatus = nextPest;
                changed = true;
            }
        } else {
            m_pestNoneStreak = 0;
            if (nextPest != m_pestStatus) {
                m_pestStatus = nextPest;
                changed = true;
            }
        }
    }

    if (!nextWither.isEmpty() && nextWither != m_witherStatus) {
        m_witherStatus = nextWither;
        changed = true;
    }

    const QString nextUdpStatus = QStringLiteral("正在接收来自 %1:9000 的 K230 广播。").arg(cleanIp);
    const QString nextK230Status = QStringLiteral("K230 已在线，RTSP 地址已定位到 %1。").arg(cleanIp);
    if (!m_udpOnline || m_udpStatus != nextUdpStatus || m_k230Status != nextK230Status) {
        m_udpOnline = true;
        m_udpStatus = nextUdpStatus;
        m_k230Status = nextK230Status;
        changed = true;
    }

    m_udpWatchdogTimer.start();
    refreshVideoAdvice();
    if (changed) {
        emit videoStateChanged();
    }
}

QString AppController::buildRtspUrl(const QString &cameraIp) const
{
    if (cameraIp.isEmpty()) {
        return QString();
    }
    return QStringLiteral("rtsp://%1:8554/face").arg(cameraIp);
}

QString AppController::prettifyPestStatus(const QString &pest) const
{
    const QString text = pest.trimmed();
    if (text.isEmpty()) {
        return QStringLiteral("等待虫害识别结果");
    }

    const QString upper = text.toUpper();
    if (upper == QStringLiteral("NONE_DETECTED")) {
        return QStringLiteral("未识别到虫害");
    }
    if (upper == QStringLiteral("UNKNOWN")) {
        return QStringLiteral("虫害类别待确认");
    }
    return text;
}

QString AppController::prettifyWitherStatus(const QString &wither) const
{
    const QString text = wither.trimmed();
    if (text.isEmpty()) {
        return QStringLiteral("等待植株状态结果");
    }

    const QString upper = text.toUpper();
    if (upper == QStringLiteral("HEALTHY")) {
        return QStringLiteral("植株状态健康");
    }
    if (upper == QStringLiteral("WITHERED")) {
        return QStringLiteral("检测到枯萎风险");
    }
    if (upper == QStringLiteral("NONE")) {
        return QStringLiteral("植株状态待确认");
    }
    return text;
}

void AppController::refreshVideoAdvice()
{
    QString nextAdvice;
    if (!m_udpOnline) {
        nextAdvice = QStringLiteral("当前风险：K230 广播尚未到达，视频页暂时无法判断虫害与植株状态。\n\n未来预判：在 UDP 广播上线之前，RTSP 页面只能等待摄像头入口自动发现，监测结论不会稳定输出。\n\n建议动作：确认 K230 与 Qt 主机连接到同一 Wi-Fi，并检查 UDP 9000 与 RTSP 8554 端口是否可达。");
    } else if (m_witherStatus.contains(QStringLiteral("枯萎"), Qt::CaseInsensitive)) {
        nextAdvice = QStringLiteral("当前风险：视频检测已经出现植株枯萎信号。\n\n未来预判：如果土壤水分和通风条件没有及时改善，后续画面中的黄化区域可能继续扩大。\n\n建议动作：优先检查补水情况，再结合湿度和温度曲线确认植株是否正在恢复。");
    } else if (!m_pestStatus.contains(QStringLiteral("未识别到"), Qt::CaseInsensitive)
               && !m_pestStatus.contains(QStringLiteral("等待"), Qt::CaseInsensitive)) {
        nextAdvice = QStringLiteral("当前风险：K230 摄像头已经识别到疑似虫害目标。\n\n未来预判：如果同类目标持续在后续帧中出现，作物受损风险可能在下一轮监测周期上升。\n\n建议动作：尽快到现场核查对应区域，并按需要联动蜂鸣器或诱虫灯策略。");
    } else if (m_temperature.valid && m_temperature.value > 32) {
        nextAdvice = QStringLiteral("当前风险：视频画面暂时稳定，但温室温度已经偏高。\n\n未来预判：如果高温持续，后续画面中可能出现叶片卷曲或热胁迫迹象。\n\n建议动作：先加强通风，再结合视频和 CO2 曲线继续观察环境变化。");
    } else {
        nextAdvice = QStringLiteral("当前风险：视频链路稳定，暂未看到明显虫害或枯萎信号。\n\n未来预判：如果当前环境继续保持，作物大概率仍会维持在可控生长状态。\n\n建议动作：保持 RTSP 在线监控，并持续将视频结果与传感器曲线交叉验证。");
    }

    if (nextAdvice != m_videoAdvice) {
        m_videoAdvice = nextAdvice;
        emit videoStateChanged();
    }
}

void AppController::markUdpOffline()
{
    if (!m_udpOnline) {
        return;
    }

    m_udpOnline = false;
    m_udpStatus = QStringLiteral("UDP 心跳超时，正在等待下一条 K230 广播。");
    m_k230Status = m_cameraIp.isEmpty()
        ? QStringLiteral("等待 K230 广播上线。")
        : QStringLiteral("K230 心跳丢失，最近一次摄像头 IP 为 %1。").arg(m_cameraIp);
    refreshVideoAdvice();
    emit videoStateChanged();
}

void AppController::setWeather(const QString &weatherText)
{
    m_weather = weatherText.isEmpty() ? QStringLiteral("Cloudy") : weatherText;
    m_weatherType = weatherTypeFromText(m_weather);
}

void AppController::sendModeCommand()
{
    if (!connected()) {
        return;
    }

    QJsonObject object;
    object.insert(QStringLiteral("type"), QStringLiteral("mode"));
    object.insert(QStringLiteral("auto"), m_autoMode);
    sendJson(object);
}

void AppController::sendControlCommand(const QString &device, int state)
{
    if (!connected() || device.isEmpty()) {
        return;
    }

    QJsonObject object;
    object.insert(QStringLiteral("type"), QStringLiteral("control"));
    object.insert(QStringLiteral("device"), device);
    object.insert(QStringLiteral("state"), state);
    sendJson(object);
}

void AppController::sendJson(const QJsonObject &object)
{
    if (!connected()) {
        return;
    }

    const QByteArray payload = QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
#ifdef Q_OS_WIN
    DWORD bytesWritten = 0;
    if (!WriteFile(m_serialHandle, payload.constData(), static_cast<DWORD>(payload.size()), &bytesWritten, nullptr)) {
        m_lastError = windowsErrorMessage(GetLastError());
        appendSerialLog(QStringLiteral("SYS"), QStringLiteral("Write failed: %1").arg(m_lastError));
        closeSerialPort();
        emit connectionStateChanged();
        return;
    }
    if (bytesWritten != static_cast<DWORD>(payload.size())) {
        appendSerialLog(QStringLiteral("SYS"),
                        QStringLiteral("Partial write: %1/%2 bytes").arg(bytesWritten).arg(payload.size()));
    }
    FlushFileBuffers(m_serialHandle);
#endif

    appendSerialLog(QStringLiteral("TX"), QString::fromUtf8(payload.trimmed()));
}

void AppController::closeSerialPort()
{
    m_serialPollTimer.stop();
#ifdef Q_OS_WIN
    if (m_serialHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_serialHandle);
        m_serialHandle = INVALID_HANDLE_VALUE;
    }
#endif
}

void AppController::pollSerialPort()
{
#ifdef Q_OS_WIN
    if (m_serialHandle == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD errors = 0;
    COMSTAT status = {};
    if (!ClearCommError(m_serialHandle, &errors, &status)) {
        m_lastError = windowsErrorMessage(GetLastError());
        appendSerialLog(QStringLiteral("SYS"), QStringLiteral("Serial poll failed: %1").arg(m_lastError));
        closeSerialPort();
        emit connectionStateChanged();
        return;
    }

    if (status.cbInQue == 0) {
        return;
    }

    QByteArray chunk(static_cast<int>(status.cbInQue), Qt::Uninitialized);
    DWORD bytesRead = 0;
    if (!ReadFile(m_serialHandle, chunk.data(), status.cbInQue, &bytesRead, nullptr)) {
        m_lastError = windowsErrorMessage(GetLastError());
        appendSerialLog(QStringLiteral("SYS"), QStringLiteral("Read failed: %1").arg(m_lastError));
        closeSerialPort();
        emit connectionStateChanged();
        return;
    }

    chunk.truncate(static_cast<int>(bytesRead));
    m_serialBuffer += chunk;
    processIncomingData();
#endif
}

void AppController::processIncomingData()
{
    while (true) {
        const int newlineIndex = m_serialBuffer.indexOf('\n');
        if (newlineIndex < 0) {
            break;
        }

        QByteArray line = m_serialBuffer.left(newlineIndex).trimmed();
        m_serialBuffer.remove(0, newlineIndex + 1);
        if (line.isEmpty()) {
            continue;
        }

        appendSerialLog(QStringLiteral("RX"), QString::fromUtf8(line));
        const QJsonDocument document = QJsonDocument::fromJson(line);
        if (!document.isObject()) {
            continue;
        }
        processJsonObject(document.object());
    }
}

void AppController::processJsonObject(const QJsonObject &object)
{
    const QString type = object.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("telemetry")) {
        applyTelemetryObject(object.value(QStringLiteral("data")).isObject() ? object.value(QStringLiteral("data")).toObject() : object);
    } else if (type == QStringLiteral("control_state")) {
        if (object.contains(QStringLiteral("controls")) && object.value(QStringLiteral("controls")).isObject()) {
            applyControlsObject(object.value(QStringLiteral("controls")).toObject());
        } else {
            updateControlState(object.value(QStringLiteral("device")).toString(),
                               parseControlState(object.value(QStringLiteral("device")).toString(), object.value(QStringLiteral("state"))),
                               true);
        }
    } else if (type == QStringLiteral("advice")) {
        m_externalCropAdvice = object.value(QStringLiteral("crop")).toString();
        m_externalControlAdvice = object.value(QStringLiteral("control")).toString();
        refreshAdviceTexts();
    } else {
        applyTelemetryObject(object);
    }
}

void AppController::applyTelemetryObject(const QJsonObject &object)
{
    bool telemetryUpdated = false;

    telemetryUpdated |= updateTelemetryValue(m_temperature, object.value(QStringLiteral("temperature")));
    telemetryUpdated |= updateTelemetryValue(m_temperature, object.value(QStringLiteral("temp")));
    telemetryUpdated |= updateTelemetryValue(m_humidity, object.value(QStringLiteral("humidity")));
    telemetryUpdated |= updateTelemetryValue(m_light, object.value(QStringLiteral("light")));
    telemetryUpdated |= updateTelemetryValue(m_light, object.value(QStringLiteral("lightPercent")));
    telemetryUpdated |= updateTelemetryValue(m_soil, object.value(QStringLiteral("soil")));
    telemetryUpdated |= updateTelemetryValue(m_soil, object.value(QStringLiteral("soilMoisture")));
    telemetryUpdated |= updateTelemetryValue(m_co2, object.value(QStringLiteral("co2")));

    if (object.contains(QStringLiteral("location"))) {
        if (object.value(QStringLiteral("location")).isString()) {
            const QString text = object.value(QStringLiteral("location")).toString();
            if (!text.isEmpty() && text != m_location) {
                m_location = text;
                telemetryUpdated = true;
            }
        } else if (object.value(QStringLiteral("location")).isObject()) {
            const QString text = object.value(QStringLiteral("location")).toObject().value(QStringLiteral("name")).toString();
            if (!text.isEmpty() && text != m_location) {
                m_location = text;
                telemetryUpdated = true;
            }
        }
    }

    if (object.contains(QStringLiteral("weather"))) {
        const QString nextWeather = object.value(QStringLiteral("weather")).toString();
        if (!nextWeather.isEmpty() && nextWeather != m_weather) {
            setWeather(nextWeather);
            telemetryUpdated = true;
        }
    }

    if (object.contains(QStringLiteral("auto")) && object.value(QStringLiteral("auto")).isBool()) {
        const bool nextAuto = object.value(QStringLiteral("auto")).toBool();
        if (nextAuto != m_autoMode) {
            m_autoMode = nextAuto;
            m_controlModel.setAutoMode(m_autoMode);
            emit autoModeChanged();
        }
    }

    if (object.contains(QStringLiteral("cropAdvice"))) {
        m_externalCropAdvice = object.value(QStringLiteral("cropAdvice")).toString();
    }
    if (object.contains(QStringLiteral("controlAdvice"))) {
        m_externalControlAdvice = object.value(QStringLiteral("controlAdvice")).toString();
    }
    if (object.contains(QStringLiteral("controls")) && object.value(QStringLiteral("controls")).isObject()) {
        applyControlsObject(object.value(QStringLiteral("controls")).toObject());
    }

    appendHistorySample();
    refreshAdviceTexts();
    if (telemetryUpdated) {
        emit telemetryChanged();
    }
}

void AppController::applyControlsObject(const QJsonObject &object)
{
    for (auto it = object.begin(); it != object.end(); ++it) {
        updateControlState(it.key(), parseControlState(it.key(), it.value()), true);
    }
    refreshAdviceTexts();
}

int AppController::parseControlState(const QString &key, const QJsonValue &value) const
{
    if (value.isDouble()) {
        return value.toInt();
    }

    const QString text = value.toString().trimmed().toLower();
    if (text.isEmpty()) {
        return 0;
    }

    if (key == QStringLiteral("window")) {
        if (text == QStringLiteral("full") || text == QStringLiteral("open")) {
            return 2;
        }
        if (text == QStringLiteral("half") || text == QStringLiteral("half_open")) {
            return 1;
        }
        return 0;
    }

    if (key == QStringLiteral("buzzer")) {
        if (text == QStringLiteral("locust")) {
            return 1;
        }
        if (text == QStringLiteral("cabbage_worm")) {
            return 2;
        }
        return 0;
    }

    if (text == QStringLiteral("on") || text == QStringLiteral("1") || text == QStringLiteral("true")) {
        return 1;
    }
    return 0;
}

void AppController::updateControlState(const QString &key, int state, bool fromRemote)
{
    Q_UNUSED(fromRemote);
    if (!m_controlModel.setStateByKey(key, state)) {
        return;
    }
}

bool AppController::updateTelemetryValue(TelemetryValue &target, const QJsonValue &value)
{
    if (!value.isDouble()) {
        return false;
    }

    const int nextValue = value.toInt();
    const bool changed = (target.value != nextValue) || !target.valid;
    target.value = nextValue;
    target.valid = true;
    return changed;
}

void AppController::appendSerialLog(const QString &channel, const QString &text)
{
    const QString cleanText = text.trimmed();
    if (cleanText.isEmpty()) {
        return;
    }

    const QString stamp = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));
    m_serialLogLines.append(QStringLiteral("[%1] %2  %3").arg(stamp, channel, cleanText));

    constexpr int maxLogLines = 240;
    while (m_serialLogLines.size() > maxLogLines) {
        m_serialLogLines.removeFirst();
    }

    m_serialLog = m_serialLogLines.join(QLatin1Char('\n'));
    emit serialLogChanged();
}

void AppController::resetHistory()
{
    m_temperatureHistory.clear();
    m_humidityHistory.clear();
    m_lightHistory.clear();
    m_soilHistory.clear();
    m_co2History.clear();
    emit historyChanged();
}

void AppController::appendHistorySample()
{
    appendHistoryValue(m_temperatureHistory, m_temperature);
    appendHistoryValue(m_humidityHistory, m_humidity);
    appendHistoryValue(m_lightHistory, m_light);
    appendHistoryValue(m_soilHistory, m_soil);
    appendHistoryValue(m_co2History, m_co2);
    emit historyChanged();
}

void AppController::appendHistoryValue(QVariantList &history, const TelemetryValue &value)
{
    if (value.valid) {
        history.append(value.value);
    } else if (!history.isEmpty()) {
        history.append(history.constLast());
    } else {
        history.append(0);
    }

    while (history.size() > m_historyCapacity) {
        history.removeFirst();
    }
}

double AppController::historyDrift(const QVariantList &history) const
{
    if (history.size() < 2) {
        return 0.0;
    }

    const int halfWindow = static_cast<int>(history.size() / 2);
    const int window = std::max(1, std::min(4, halfWindow));
    double headTotal = 0.0;
    double tailTotal = 0.0;
    for (int index = 0; index < window; ++index) {
        headTotal += history.at(index).toDouble();
        tailTotal += history.at(history.size() - window + index).toDouble();
    }

    return (tailTotal / window) - (headTotal / window);
}
