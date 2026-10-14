/*
    SPDX-FileCopyrightText: 2026 Ramil Nurmanov <ramil2004nur@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <QAbstractListModel>

#include "../systemtraymodel.h"

class MockTrayModel : public QAbstractListModel
{
public:
    struct Item {
        QString itemId;
        QString category;
        QString name;
    };

    explicit MockTrayModel(const QList<Item> &items = {})
        : m_items(items)
    {
    }

    void append(const Item &item)
    {
        beginInsertRows(QModelIndex(), m_items.size(), m_items.size());
        m_items.append(item);
        endInsertRows();
    }

    void remove(int row)
    {
        beginRemoveRows(QModelIndex(), row, row);
        m_items.removeAt(row);
        endRemoveRows();
    }

    void move(int from, int to)
    {
        beginMoveRows(QModelIndex(), from, from, QModelIndex(), to > from ? to + 1 : to);
        m_items.move(from, to);
        endMoveRows();
    }

    void setName(int row, const QString &name, const QList<int> &roles = {Qt::DisplayRole})
    {
        m_items[row].name = name;
        Q_EMIT dataChanged(index(row, 0), index(row, 0), roles);
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : m_items.size();
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!checkIndex(index, CheckIndexOption::IndexIsValid)) {
            return {};
        }
        const Item &item = m_items.at(index.row());
        switch (role) {
        case Qt::DisplayRole:
            return item.name.isEmpty() ? item.itemId : item.name;
        case static_cast<int>(BaseModel::BaseRole::ItemId):
            return item.itemId;
        case static_cast<int>(BaseModel::BaseRole::Category):
            return item.category;
        default:
            return {};
        }
    }

    QHash<int, QByteArray> roleNames() const override
    {
        return {
            {Qt::DisplayRole, QByteArrayLiteral("displayText")},
            {static_cast<int>(BaseModel::BaseRole::ItemId), QByteArrayLiteral("itemId")},
            {static_cast<int>(BaseModel::BaseRole::Category), QByteArrayLiteral("category")},
        };
    }

private:
    QList<Item> m_items;
};
