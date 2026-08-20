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

    QVariant itemIdLeft = left.data(static_cast<int>(BaseModel::BaseRole::ItemId));
    QVariant itemIdRight = right.data(static_cast<int>(BaseModel::BaseRole::ItemId));
    if (itemIdLeft.toString() == QLatin1String("org.kde.plasma.notifications")) {
        return false;
    } else if (itemIdRight.toString() == QLatin1String("org.kde.plasma.notifications")) {
        return true;
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

    const QString itemIdLeft = left.data(static_cast<int>(BaseModel::BaseRole::ItemId)).toString();
    const QString itemIdRight = right.data(static_cast<int>(BaseModel::BaseRole::ItemId)).toString();

    int leftIndex = order.indexOf(itemIdLeft);
    int rightIndex = order.indexOf(itemIdRight);

    if (leftIndex == -1 && rightIndex == -1) {
        return std::nullopt;
    }
    if (leftIndex == -1) {
        leftIndex = order.size();
    }
    if (rightIndex == -1) {
        rightIndex = order.size();
    }

    return leftIndex - rightIndex;
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
