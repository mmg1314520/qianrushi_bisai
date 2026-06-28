pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

ApplicationWindow {
    id: window
    width: 1500
    height: 960
    visible: true
    title: "AI番茄生长监测与联动控制系统"
    color: "#07141a"

    property int currentPage: 0
    property int trendHistoryCapacity: 30
    property var temperatureHistoryData: []
    property var humidityHistoryData: []
    property var lightHistoryData: []
    property var soilHistoryData: []
    property var co2HistoryData: []
    property string trendAdviceText: "趋势窗口建立中，等待更多实时数据。"

    function readingText(value, suffix, valid) {
        return valid ? value + " " + suffix : "-- " + suffix
    }

    function weatherGlyph(kind) {
        if (kind === "sunny")
            return "SUN"
        if (kind === "cloudy")
            return "CLD"
        if (kind === "rainy")
            return "RAN"
        return "AIR"
    }

    function environmentScore() {
        let score = 92

        if (backend.temperatureValid) {
            if (backend.temperature < 18 || backend.temperature > 34)
                score -= 12
            else if (backend.temperature < 22 || backend.temperature > 30)
                score -= 6
        }

        if (backend.humidityValid) {
            if (backend.humidity < 35 || backend.humidity > 88)
                score -= 10
            else if (backend.humidity < 45 || backend.humidity > 78)
                score -= 4
        }

        if (backend.soilValid) {
            if (backend.soil < 25 || backend.soil > 78)
                score -= 12
            else if (backend.soil < 35 || backend.soil > 65)
                score -= 5
        }

        if (backend.co2Valid && backend.co2 > 1450)
            score -= 10

        return Math.max(55, score)
    }

    function environmentState() {
        let alerts = []

        if (backend.temperatureValid && backend.temperature > 32)
            alerts.push("温度偏高")
        if (backend.soilValid && backend.soil < 35)
            alerts.push("土壤偏干")
        if (backend.lightValid && backend.light < 30)
            alerts.push("光照不足")
        if (backend.co2Valid && backend.co2 > 1400)
            alerts.push("CO2 偏高")

        return alerts.length > 0 ? alerts.join(" / ") : "环境整体稳定，适合保持当前种植节奏"
    }

    function dataAiDigest() {
        const modeText = backend.autoMode ? "系统当前处于自动联动模式。" : "系统当前处于人工接管模式。"
        return "分析状态：" + environmentState()
                + "\n\n环境评分：" + environmentScore() + " 分。"
                + "\n\n运行模式：" + modeText
                + "\n\nAI 番茄建议：" + backend.cropAdvice
    }

    function controlAiDigest() {
        const portText = backend.connected ? "串口在线，收发状态正常。" : "串口未连接，当前只显示本地控制建议。"
        const modeText = backend.autoMode ? "自动策略正在接管部分设备动作。" : "当前允许人工手动控制下位机设备。"
        return "联动状态：" + portText
                + "\n\n控制模式：" + modeText
                + "\n\nAI 联动建议：" + backend.controlAdvice
    }

    function trendAiDigestCurve() {
        const sourceText = backend.connected ? "曲线数据来自串口实时接入。" : "当前曲线数据来自本地模拟通道。"
        const windowText = "当前曲线窗口保存最近 " + trendHistoryCapacity + " 个采样点，可持续观察波动趋势。"
        return "趋势来源：" + sourceText
                + "\n\n趋势窗口：" + windowText
                + "\n\nAI 番茄预判：" + trendAdviceText
    }

    function growthForecastLabelCurve() {
        if (!backend.temperatureValid || !backend.soilValid || !backend.co2Valid)
            return "等待数据"
        if (backend.soil < 35 || backend.co2 > 1450 || backend.temperature > 32)
            return "需干预"
        if (backend.light > 45 && backend.soil > 38 && backend.temperature >= 22 && backend.temperature <= 30)
            return "稳步生长"
        return "持续观察"
    }

    function appendTrendPoint(series, value, valid) {
        let nextSeries = series.slice(0)
        if (valid)
            nextSeries.push(value)
        else if (nextSeries.length > 0)
            nextSeries.push(nextSeries[nextSeries.length - 1])
        else
            nextSeries.push(0)

        while (nextSeries.length > trendHistoryCapacity)
            nextSeries.shift()
        return nextSeries
    }

    function seriesDrift(series) {
        if (series.length < 2)
            return 0

        const windowSize = Math.max(1, Math.min(4, Math.floor(series.length / 2)))
        let headTotal = 0
        let tailTotal = 0

        for (let i = 0; i < windowSize; ++i) {
            headTotal += Number(series[i])
            tailTotal += Number(series[series.length - windowSize + i])
        }

        return tailTotal / windowSize - headTotal / windowSize
    }

    function refreshTrendHistory() {
        temperatureHistoryData = appendTrendPoint(temperatureHistoryData, backend.temperature, backend.temperatureValid)
        humidityHistoryData = appendTrendPoint(humidityHistoryData, backend.humidity, backend.humidityValid)
        lightHistoryData = appendTrendPoint(lightHistoryData, backend.light, backend.lightValid)
        soilHistoryData = appendTrendPoint(soilHistoryData, backend.soil, backend.soilValid)
        co2HistoryData = appendTrendPoint(co2HistoryData, backend.co2, backend.co2Valid)
        refreshTrendAdvice()
    }

    function refreshTrendAdvice() {
        const temperatureTrend = seriesDrift(temperatureHistoryData)
        const humidityTrend = seriesDrift(humidityHistoryData)
        const lightTrend = seriesDrift(lightHistoryData)
        const soilTrend = seriesDrift(soilHistoryData)
        const co2Trend = seriesDrift(co2HistoryData)

        if (temperatureHistoryData.length < 4 || co2HistoryData.length < 4) {
            trendAdviceText = "趋势窗口还在建立中，继续接收串口数据后，AI 会结合五条曲线判断番茄生长态势。"
        } else if (soilTrend <= -6 && humidityTrend <= -4) {
            trendAdviceText = "土壤湿度和空气湿度同步下滑，植株蒸腾可能正在增强，建议提前补水并关注叶片失水迹象。"
        } else if (temperatureTrend >= 3 && co2Trend >= 120) {
            trendAdviceText = "温度与二氧化碳曲线同时上扬，说明番茄温室换气效率在下降，若持续发展，番茄可能出现热胁迫前兆。"
        } else if (lightTrend <= -10 && temperatureTrend <= -2) {
            trendAdviceText = "光照和温度趋势同步走弱，环境可能进入阴雨或傍晚阶段，植物光合效率会下降，建议准备补光或保温。"
        } else if (co2Trend >= 180) {
            trendAdviceText = "二氧化碳曲线在短时间快速抬升，更像是通风不足带来的积聚，建议优先检查风扇和天窗联动是否及时。"
        } else if (soilTrend >= 4 && temperatureTrend >= -1 && temperatureTrend <= 2 && lightTrend >= 6) {
            trendAdviceText = "光照趋势向好、土壤水分平稳回升，番茄整体处在较积极的生长节奏，可继续保持当前管理策略。"
        } else {
            trendAdviceText = "五条曲线整体波动平缓，当前环境处于可控区间，番茄大概率维持稳定生长，可继续观察下一轮变化。"
        }
    }

    function trendAiDigest() {
        const sourceText = backend.connected ? "曲线数据来自串口实时接入。" : "当前曲线数据来自本地模拟通道。"
        const windowText = "当前曲线窗口保存最近 " + trendHistoryCapacity + " 个采样点，可持续观察波动趋势。"
        return "趋势来源：" + sourceText
                + "\n\n趋势窗口：" + windowText
                + "\n\nAI 番茄预判：" + trendAdviceText
    }

    function growthForecastLabel() {
        if (!backend.temperatureValid || !backend.soilValid || !backend.co2Valid)
            return "等待数据"
        if (backend.soil < 35 || backend.co2 > 1450 || backend.temperature > 32)
            return "需干预"
        if (backend.light > 45 && backend.soil > 38 && backend.temperature >= 22 && backend.temperature <= 30)
            return "稳步生长"
        return "持续观察"
    }

    function trendAiDigestStructured() {
        let currentRisk = "当前环境整体稳定，风险等级低。"
        if (backend.co2Valid && backend.co2 > 1450)
            currentRisk = "CO2 浓度处于高位，当前主要风险来自通风不足。"
        else if (backend.soilValid && backend.soil < 35)
            currentRisk = "土壤水分偏低，当前主要风险是番茄供水不足。"
        else if (backend.temperatureValid && backend.temperature > 32)
            currentRisk = "温度偏高，当前已经出现热胁迫前的压力信号。"
        else if (backend.lightValid && backend.light < 30)
            currentRisk = "光照偏弱，当前主要影响是光合效率下降。"

        let futurePrediction = trendAdviceText
        if (growthForecastLabel() === "绋虫鐢熼暱")
            futurePrediction = "未来一段时间内番茄大概率保持稳步生长，可继续按当前节奏运行。"
        else if (growthForecastLabel() === "闇€骞查")
            futurePrediction = "如果不及时干预，下一轮曲线可能会继续向不利方向放大。"

        let recommendedAction = "继续保持当前监控，并观察下一个采样窗口的曲线变化。"
        if (backend.co2Valid && backend.co2 > 1450)
            recommendedAction = "建议立刻检查风扇和天窗联动，先把 CO2 拉回安全区间。"
        else if (backend.soilValid && backend.soil < 35)
            recommendedAction = "建议优先执行补水或短时间开启水泵，同时观察土壤曲线是否回升。"
        else if (backend.temperatureValid && backend.temperature > 32)
            recommendedAction = "建议先加强通风与降温，再观察 CO2 和湿度是否同步回落。"
        else if (backend.lightValid && backend.light < 30)
            recommendedAction = "建议结合时段判断是否补光，并继续观察温度曲线是否同步下滑。"

        return "当前风险：" + currentRisk
                + "\n\n未来预判：" + futurePrediction
                + "\n\n建议动作：" + recommendedAction
    }

    component GlassCard: Rectangle {
        id: glassCardRoot
        property color accent: "#57c79a"
        property real tintStrength: 0.28

        radius: 28
        color: Qt.rgba(1, 1, 1, 0.05)
        border.width: 1
        border.color: Qt.rgba(0.80, 1.0, 0.92, 0.13)
        layer.enabled: true
        layer.samples: 4

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: parent.radius - 1
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(0.86, 1.0, 0.94, glassCardRoot.tintStrength) }
                GradientStop { position: 0.34; color: Qt.rgba(0.12, 0.31, 0.28, 0.30) }
                GradientStop { position: 1.0; color: Qt.rgba(0.03, 0.09, 0.11, 0.75) }
            }
        }
    }

    component SectionTitle: Column {
        property string eyebrow: ""
        property string title: ""
        property string caption: ""
        spacing: 4

        Text {
            text: parent.eyebrow
            color: "#7fe4c6"
            font.family: "Microsoft YaHei UI"
            font.pixelSize: 12
            font.letterSpacing: 2.5
        }

        Text {
            text: parent.title
            color: "#f3fff8"
            font.family: "DengXian"
            font.pixelSize: 27
            font.bold: true
        }

        Text {
            visible: parent.caption.length > 0
            text: parent.caption
            color: "#b7e8d8"
            font.family: "Microsoft YaHei UI"
            font.pixelSize: 13
        }
    }

    component HeaderBadge: Rectangle {
        property string badgeText: ""
        property color badgeColor: "#4bb98a"

        radius: 21
        color: Qt.rgba(0.05, 0.17, 0.19, 0.86)
        border.width: 1
        border.color: badgeColor
        implicitWidth: 148
        implicitHeight: 58

        Text {
            anchors.centerIn: parent
            text: parent.badgeText
            color: "#f1fff7"
            font.family: "Microsoft YaHei UI"
            font.pixelSize: 15
        }
    }

    component OverviewTile: GlassCard {
        id: overviewRoot
        property string tileTitle: ""
        property string tileValue: ""
        property string tileNote: ""
        property color accentColor: "#57c79a"

        accent: accentColor
        radius: 24

        Column {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 8

            Text {
                text: overviewRoot.tileTitle
                color: "#d7f7ea"
                font.family: "Microsoft YaHei UI"
                font.pixelSize: 14
            }

            Text {
                text: overviewRoot.tileValue
                color: "#ffffff"
                font.family: "DengXian"
                font.pixelSize: 30
                font.bold: true
            }

            Text {
                text: overviewRoot.tileNote
                wrapMode: Text.WordWrap
                color: "#a4ddcb"
                font.family: "Microsoft YaHei UI"
                font.pixelSize: 12
                lineHeight: 1.3
            }
        }
    }

    component SensorCard: GlassCard {
        id: sensorRoot
        property string sensorTitle: ""
        property string sensorValue: "--"
        property string sensorDetail: ""
        property string sensorTag: ""
        property color sensorValueColor: "#ffffff"

        radius: 26
        tintStrength: 0.34

        Rectangle {
            width: 6
            height: parent.height - 30
            radius: 3
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            color: sensorRoot.accent
            opacity: 0.95
        }

        Column {
            anchors.fill: parent
            anchors.margins: 22
            anchors.leftMargin: 30
            spacing: 12

            RowLayout {
                width: parent.width

                Text {
                    text: sensorRoot.sensorTitle
                    color: "#ddfaef"
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: 17
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    visible: sensorRoot.sensorTag.length > 0
                    radius: 13
                    color: Qt.rgba(1, 1, 1, 0.07)
                    border.width: 1
                    border.color: Qt.rgba(0.80, 1.0, 0.92, 0.16)
                    implicitWidth: sensorTagLabel.implicitWidth + 16
                    implicitHeight: 28

                    Text {
                        id: sensorTagLabel
                        anchors.centerIn: parent
                        text: sensorRoot.sensorTag
                        color: "#e9fff5"
                        font.family: "Microsoft YaHei UI"
                        font.pixelSize: 12
                    }
                }
            }

            Text {
                text: sensorRoot.sensorValue
                color: sensorRoot.sensorValueColor
                font.family: "DengXian"
                font.pixelSize: 40
                font.bold: true
            }

            Text {
                text: sensorRoot.sensorDetail
                wrapMode: Text.WordWrap
                color: "#a5dbc9"
                font.family: "Microsoft YaHei UI"
                font.pixelSize: 13
                lineHeight: 1.34
            }
        }
    }

    component AdvicePanel: GlassCard {
        id: adviceRoot
        property string panelTitle: ""
        property string panelBody: ""
        property string panelTag: ""

        Column {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 14

            RowLayout {
                width: parent.width

                Text {
                    text: adviceRoot.panelTitle
                    color: "#f4fff8"
                    font.family: "DengXian"
                    font.pixelSize: 22
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    visible: adviceRoot.panelTag.length > 0
                    radius: 14
                    color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1
                    border.color: Qt.rgba(0.75, 1.0, 0.88, 0.18)
                    implicitWidth: adviceTagLabel.implicitWidth + 18
                    implicitHeight: 28

                    Text {
                        id: adviceTagLabel
                        anchors.centerIn: parent
                        text: adviceRoot.panelTag
                        color: "#d7fff0"
                        font.family: "Microsoft YaHei UI"
                        font.pixelSize: 12
                    }
                }
            }

            Text {
                text: adviceRoot.panelBody
                wrapMode: Text.WordWrap
                color: "#ddfff2"
                font.family: "Microsoft YaHei UI"
                font.pixelSize: 15
                lineHeight: 1.42
            }
        }
    }

    component TrendChartCard: GlassCard {
        id: trendCardRoot
        property string chartTitle: ""
        property string chartCaption: ""
        property string chartTag: ""
        property string chartEyebrow: "TOMATO TREND MONITOR"
        property var seriesList: []
        property bool normalizeEachSeries: false
        property bool autoRange: false
        property real rangeMin: 0
        property real rangeMax: 100
        property real autoPaddingRatio: 0.22
        property real minimumSpan: 1
        property int gridRows: 6
        property int gridColumns: 8
        property string xStartLabel: "T-29"
        property string xMidLabel: "T-15"
        property string xEndLabel: "NOW"
        readonly property var primarySeries: seriesList.length > 0 ? seriesList[0] : ({ values: [] })
        readonly property var primaryBounds: resolveBounds(primarySeries)
        implicitHeight: 404

        function seriesMin(seriesValues) {
            let result = Number.POSITIVE_INFINITY
            for (let i = 0; i < seriesValues.length; ++i)
                result = Math.min(result, Number(seriesValues[i]))
            return result === Number.POSITIVE_INFINITY ? 0 : result
        }

        function seriesMax(seriesValues) {
            let result = Number.NEGATIVE_INFINITY
            for (let i = 0; i < seriesValues.length; ++i)
                result = Math.max(result, Number(seriesValues[i]))
            return result === Number.NEGATIVE_INFINITY ? 1 : result
        }

        function paddedBounds(seriesValues, fallbackMin, fallbackMax) {
            let minValue = seriesMin(seriesValues)
            let maxValue = seriesMax(seriesValues)

            if (!isFinite(minValue) || !isFinite(maxValue)) {
                minValue = fallbackMin
                maxValue = fallbackMax
            }

            if (minValue === maxValue) {
                minValue -= minimumSpan * 0.5
                maxValue += minimumSpan * 0.5
            }

            const span = Math.max(minimumSpan, maxValue - minValue)
            const padding = Math.max(minimumSpan * 0.5, span * autoPaddingRatio)
            return {
                min: minValue - padding,
                max: maxValue + padding
            }
        }

        function resolveBounds(series) {
            const values = series && series.values ? series.values : []
            let minValue = normalizeEachSeries ? Number(series.low) : rangeMin
            let maxValue = normalizeEachSeries ? Number(series.high) : rangeMax

            if (normalizeEachSeries || autoRange) {
                return paddedBounds(values, minValue, maxValue)
            }

            if (minValue === maxValue) {
                maxValue = minValue + 1
            }
            return { min: minValue, max: maxValue }
        }

        function axisLabel(value) {
            if (!isFinite(value))
                return "--"
            if (Math.abs(value) >= 1000)
                return Math.round(value).toString()
            if (Math.abs(value) >= 100)
                return value.toFixed(0)
            if (Math.abs(value) >= 10)
                return value.toFixed(1)
            return value.toFixed(2)
        }

        function leftAxisTopLabel() {
            if (normalizeEachSeries)
                return "HIGH"
            return axisLabel(primaryBounds.max)
        }

        function leftAxisMidLabel() {
            if (normalizeEachSeries)
                return "MID"
            return axisLabel((primaryBounds.min + primaryBounds.max) / 2)
        }

        function leftAxisBottomLabel() {
            if (normalizeEachSeries)
                return "LOW"
            return axisLabel(primaryBounds.min)
        }

        function paintChart(ctx, width, height) {
            ctx.clearRect(0, 0, width, height)

            const left = 20
            const top = 12
            const right = width - 20
            const bottom = height - 18
            const plotWidth = Math.max(1, right - left)
            const plotHeight = Math.max(1, bottom - top)

            ctx.strokeStyle = "rgba(124,255,223,0.18)"
            ctx.lineWidth = 1
            for (let row = 0; row <= gridRows; ++row) {
                const y = top + (plotHeight * row / gridRows)
                ctx.beginPath()
                ctx.moveTo(left, y)
                ctx.lineTo(right, y)
                ctx.stroke()
            }
            for (let col = 0; col <= gridColumns; ++col) {
                const x = left + (plotWidth * col / gridColumns)
                ctx.beginPath()
                ctx.moveTo(x, top)
                ctx.lineTo(x, bottom)
                ctx.stroke()
            }

            ctx.strokeStyle = "rgba(108,255,224,0.42)"
            ctx.lineWidth = 1.2
            ctx.strokeRect(left, top, plotWidth, plotHeight)

            const corners = 14
            ctx.strokeStyle = "rgba(111,241,214,0.85)"
            ctx.lineWidth = 2
            ctx.beginPath()
            ctx.moveTo(left, top + corners)
            ctx.lineTo(left, top)
            ctx.lineTo(left + corners, top)
            ctx.moveTo(right - corners, top)
            ctx.lineTo(right, top)
            ctx.lineTo(right, top + corners)
            ctx.moveTo(left, bottom - corners)
            ctx.lineTo(left, bottom)
            ctx.lineTo(left + corners, bottom)
            ctx.moveTo(right - corners, bottom)
            ctx.lineTo(right, bottom)
            ctx.lineTo(right, bottom - corners)
            ctx.stroke()

            for (let seriesIndex = 0; seriesIndex < seriesList.length; ++seriesIndex) {
                const series = seriesList[seriesIndex]
                const values = series.values || []
                if (values.length < 2)
                    continue

                const bounds = resolveBounds(series)
                const minValue = bounds.min
                const maxValue = bounds.max

                ctx.beginPath()
                ctx.lineWidth = 2.8
                ctx.strokeStyle = series.color
                ctx.shadowColor = series.color
                ctx.shadowBlur = 8

                for (let valueIndex = 0; valueIndex < values.length; ++valueIndex) {
                    const value = Number(values[valueIndex])
                    const ratio = Math.max(0, Math.min(1, (value - minValue) / (maxValue - minValue)))
                    const x = left + (plotWidth * valueIndex / Math.max(1, values.length - 1))
                    const y = bottom - ratio * plotHeight
                    if (valueIndex === 0)
                        ctx.moveTo(x, y)
                    else
                        ctx.lineTo(x, y)
                }
                ctx.stroke()
                ctx.shadowBlur = 0

                const lastValue = Number(values[values.length - 1])
                const lastRatio = Math.max(0, Math.min(1, (lastValue - minValue) / (maxValue - minValue)))
                const lastX = right
                const lastY = bottom - lastRatio * plotHeight

                ctx.fillStyle = "#07141b"
                ctx.beginPath()
                ctx.arc(lastX, lastY, 6, 0, Math.PI * 2)
                ctx.fill()

                ctx.fillStyle = series.color
                ctx.beginPath()
                ctx.arc(lastX, lastY, 3.5, 0, Math.PI * 2)
                ctx.fill()
            }
        }

        onSeriesListChanged: trendCanvas.requestPaint()

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 96
                radius: 18
                color: Qt.rgba(0.03, 0.13, 0.16, 0.88)
                border.width: 1
                border.color: Qt.rgba(0.53, 1.0, 0.86, 0.20)

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 14

                    ColumnLayout {
                        id: trendHeaderInfo
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        spacing: 2

                        Text {
                            text: trendCardRoot.chartEyebrow
                            color: "#7ce8c8"
                            font.family: "Consolas"
                            font.pixelSize: 11
                            font.letterSpacing: 2.6
                        }

                        Text {
                            text: trendCardRoot.chartTitle
                            color: "#f4fff8"
                            font.family: "DengXian"
                            font.pixelSize: 24
                            font.bold: true
                        }

                        Text {
                            text: trendCardRoot.chartCaption
                            wrapMode: Text.WordWrap
                            color: "#afdccc"
                            font.family: "Microsoft YaHei UI"
                            font.pixelSize: 12
                        }
                    }

                    ColumnLayout {
                        Layout.alignment: Qt.AlignTop
                        spacing: 6

                        Rectangle {
                            radius: 14
                            color: Qt.rgba(0.05, 0.20, 0.22, 0.96)
                            border.width: 1
                            border.color: Qt.rgba(0.62, 1.0, 0.88, 0.32)
                            implicitWidth: modeChipLabel.implicitWidth + 18
                            implicitHeight: 28

                            Text {
                                id: modeChipLabel
                                anchors.centerIn: parent
                                text: trendCardRoot.chartTag
                                color: "#effff8"
                                font.family: "Consolas"
                                font.pixelSize: 12
                            }
                        }

                        Rectangle {
                            radius: 14
                            color: Qt.rgba(0.07, 0.14, 0.18, 0.96)
                            border.width: 1
                            border.color: Qt.rgba(0.46, 0.89, 0.77, 0.18)
                            implicitWidth: statusChipLabel.implicitWidth + 18
                            implicitHeight: 26

                            Text {
                                id: statusChipLabel
                                anchors.centerIn: parent
                                text: "SYNC ACTIVE"
                                color: "#92f0cf"
                                font.family: "Consolas"
                                font.pixelSize: 11
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 252
                radius: 22
                color: "#061118"
                border.width: 1
                border.color: "#1e5860"

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 10
                    radius: 18
                    color: "transparent"
                    border.width: 1
                    border.color: Qt.rgba(0.42, 0.96, 0.82, 0.08)
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.top: parent.top
                    anchors.topMargin: 10
                    text: trendCardRoot.leftAxisTopLabel()
                    color: "#8de8ca"
                    font.family: "Consolas"
                    font.pixelSize: 11
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    text: trendCardRoot.leftAxisMidLabel()
                    color: "#6bbda9"
                    font.family: "Consolas"
                    font.pixelSize: 11
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 12
                    text: trendCardRoot.leftAxisBottomLabel()
                    color: "#8de8ca"
                    font.family: "Consolas"
                    font.pixelSize: 11
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 48
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 12
                    text: trendCardRoot.xStartLabel
                    color: "#79cbb4"
                    font.family: "Consolas"
                    font.pixelSize: 11
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 12
                    text: trendCardRoot.xMidLabel
                    color: "#79cbb4"
                    font.family: "Consolas"
                    font.pixelSize: 11
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 18
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 12
                    text: trendCardRoot.xEndLabel
                    color: "#79cbb4"
                    font.family: "Consolas"
                    font.pixelSize: 11
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 16
                    anchors.top: parent.top
                    anchors.topMargin: 12
                    text: "Y-AXIS"
                    color: "#69ceb0"
                    font.family: "Consolas"
                    font.pixelSize: 10
                    font.letterSpacing: 2
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 16
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 30
                    text: "TIME"
                    color: "#69ceb0"
                    font.family: "Consolas"
                    font.pixelSize: 10
                    font.letterSpacing: 2
                }

                Canvas {
                    id: trendCanvas
                    anchors.fill: parent
                    anchors.leftMargin: 56
                    anchors.rightMargin: 18
                    anchors.topMargin: 16
                    anchors.bottomMargin: 32
                    antialiasing: true
                    renderTarget: Canvas.FramebufferObject
                    onPaint: {
                        const ctx = getContext("2d")
                        trendCardRoot.paintChart(ctx, width, height)
                    }
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                    Component.onCompleted: requestPaint()
                }
            }

            Flow {
                Layout.fillWidth: true
                spacing: 10

                Repeater {
                    model: trendCardRoot.seriesList

                    delegate: Rectangle {
                        id: legendChip
                        required property var modelData
                        radius: 18
                        color: Qt.rgba(0.05, 0.16, 0.18, 0.86)
                        border.width: 1
                        border.color: modelData.color
                        implicitWidth: legendGrid.implicitWidth + 22
                        implicitHeight: 42

                        Row {
                            id: legendGrid
                            anchors.centerIn: parent
                            spacing: 10

                            Rectangle {
                                width: 14
                                height: 14
                                radius: 7
                                color: legendChip.modelData.color
                                border.width: 2
                                border.color: "#d6fff1"
                            }

                            Column {
                                spacing: 1

                                Text {
                                    text: legendChip.modelData.label
                                    color: "#f2fff8"
                                    font.family: "Consolas"
                                    font.pixelSize: 11
                                    font.letterSpacing: 1.1
                                }

                                Text {
                                    text: legendChip.modelData.latest
                                    color: "#8fe6c7"
                                    font.family: "Microsoft YaHei UI"
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    component ControlCard: Item {
        id: controlRoot
        property string controlTitle: ""
        property string controlState: ""
        property color controlAccent: "#4CBF9D"
        property bool controlInteractive: true
        signal triggered()

        implicitHeight: 160

        GlassCard {
            anchors.fill: parent
            accent: controlRoot.controlAccent
            tintStrength: controlRoot.controlInteractive ? 0.28 : 0.14
            opacity: controlRoot.controlInteractive ? 1.0 : 0.74

            Column {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 16

                RowLayout {
                    width: parent.width

                    Text {
                        text: controlRoot.controlTitle
                        color: "#effff8"
                        font.family: "DengXian"
                        font.pixelSize: 22
                        font.bold: true
                    }

                    Item { Layout.fillWidth: true }

                    Rectangle {
                        implicitWidth: 14
                        implicitHeight: 14
                        radius: 7
                        color: controlRoot.controlAccent
                    }
                }

                Text {
                    text: controlRoot.controlState
                    color: "#d1ffeb"
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: 15
                }

                Rectangle {
                    width: parent.width
                    height: 48
                    radius: 15
                    color: controlRoot.controlInteractive ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(1, 1, 1, 0.04)
                    border.width: 1
                    border.color: controlRoot.controlInteractive ? controlRoot.controlAccent : "#5f7f76"

                    Text {
                        anchors.centerIn: parent
                        text: controlRoot.controlInteractive ? "点击切换状态" : "自动模式接管中"
                        color: controlRoot.controlInteractive ? "#f6fff9" : "#acd2c1"
                        font.family: "Microsoft YaHei UI"
                        font.pixelSize: 14
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                enabled: controlRoot.controlInteractive
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: controlRoot.triggered()
            }
        }
    }

    Connections {
        target: backend

        function onTelemetryChanged() {
            window.refreshTrendHistory()
        }
    }

    Component.onCompleted: refreshTrendHistory()

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#0d3431" }
            GradientStop { position: 0.42; color: "#091d24" }
            GradientStop { position: 1.0; color: "#050d12" }
        }
    }

    Repeater {
        model: 18

        Rectangle {
            required property int index
            width: parent.width
            height: 1
            y: index * (window.height / 18)
            color: Qt.rgba(0.60, 1.0, 0.85, 0.05)
        }
    }

    Repeater {
        model: 28

        Rectangle {
            required property int index
            width: 1
            height: parent.height
            x: index * (window.width / 28)
            color: Qt.rgba(0.60, 1.0, 0.85, 0.035)
        }
    }

    Rectangle {
        width: 520
        height: 520
        radius: 260
        x: window.width - width * 0.75
        y: -height * 0.2
        color: "#ecd06e"
        opacity: 0.08
    }

    Rectangle {
        width: 420
        height: 420
        radius: 210
        x: -width * 0.25
        y: window.height - height * 0.72
        color: "#56d9a2"
        opacity: 0.08
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 20

        GlassCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 184
            accent: "#69cf98"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 24

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text {
                        text: "AI TOMATO CONTROL CENTER"
                        color: "#8ce8c6"
                        font.family: "DengXian"
                        font.pixelSize: 14
                        font.letterSpacing: 3.2
                    }

                    Text {
                        text: "AI番茄生长监测与联动控制系统"
                        color: "#f4fff8"
                        font.family: "DengXian"
                        font.pixelSize: 36
                        font.bold: true
                    }

                    Text {
                        text: "通过串口接收下位机数据，完成番茄环境监测、设备联动控制与 AI 生长分析。"
                        color: "#b6ead7"
                        font.family: "Microsoft YaHei UI"
                        font.pixelSize: 14
                    }

                    Row {
                        spacing: 12

                        HeaderBadge {
                            badgeText: backend.currentTime
                            badgeColor: "#4cbf9d"
                        }

                        HeaderBadge {
                            badgeText: weatherGlyph(backend.weatherType) + "  " + backend.weather
                            badgeColor: backend.weatherType === "sunny" ? "#f0c85d"
                                      : backend.weatherType === "rainy" ? "#64a9ff"
                                      : "#8fc9df"
                        }

                        HeaderBadge {
                            badgeText: backend.location
                            badgeColor: "#6fd58a"
                            implicitWidth: 300
                        }
                    }
                }

                GlassCard {
                    Layout.preferredWidth: 320
                    Layout.fillHeight: true
                    accent: backend.connected ? "#4dc59c" : "#7aa9b6"

                    Column {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12

                        Text {
                            text: "运行状态"
                            color: "#d8f7ea"
                            font.family: "Microsoft YaHei UI"
                            font.pixelSize: 14
                        }

                        Text {
                            text: backend.connectionStatus
                            wrapMode: Text.WordWrap
                            color: "#f6fff9"
                            font.family: "Microsoft YaHei UI"
                            font.pixelSize: 14
                            lineHeight: 1.3
                        }

                        RowLayout {
                            width: parent.width
                            spacing: 10

                            Button {
                                id: modeButton
                                Layout.fillWidth: true
                                text: backend.modeLabel
                                onClicked: backend.toggleAutoMode()

                                background: Rectangle {
                                    radius: 16
                                    color: backend.autoMode ? "#59b96c" : "#ddeee3"
                                }

                                contentItem: Text {
                                    text: modeButton.text
                                    color: backend.autoMode ? "#ffffff" : "#1f4338"
                                    font.family: "DengXian"
                                    font.pixelSize: 17
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: 94
                                Layout.preferredHeight: 46
                                radius: 16
                                color: Qt.rgba(1, 1, 1, 0.07)
                                border.width: 1
                                border.color: Qt.rgba(0.75, 1.0, 0.88, 0.18)

                                Column {
                                    anchors.centerIn: parent
                                    spacing: 2

                                    Text {
                                        text: environmentScore()
                                        color: "#f7fff9"
                                        font.family: "DengXian"
                                        font.pixelSize: 22
                                        font.bold: true
                                    }

                                    Text {
                                        text: "环境分"
                                        color: "#a9dac8"
                                        font.family: "Microsoft YaHei UI"
                                        font.pixelSize: 11
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 20

            GlassCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                accent: "#58cb95"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 20

                    RowLayout {
                        Layout.fillWidth: true

                        SectionTitle {
                            eyebrow: "AI TOMATO PAGES"
                            title: window.currentPage === 0
                                   ? "番茄环境监测页"
                                   : window.currentPage === 1
                                     ? "联动控制页"
                                     : window.currentPage === 2
                                       ? "番茄曲线分析页"
                                       : "番茄视频监测页"
                            caption: window.currentPage === 0
                                     ? "串口接收到的温湿度、光照、土壤和 CO2 数据会在这里集中展示，用于番茄生长监测。"
                                     : window.currentPage === 1
                                       ? "通过 Qt 对风扇、水泵、天窗等下位机设备进行联动控制。"
                                       : window.currentPage === 2
                                         ? "曲线分析页负责实时趋势判断，两张曲线图会根据串口数据持续刷新。"
                                         : "视频监控页通过 RTSP 显示 K230 画面，并通过 UDP 广播接收虫害和番茄植株状态。"
                        }

                        Item { Layout.fillWidth: true }

                        Row {
                            spacing: 10

                            Repeater {
                                model: [
                                    { text: "数据显示", page: 0 },
                                    { text: "设备控制", page: 1 },
                                    { text: "曲线分析", page: 2 },
                                    { text: "视频监控", page: 3 }
                                ]

                                delegate: Button {
                                    required property var modelData
                                    id: pageButton
                                    text: modelData.text
                                    width: 126
                                    height: 42
                                    onClicked: window.currentPage = modelData.page

                                    background: Rectangle {
                                        radius: 15
                                        color: window.currentPage === pageButton.modelData.page ? "#4d9f61" : "#102832"
                                        border.width: 1
                                        border.color: window.currentPage === pageButton.modelData.page ? "#95e2b6" : "#34535f"
                                    }

                                    contentItem: Text {
                                        text: pageButton.text
                                        color: "#f5fff8"
                                        font.family: "Microsoft YaHei UI"
                                        font.pixelSize: 15
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }
                        }
                    }

                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: window.currentPage

                        Item {
                            ScrollView {
                                id: dataScroll
                                anchors.fill: parent
                                clip: true
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                                ColumnLayout {
                                    width: Math.max(dataScroll.availableWidth, 920)
                                    spacing: 18

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 136
                                        spacing: 16

                                        OverviewTile {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            tileTitle: "环境评分"
                                            tileValue: environmentScore() + " 分"
                                            tileNote: environmentState()
                                            accentColor: "#61cc90"
                                        }

                                        OverviewTile {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            tileTitle: "联动模式"
                                            tileValue: backend.autoMode ? "AUTO" : "MANUAL"
                                            tileNote: backend.autoMode ? "下位机自动策略正在参与控制。" : "当前允许人工直接切换设备。"
                                            accentColor: backend.autoMode ? "#4ebf8e" : "#efb85f"
                                        }

                                        OverviewTile {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            tileTitle: "串口状态"
                                            tileValue: backend.connected ? "ONLINE" : "STANDBY"
                                            tileNote: backend.connected ? "串口已接入，数据正在实时刷新。" : "等待串口接入，当前显示本地演示状态。"
                                            accentColor: backend.connected ? "#4aa7ff" : "#829fb0"
                                        }
                                    }

                                    GridLayout {
                                        Layout.fillWidth: true
                                        columns: 2
                                        rowSpacing: 18
                                        columnSpacing: 18

                                        SensorCard {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 182
                                            accent: "#ff8854"
                                            sensorTitle: "温度"
                                            sensorValue: readingText(backend.temperature, "°C", backend.temperatureValid)
                                            sensorDetail: backend.temperatureValid ? "适宜参考范围 22-30°C，用于风扇和天窗联动判断。" : "等待下位机传回温度数据。"
                                            sensorTag: "TEMP"
                                            sensorValueColor: "#ffe7d8"
                                        }

                                        SensorCard {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 182
                                            accent: "#49abff"
                                            sensorTitle: "湿度"
                                            sensorValue: readingText(backend.humidity, "%", backend.humidityValid)
                                            sensorDetail: backend.humidityValid ? "建议结合叶面蒸腾情况，判断灌溉与通风节奏。" : "等待下位机传回湿度数据。"
                                            sensorTag: "HUM"
                                            sensorValueColor: "#def3ff"
                                        }

                                        SensorCard {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 182
                                            accent: "#f7bb3c"
                                            sensorTitle: "光照"
                                            sensorValue: readingText(backend.light, "%", backend.lightValid)
                                            sensorDetail: backend.lightValid ? "可用于补光、通风和病虫害活跃时段判断。" : "等待下位机传回光照数据。"
                                            sensorTag: "LUX"
                                            sensorValueColor: "#fff4cc"
                                        }

                                        SensorCard {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 182
                                            accent: "#9f7154"
                                            sensorTitle: "土壤湿度"
                                            sensorValue: readingText(backend.soil, "%", backend.soilValid)
                                            sensorDetail: backend.soilValid ? "低于 35% 建议重点关注补水和水泵联动。" : "等待下位机传回土壤湿度数据。"
                                            sensorTag: "SOIL"
                                            sensorValueColor: "#f6e7dc"
                                        }

                                        SensorCard {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 188
                                            Layout.columnSpan: 2
                                            accent: "#7e6cff"
                                            sensorTitle: "CO2 浓度"
                                            sensorValue: readingText(backend.co2, "ppm", backend.co2Valid)
                                            sensorDetail: backend.co2Valid ? "高于 1400 ppm 时建议加强通风，并结合温度共同判断设备动作。" : "等待下位机传回 CO2 数据。"
                                            sensorTag: "CO2"
                                            sensorValueColor: "#e6e3ff"
                                        }
                                    }

                                    SensorCard {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 150
                                        accent: "#54d7b1"
                                        sensorTitle: "天气 / 位置"
                                        sensorValue: backend.weather
                                        sensorDetail: backend.location
                                        sensorTag: weatherGlyph(backend.weatherType)
                                        sensorValueColor: "#e1fff4"
                                    }

                                    AdvicePanel {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 270
                                        accent: "#70c67c"
        panelTitle: "AI 番茄生长建议"
                                        panelTag: backend.connected ? "实时接入" : "本地建议"
                                        panelBody: dataAiDigest()
                                    }
                                }
                            }
                        }

                        Item {
                            ScrollView {
                                id: controlScroll
                                anchors.fill: parent
                                clip: true
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                                ColumnLayout {
                                    width: Math.max(controlScroll.availableWidth, 920)
                                    spacing: 18

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 126
                                        spacing: 16

                                        OverviewTile {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            tileTitle: "控制模式"
                                            tileValue: backend.autoMode ? "自动托管" : "手动接管"
                                            tileNote: backend.autoMode ? "设备由策略和下位机状态共同决定。" : "点击卡片即可切换对应设备状态。"
                                            accentColor: backend.autoMode ? "#69c986" : "#f0b864"
                                        }

                                        OverviewTile {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            tileTitle: "串口通道"
                                            tileValue: backend.connected ? "已连接" : "未连接"
                                            tileNote: backend.connectionStatus
                                            accentColor: backend.connected ? "#44b6ff" : "#889bab"
                                        }

                                        OverviewTile {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            tileTitle: "控制建议"
                                            tileValue: backend.autoMode ? "策略优先" : "人工优先"
                                            tileNote: backend.controlAdvice
                                            accentColor: "#59c3a3"
                                        }
                                    }

                                    GridLayout {
                                        Layout.fillWidth: true
                                        columns: 2
                                        rowSpacing: 18
                                        columnSpacing: 18

                                        Repeater {
                                            model: backend.controlModel

                                            delegate: ControlCard {
                                                required property int index
                                                required property string name
                                                required property string stateText
                                                required property color accent
                                                required property bool controlEnabled

                                                Layout.fillWidth: true
                                                Layout.preferredHeight: index === 4 ? 188 : 182
                                                Layout.columnSpan: index === 4 ? 2 : 1

                                                controlTitle: name
                                                controlState: "当前状态：" + stateText
                                                controlAccent: accent
                                                controlInteractive: controlEnabled
                                                onTriggered: backend.toggleControl(index)
                                            }
                                        }
                                    }

                                    AdvicePanel {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 230
                                        accent: "#73b96c"
                                        panelTitle: "AI 联动控制建议"
                                        panelTag: backend.autoMode ? "自动联动" : "手动控制"
                                        panelBody: controlAiDigest()
                                    }
                                }
                            }
                        }

                        Item {
                            ScrollView {
                                id: trendScroll
                                anchors.fill: parent
                                clip: true
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                                ColumnLayout {
                                    width: Math.max(trendScroll.availableWidth, 920)
                                    spacing: 12

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 126
                                        spacing: 16

                                        OverviewTile {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            tileTitle: "曲线窗口"
                                            tileValue: trendHistoryCapacity + " 点"
                                            tileNote: "五大环境数据持续进入历史队列，用于生成实时趋势图。"
                                            accentColor: "#68c9a0"
                                        }

                                        OverviewTile {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            tileTitle: "数据来源"
                                            tileValue: backend.connected ? "SERIAL" : "MOCK"
                                            tileNote: backend.connected ? "当前曲线来自串口实时上报。" : "当前曲线来自本地模拟数据。"
                                            accentColor: backend.connected ? "#44b6ff" : "#8ea4b0"
                                        }

                                        OverviewTile {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            tileTitle: "番茄生长预判"
                                            tileValue: growthForecastLabel()
                                            tileNote: trendAdviceText
                                            accentColor: "#7dbe66"
                                        }
                                    }

                                    TrendChartCard {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 380
                                        accent: "#7e6cff"
                                        chartTitle: "二氧化碳趋势曲线"
                                        chartCaption: "单独观察 CO2 变化，用来判断番茄温室通风与植株呼吸环境是否稳定。"
                                        chartTag: "CO2 / ppm"
                                        autoRange: true
                                        autoPaddingRatio: 0.28
                                        minimumSpan: 40
                                        gridRows: 7
                                        gridColumns: 10
                                        seriesList: [
                                            {
                                                label: "CO2",
                                                color: "#9584ff",
                                                values: co2HistoryData,
                                                latest: readingText(backend.co2, "ppm", backend.co2Valid)
                                            }
                                        ]
                                    }

                                    TrendChartCard {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 448
                                        accent: "#57c79a"
                                        chartTitle: "四项环境综合曲线"
                                        chartCaption: "同一张图内叠加温度、湿度、光照和土壤湿度，便于对比四类环境变化节奏。"
                                        chartTag: "归一化趋势"
                                        normalizeEachSeries: true
                                        autoPaddingRatio: 0.2
                                        minimumSpan: 4
                                        gridRows: 7
                                        gridColumns: 10
                                        seriesList: [
                                            {
                                                label: "温度",
                                                color: "#ff8b5d",
                                                values: temperatureHistoryData,
                                                low: 10,
                                                high: 40,
                                                latest: readingText(backend.temperature, "°C", backend.temperatureValid)
                                            },
                                            {
                                                label: "湿度",
                                                color: "#4bb4ff",
                                                values: humidityHistoryData,
                                                low: 0,
                                                high: 100,
                                                latest: readingText(backend.humidity, "%", backend.humidityValid)
                                            },
                                            {
                                                label: "光照",
                                                color: "#f4c04a",
                                                values: lightHistoryData,
                                                low: 0,
                                                high: 100,
                                                latest: readingText(backend.light, "%", backend.lightValid)
                                            },
                                            {
                                                label: "土壤",
                                                color: "#c68d68",
                                                values: soilHistoryData,
                                                low: 0,
                                                high: 100,
                                                latest: readingText(backend.soil, "%", backend.soilValid)
                                            }
                                        ]
                                    }

                                    AdvicePanel {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 230
                                        accent: "#6ec67a"
                                        panelTitle: "AI 曲线预判建议"
                                        panelTag: backend.connected ? "趋势预判" : "本地预测"
                                        panelBody: trendAiDigestStructured()
                                    }
                                }
                            }
                        }

                        Item {
                            id: videoMonitorPage
                            function reconnectStream() {
                                if (backend.rtspUrl.length === 0)
                                    return
                                monitorPlayer.stop()
                                monitorPlayer.play()
                            }

                            MediaPlayer {
                                id: monitorPlayer
                                source: backend.rtspUrl
                                videoOutput: monitorOutput

                                onSourceChanged: {
                                    if (source.toString().length > 0)
                                        play()
                                    else
                                        stop()
                                }
                            }

                            ScrollView {
                                id: videoScroll
                                anchors.fill: parent
                                clip: true
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                                ColumnLayout {
                                    width: Math.max(videoScroll.availableWidth, 920)
                                    spacing: 18

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 126
                                        spacing: 16

                                        OverviewTile {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            tileTitle: "RTSP 源"
                                            tileValue: backend.cameraIp.length > 0 ? backend.cameraIp : "等待发现"
                                            tileNote: backend.rtspUrl.length > 0
                                                      ? backend.rtspUrl
                                                      : "等待 K230 UDP 广播自动带出摄像头 IP"
                                            accentColor: "#5cc6a1"
                                        }

                                        OverviewTile {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            tileTitle: "UDP 广播"
                                            tileValue: backend.udpOnline ? "ONLINE" : "LISTENING"
                                            tileNote: backend.udpStatus
                                            accentColor: backend.udpOnline ? "#49b2ff" : "#86a8b7"
                                        }

                                        OverviewTile {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            tileTitle: "K230 状态"
                                            tileValue: backend.udpOnline ? "已接入" : "等待中"
                                            tileNote: backend.k230Status
                                            accentColor: backend.udpOnline ? "#77d46c" : "#e6b25f"
                                        }
                                    }

                                    GlassCard {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 560
                                        accent: "#5dc1e8"

                                        ColumnLayout {
                                            anchors.fill: parent
                                            anchors.margins: 22
                                            spacing: 16

                                            RowLayout {
                                                Layout.fillWidth: true

                                                SectionTitle {
                                                    eyebrow: "K230 TOMATO MONITOR"
                                                    title: "番茄视频监测 / RTSP 实时画面"
                                                    caption: "K230 与 Qt 主机处于同一局域网时，Qt 会通过 UDP 广播自动识别摄像头 IP，并拉取 rtsp://IP:8554/face 实时画面。"
                                                }

                                                Item { Layout.fillWidth: true }

                                                RowLayout {
                                                    spacing: 10

                                                    TextField {
                                                        id: cameraIpField
                                                        Layout.preferredWidth: 180
                                                        placeholderText: "手动输入 K230 IP"
                                                        text: backend.cameraIp
                                                        color: "#ecfff6"
                                                        selectByMouse: true

                                                        background: Rectangle {
                                                            radius: 12
                                                            color: "#10242d"
                                                            border.width: 1
                                                            border.color: "#3f6875"
                                                        }

                                                        onEditingFinished: backend.cameraIp = text
                                                    }

                                                    Button {
                                                        id: applyIpButton
                                                        text: "应用 IP"
                                                        onClicked: backend.cameraIp = cameraIpField.text

                                                        background: Rectangle {
                                                            radius: 12
                                                            color: "#2f7c65"
                                                            border.width: 1
                                                            border.color: "#7edeb5"
                                                        }

                                                        contentItem: Text {
                                                            text: applyIpButton.text
                                                            color: "#f5fff9"
                                                            font.family: "Microsoft YaHei UI"
                                                            font.pixelSize: 14
                                                            horizontalAlignment: Text.AlignHCenter
                                                            verticalAlignment: Text.AlignVCenter
                                                        }
                                                    }

                                                    Button {
                                                        id: reconnectVideoButton
                                                        text: "重连视频"
                                                        onClicked: videoMonitorPage.reconnectStream()

                                                        background: Rectangle {
                                                            radius: 12
                                                            color: "#184353"
                                                            border.width: 1
                                                            border.color: "#6ec2dc"
                                                        }

                                                        contentItem: Text {
                                                            text: reconnectVideoButton.text
                                                            color: "#effff8"
                                                            font.family: "Microsoft YaHei UI"
                                                            font.pixelSize: 14
                                                            horizontalAlignment: Text.AlignHCenter
                                                            verticalAlignment: Text.AlignVCenter
                                                        }
                                                    }

                                                    Button {
                                                        id: restartUdpButton
                                                        text: "重启监听"
                                                        onClicked: backend.restartUdpListener()

                                                        background: Rectangle {
                                                            radius: 12
                                                            color: "#2b3442"
                                                            border.width: 1
                                                            border.color: "#8ca8b8"
                                                        }

                                                        contentItem: Text {
                                                            text: restartUdpButton.text
                                                            color: "#effff8"
                                                            font.family: "Microsoft YaHei UI"
                                                            font.pixelSize: 14
                                                            horizontalAlignment: Text.AlignHCenter
                                                            verticalAlignment: Text.AlignVCenter
                                                        }
                                                    }
                                                }
                                            }

                                            Rectangle {
                                                Layout.fillWidth: true
                                                Layout.fillHeight: true
                                                radius: 24
                                                color: "#061016"
                                                border.width: 1
                                                border.color: "#2e7287"
                                                clip: true

                                                Rectangle {
                                                    anchors.fill: parent
                                                    anchors.margins: 10
                                                    radius: 18
                                                    color: "#02070b"
                                                    border.width: 1
                                                    border.color: Qt.rgba(0.55, 0.94, 0.90, 0.12)
                                                }

                                                VideoOutput {
                                                    id: monitorOutput
                                                    anchors.fill: parent
                                                    anchors.margins: 12
                                                    fillMode: VideoOutput.PreserveAspectFit
                                                }

                                                Column {
                                                    anchors.centerIn: parent
                                                    width: parent.width * 0.56
                                                    spacing: 10
                                                    visible: backend.rtspUrl.length === 0
                                                             || monitorPlayer.playbackState !== MediaPlayer.PlayingState

                                                    Text {
                                                        width: parent.width
                                                        text: backend.rtspUrl.length === 0
                                                              ? "等待 K230 UDP 广播，识别到 IP 后会自动连接 RTSP 视频流。"
                                                              : monitorPlayer.errorString.length > 0
                                                                ? "RTSP 拉流失败：\n" + monitorPlayer.errorString
                                                                : "正在连接 RTSP 视频流，请稍候..."
                                                        wrapMode: Text.WordWrap
                                                        horizontalAlignment: Text.AlignHCenter
                                                        color: "#d9fff0"
                                                        font.family: "Microsoft YaHei UI"
                                                        font.pixelSize: 18
                                                        lineHeight: 1.35
                                                    }

                                                    Text {
                                                        width: parent.width
                                                        text: backend.rtspUrl.length > 0
                                                              ? backend.rtspUrl
                                                              : "默认地址格式：rtsp://K230_IP:8554/face"
                                                        wrapMode: Text.WordWrap
                                                        horizontalAlignment: Text.AlignHCenter
                                                        color: "#88c8c0"
                                                        font.family: "Consolas"
                                                        font.pixelSize: 13
                                                    }
                                                }

                                                Rectangle {
                                                    anchors.left: parent.left
                                                    anchors.top: parent.top
                                                    anchors.margins: 18
                                                    radius: 14
                                                    color: Qt.rgba(0.04, 0.20, 0.24, 0.92)
                                                    border.width: 1
                                                    border.color: "#5bd6c0"
                                                    implicitWidth: streamLabel.implicitWidth + 20
                                                    implicitHeight: 30

                                                    Text {
                                                        id: streamLabel
                                                        anchors.centerIn: parent
                                                        text: monitorPlayer.playbackState === MediaPlayer.PlayingState
                                                              ? "RTSP / LIVE"
                                                              : "RTSP / WAITING"
                                                        color: "#e8fff6"
                                                        font.family: "Consolas"
                                                        font.pixelSize: 12
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    GridLayout {
                                        Layout.fillWidth: true
                                        columns: 2
                                        rowSpacing: 18
                                        columnSpacing: 18

                                        SensorCard {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 182
                                            accent: "#f28e57"
                                            sensorTitle: "虫害识别"
                                            sensorValue: backend.pestStatus
                                            sensorDetail: "来自 K230 UDP 广播的最新虫害目标结果，可与蜂鸣器 / 诱虫灯策略联动。"
                                            sensorTag: "PEST"
                                            sensorValueColor: "#ffe7d9"
                                        }

                                        SensorCard {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 182
                                            accent: "#76c46d"
                                            sensorTitle: "植株状态"
                                            sensorValue: backend.witherStatus
                                            sensorDetail: "K230 会根据叶片颜色与区域趋势给出植株健康 / 枯萎风险状态。"
                                            sensorTag: "PLANT"
                                            sensorValueColor: "#e4ffe2"
                                        }

                                        SensorCard {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 182
                                            accent: "#4ba9ff"
                                            sensorTitle: "局域网链路"
                                            sensorValue: backend.udpOnline ? "广播在线" : "等待广播"
                                            sensorDetail: backend.k230Status
                                            sensorTag: "LAN"
                                            sensorValueColor: "#e1f2ff"
                                        }

                                        SensorCard {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 182
                                            accent: "#8c7eff"
                                            sensorTitle: "视频入口"
                                            sensorValue: backend.cameraIp.length > 0 ? backend.cameraIp : "未发现 IP"
                                            sensorDetail: backend.rtspUrl.length > 0
                                                          ? backend.rtspUrl
                                                          : "K230 一旦广播上线，Qt 会自动拼出 RTSP 地址。"
                                            sensorTag: "RTSP"
                                            sensorValueColor: "#ece7ff"
                                        }
                                    }

                                    AdvicePanel {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 250
                                        accent: "#6bbf7b"
                                        panelTitle: "AI 视频监测建议"
                                        panelTag: backend.udpOnline ? "K230 在线" : "等待广播"
                                        panelBody: backend.videoAdvice
                                    }
                                }
                            }
                        }
                    }
                }
            }

            GlassCard {
                Layout.preferredWidth: 390
                Layout.fillHeight: true
                accent: "#4bb8a4"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 16

                    SectionTitle {
                        eyebrow: "SERIAL HUB"
                        title: "串口日志中心"
                        caption: "这里显示串口接收的数据、Qt 下发的命令，以及连接状态。"
                    }

                    Text {
                        text: "右侧日志会区分 SYS / TX / RX，方便你直接查看串口收发内容。"
                        wrapMode: Text.WordWrap
                        color: "#c7f6e6"
                        font.family: "Microsoft YaHei UI"
                        font.pixelSize: 13
                        lineHeight: 1.35
                        Layout.fillWidth: true
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: 10
                        columnSpacing: 10

                        ComboBox {
                            id: portCombo
                            Layout.fillWidth: true
                            model: backend.portNames
                            currentIndex: Math.max(0, backend.portNames.indexOf(backend.selectedPortName))
                            onActivated: backend.selectedPortName = currentText

                            background: Rectangle {
                                radius: 14
                                color: "#0f2630"
                                border.width: 1
                                border.color: "#355e67"
                            }

                            contentItem: Text {
                                text: portCombo.displayText
                                color: "#f4fff8"
                                font.family: "Microsoft YaHei UI"
                                font.pixelSize: 14
                                leftPadding: 14
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        TextField {
                            Layout.fillWidth: true
                            text: backend.baudRate.toString()
                            placeholderText: "9600"
                            inputMethodHints: Qt.ImhDigitsOnly
                            onEditingFinished: backend.baudRate = Number(text)

                            background: Rectangle {
                                radius: 14
                                color: "#0f2630"
                                border.width: 1
                                border.color: "#355e67"
                            }

                            color: "#f4fff8"
                            font.family: "Microsoft YaHei UI"
                            font.pixelSize: 14
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Button {
                            id: refreshButton
                            Layout.fillWidth: true
                            text: "刷新串口"
                            onClicked: backend.refreshPorts()

                            background: Rectangle {
                                radius: 14
                                color: "#11303a"
                                border.width: 1
                                border.color: "#3a6d70"
                            }

                            contentItem: Text {
                                text: refreshButton.text
                                color: "#effff8"
                                font.family: "Microsoft YaHei UI"
                                font.pixelSize: 14
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        Button {
                            id: connectButton
                            Layout.fillWidth: true
                            text: backend.connected ? "断开连接" : "连接串口"
                            onClicked: backend.connected ? backend.disconnectSerial() : backend.connectSerial()

                            background: Rectangle {
                                radius: 14
                                color: backend.connected ? "#5e2c32" : "#4a9e62"
                            }

                            contentItem: Text {
                                text: connectButton.text
                                color: "#ffffff"
                                font.family: "DengXian"
                                font.pixelSize: 15
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        HeaderBadge {
                            badgeText: backend.connected ? "串口在线" : "等待连接"
                            badgeColor: backend.connected ? "#4dc59c" : "#8099a8"
                            implicitWidth: 110
                        }

                        HeaderBadge {
                            badgeText: backend.autoMode ? "自动联动" : "手动接管"
                            badgeColor: backend.autoMode ? "#67c47a" : "#efb25a"
                            implicitWidth: 110
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            id: clearLogButton
                            text: "清空日志"
                            onClicked: backend.clearSerialLog()
                            Layout.preferredWidth: 100
                            Layout.preferredHeight: 36

                            background: Rectangle {
                                radius: 13
                                color: "#0e2730"
                                border.width: 1
                                border.color: "#45656a"
                            }

                            contentItem: Text {
                                text: clearLogButton.text
                                color: "#dffbef"
                                font.family: "Microsoft YaHei UI"
                                font.pixelSize: 13
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }

                    GlassCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 76
                        accent: "#439dd4"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            Repeater {
                                model: [
                                    { text: "RX", color: "#74e0bf" },
                                    { text: "TX", color: "#74b9ff" },
                                    { text: "SYS", color: "#f0c675" }
                                ]

                                delegate: Rectangle {
                                    required property var modelData
                                    implicitWidth: 66
                                    implicitHeight: 30
                                    radius: 15
                                    color: Qt.rgba(1, 1, 1, 0.08)
                                    border.width: 1
                                    border.color: modelData.color

                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.text
                                        color: "#eafff7"
                                        font.family: "Consolas"
                                        font.pixelSize: 14
                                    }
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: "原始 JSON、状态通知、错误信息都会持续叠加在下方日志窗口。"
                                wrapMode: Text.WordWrap
                                color: "#d3fff0"
                                font.family: "Microsoft YaHei UI"
                                font.pixelSize: 12
                            }
                        }
                    }

                    GlassCard {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        accent: "#3bb2c8"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 12

                            RowLayout {
                                Layout.fillWidth: true

                                Text {
                                    text: "实时串口日志"
                                    color: "#f4fff8"
                                    font.family: "DengXian"
                                    font.pixelSize: 20
                                    font.bold: true
                                }

                                Item { Layout.fillWidth: true }

                                Text {
                                    text: backend.selectedPortName.length > 0 ? backend.selectedPortName : "未选择串口"
                                    color: "#9cdac8"
                                    font.family: "Consolas"
                                    font.pixelSize: 12
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: 20
                                color: "#07141b"
                                border.width: 1
                                border.color: "#1d4a54"

                                Flickable {
                                    id: logFlick
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    clip: true
                                    contentWidth: Math.max(width, logArea.implicitWidth)
                                    contentHeight: Math.max(height, logArea.implicitHeight)

                                    ScrollBar.vertical: ScrollBar { }
                                    ScrollBar.horizontal: ScrollBar { }

                                    TextArea {
                                        id: logArea
                                        width: Math.max(logFlick.width, implicitWidth)
                                        text: backend.serialLog.length > 0
                                              ? backend.serialLog
                                              : "[等待日志] 串口连接后，这里会显示接收和发送的原始数据。"
                                        readOnly: true
                                        selectByMouse: true
                                        wrapMode: TextEdit.NoWrap
                                        color: backend.serialLog.length > 0 ? "#d8fff2" : "#84b7aa"
                                        font.family: "Consolas"
                                        font.pixelSize: 13
                                        background: null

                                        onTextChanged: {
                                            cursorPosition = length
                                            logFlick.contentY = Math.max(0, logFlick.contentHeight - logFlick.height)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
