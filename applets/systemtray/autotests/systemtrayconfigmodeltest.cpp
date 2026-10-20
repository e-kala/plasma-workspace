/*
    SPDX-FileCopyrightText: 2026 Ramil Nurmanov <ramil2004nur@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTest>

#include "../systemtrayconfigmodel.h"
#include "../systemtraymodel.h"
#include "mocktraymodel.h"

using namespace Qt::StringLiterals;

class SystemTrayConfigModelTest : public QObject
{
    Q_OBJECT
private:
    static QStringList rows(const SystemTrayConfigModel &model);
    static QStringList names(const QAbstractItemModel &model);
    static QList<MockTrayModel::Item> items();

private Q_SLOTS:
    void testGroupedByCategory();
    void testUnknownCategoriesShareSection();
    void testManualOrder();
    void testSwitchingOrderMode();
    void testSwitchingOrderModeKeepsNotificationsFirst();
    void testMove();
    void testMoveKeepsPositionOfAbsentItems();
    void testMoveIgnoredWhenGroupedOrFiltered();
    void testFilter();
    void testSourceChangesWhileGrouped();
    void testSourceChangesWithManualOrder();
    void testSourceRowsMoved();
    void testDataChangedWithoutOrderRoles();
    void testMoveWithDuplicateItemIds();
};

QStringList SystemTrayConfigModelTest::rows(const SystemTrayConfigModel &model)
{
    QStringList rows;
    QString lastCategory;
    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row, 0);
        const QString category = index.data(static_cast<int>(BaseModel::BaseRole::Category)).toString();
        if (model.isGrouped() && category != lastCategory) {
            rows.append(u'#' + category);
            lastCategory = category;
        }
        rows.append(index.data(static_cast<int>(BaseModel::BaseRole::ItemId)).toString());
    }
    return rows;
}

QStringList SystemTrayConfigModelTest::names(const QAbstractItemModel &model)
{
    QStringList names;
    for (int row = 0; row < model.rowCount(); ++row) {
        names.append(model.index(row, 0).data(Qt::DisplayRole).toString());
    }
    return names;
}

QList<MockTrayModel::Item> SystemTrayConfigModelTest::items()
{
    return {
        {u"hotel"_s, u"Hardware"_s},
        {u"bravo"_s, u"ApplicationStatus"_s},
        {u"alpha"_s, u"ApplicationStatus"_s},
    };
}

void SystemTrayConfigModelTest::testGroupedByCategory()
{
    MockTrayModel source(items());
    SystemTrayConfigModel model(&source);
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);

    QVERIFY(model.isGrouped());
    QCOMPARE(rows(model), (QStringList{u"#ApplicationStatus"_s, u"alpha"_s, u"bravo"_s, u"#Hardware"_s, u"hotel"_s}));
    QCOMPARE(model.rowCount(), 3);
}

void SystemTrayConfigModelTest::testUnknownCategoriesShareSection()
{
    MockTrayModel source({
        {u"alpha"_s, u"Foo"_s},
        {u"bravo"_s, QString()},
        {u"charlie"_s, u"UnknownCategory"_s},
    });
    SystemTrayConfigModel model(&source);
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);

    QCOMPARE(rows(model), (QStringList{u"#UnknownCategory"_s, u"alpha"_s, u"bravo"_s, u"charlie"_s}));
}

void SystemTrayConfigModelTest::testManualOrder()
{
    MockTrayModel source(items());
    SystemTrayConfigModel model(&source);
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    model.setOrder({u"hotel"_s, u"alpha"_s, u"bravo"_s});
    model.setGrouped(false);

    QVERIFY(!model.isGrouped());
    QCOMPARE(rows(model), (QStringList{u"hotel"_s, u"alpha"_s, u"bravo"_s}));
    QCOMPARE(resetSpy.count(), 0);
}

void SystemTrayConfigModelTest::testSwitchingOrderMode()
{
    MockTrayModel source(items());
    SystemTrayConfigModel model(&source);
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QSignalSpy orderSpy(&model, &SystemTrayConfigModel::orderChanged);
    QSignalSpy groupedSpy(&model, &SystemTrayConfigModel::groupedChanged);
    QSignalSpy layoutSpy(&model, &QAbstractItemModel::layoutChanged);
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    model.setGrouped(false);
    QCOMPARE(groupedSpy.count(), 1);
    QCOMPARE(orderSpy.count(), 0);
    QVERIFY(model.order().isEmpty());
    QCOMPARE(rows(model), (QStringList{u"alpha"_s, u"bravo"_s, u"hotel"_s}));

    model.setGrouped(false);
    QCOMPARE(groupedSpy.count(), 1);

    model.move(0, 2);
    QCOMPARE(rows(model), (QStringList{u"bravo"_s, u"hotel"_s, u"alpha"_s}));
    QCOMPARE(model.order(), (QStringList{u"bravo"_s, u"hotel"_s, u"alpha"_s}));
    QCOMPARE(orderSpy.count(), 1);
    QSignalSpy moveSpy(&model, &QAbstractItemModel::rowsMoved);

    model.setGrouped(true);
    QCOMPARE(groupedSpy.count(), 2);
    QCOMPARE(rows(model), (QStringList{u"#ApplicationStatus"_s, u"alpha"_s, u"bravo"_s, u"#Hardware"_s, u"hotel"_s}));
    QVERIFY(moveSpy.count() > 0);

    model.setGrouped(false);
    QCOMPARE(groupedSpy.count(), 3);
    QCOMPARE(orderSpy.count(), 1);
    QCOMPARE(model.order(), (QStringList{u"bravo"_s, u"hotel"_s, u"alpha"_s}));
    QCOMPARE(rows(model), (QStringList{u"bravo"_s, u"hotel"_s, u"alpha"_s}));
    QCOMPARE(layoutSpy.count(), 0);
    QCOMPARE(resetSpy.count(), 0);
}

void SystemTrayConfigModelTest::testSwitchingOrderModeKeepsNotificationsFirst()
{
    QList<MockTrayModel::Item> trayItems = items();
    trayItems.append({u"org.kde.plasma.notifications"_s, u"SystemServices"_s});
    MockTrayModel source(trayItems);
    SystemTrayConfigModel model(&source);
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);

    QCOMPARE(rows(model),
             (QStringList{u"#ApplicationStatus"_s,
                          u"alpha"_s,
                          u"bravo"_s,
                          u"#SystemServices"_s,
                          u"org.kde.plasma.notifications"_s,
                          u"#Hardware"_s,
                          u"hotel"_s}));

    model.setGrouped(false);
    QCOMPARE(rows(model), (QStringList{u"org.kde.plasma.notifications"_s, u"alpha"_s, u"bravo"_s, u"hotel"_s}));

    model.move(3, 2);
    QCOMPARE(model.order(), (QStringList{u"org.kde.plasma.notifications"_s, u"alpha"_s, u"hotel"_s, u"bravo"_s}));
}

void SystemTrayConfigModelTest::testMove()
{
    MockTrayModel source(items());
    SystemTrayConfigModel model(&source);
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    model.setOrder({u"alpha"_s, u"bravo"_s, u"hotel"_s});
    model.setGrouped(false);

    QSignalSpy moveSpy(&model, &QAbstractItemModel::rowsMoved);
    QSignalSpy layoutSpy(&model, &QAbstractItemModel::layoutChanged);
    QSignalSpy orderSpy(&model, &SystemTrayConfigModel::orderChanged);

    model.move(0, 2);
    QCOMPARE(rows(model), (QStringList{u"bravo"_s, u"hotel"_s, u"alpha"_s}));
    QCOMPARE(model.order(), (QStringList{u"bravo"_s, u"hotel"_s, u"alpha"_s}));

    model.move(2, 1);
    QCOMPARE(rows(model), (QStringList{u"bravo"_s, u"alpha"_s, u"hotel"_s}));
    QCOMPARE(model.order(), (QStringList{u"bravo"_s, u"alpha"_s, u"hotel"_s}));

    QCOMPARE(moveSpy.count(), 2);
    QCOMPARE(layoutSpy.count(), 0);
    QCOMPARE(orderSpy.count(), 2);
}

void SystemTrayConfigModelTest::testMoveKeepsPositionOfAbsentItems()
{
    MockTrayModel source(items());
    SystemTrayConfigModel model(&source);
    model.setOrder({u"xray"_s, u"alpha"_s, u"yankee"_s, u"bravo"_s, u"hotel"_s});
    model.setGrouped(false);
    QCOMPARE(rows(model), (QStringList{u"alpha"_s, u"bravo"_s, u"hotel"_s}));

    model.move(2, 0);

    QCOMPARE(rows(model), (QStringList{u"hotel"_s, u"alpha"_s, u"bravo"_s}));
    QCOMPARE(model.order(), (QStringList{u"xray"_s, u"hotel"_s, u"yankee"_s, u"alpha"_s, u"bravo"_s}));
}

void SystemTrayConfigModelTest::testMoveIgnoredWhenGroupedOrFiltered()
{
    MockTrayModel source(items());
    SystemTrayConfigModel model(&source);
    QSignalSpy orderSpy(&model, &SystemTrayConfigModel::orderChanged);

    model.move(1, 2);
    QCOMPARE(orderSpy.count(), 0);

    model.setOrder({u"alpha"_s, u"bravo"_s, u"hotel"_s});
    model.setGrouped(false);
    model.setFilterText(u"o"_s);
    QCOMPARE(rows(model), (QStringList{u"bravo"_s, u"hotel"_s}));
    model.move(0, 1);
    QCOMPARE(rows(model), (QStringList{u"bravo"_s, u"hotel"_s}));
    QCOMPARE(orderSpy.count(), 1);
}

void SystemTrayConfigModelTest::testFilter()
{
    MockTrayModel source(items());
    SystemTrayConfigModel model(&source);
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);

    model.setFilterText(u"HOT"_s);
    QCOMPARE(rows(model), (QStringList{u"#Hardware"_s, u"hotel"_s}));

    model.setFilterText(u"a"_s);
    QCOMPARE(rows(model), (QStringList{u"#ApplicationStatus"_s, u"alpha"_s, u"bravo"_s}));

    model.setFilterText(QString());
    QCOMPARE(rows(model), (QStringList{u"#ApplicationStatus"_s, u"alpha"_s, u"bravo"_s, u"#Hardware"_s, u"hotel"_s}));
}

void SystemTrayConfigModelTest::testSourceChangesWhileGrouped()
{
    MockTrayModel source(items());
    SystemTrayConfigModel model(&source);
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QSignalSpy layoutSpy(&model, &QAbstractItemModel::layoutChanged);
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    source.append({u"charlie"_s, u"Communications"_s});
    QCOMPARE(rows(model), (QStringList{u"#ApplicationStatus"_s, u"alpha"_s, u"bravo"_s, u"#Communications"_s, u"charlie"_s, u"#Hardware"_s, u"hotel"_s}));

    source.setName(1, u"zulu"_s);
    QCOMPARE(rows(model), (QStringList{u"#ApplicationStatus"_s, u"alpha"_s, u"bravo"_s, u"#Communications"_s, u"charlie"_s, u"#Hardware"_s, u"hotel"_s}));
    source.setName(2, u"zulu zulu"_s);
    QCOMPARE(rows(model), (QStringList{u"#ApplicationStatus"_s, u"bravo"_s, u"alpha"_s, u"#Communications"_s, u"charlie"_s, u"#Hardware"_s, u"hotel"_s}));

    source.remove(0);
    QCOMPARE(rows(model), (QStringList{u"#ApplicationStatus"_s, u"bravo"_s, u"alpha"_s, u"#Communications"_s, u"charlie"_s}));

    QCOMPARE(layoutSpy.count(), 0);
    QCOMPARE(resetSpy.count(), 0);
}

void SystemTrayConfigModelTest::testSourceChangesWithManualOrder()
{
    MockTrayModel source(items());
    SystemTrayConfigModel model(&source);
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    model.setOrder({u"bravo"_s, u"hotel"_s, u"alpha"_s});
    model.setGrouped(false);
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy orderSpy(&model, &SystemTrayConfigModel::orderChanged);

    source.append({u"charlie"_s, u"UnknownCategory"_s});
    QCOMPARE(rows(model), (QStringList{u"bravo"_s, u"hotel"_s, u"alpha"_s, u"charlie"_s}));

    source.remove(0);
    QCOMPARE(rows(model), (QStringList{u"bravo"_s, u"alpha"_s, u"charlie"_s}));

    QCOMPARE(model.order(), (QStringList{u"bravo"_s, u"hotel"_s, u"alpha"_s}));
    QCOMPARE(orderSpy.count(), 0);
    QCOMPARE(resetSpy.count(), 0);
}

void SystemTrayConfigModelTest::testSourceRowsMoved()
{
    MockTrayModel source({
        {u"first"_s, u"ApplicationStatus"_s, u"Same name"_s},
        {u"second"_s, u"ApplicationStatus"_s, u"Same name"_s},
    });
    SystemTrayConfigModel model(&source);
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QCOMPARE(rows(model), (QStringList{u"#ApplicationStatus"_s, u"first"_s, u"second"_s}));

    source.move(1, 0);

    QCOMPARE(rows(model), (QStringList{u"#ApplicationStatus"_s, u"second"_s, u"first"_s}));
}

void SystemTrayConfigModelTest::testDataChangedWithoutOrderRoles()
{
    MockTrayModel source(items());
    SystemTrayConfigModel model(&source);
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QSignalSpy dataSpy(&model, &QAbstractItemModel::dataChanged);
    QSignalSpy moveSpy(&model, &QAbstractItemModel::rowsMoved);

    source.setName(1, u"aaa"_s, {Qt::DecorationRole});
    QCOMPARE(dataSpy.count(), 1);
    QCOMPARE(dataSpy.at(0).at(0).toModelIndex(), model.index(1, 0));
    QCOMPARE(moveSpy.count(), 0);
    QCOMPARE(rows(model), (QStringList{u"#ApplicationStatus"_s, u"alpha"_s, u"bravo"_s, u"#Hardware"_s, u"hotel"_s}));

    source.setName(1, u"aaa"_s, {});
    QCOMPARE(dataSpy.count(), 2);
    QCOMPARE(moveSpy.count(), 1);
    QCOMPARE(rows(model), (QStringList{u"#ApplicationStatus"_s, u"bravo"_s, u"alpha"_s, u"#Hardware"_s, u"hotel"_s}));
}

void SystemTrayConfigModelTest::testMoveWithDuplicateItemIds()
{
    MockTrayModel source({
        {u"foo"_s, u"ApplicationStatus"_s, u"Foo 1"_s},
        {u"bar"_s, u"ApplicationStatus"_s, u"Bar"_s},
        {u"foo"_s, u"ApplicationStatus"_s, u"Foo 2"_s},
    });
    SystemTrayConfigModel model(&source);
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    model.setOrder({u"foo"_s, u"bar"_s, u"foo"_s});
    model.setGrouped(false);
    QCOMPARE(names(model), (QStringList{u"Foo 1"_s, u"Bar"_s, u"Foo 2"_s}));

    QSignalSpy moveSpy(&model, &QAbstractItemModel::rowsMoved);
    QSignalSpy layoutSpy(&model, &QAbstractItemModel::layoutChanged);
    QSignalSpy orderSpy(&model, &SystemTrayConfigModel::orderChanged);

    model.move(2, 0);
    QCOMPARE(names(model), (QStringList{u"Foo 1"_s, u"Foo 2"_s, u"Bar"_s}));
    QCOMPARE(model.order(), (QStringList{u"foo"_s, u"foo"_s, u"bar"_s}));
    QCOMPARE(moveSpy.count(), 1);
    QCOMPARE(orderSpy.count(), 1);

    model.move(1, 0);
    QCOMPARE(names(model), (QStringList{u"Foo 1"_s, u"Foo 2"_s, u"Bar"_s}));
    QCOMPARE(moveSpy.count(), 1);
    QCOMPARE(orderSpy.count(), 1);

    model.move(0, 2);
    QCOMPARE(names(model), (QStringList{u"Foo 1"_s, u"Foo 2"_s, u"Bar"_s}));
    QCOMPARE(moveSpy.count(), 1);
    QCOMPARE(orderSpy.count(), 1);
    QCOMPARE(layoutSpy.count(), 0);
}

QTEST_MAIN(SystemTrayConfigModelTest)

#include "systemtrayconfigmodeltest.moc"
