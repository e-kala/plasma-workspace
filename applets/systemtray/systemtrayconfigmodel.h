/*
    SPDX-FileCopyrightText: 2026 Ramil Nurmanov <ramil2004nur@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QAbstractListModel>
#include <QPersistentModelIndex>
#include <QPointer>

class SystemTrayConfigModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QStringList order READ order WRITE setOrder NOTIFY orderChanged)
    Q_PROPERTY(bool grouped READ isGrouped WRITE setGrouped NOTIFY groupedChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    explicit SystemTrayConfigModel(QAbstractItemModel *sourceModel, QObject *parent = nullptr);

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;

    QStringList order() const;
    void setOrder(const QStringList &order);
    bool isGrouped() const;
    void setGrouped(bool grouped);

    QString filterText() const;
    void setFilterText(const QString &filterText);

    Q_INVOKABLE void move(int from, int to);

Q_SIGNALS:
    void orderChanged();
    void groupedChanged();
    void filterTextChanged();

private:
    QList<QPersistentModelIndex> sortedEntries(int excludeFirst = -1, int excludeLast = -1) const;
    QList<QPersistentModelIndex> buildRows(int excludeFirst = -1, int excludeLast = -1) const;
    void update(int excludeFirst = -1, int excludeLast = -1);
    void applyRows(const QList<QPersistentModelIndex> &rows);
    QStringList mergedOrder() const;
    void setOrderInternal(const QStringList &order);

    QPointer<QAbstractItemModel> m_sourceModel;
    QList<QPersistentModelIndex> m_rows;
    QStringList m_order;
    bool m_grouped = true;
    QString m_filterText;
};
