#include "controlmodel.h"

#include <algorithm>

ControlModel::ControlModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_items = {
        {0, QStringLiteral("pump"), QStringLiteral("水泵"),
         QStringLiteral("水泵已开启，请留意土壤湿度变化。"),
         QColor(QStringLiteral("#3F9BFF")),
         {QStringLiteral("关"), QStringLiteral("开")}, 0, false},
        {1, QStringLiteral("fan"), QStringLiteral("风扇"),
         QStringLiteral("风扇已开启，当前适合加强通风。"),
         QColor(QStringLiteral("#4CBF9D")),
         {QStringLiteral("关"), QStringLiteral("开")}, 0, false},
        {2, QStringLiteral("buzzer"), QStringLiteral("蜂鸣器"),
         QStringLiteral("蜂鸣器可在蝗虫与菜青虫模式之间切换。"),
         QColor(QStringLiteral("#F2A84A")),
         {QStringLiteral("关"), QStringLiteral("蝗虫"), QStringLiteral("菜青虫")}, 0, false},
        {3, QStringLiteral("window"), QStringLiteral("天窗"),
         QStringLiteral("天窗支持关闭、半开和全开三种通风状态。"),
         QColor(QStringLiteral("#79B85D")),
         {QStringLiteral("关闭"), QStringLiteral("半开"), QStringLiteral("全开")}, 0, false},
        {4, QStringLiteral("pestLamp"), QStringLiteral("诱虫灯"),
         QStringLiteral("诱虫灯已开启，可持续诱捕夜间害虫。"),
         QColor(QStringLiteral("#8E67D9")),
         {QStringLiteral("关"), QStringLiteral("开")}, 0, false},
    };
}

int ControlModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant ControlModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    const ControlItem &item = m_items.at(index.row());
    switch (role) {
    case IdRole:
        return item.id;
    case KeyRole:
        return item.key;
    case NameRole:
        return item.name;
    case StateRole:
        return item.state;
    case StateTextRole:
        return item.stateTexts.value(item.state);
    case StateCountRole:
        return item.stateTexts.size();
    case AccentRole:
        return item.accent;
    case EnabledRole:
        return item.enabled;
    default:
        return {};
    }
}

QHash<int, QByteArray> ControlModel::roleNames() const
{
    return {
        {IdRole, "controlId"},
        {KeyRole, "key"},
        {NameRole, "name"},
        {StateRole, "state"},
        {StateTextRole, "stateText"},
        {StateCountRole, "stateCount"},
        {AccentRole, "accent"},
        {EnabledRole, "controlEnabled"},
    };
}

bool ControlModel::setAutoMode(bool autoMode)
{
    bool changed = false;
    for (int row = 0; row < m_items.size(); ++row) {
        ControlItem &item = m_items[row];
        const bool enabled = !autoMode;
        if (item.enabled == enabled) {
            continue;
        }
        item.enabled = enabled;
        const QModelIndex modelIndex = index(row);
        emit dataChanged(modelIndex, modelIndex, {EnabledRole});
        changed = true;
    }
    return changed;
}

bool ControlModel::cycleState(int row)
{
    if (row < 0 || row >= m_items.size()) {
        return false;
    }

    ControlItem &item = m_items[row];
    return updateState(item, row, (item.state + 1) % item.stateTexts.size());
}

bool ControlModel::setStateByKey(const QString &key, int state)
{
    for (int row = 0; row < m_items.size(); ++row) {
        ControlItem &item = m_items[row];
        if (item.key == key) {
            return updateState(item, row, state);
        }
    }
    return false;
}

bool ControlModel::setStateByIndex(int row, int state)
{
    if (row < 0 || row >= m_items.size()) {
        return false;
    }
    return updateState(m_items[row], row, state);
}

QString ControlModel::keyAt(int row) const
{
    if (row < 0 || row >= m_items.size()) {
        return {};
    }
    return m_items.at(row).key;
}

int ControlModel::stateAt(int row) const
{
    if (row < 0 || row >= m_items.size()) {
        return 0;
    }
    return m_items.at(row).state;
}

QString ControlModel::activeAdvice() const
{
    auto it = std::find_if(m_items.cbegin(), m_items.cend(), [](const ControlItem &item) {
        return item.state != 0;
    });
    return it == m_items.cend() ? QString() : it->activeAdvice;
}

QString ControlModel::stateLabelAt(int row) const
{
    if (row < 0 || row >= m_items.size()) {
        return {};
    }
    const ControlItem &item = m_items.at(row);
    return item.name + QStringLiteral("：") + item.stateTexts.value(item.state);
}

bool ControlModel::updateState(ControlItem &item, int row, int state)
{
    if (item.stateTexts.isEmpty()) {
        return false;
    }

    if (state < 0 || state >= item.stateTexts.size()) {
        state = 0;
    }

    if (item.state == state) {
        return false;
    }

    item.state = state;
    const QModelIndex modelIndex = index(row);
    emit dataChanged(modelIndex, modelIndex, {StateRole, StateTextRole});
    return true;
}
