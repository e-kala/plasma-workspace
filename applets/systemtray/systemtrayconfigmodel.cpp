/*
    SPDX-FileCopyrightText: 2026 Ramil Nurmanov <ramil2004nur@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "systemtrayconfigmodel.h"
#include "sortedsystemtraymodel.h"
#include "systemtraymodel.h"

#include <algorithm>

static constexpr int s_itemIdRole = static_cast<int>(BaseModel::BaseRole::ItemId);
static constexpr int s_categoryRole = static_cast<int>(BaseModel::BaseRole::Category);

static bool affectsRows(const QList<int> &roles)
{
    return roles.isEmpty() || roles.contains(Qt::DisplayRole) || roles.contains(s_itemIdRole) || roles.contains(s_categoryRole);
}

SystemTrayConfigModel::SystemTrayConfigModel(QAbstractItemModel *sourceModel, QObject *parent)
    : QAbstractListModel(parent)
    , m_sourceModel(sourceModel)
{
    connect(sourceModel, &QAbstractItemModel::rowsInserted, this, [this]() {
        update();
    });
    connect(sourceModel, &QAbstractItemModel::rowsAboutToBeRemoved, this, [this](const QModelIndex &sourceParent, int first, int last) {
        if (!sourceParent.isValid()) {
            update(first, last);
        }
    });
    connect(sourceModel, &QAbstractItemModel::rowsRemoved, this, [this]() {
        update();
    });
    connect(sourceModel, &QAbstractItemModel::rowsMoved, this, [this]() {
        update();
    });
    connect(sourceModel, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles) {
        for (int sourceRow = topLeft.row(); sourceRow <= bottomRight.row(); ++sourceRow) {
            const int row = m_rows.indexOf(m_sourceModel->index(sourceRow, 0));
            if (row != -1) {
                Q_EMIT dataChanged(index(row, 0), index(row, 0), roles);
            }
        }
        if (affectsRows(roles)) {
            update();
        }
    });
    connect(sourceModel, &QAbstractItemModel::layoutChanged, this, [this]() {
        update();
    });
    connect(sourceModel, &QAbstractItemModel::modelAboutToBeReset, this, [this]() {
        beginResetModel();
        m_rows.clear();
    });
    connect(sourceModel, &QAbstractItemModel::modelReset, this, [this]() {
        m_rows = buildRows();
        endResetModel();
    });

    m_rows = buildRows();
}

QVariant SystemTrayConfigModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }

    const QPersistentModelIndex &sourceIndex = m_rows.at(index.row());
    if (role == s_categoryRole) {
        return SortedSystemTrayModel::category(sourceIndex);
    }
    return sourceIndex.data(role);
}

int SystemTrayConfigModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QHash<int, QByteArray> SystemTrayConfigModel::roleNames() const
{
    return m_sourceModel ? m_sourceModel->roleNames() : QAbstractListModel::roleNames();
}

QStringList SystemTrayConfigModel::order() const
{
    return m_order;
}

void SystemTrayConfigModel::setOrder(const QStringList &order)
{
    if (order != m_order) {
        setOrderInternal(order);
    }
}

bool SystemTrayConfigModel::isGrouped() const
{
    return m_order.isEmpty();
}

QString SystemTrayConfigModel::filterText() const
{
    return m_filterText;
}

void SystemTrayConfigModel::setFilterText(const QString &filterText)
{
    if (filterText == m_filterText) {
        return;
    }
    m_filterText = filterText;
    update();
    Q_EMIT filterTextChanged();
}

void SystemTrayConfigModel::setGrouped(bool grouped)
{
    if (grouped == isGrouped()) {
        return;
    }

    if (grouped) {
        setOrderInternal({});
        return;
    }

    QList<QPersistentModelIndex> entries = sortedEntries();
    std::stable_sort(entries.begin(), entries.end(), [](const QPersistentModelIndex &left, const QPersistentModelIndex &right) {
        return SortedSystemTrayModel::compare(left, right, QStringList()) < 0;
    });

    QStringList order;
    for (const QPersistentModelIndex &entry : std::as_const(entries)) {
        order.append(entry.data(s_itemIdRole).toString());
    }
    setOrderInternal(order);
}

void SystemTrayConfigModel::move(int from, int to)
{
    if (isGrouped() || !m_filterText.isEmpty() || from == to || from < 0 || to < 0 || from >= m_rows.size() || to >= m_rows.size()) {
        return;
    }

    const QString itemId = m_rows.at(from).data(s_itemIdRole).toString();
    const auto hasSameId = [this, &itemId](int row) {
        return m_rows.at(row).data(s_itemIdRole).toString() == itemId;
    };
    if (to < from) {
        for (int row = from - 1; row >= to; --row) {
            if (hasSameId(row)) {
                to = row + 1;
                break;
            }
        }
    } else {
        for (int row = from + 1; row <= to; ++row) {
            if (hasSameId(row)) {
                to = row - 1;
                break;
            }
        }
    }
    if (to == from) {
        return;
    }

    beginMoveRows(QModelIndex(), from, from, QModelIndex(), to > from ? to + 1 : to);
    m_rows.move(from, to);
    endMoveRows();

    setOrderInternal(mergedOrder());
}

QList<QPersistentModelIndex> SystemTrayConfigModel::sortedEntries(int excludeFirst, int excludeLast) const
{
    QList<QPersistentModelIndex> entries;
    if (!m_sourceModel) {
        return entries;
    }

    for (int sourceRow = 0; sourceRow < m_sourceModel->rowCount(); ++sourceRow) {
        if (sourceRow >= excludeFirst && sourceRow <= excludeLast) {
            continue;
        }
        const QModelIndex sourceIndex = m_sourceModel->index(sourceRow, 0);
        if (!sourceIndex.data(s_itemIdRole).toString().isEmpty()) {
            entries.append(sourceIndex);
        }
    }

    std::stable_sort(entries.begin(), entries.end(), [this](const QPersistentModelIndex &left, const QPersistentModelIndex &right) {
        if (isGrouped()) {
            return SortedSystemTrayModel::compareByCategory(left, right) < 0;
        }
        return SortedSystemTrayModel::compare(left, right, m_order) < 0;
    });

    return entries;
}

QList<QPersistentModelIndex> SystemTrayConfigModel::buildRows(int excludeFirst, int excludeLast) const
{
    QList<QPersistentModelIndex> rows = sortedEntries(excludeFirst, excludeLast);
    if (!m_filterText.isEmpty()) {
        rows.removeIf([this](const QPersistentModelIndex &entry) {
            return !entry.data(Qt::DisplayRole).toString().contains(m_filterText, Qt::CaseInsensitive);
        });
    }
    return rows;
}

void SystemTrayConfigModel::update(int excludeFirst, int excludeLast)
{
    applyRows(buildRows(excludeFirst, excludeLast));
}

void SystemTrayConfigModel::applyRows(const QList<QPersistentModelIndex> &rows)
{
    for (int row = m_rows.size() - 1; row >= 0; --row) {
        if (!rows.contains(m_rows.at(row))) {
            beginRemoveRows(QModelIndex(), row, row);
            m_rows.removeAt(row);
            endRemoveRows();
        }
    }

    QList<QPersistentModelIndex> kept;
    for (const QPersistentModelIndex &row : rows) {
        if (m_rows.contains(row)) {
            kept.append(row);
        }
    }
    for (int target = 0; target < kept.size(); ++target) {
        const int current = m_rows.indexOf(kept.at(target), target);
        if (current != target) {
            beginMoveRows(QModelIndex(), current, current, QModelIndex(), target);
            m_rows.move(current, target);
            endMoveRows();
        }
    }

    for (int row = 0; row < rows.size(); ++row) {
        if (row >= m_rows.size() || m_rows.at(row) != rows.at(row)) {
            beginInsertRows(QModelIndex(), row, row);
            m_rows.insert(row, rows.at(row));
            endInsertRows();
        }
    }
}

QStringList SystemTrayConfigModel::mergedOrder() const
{
    QStringList shown;
    for (const QPersistentModelIndex &row : m_rows) {
        shown.append(row.data(s_itemIdRole).toString());
    }

    QHash<QString, int> remaining;
    for (const QString &itemId : std::as_const(shown)) {
        ++remaining[itemId];
    }

    QStringList order;
    auto next = shown.cbegin();
    for (const QString &itemId : m_order) {
        if (remaining.value(itemId) > 0) {
            --remaining[itemId];
            order.append(*next++);
        } else {
            order.append(itemId);
        }
    }
    while (next != shown.cend()) {
        order.append(*next++);
    }

    return order;
}

void SystemTrayConfigModel::setOrderInternal(const QStringList &order)
{
    m_order = order;
    update();
    Q_EMIT orderChanged();
}

#include "moc_systemtrayconfigmodel.cpp"
