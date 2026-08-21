/*
    SPDX-FileCopyrightText: 2026 Ramil Nurmanov <ramil2004nur@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <QAbstractListModel>
#include <QPointer>
#include <QTemporaryDir>
#include <QTest>

#include <KConfigLoader>

#include "../sortedsystemtraymodel.h"
#include "../systemtraymodel.h"
#include "../systemtraysettings.h"

using namespace Qt::StringLiterals;

class MockTrayModel : public QAbstractListModel
{
public:
    struct Item {
        QString itemId;
        QString category;
    };
    QList<Item> items;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : items.size();
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!checkIndex(index, CheckIndexOption::IndexIsValid)) {
            return {};
        }
        const Item &item = items.at(index.row());
        switch (static_cast<BaseModel::BaseRole>(role)) {
        case BaseModel::BaseRole::ItemId:
            return item.itemId;
        case BaseModel::BaseRole::Category:
            return item.category;
        default:
            return {};
        }
    }

    QHash<int, QByteArray> roleNames() const override
    {
        return {
            {static_cast<int>(BaseModel::BaseRole::ItemId), QByteArrayLiteral("itemId")},
            {static_cast<int>(BaseModel::BaseRole::Category), QByteArrayLiteral("category")},
        };
    }
};

class SortedSystemTrayModelTest : public QObject
{
    Q_OBJECT
private:
    SystemTraySettings *createSettings();

    QTemporaryDir *m_tempDir = nullptr;

private Q_SLOTS:
    void init();
    void cleanup();

    void testCustomOrderNoDuplicates();
    void testCustomOrderWithDuplicateItemIds();
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

void SortedSystemTrayModelTest::testCustomOrderNoDuplicates()
{
    QPointer<SystemTraySettings> settings = createSettings();

    MockTrayModel source;
    source.items = {
        {u"a"_s, u"ApplicationStatus"_s},
        {u"b"_s, u"ApplicationStatus"_s},
        {u"c"_s, u"ApplicationStatus"_s},
    };

    SortedSystemTrayModel proxy(SortedSystemTrayModel::SortingType::SystemTray, nullptr, settings);
    proxy.setSourceModel(&source);

    settings->setItemOrder({u"c"_s, u"a"_s, u"b"_s});

    QCOMPARE(proxy.rowCount(), 3);
    QCOMPARE(proxy.index(0, 0).data(static_cast<int>(BaseModel::BaseRole::ItemId)).toString(), u"c"_s);
    QCOMPARE(proxy.index(1, 0).data(static_cast<int>(BaseModel::BaseRole::ItemId)).toString(), u"a"_s);
    QCOMPARE(proxy.index(2, 0).data(static_cast<int>(BaseModel::BaseRole::ItemId)).toString(), u"b"_s);
}

void SortedSystemTrayModelTest::testCustomOrderWithDuplicateItemIds()
{
    QPointer<SystemTraySettings> settings = createSettings();

    MockTrayModel source;
    source.items = {
        {u"foo"_s, u"ApplicationStatus"_s},
        {u"foo"_s, u"ApplicationStatus"_s},
        {u"bar"_s, u"ApplicationStatus"_s},
    };

    SortedSystemTrayModel proxy(SortedSystemTrayModel::SortingType::SystemTray, nullptr, settings);
    proxy.setSourceModel(&source);

    settings->setItemOrder({u"foo"_s, u"bar"_s, u"foo"_s});

    QCOMPARE(proxy.rowCount(), 3);
    QCOMPARE(proxy.index(0, 0).data(static_cast<int>(BaseModel::BaseRole::ItemId)).toString(), u"foo"_s);
    QCOMPARE(proxy.index(1, 0).data(static_cast<int>(BaseModel::BaseRole::ItemId)).toString(), u"bar"_s);
    QCOMPARE(proxy.index(2, 0).data(static_cast<int>(BaseModel::BaseRole::ItemId)).toString(), u"foo"_s);

    QCOMPARE(proxy.mapToSource(proxy.index(0, 0)).row(), 0);
    QCOMPARE(proxy.mapToSource(proxy.index(2, 0)).row(), 1);
}

QTEST_MAIN(SortedSystemTrayModelTest)

#include "sortedsystemtraymodeltest.moc"
