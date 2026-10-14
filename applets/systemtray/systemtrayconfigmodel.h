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
    Q_PROPERTY(bool grouped READ isGrouped NOTIFY orderChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    enum class Role {
        IsHeader = Qt::UserRole + 2000,
    };

    explicit SystemTrayConfigModel(QAbstractItemModel *sourceModel, QObject *parent = nullptr);

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;

    QStringList order() const;
    void setOrder(const QStringList &order);
    bool isGrouped() const;

    QString filterText() const;
    void setFilterText(const QString &filterText);

    Q_INVOKABLE void setGrouped(bool grouped);

    Q_INVOKABLE void move(int from, int to);

Q_SIGNALS:
    void orderChanged();
    void filterTextChanged();

private:
    struct Row {
        QPersistentModelIndex sourceIndex;
        QString category;

        bool isHeader() const
        {
            return !category.isEmpty();
        }
        bool operator==(const Row &other) const
        {
            return isHeader() ? category == other.category : sourceIndex == other.sourceIndex;
        }
    };

    QList<QPersistentModelIndex> sortedEntries(int excludeFirst = -1, int excludeLast = -1) const;
    QList<Row> buildRows(int excludeFirst = -1, int excludeLast = -1) const;
    void update(int excludeFirst = -1, int excludeLast = -1);
    void applyRows(const QList<Row> &rows);
    QStringList mergedOrder() const;
    void setOrderInternal(const QStringList &order);
    int rowForSourceIndex(const QModelIndex &sourceIndex) const;

    QPointer<QAbstractItemModel> m_sourceModel;
    QList<Row> m_rows;
    QStringList m_order;
    QString m_filterText;
};
