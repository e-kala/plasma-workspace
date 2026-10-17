/*
    SPDX-FileCopyrightText: 2019 Konrad Materka <materka@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QPointer>
#include <QSortFilterProxyModel>

class SystemTraySettings;

class SortedSystemTrayModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit SortedSystemTrayModel(QPointer<SystemTraySettings> settings, QObject *parent = nullptr);

    static int compare(const QModelIndex &left, const QModelIndex &right, const QStringList &order);
    static int compareByCategory(const QModelIndex &left, const QModelIndex &right);

    static QString category(const QModelIndex &index);

protected:
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;

private:
    static int orderIndex(const QModelIndex &index, const QStringList &order);

    QPointer<SystemTraySettings> m_settings;
};
