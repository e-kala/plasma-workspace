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

SortedSystemTrayModel::SortedSystemTrayModel(SortingType sorting, QObject *parent, QPointer<SystemTraySettings> settings)
    : QSortFilterProxyModel(parent)
    , m_sorting(sorting)
    , m_settings(settings)
{
    setSortLocaleAware(true);
    sort(0);

    if (m_settings) {
        connect(m_settings.data(), &SystemTraySettings::itemOrderChanged, this, [this]() {
            invalidate();
        });
    }
}

bool SortedSystemTrayModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    switch (m_sorting) {
    case SortedSystemTrayModel::SortingType::ConfigurationPage:
        return lessThanConfigurationPage(left, right);
    case SortedSystemTrayModel::SortingType::SystemTray:
        return lessThanSystemTray(left, right);
    }

    return QSortFilterProxyModel::lessThan(left, right);
}

bool SortedSystemTrayModel::lessThanConfigurationPage(const QModelIndex &left, const QModelIndex &right) const
{
    if (const auto userOrderComparison = compareUserDefinedOrder(left, right)) {
        return *userOrderComparison < 0;
    }

    const int categoriesComparison = compareCategoriesAlphabetically(left, right);
    if (categoriesComparison == 0) {
        return QSortFilterProxyModel::lessThan(left, right);
    } else {
        return categoriesComparison < 0;
    }
}

bool SortedSystemTrayModel::lessThanSystemTray(const QModelIndex &left, const QModelIndex &right) const
{
    if (const auto userOrderComparison = compareUserDefinedOrder(left, right)) {
        return *userOrderComparison < 0;
    }

    const int categoriesComparison = compareCategoriesOrderly(left, right);
    if (categoriesComparison == 0) {
        return QSortFilterProxyModel::lessThan(left, right);
    } else {
        return categoriesComparison < 0;
    }
}

std::optional<int> SortedSystemTrayModel::compareUserDefinedOrder(const QModelIndex &left, const QModelIndex &right) const
{
    if (!m_settings) {
        return std::nullopt;
    }

    const QStringList order = m_settings->itemOrder();
    if (order.isEmpty()) {
        return std::nullopt;
    }

    const int leftIndex = orderIndexForRow(left, order);
    const int rightIndex = orderIndexForRow(right, order);

    if (leftIndex == -1 || rightIndex == -1) {
        return std::nullopt;
    }

    return leftIndex - rightIndex;
}

int SortedSystemTrayModel::orderIndexForRow(const QModelIndex &index, const QStringList &order) const
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
    int searchFrom = 0;
    for (int seen = 0; seen <= occurrence; ++seen) {
        orderIndex = order.indexOf(itemId, searchFrom);
        if (orderIndex == -1) {
            break;
        }
        searchFrom = orderIndex + 1;
    }

    return orderIndex;
}

int SortedSystemTrayModel::compareCategoriesAlphabetically(const QModelIndex &left, const QModelIndex &right) const
{
    QVariant leftData = left.data(static_cast<int>(BaseModel::BaseRole::Category));
    QString leftCategory = leftData.isNull() ? QStringLiteral("UnknownCategory") : leftData.toString();

    QVariant rightData = right.data(static_cast<int>(BaseModel::BaseRole::Category));
    QString rightCategory = rightData.isNull() ? QStringLiteral("UnknownCategory") : rightData.toString();

    return QString::localeAwareCompare(leftCategory, rightCategory);
}

int SortedSystemTrayModel::compareCategoriesOrderly(const QModelIndex &left, const QModelIndex &right) const
{
    QVariant leftData = left.data(static_cast<int>(BaseModel::BaseRole::Category));
    QString leftCategory = leftData.isNull() ? QStringLiteral("UnknownCategory") : leftData.toString();

    QVariant rightData = right.data(static_cast<int>(BaseModel::BaseRole::Category));
    QString rightCategory = rightData.isNull() ? QStringLiteral("UnknownCategory") : rightData.toString();

    int leftIndex = s_categoryOrder.indexOf(leftCategory);
    if (leftIndex == -1) {
        leftIndex = s_categoryOrder.indexOf(QStringLiteral("UnknownCategory"));
    }

    int rightIndex = s_categoryOrder.indexOf(rightCategory);
    if (rightIndex == -1) {
        rightIndex = s_categoryOrder.indexOf(QStringLiteral("UnknownCategory"));
    }

    return leftIndex - rightIndex;
}

#include "moc_sortedsystemtraymodel.cpp"
