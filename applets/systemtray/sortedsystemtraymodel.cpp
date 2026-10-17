/*
    SPDX-FileCopyrightText: 2019 Konrad Materka <materka@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sortedsystemtraymodel.h"
#include "debug.h"
#include "systemtraymodel.h"
#include "systemtraysettings.h"

#include <QList>

static const QList<QString> s_categoryOrder = {
    QStringLiteral("UnknownCategory"),
    QStringLiteral("ApplicationStatus"),
    QStringLiteral("Communications"),
    QStringLiteral("SystemServices"),
    QStringLiteral("Hardware"),
};

SortedSystemTrayModel::SortedSystemTrayModel(QPointer<SystemTraySettings> settings, QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_settings(settings)
{
    setSortLocaleAware(true);
    sort(0);

    if (m_settings) {
        connect(m_settings.data(), &SystemTraySettings::itemOrderChanged, this, &SortedSystemTrayModel::invalidate);
    }
}

bool SortedSystemTrayModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    return compare(left, right, m_settings ? m_settings->itemOrder() : QStringList()) < 0;
}

int SortedSystemTrayModel::compare(const QModelIndex &left, const QModelIndex &right, const QStringList &order)
{
    if (!order.isEmpty()) {
        const int leftIndex = orderIndex(left, order);
        const int rightIndex = orderIndex(right, order);
        if (leftIndex != -1 && rightIndex != -1) {
            return leftIndex - rightIndex;
        }
        if (leftIndex != -1 || rightIndex != -1) {
            return leftIndex != -1 ? -1 : 1;
        }
    } else {
        const bool leftIsNotifications = left.data(static_cast<int>(BaseModel::BaseRole::ItemId)).toString() == QLatin1String("org.kde.plasma.notifications");
        const bool rightIsNotifications = right.data(static_cast<int>(BaseModel::BaseRole::ItemId)).toString() == QLatin1String("org.kde.plasma.notifications");
        if (leftIsNotifications != rightIsNotifications) {
            return leftIsNotifications ? -1 : 1;
        }
    }

    return compareByCategory(left, right);
}

int SortedSystemTrayModel::compareByCategory(const QModelIndex &left, const QModelIndex &right)
{
    const int categoriesComparison = s_categoryOrder.indexOf(category(left)) - s_categoryOrder.indexOf(category(right));
    if (categoriesComparison != 0) {
        return categoriesComparison;
    }

    return QString::localeAwareCompare(left.data(Qt::DisplayRole).toString(), right.data(Qt::DisplayRole).toString());
}

QString SortedSystemTrayModel::category(const QModelIndex &index)
{
    const QString category = index.data(static_cast<int>(BaseModel::BaseRole::Category)).toString();
    return s_categoryOrder.contains(category) ? category : QStringLiteral("UnknownCategory");
}

int SortedSystemTrayModel::orderIndex(const QModelIndex &index, const QStringList &order)
{
    const QString itemId = index.data(static_cast<int>(BaseModel::BaseRole::ItemId)).toString();
    if (itemId.isEmpty()) {
        return -1;
    }

    int occurrence = 0;
    for (int row = 0; row < index.row(); ++row) {
        if (index.model()->index(row, 0, index.parent()).data(static_cast<int>(BaseModel::BaseRole::ItemId)).toString() == itemId) {
            ++occurrence;
        }
    }

    int orderIndex = -1;
    for (int seen = 0; seen <= occurrence; ++seen) {
        orderIndex = order.indexOf(itemId, orderIndex + 1);
        if (orderIndex == -1) {
            break;
        }
    }

    return orderIndex;
}

#include "moc_sortedsystemtraymodel.cpp"
