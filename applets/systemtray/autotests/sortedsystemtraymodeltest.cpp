/*
    SPDX-FileCopyrightText: 2026 Ramil Nurmanov <ramil2004nur@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <QPointer>
#include <QTemporaryDir>
#include <QTest>

#include <KConfigLoader>

#include "../sortedsystemtraymodel.h"
#include "../systemtraymodel.h"
#include "../systemtraysettings.h"
#include "mocktraymodel.h"

using namespace Qt::StringLiterals;

class SortedSystemTrayModelTest : public QObject
{
    Q_OBJECT
private:
    SystemTraySettings *createSettings();
    static QStringList itemIds(const QAbstractItemModel &model);

    QTemporaryDir *m_tempDir = nullptr;

private Q_SLOTS:
    void init();
    void cleanup();

    void testDefaultOrder();
    void testDefaultOrderNotificationsFirst();
    void testCustomOrderMovesNotifications();
    void testCustomOrderNoDuplicates();
    void testCustomOrderWithDuplicateItemIds();
    void testUnlistedItemsFollowListedOnes();
};

void SortedSystemTrayModelTest::init()
{
    m_tempDir = new QTemporaryDir();
    QVERIFY(m_tempDir->isValid());
}

void SortedSystemTrayModelTest::cleanup()
{
    delete m_tempDir;
    m_tempDir = nullptr;
}

SystemTraySettings *SortedSystemTrayModelTest::createSettings()
{
    const QString configFileName = m_tempDir->filePath(QStringLiteral("systraysettingsrc"));
    const QString schemaFileName = QFINDTESTDATA("../main.xml");
    auto *schemaFile = new QFile(schemaFileName, this);
    auto *loader = new KConfigLoader(configFileName, schemaFile, this);
    return new SystemTraySettings(loader, this);
}

QStringList SortedSystemTrayModelTest::itemIds(const QAbstractItemModel &model)
{
    QStringList ids;
    for (int row = 0; row < model.rowCount(); ++row) {
        ids.append(model.index(row, 0).data(static_cast<int>(BaseModel::BaseRole::ItemId)).toString());
    }
    return ids;
}

void SortedSystemTrayModelTest::testDefaultOrder()
{
    QPointer<SystemTraySettings> settings = createSettings();

    MockTrayModel source({
        {u"hardware"_s, u"Hardware"_s},
        {u"zulu"_s, u"ApplicationStatus"_s},
        {u"alpha"_s, u"ApplicationStatus"_s},
        {u"unknown"_s, u"SomethingElse"_s},
    });

    SortedSystemTrayModel proxy(settings);
    proxy.setSourceModel(&source);

    QCOMPARE(itemIds(proxy), (QStringList{u"unknown"_s, u"alpha"_s, u"zulu"_s, u"hardware"_s}));
}

void SortedSystemTrayModelTest::testDefaultOrderNotificationsFirst()
{
    QPointer<SystemTraySettings> settings = createSettings();

    MockTrayModel source({
        {u"alpha"_s, u"ApplicationStatus"_s},
        {u"org.kde.plasma.notifications"_s, u"SystemServices"_s},
        {u"unknown"_s, u"UnknownCategory"_s},
    });

    SortedSystemTrayModel proxy(settings);
    proxy.setSourceModel(&source);

    QCOMPARE(itemIds(proxy), (QStringList{u"org.kde.plasma.notifications"_s, u"unknown"_s, u"alpha"_s}));
}

void SortedSystemTrayModelTest::testCustomOrderMovesNotifications()
{
    QPointer<SystemTraySettings> settings = createSettings();

    MockTrayModel source({
        {u"alpha"_s, u"ApplicationStatus"_s},
        {u"org.kde.plasma.notifications"_s, u"SystemServices"_s},
        {u"unknown"_s, u"UnknownCategory"_s},
    });

    SortedSystemTrayModel proxy(settings);
    proxy.setSourceModel(&source);

    settings->setItemOrder({u"alpha"_s, u"org.kde.plasma.notifications"_s});

    QCOMPARE(itemIds(proxy), (QStringList{u"alpha"_s, u"org.kde.plasma.notifications"_s, u"unknown"_s}));
}

void SortedSystemTrayModelTest::testCustomOrderNoDuplicates()
{
    QPointer<SystemTraySettings> settings = createSettings();

    MockTrayModel source({
        {u"a"_s, u"ApplicationStatus"_s},
        {u"b"_s, u"ApplicationStatus"_s},
        {u"c"_s, u"ApplicationStatus"_s},
    });

    SortedSystemTrayModel proxy(settings);
    proxy.setSourceModel(&source);

    settings->setItemOrder({u"c"_s, u"a"_s, u"b"_s});

    QCOMPARE(itemIds(proxy), (QStringList{u"c"_s, u"a"_s, u"b"_s}));
}

void SortedSystemTrayModelTest::testCustomOrderWithDuplicateItemIds()
{
    QPointer<SystemTraySettings> settings = createSettings();

    MockTrayModel source({
        {u"foo"_s, u"ApplicationStatus"_s},
        {u"foo"_s, u"ApplicationStatus"_s},
        {u"bar"_s, u"ApplicationStatus"_s},
    });

    SortedSystemTrayModel proxy(settings);
    proxy.setSourceModel(&source);

    settings->setItemOrder({u"foo"_s, u"bar"_s, u"foo"_s});

    QCOMPARE(itemIds(proxy), (QStringList{u"foo"_s, u"bar"_s, u"foo"_s}));
    QCOMPARE(proxy.mapToSource(proxy.index(0, 0)).row(), 0);
    QCOMPARE(proxy.mapToSource(proxy.index(2, 0)).row(), 1);
}

void SortedSystemTrayModelTest::testUnlistedItemsFollowListedOnes()
{
    QPointer<SystemTraySettings> settings = createSettings();

    MockTrayModel source({
        {u"app"_s, u"ApplicationStatus"_s},
        {u"hardware"_s, u"Hardware"_s},
        {u"communications"_s, u"Communications"_s},
        {u"unknown"_s, u"UnknownCategory"_s},
    });

    SortedSystemTrayModel proxy(settings);
    proxy.setSourceModel(&source);

    settings->setItemOrder({u"hardware"_s, u"app"_s});

    QCOMPARE(itemIds(proxy), (QStringList{u"hardware"_s, u"app"_s, u"unknown"_s, u"communications"_s}));
}

QTEST_MAIN(SortedSystemTrayModelTest)

#include "sortedsystemtraymodeltest.moc"
