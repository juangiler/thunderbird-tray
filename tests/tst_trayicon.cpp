/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Juan Giler <contact@example.org>
 */

#include <QtTest>

#include "themecolors.h"
#include "trayicon.h"

class TestTrayIcon : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void badgeUsesTheConfiguredColor();
    void badgeFollowsTheAccentColor();
    void accountColorWinsOverTheAccent();
    void counterIsWhiteOnDarkThemes();
    void counterIsBlackOnLightThemes();
    void counterColorFollowsTheTheme();
    void noBadgeWithoutUnreadMessages();
    void animationFadesTheBadgeSmoothly();
    void runningIndicatorShownWhenThunderbirdIsRunning();
    void runningIndicatorHiddenWhenThunderbirdIsNotRunning();
    void runningIndicatorCoexistsWithUnreadBadge();

private:
    /// A pixel inside the badge, but away from the digits.
    static QColor badgePixel(const QImage &image)
    {
        return image.pixelColor(54, 10);
    }

    /// A pixel inside the "Thunderbird is running" indicator.
    static QColor runningPixel(const QImage &image)
    {
        return image.pixelColor(9, 55);
    }
};

void TestTrayIcon::initTestCase()
{
    // The tests run without a display server.
    qputenv("QT_QPA_PLATFORM", "offscreen");
}

void TestTrayIcon::badgeUsesTheConfiguredColor()
{
    TrayIcon tray;
    tray.setAnimationEnabled(false);
    tray.setBadgeColorFollowsAccent(false);
    tray.setBadgeColor(QColor(255, 0, 0));
    tray.setUnreadCount(7);

    const QImage image = tray.icon().pixmap(64, 64).toImage();
    const QColor pixel = badgePixel(image);

    QCOMPARE(pixel.red(), 255);
    QCOMPARE(pixel.green(), 0);
    QCOMPARE(pixel.blue(), 0);
}

void TestTrayIcon::badgeFollowsTheAccentColor()
{
    TrayIcon tray;
    tray.setAnimationEnabled(false);
    tray.setBadgeColorFollowsAccent(true);
    // A color that is only used when the accent is not followed.
    tray.setBadgeColor(QColor(255, 0, 0));
    tray.setUnreadCount(7);

    const QColor accent = tray.themeAccentColor();
    QVERIFY(accent.isValid());

    const QColor pixel = badgePixel(tray.icon().pixmap(64, 64).toImage());
    QCOMPARE(pixel.red(), accent.red());
    QCOMPARE(pixel.green(), accent.green());
    QCOMPARE(pixel.blue(), accent.blue());
}

void TestTrayIcon::accountColorWinsOverTheAccent()
{
    TrayIcon tray;
    tray.setAnimationEnabled(false);
    // The global color is the accent color of the theme ...
    tray.setBadgeColorFollowsAccent(true);
    // ... but an account with a color of its own speaks for the badge.
    tray.setAccountBadgeColor(QColor(1, 2, 3));
    tray.setUnreadCount(5);

    const QColor pixel = badgePixel(tray.icon().pixmap(64, 64).toImage());
    QCOMPARE(pixel.red(), 1);
    QCOMPARE(pixel.green(), 2);
    QCOMPARE(pixel.blue(), 3);
}

void TestTrayIcon::counterIsWhiteOnDarkThemes(){
    TrayIcon tray;
    tray.setAnimationEnabled(false);
    tray.setBadgeColorFollowsAccent(false);
    tray.setBadgeColor(QColor(0, 0, 0));
    tray.setBadgeTextColor(QColor(Qt::white));
    tray.setUnreadCount(42);

    // The counter is painted in a light color on the dark badge.
    const QImage image = tray.icon().pixmap(64, 64).toImage();
    bool foundLightPixel = false;
    for (int y = 10; y < 32 && !foundLightPixel; ++y) {
        for (int x = 34; x < 54; ++x) {
            if (image.pixelColor(x, y).lightness() > 200) {
                foundLightPixel = true;
                break;
            }
        }
    }
    QVERIFY2(foundLightPixel, "the counter should be painted in a light color");
}

void TestTrayIcon::counterIsBlackOnLightThemes()
{
    TrayIcon tray;
    tray.setAnimationEnabled(false);
    tray.setBadgeColorFollowsAccent(false);
    tray.setBadgeColor(QColor(255, 255, 255));
    tray.setBadgeTextColor(QColor(Qt::black));
    tray.setUnreadCount(42);

    const QImage image = tray.icon().pixmap(64, 64).toImage();
    bool foundDarkPixel = false;
    for (int y = 10; y < 32 && !foundDarkPixel; ++y) {
        for (int x = 34; x < 54; ++x) {
            if (image.pixelColor(x, y).lightness() < 40) {
                foundDarkPixel = true;
                break;
            }
        }
    }
    QVERIFY2(foundDarkPixel, "the counter should be painted in a dark color");
}

void TestTrayIcon::counterColorFollowsTheTheme()
{
    // Automatic: white on dark themes, black on light ones.
    QCOMPARE(ThemeColors::counterColor(QColor(), true), QColor(Qt::white));
    QCOMPARE(ThemeColors::counterColor(QColor(), false), QColor(Qt::black));

    // An explicit color always wins.
    QCOMPARE(ThemeColors::counterColor(QColor(1, 2, 3), true), QColor(1, 2, 3));
    QCOMPARE(ThemeColors::counterColor(QColor(1, 2, 3), false), QColor(1, 2, 3));
}

void TestTrayIcon::noBadgeWithoutUnreadMessages()
{
    TrayIcon tray;
    tray.setAnimationEnabled(false);
    tray.setBadgeColorFollowsAccent(false);
    tray.setBadgeColor(QColor(255, 0, 0));
    tray.setUnreadCount(0);

    const QImage image = tray.icon().pixmap(64, 64).toImage();
    // The badge must not be painted when there is nothing to show.
    QVERIFY(badgePixel(image) != QColor(255, 0, 0));
}

void TestTrayIcon::animationFadesTheBadgeSmoothly()
{
    TrayIcon tray;
    tray.setAnimationEnabled(true);
    tray.setAnimationInterval(200);
    tray.setBadgeColorFollowsAccent(false);
    tray.setBadgeColor(QColor(0, 0, 0));
    tray.setUnreadCount(5);

    // The sum over all channels changes while the badge fades, no matter
    // whether an icon is painted below it or not.
    QSet<qint64> weights;
    for (int frame = 0; frame < 40; ++frame) {
        // qWait processes the events and lets the animation timer tick.
        QTest::qWait(20);

        const QImage image = tray.icon().pixmap(64, 64).toImage();
        qint64 weight = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const QColor pixel = image.pixelColor(x, y);
                weight += pixel.red() + pixel.green() + pixel.blue() + pixel.alpha();
            }
        }
        weights.insert(weight);
    }

    // The badge has to fade gradually, not blink between two states.
    QVERIFY2(weights.size() > 5, "the badge should fade through many shades");
}

void TestTrayIcon::runningIndicatorShownWhenThunderbirdIsRunning()
{
    TrayIcon tray;
    tray.setAnimationEnabled(false);
    tray.setUnreadCount(0);
    tray.setThunderbirdRunning(true);

    const QColor pixel = runningPixel(tray.icon().pixmap(64, 64).toImage());
    // The running indicator is a green dot.
    QVERIFY2(pixel.green() > 150 && pixel.green() > pixel.red() + 40 && pixel.alpha() > 200,
             "a green running indicator should be painted in the bottom-left corner");
}

void TestTrayIcon::runningIndicatorHiddenWhenThunderbirdIsNotRunning()
{
    TrayIcon tray;
    tray.setAnimationEnabled(false);
    tray.setUnreadCount(0);
    tray.setThunderbirdRunning(false);

    const QColor pixel = runningPixel(tray.icon().pixmap(64, 64).toImage());
    QVERIFY2(!(pixel.green() > 150 && pixel.green() > pixel.red() + 40 && pixel.alpha() > 200),
             "no running indicator should be painted when Thunderbird is not running");
}

void TestTrayIcon::runningIndicatorCoexistsWithUnreadBadge()
{
    TrayIcon tray;
    tray.setAnimationEnabled(false);
    tray.setBadgeColorFollowsAccent(false);
    tray.setBadgeColor(QColor(255, 0, 0));
    tray.setUnreadCount(7);
    tray.setThunderbirdRunning(true);

    const QImage image = tray.icon().pixmap(64, 64).toImage();
    const QColor badge = badgePixel(image);
    const QColor running = runningPixel(image);

    QCOMPARE(badge.red(), 255);
    QCOMPARE(badge.green(), 0);
    QCOMPARE(badge.blue(), 0);
    QVERIFY2(running.green() > 150 && running.green() > running.red() + 40 && running.alpha() > 200,
             "the running indicator should be visible together with the unread badge");
}

int main(int argc, char *argv[])
{
    // The test does not need a display server.
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QApplication app(argc, argv);
    TestTrayIcon test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_trayicon.moc"