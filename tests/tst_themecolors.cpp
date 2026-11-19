/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Juan Giler <contact@example.org>
 */

#include <QtTest>

#include "themecolors.h"

class TestThemeColors : public QObject
{
    Q_OBJECT

private slots:
    void parseColor_data();
    void parseColor();
    void readReadsAccentAndDarkness();
    void readFallsBackToLightTheme();
    void dominantAccountColor_data();
    void dominantAccountColor();
    void dominantAccountReportsTheName();
};

void TestThemeColors::parseColor_data()
{
    QTest::addColumn<QString>("value");
    QTest::addColumn<QString>("expected");

    QTest::newRow("empty") << "" << QString();
    QTest::newRow("rgb") << "102,83,26" << QStringLiteral("#66531a");
    QTest::newRow("rgb with spaces") << " 102 , 83 , 26 " << QStringLiteral("#66531a");
    QTest::newRow("hex") << "#3daee9" << QStringLiteral("#3daee9");
    QTest::newRow("garbage") << "not a color" << QString();
    QTest::newRow("too few parts") << "1,2" << QString();
}

void TestThemeColors::parseColor()
{
    QFETCH(QString, value);
    QFETCH(QString, expected);

    const QColor color = ThemeColors::parseColor(value);

    if (expected.isEmpty()) {
        QVERIFY(!color.isValid());
        return;
    }
    QCOMPARE(color.name(), expected);
}

void TestThemeColors::readReadsAccentAndDarkness()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = dir.filePath(QStringLiteral("kdeglobals"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("[General]\n"
               "AccentColor=102,83,26\n"
               "\n"
               "[Colors:View]\n"
               "BackgroundNormal=44,44,44\n");
    file.close();

    const ThemeColors::Theme theme = ThemeColors::read(path);

    QCOMPARE(theme.accentColor.name(), QStringLiteral("#66531a"));
    QCOMPARE(theme.backgroundColor.name(), QStringLiteral("#2c2c2c"));
    QVERIFY(theme.darkTheme);
}

void TestThemeColors::readFallsBackToLightTheme()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = dir.filePath(QStringLiteral("kdeglobals"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("[General]\n"
               "AccentColor=255,0,0\n"
               "\n"
               "[Colors:View]\n"
               "BackgroundNormal=239,240,241\n");
    file.close();

    const ThemeColors::Theme theme = ThemeColors::read(path);

    QCOMPARE(theme.accentColor.name(), QStringLiteral("#ff0000"));
    QVERIFY(!theme.darkTheme);
}

namespace
{
/// QFETCH cannot handle types with a comma, so they need a name.
using UnreadMap = QMap<QString, int>;
using ColorMap = QHash<QString, QColor>;
} // namespace

void TestThemeColors::dominantAccountColor_data()
{
    QTest::addColumn<UnreadMap>("unread");
    QTest::addColumn<ColorMap>("colors");
    QTest::addColumn<QString>("expected");

    const QColor red(255, 0, 0);
    const QColor blue(0, 0, 255);

    // No account has a color: the fallback is used.
    QTest::newRow("no colors") << UnreadMap{{"a", 10}} << ColorMap() << QStringLiteral("#010203");

    // Only one account has a color: it wins.
    QTest::newRow("one colored account")
        << UnreadMap{{"a", 10}, {"b", 3}} << ColorMap{{"b", blue}} << QStringLiteral("#0000ff");

    // Several accounts have a color: the one with the most unread wins.
    QTest::newRow("most unread wins")
        << UnreadMap{{"a", 10}, {"b", 30}} << ColorMap{{"a", red}, {"b", blue}} << QStringLiteral("#0000ff");

    // An account without unread messages does not compete.
    QTest::newRow("empty account does not win")
        << UnreadMap{{"a", 0}, {"b", 2}} << ColorMap{{"a", red}, {"b", blue}} << QStringLiteral("#0000ff");

    // An account with a color but no unread messages is ignored.
    QTest::newRow("colored but empty")
        << UnreadMap{{"a", 5}} << ColorMap{{"a", red}, {"b", blue}} << QStringLiteral("#ff0000");

    // Nobody has unread messages: the fallback is used.
    QTest::newRow("nothing unread")
        << UnreadMap{{"a", 0}} << ColorMap{{"a", red}} << QStringLiteral("#010203");
}

void TestThemeColors::dominantAccountColor()
{
    QFETCH(UnreadMap, unread);
    QFETCH(ColorMap, colors);
    QFETCH(QString, expected);

    const QColor result = ThemeColors::dominantAccountColor(unread, colors, QColor(1, 2, 3));

    QCOMPARE(result.name(), expected);
}

void TestThemeColors::dominantAccountReportsTheName()
{
    const UnreadMap unread{{"work@example.org", 42}, {"private@example.org", 7}};
    const ColorMap colors{{"work@example.org", QColor(255, 0, 0)},
                          {"private@example.org", QColor(0, 0, 255)}};

    QCOMPARE(ThemeColors::dominantAccount(unread, colors), QStringLiteral("work@example.org"));

    // Without colors no account competes.
    QCOMPARE(ThemeColors::dominantAccount(unread, ColorMap()), QString());

    // An account without unread mail does not win either.
    const UnreadMap empty{{"work@example.org", 0}, {"private@example.org", 7}};
    QCOMPARE(ThemeColors::dominantAccount(empty, colors), QStringLiteral("private@example.org"));
}

QTEST_GUILESS_MAIN(TestThemeColors)
#include "tst_themecolors.moc"
