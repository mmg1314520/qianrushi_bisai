#pragma once

#include <QAbstractListModel>
#include <QColor>
#include <QStringList>
#include <QVector>

class ControlModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        KeyRole,
        NameRole,
        StateRole,
        StateTextRole,
        StateCountRole,
        AccentRole,
        EnabledRole,
    };

    explicit ControlModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool setAutoMode(bool autoMode);
    bool cycleState(int row);
    bool setStateByKey(const QString &key, int state);
    bool setStateByIndex(int row, int state);
    QString keyAt(int row) const;
    int stateAt(int row) const;
    QString activeAdvice() const;
    QString stateLabelAt(int row) const;

private:
    struct ControlItem {
        int id = 0;
        QString key;
        QString name;
        QString activeAdvice;
        QColor accent;
        QStringList stateTexts;
        int state = 0;
        bool enabled = false;
    };

    bool updateState(ControlItem &item, int row, int state);

    QVector<ControlItem> m_items;
};
