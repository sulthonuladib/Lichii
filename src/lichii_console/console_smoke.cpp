// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstdio>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QProcess>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest/QtTest>
#include <SDL3/SDL.h>

#include "common/fs/path_util.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "lichii_console/console_app.h"
#include "lichii_console/library_smoke.h"
#include "qt_common/config/qt_config.h"
#include "qt_common/config/uisettings.h"

namespace Horizon {
namespace {

QQuickItem* FindItem(QQuickItem* item, QLatin1StringView name) {
    if (item->objectName() == name) {
        return item;
    }
    for (auto* child : item->childItems()) {
        if (auto* match = FindItem(child, name)) {
            return match;
        }
    }
    return nullptr;
}

QRectF BoundsIn(const QQuickItem* item, const QQuickItem* parent) {
    return item->mapRectToItem(parent, QRectF{QPointF{}, item->size()});
}

QJsonArray RectToJson(const QRectF& bounds) {
    return {bounds.x(), bounds.y(), bounds.width(), bounds.height()};
}

void CollectGeometry(QQuickItem* item, QQuickItem* stage, QJsonArray& geometry) {
    if (!item->isVisible()) {
        return;
    }
    if (!item->objectName().isEmpty()) {
        geometry.append(QJsonObject{
            {QStringLiteral("name"), item->objectName()},
            {QStringLiteral("bounds"), RectToJson(BoundsIn(item, stage))},
            {QStringLiteral("focused"), item->hasActiveFocus() ||
                                            item->property("focused").toBool() ||
                                            item->property("highlighted").toBool()},
        });
    }
    for (auto* child : item->childItems()) {
        CollectGeometry(child, stage, geometry);
    }
}

class ConsoleSmoke final : public QObject {
    Q_OBJECT

public:
    explicit ConsoleSmoke(ConsoleApp& app_) : app{app_} {}

private slots:
    void navigation();
    void controllerNavigation();
    void settings();
    void libraryAndLifecycle();

private:
    QQuickItem* item(const char* name) const {
        return FindItem(app.window()->contentItem(), QLatin1StringView(name));
    }
    void verifyHomeGeometry(int game_count);

    ConsoleApp& app;
};

void ConsoleSmoke::navigation() {
    auto* window = app.window();
    QTRY_VERIFY_WITH_TIMEOUT(window->isExposed(), 5000);
    if (QGuiApplication::platformName() == QStringLiteral("wayland")) {
        QTRY_COMPARE(QGuiApplication::focusWindow(), window);
    }
    QTRY_VERIFY(item("homeScreen"));
    QVERIFY(item("homeScreen")->isVisible());
    QTRY_VERIFY(!app.library()->scanning());
    QCOMPARE(app.library()->count(), 0);

    auto* settings_navigation = item("settingsNavigation");
    QVERIFY(settings_navigation);
    settings_navigation->forceActiveFocus();
    QTest::keyClick(window, Qt::Key_Return);
    QTRY_VERIFY(item("settingsScreen") && item("settingsScreen")->isVisible());
    auto* settings_categories = item("settingsCategories");
    auto* settings_values = item("settingsValues");
    QVERIFY(settings_categories && settings_values);
    QTRY_VERIFY(settings_categories->hasActiveFocus());
    QTest::keyClick(window, Qt::Key_Right);
    QTRY_VERIFY(settings_values->hasActiveFocus());
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(item("homeScreen")->isVisible());
    QTRY_VERIFY(settings_navigation->hasActiveFocus());

    auto* all_software = item("allSoftwareAction");
    QVERIFY(all_software);
    all_software->forceActiveFocus();
    QTest::keyClick(window, Qt::Key_Return);
    QTRY_VERIFY(item("libraryScreen") && item("libraryScreen")->isVisible());
    auto* controls = item("libraryControlsAction");
    QVERIFY(controls);
    controls->forceActiveFocus();
    QTest::keyClick(window, Qt::Key_Return);
    QTRY_VERIFY(item("searchGamesField") && item("searchGamesField")->isVisible());
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(!item("searchGamesField") || !item("searchGamesField")->isVisible());
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(item("homeScreen")->isVisible());

    QTest::keyClick(window, Qt::Key_F11);
    QTRY_COMPARE(window->visibility(), QWindow::FullScreen);
    if (QGuiApplication::platformName() == QStringLiteral("wayland")) {
        // Wayland applies the fullscreen configure and restores keyboard focus asynchronously.
        QTest::qWait(200);
        QTRY_COMPARE(QGuiApplication::focusWindow(), window);
    }
    QTest::keyClick(window, Qt::Key_F11);
    QTRY_COMPARE_NE(window->visibility(), QWindow::FullScreen);
}

void ConsoleSmoke::controllerNavigation() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        QSKIP("Controller navigation requires a window with compositor focus");
    }
    int attached = 0;
    SDL_free(SDL_GetGamepads(&attached));
    if (attached) {
        QSKIP("An existing gamepad has navigation priority over the virtual device");
    }

    SDL_VirtualJoystickDesc descriptor;
    SDL_INIT_INTERFACE(&descriptor);
    descriptor.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    descriptor.name = "Lichii test gamepad";
    descriptor.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    const auto id = SDL_AttachVirtualJoystick(&descriptor);
    QVERIFY2(id, SDL_GetError());
    SCOPE_EXIT { SDL_DetachVirtualJoystick(id); };
    auto* pad = SDL_OpenGamepad(id);
    QVERIFY2(pad, SDL_GetError());
    SCOPE_EXIT { SDL_CloseGamepad(pad); };
    auto* joystick = SDL_GetGamepadJoystick(pad);
    QVERIFY(joystick);
    const auto verify_prompts = [&](QStringView confirm, QStringView back, QStringView options,
                                    QStringView home) {
        const auto prompts = app.gamepadPrompts();
        QCOMPARE(prompts.value(QStringLiteral("confirm")).toString(), confirm);
        QCOMPARE(prompts.value(QStringLiteral("back")).toString(), back);
        QCOMPARE(prompts.value(QStringLiteral("options")).toString(), options);
        QCOMPARE(prompts.value(QStringLiteral("home")).toString(), home);
    };

    QTRY_COMPARE(QGuiApplication::focusWindow(), app.window());
    QTest::keyClick(app.window(), Qt::Key_F1);
    QTRY_VERIFY(item("homeScreen")->isVisible());
    auto* settings_navigation = item("settingsNavigation");
    QVERIFY(settings_navigation);
    QVERIFY(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_DPAD_DOWN, true));
    QTRY_VERIFY(settings_navigation->hasActiveFocus());
    QVERIFY(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_DPAD_DOWN, false));
    QVERIFY(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_EAST, true));
    QTRY_VERIFY(item("settingsScreen")->isVisible());
    QVERIFY(app.gamepadHints());
    verify_prompts(u"B", u"A", u"Start", u"Guide");
    auto* confirm = item("confirmPrompt");
    auto* back = item("backPrompt");
    QVERIFY(confirm && back);
    QTRY_COMPARE(confirm->property("button").toString(), QStringLiteral("B"));
    QTRY_COMPARE(back->property("button").toString(), QStringLiteral("A"));
    QVERIFY(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_EAST, false));
    QTest::mouseClick(app.window(), Qt::LeftButton, Qt::NoModifier, QPoint{4, 4});
    QTRY_VERIFY(!app.gamepadHints());
    QTRY_COMPARE(confirm->property("button").toString(), QStringLiteral("Enter"));
    QVERIFY(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, true));
    QTRY_VERIFY(item("homeScreen")->isVisible());
    QTRY_VERIFY(settings_navigation->hasActiveFocus());
    QVERIFY(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, false));

    QVERIFY(app.setSetting(QStringLiteral("controller_confirm"), 1));
    QTest::qWait(40);
    QVERIFY(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, true));
    QTRY_VERIFY(item("settingsScreen")->isVisible());
    verify_prompts(u"A", u"B", u"Start", u"Guide");
    QTRY_COMPARE(confirm->property("button").toString(), QStringLiteral("A"));
    QTRY_COMPARE(back->property("button").toString(), QStringLiteral("B"));
    QVERIFY(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, false));
    QVERIFY(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_GUIDE, true));
    QTRY_VERIFY(item("homeScreen")->isVisible());
    QVERIFY(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_GUIDE, false));
    QVERIFY(app.setSetting(QStringLiteral("controller_confirm"), 0));

    QTest::keyClick(app.window(), Qt::Key_F1);
    QVERIFY(!app.gamepadHints());

    const auto remap = [&](SDL_GamepadType type) {
        char guid[33];
        SDL_GUIDToString(SDL_GetGamepadGUIDForID(id), guid, sizeof(guid));
        const QByteArray mapping = QByteArray(guid) + ",Lichii test gamepad,type:" +
                                   SDL_GetGamepadStringForType(type) + ",a:b0,b:b1,dpdown:b12,";
        if (SDL_AddGamepadMapping(mapping.constData()) < 0) {
            return false;
        }
        SDL_UpdateGamepads();
        return SDL_GetGamepadType(pad) == type;
    };
    QVERIFY2(remap(SDL_GAMEPAD_TYPE_PS5), SDL_GetError());
    QVERIFY(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_DPAD_DOWN, true));
    QTRY_VERIFY(settings_navigation->hasActiveFocus());
    verify_prompts(u"○", u"×", u"Options", u"PS");
    QVERIFY(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_DPAD_DOWN, false));
    QTest::qWait(40);
    QTest::keyClick(app.window(), Qt::Key_F1);
    QVERIFY(!app.gamepadHints());

    QVERIFY2(remap(SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO), SDL_GetError());
    QVERIFY(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_DPAD_DOWN, true));
    QTRY_VERIFY(settings_navigation->hasActiveFocus());
    verify_prompts(u"A", u"B", u"+", u"Home");
    QVERIFY(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_DPAD_DOWN, false));
    QTest::keyClick(app.window(), Qt::Key_F1);
    QVERIFY(!app.gamepadHints());
}

QVariant SettingValue(ConsoleApp& app, const QString& group, const QString& key,
                      const QString& id = {}) {
    for (const auto& entry : app.settings(group, id)) {
        const auto row = entry.toMap();
        if (row.value(QStringLiteral("key")).toString() == key) {
            return row.value(QStringLiteral("value"));
        }
    }
    return {};
}

void ConsoleSmoke::settings() {
    const auto general = QStringLiteral("general");
    const auto speed = QStringLiteral("speed_limit");
    const auto id = QStringLiteral("0100123456700000");
    const auto old_speed = SettingValue(app, general, speed);
    QVERIFY(old_speed.isValid());

    QVERIFY(app.setSetting(speed, 125, {}));
    QVERIFY(app.setSetting(speed, 165, id));
    QCOMPARE(SettingValue(app, general, speed).toInt(), 125);
    QCOMPARE(SettingValue(app, general, speed, id).toInt(), 165);

    app.appearance()->setTheme(QStringLiteral("light"));
    app.appearance()->setMuted(true);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
    QProcess readback;
    readback.start(QCoreApplication::applicationFilePath(),
                   {QStringLiteral("--console-settings-readback")});
    QVERIFY(readback.waitForFinished(10000));
    QCOMPARE(readback.exitStatus(), QProcess::NormalExit);
    QCOMPARE(readback.exitCode(), 0);
    app.appearance()->setTheme(QStringLiteral("dark"));

    QVERIFY(app.setSetting(speed, {}, id));
    QCOMPARE(SettingValue(app, general, speed, id).toInt(), 125);
    QVERIFY(app.setSetting(speed, 175, id));
    QVERIFY(app.resetGameSettings(id));
    QCOMPARE(SettingValue(app, general, speed, id).toInt(), 125);

    auto* window = app.window();
    auto* settings_navigation = item("settingsNavigation");
    QVERIFY(settings_navigation);
    settings_navigation->forceActiveFocus();
    QTest::keyClick(window, Qt::Key_Return);
    QTRY_VERIFY(item("settingsScreen")->isVisible());
    auto* values = item("settingsValues");
    QVERIFY(values);
    QTest::keyClick(window, Qt::Key_Right);
    QTRY_VERIFY(values->hasActiveFocus());
    QTest::keyClick(window, Qt::Key_Down);
    QCOMPARE(values->property("currentIndex").toInt(), 1);
    QTest::keyClick(window, Qt::Key_Return);
    QTest::keyClick(window, Qt::Key_Right);
    QCOMPARE(SettingValue(app, general, speed).toInt(), 125);
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(values->hasActiveFocus());
    QCOMPARE(SettingValue(app, general, speed).toInt(), 125);

    QTest::keyClick(window, Qt::Key_Return);
    QTest::keyClick(window, Qt::Key_Right);
    QTest::keyClick(window, Qt::Key_Return);
    QCOMPARE(SettingValue(app, general, speed).toInt(), 126);
    QTRY_COMPARE(values->property("currentIndex").toInt(), 1);

    QTest::keyClick(window, Qt::Key_Down);
    QTest::keyClick(window, Qt::Key_Return);
    QTRY_VERIFY(window->activeFocusItem() &&
                window->activeFocusItem()->objectName() == QStringLiteral("dialogAction_0"));
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(values->hasActiveFocus());
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(item("homeScreen")->isVisible());

    QVERIFY(app.setSetting(speed, old_speed, {}));
}

void ConsoleSmoke::verifyHomeGeometry(int game_count) {
    auto* stage = item("consoleStage");
    auto* shelf = item("homeShelf");
    auto* profile = item("homeProfile");
    auto* status = item("homeStatus");
    auto* settings = item("settingsNavigation");
    auto* browse = item("allSoftwareAction");
    auto* footer = item("consoleFooter");
    QVERIFY(stage && shelf && profile && status && settings && browse && footer);
    QCOMPARE(stage->size(), QSizeF(1280, 720));
    const auto viewport = stage->mapRectToScene(stage->boundingRect());
    const auto window_size = QSizeF(app.window()->size());
    const double scale = qMin(window_size.width() / stage->width(),
                              window_size.height() / stage->height());
    QVERIFY(qAbs(viewport.width() - stage->width() * scale) <= 1);
    QVERIFY(qAbs(viewport.height() - stage->height() * scale) <= 1);
    QVERIFY(qAbs(viewport.center().x() - window_size.width() / 2) <= 1);
    QVERIFY(qAbs(viewport.center().y() - window_size.height() / 2) <= 1);
    const auto shelf_bounds = BoundsIn(shelf, stage);
    const auto footer_bounds = BoundsIn(footer, stage);
    const auto header_left = BoundsIn(profile, stage);
    const auto header_right = BoundsIn(status, stage);
    const auto settings_bounds = BoundsIn(settings, stage);
    const auto browse_bounds = BoundsIn(browse, stage);
    QVERIFY(!header_left.intersects(header_right));
    QVERIFY(header_left.bottom() <= shelf_bounds.top());
    QVERIFY(header_right.bottom() <= shelf_bounds.top());
    QVERIFY(shelf_bounds.bottom() <= browse_bounds.top());
    QVERIFY(browse_bounds.bottom() <= settings_bounds.top());
    QVERIFY(settings_bounds.bottom() <= footer_bounds.top());
    QVERIFY(qAbs(settings_bounds.width() - settings_bounds.height()) <= 1);
    const auto* background = settings->property("background").value<QQuickItem*>();
    QVERIFY(background);
    QCOMPARE(background->property("radius").toDouble(), settings->width() / 2);
    QVERIFY(qAbs(footer_bounds.bottom() - stage->height()) <= 1);

    if (!game_count) {
        QVERIFY(!item("homeGameTile"));
        return;
    }
    QTRY_VERIFY(item("homeGameTile"));
    auto* first = item("homeGameTile");
    auto* cover = FindItem(first, QLatin1StringView("gameTileCover"));
    QVERIFY(cover);
    const auto first_bounds = BoundsIn(cover, stage);
    QVERIFY2(qAbs(first_bounds.x() - 64) <= 1 && qAbs(first_bounds.y() - 215) <= 1,
             qPrintable(QStringLiteral("First game cover at (%1, %2)")
                            .arg(first_bounds.x())
                            .arg(first_bounds.y())));
    QVERIFY(qAbs(first_bounds.width() - 216) <= 1);
    QVERIFY(qAbs(first_bounds.height() - 216) <= 1);
    if (game_count > 1) {
        auto* row = first->parentItem();
        QQuickItem* second = nullptr;
        for (auto* entry : row->childItems()) {
            if (entry == first || entry->objectName() != first->objectName() ||
                entry->x() <= first->x()) {
                continue;
            }
            if (!second || entry->x() < second->x()) {
                second = entry;
            }
        }
        QVERIFY(second);
        auto* second_cover = FindItem(second, QLatin1StringView("gameTileCover"));
        QVERIFY(second_cover);
        const auto second_bounds = BoundsIn(second_cover, stage);
        QVERIFY(qAbs(second_bounds.left() - first_bounds.right() - 24) <= 1);
    }
}

void ConsoleSmoke::libraryAndLifecycle() {
    auto* library = app.library();
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto captures = qEnvironmentVariable("LICHII_CONSOLE_TEST_CAPTURES");
    const auto games = qEnvironmentVariable("LICHII_CONSOLE_TEST_LIBRARY");
    const auto test_size = qEnvironmentVariable("LICHII_CONSOLE_TEST_SIZE");
    auto* window = app.window();
    if (!test_size.isEmpty()) {
        const auto dimensions = test_size.split(QLatin1Char('x'));
        QCOMPARE(dimensions.size(), 2);
        const QSize size{dimensions[0].toInt(), dimensions[1].toInt()};
        QVERIFY(size.width() >= window->minimumWidth() &&
                size.height() >= window->minimumHeight());
        window->resize(size);
        QTRY_COMPARE(window->size(), size);
    }
    const auto capture = [&](const QString& name) {
        QTest::qWait(220);
        auto* stage = item("consoleStage");
        const auto image = window->grabWindow();
        if (!stage || image.isNull()) {
            return false;
        }
        const QRectF bounds = stage->mapRectToScene(stage->boundingRect());
        const double density = double(image.width()) / window->width();
        const QRect pixels =
            QRectF{bounds.topLeft() * density, bounds.size() * density}.toRect();
        const auto filename = name + QStringLiteral(".png");
        if (!image.copy(pixels.intersected(image.rect()))
                 .scaled(QSize{1280, 720}, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                 .save(QDir{captures}.filePath(filename)) ||
            !image.save(QDir{captures}.filePath(QStringLiteral("window/") + filename))) {
            return false;
        }
        QJsonArray geometry;
        CollectGeometry(window->contentItem(), stage, geometry);
        QFile manifest(QDir{captures}.filePath(QStringLiteral("geometry/") + name +
                                              QStringLiteral(".json")));
        const auto data = QJsonDocument(QJsonObject{
            {QStringLiteral("window"), QJsonArray{window->width(), window->height()}},
            {QStringLiteral("stageInWindow"), RectToJson(bounds)},
            {QStringLiteral("items"), geometry},
        }).toJson();
        return manifest.open(QIODevice::WriteOnly) && manifest.write(data) == data.size();
    };
    if (!captures.isEmpty()) {
        QVERIFY(QDir{}.mkpath(QDir{captures}.filePath(QStringLiteral("window"))));
        QVERIFY(QDir{}.mkpath(QDir{captures}.filePath(QStringLiteral("geometry"))));
        QTest::keyClick(window, Qt::Key_F1);
        verifyHomeGeometry(0);
        QVERIFY(capture(QStringLiteral("home-empty")));
    }
    if (!games.isEmpty()) {
        const QDir source(games);
        const auto files = source.entryList({QStringLiteral("*.nro")}, QDir::Files);
        if (files.size() > 1 && !captures.isEmpty()) {
            QTemporaryDir single;
            QVERIFY(single.isValid());
            QVERIFY(QFile::copy(source.filePath(files.first()), single.filePath(files.first())));
            QVERIFY(library->addFolder(QUrl::fromLocalFile(single.path())));
            QTRY_VERIFY(!library->scanning() && library->count() == 1);
            QTest::keyClick(window, Qt::Key_F1);
            verifyHomeGeometry(1);
            QVERIFY(capture(QStringLiteral("home-single")));
            QVERIFY(library->removeFolder(single.path()));
            QTRY_VERIFY(!library->scanning() && library->count() == 0);
        }
        QVERIFY(library->addFolder(QUrl::fromLocalFile(games)));
        QTRY_VERIFY_WITH_TIMEOUT(!library->scanning(), 12000);
        QCOMPARE(library->count(), files.size());
    }
    if (!captures.isEmpty()) {
        QTest::keyClick(window, Qt::Key_F1);
        verifyHomeGeometry(library->count());
        QVERIFY(capture(QStringLiteral("home")));
        app.appearance()->setTheme(QStringLiteral("light"));
        QVERIFY(capture(QStringLiteral("home-light")));
        app.appearance()->setTheme(QStringLiteral("dark"));

        if (library->count() > 0) {
            auto* shelf = item("homeShelf");
            QVERIFY(shelf && shelf->hasActiveFocus());
            const int home_count = static_cast<int>(library->homeGames().size());
            if (home_count > 5) {
                for (int i = 1; i < home_count; ++i) {
                    QTest::keyClick(window, Qt::Key_Right);
                }
                QCOMPARE(shelf->property("currentIndex").toInt(), home_count - 1);
                QTRY_VERIFY(shelf->property("contentX").toDouble() > 0);
                QVERIFY(capture(QStringLiteral("home-scroll")));
                for (int i = 1; i < home_count; ++i) {
                    QTest::keyClick(window, Qt::Key_Left);
                }
                QCOMPARE(shelf->property("currentIndex").toInt(), 0);
            }
            if (library->count() > 1) {
                const int previous = shelf->property("currentIndex").toInt();
                QTest::keyClick(window, Qt::Key_Right);
                QVERIFY(previous != shelf->property("currentIndex").toInt());
            }
            QVERIFY(app.setSetting(QStringLiteral("options_key"), 1));
            QTest::keyClick(window, Qt::Key_F3);
            QTRY_VERIFY(window->activeFocusItem() &&
                        window->activeFocusItem()->objectName() == QStringLiteral("dialogAction_0"));
            QTest::keyClick(window, Qt::Key_Escape);
            QTRY_VERIFY(shelf->hasActiveFocus());
            QVERIFY(app.setSetting(QStringLiteral("options_key"), 0));
            QTest::keyClick(window, Qt::Key_Menu);
            QTRY_VERIFY(window->activeFocusItem() &&
                        window->activeFocusItem()->objectName() == QStringLiteral("dialogAction_0"));
            QVERIFY(capture(QStringLiteral("game-options")));
            app.appearance()->setTheme(QStringLiteral("light"));
            QVERIFY(capture(QStringLiteral("game-options-light")));
            app.appearance()->setTheme(QStringLiteral("dark"));
            QTest::keyClick(window, Qt::Key_Down);
            QTRY_VERIFY(window->activeFocusItem() &&
                        window->activeFocusItem()->objectName() == QStringLiteral("dialogAction_1"));
            QTest::keyClick(window, Qt::Key_Return);
            QTRY_VERIFY(item("softwareOptionDetails") && item("softwareOptionDetails")->isVisible());
            QVERIFY(capture(QStringLiteral("game-details")));
            QTest::keyClick(window, Qt::Key_Escape);
            QTRY_VERIFY(!item("softwareOptionDetails") || !item("softwareOptionDetails")->isVisible());
            QTRY_VERIFY(window->activeFocusItem() &&
                        window->activeFocusItem()->objectName() == QStringLiteral("dialogAction_1"));
            if (!library->get(0).value(QStringLiteral("programId")).toString().isEmpty()) {
                bool found = false;
                for (int i = 0; i < 16; ++i) {
                    QCoreApplication::processEvents();
                    auto* focus = window->activeFocusItem();
                    if (focus && focus->property("text").toString() ==
                        QStringLiteral("Reset Game Settings")) {
                        found = true;
                        break;
                    }
                    QTest::keyClick(window, Qt::Key_Down);
                }
                QVERIFY(found);
                QTest::keyClick(window, Qt::Key_Return);
                QTRY_VERIFY(window->activeFocusItem() &&
                            window->activeFocusItem()->objectName() ==
                                QStringLiteral("dialogAction_1"));
                QVERIFY(capture(QStringLiteral("confirmation")));
            }
            for (int i = 0; i < 3 && item("softwareOptionsDialog") &&
                            item("softwareOptionsDialog")->isVisible(); ++i) {
                QTest::keyClick(window, Qt::Key_Escape);
                QTest::qWait(120);
            }
            QTRY_VERIFY(shelf->hasActiveFocus());
        }

        auto* software = item("allSoftwareAction");
        QVERIFY(software);
        software->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_VERIFY(item("libraryScreen")->isVisible());
        QVERIFY(capture(QStringLiteral("all-software")));
        app.appearance()->setTheme(QStringLiteral("light"));
        QVERIFY(capture(QStringLiteral("all-software-light")));
        app.appearance()->setTheme(QStringLiteral("dark"));

        if (library->count() > 0) {
            auto* grid = item("allSoftwareGrid");
            QVERIFY(grid && grid->hasActiveFocus());
            QVERIFY(app.setSetting(QStringLiteral("options_key"), 2));
            QTest::keyClick(window, Qt::Key_F10);
            QTRY_VERIFY(item("softwareOptionsDialog") && item("softwareOptionsDialog")->isVisible());
            QTest::keyClick(window, Qt::Key_Escape);
            QTRY_VERIFY(grid->hasActiveFocus());
            QVERIFY(app.setSetting(QStringLiteral("options_key"), 0));
        }
        auto* controls = item("libraryControlsAction");
        QVERIFY(controls);
        controls->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_VERIFY(item("searchGamesField") && item("searchGamesField")->hasActiveFocus());
        QVERIFY(capture(QStringLiteral("library-controls")));
        if (library->count() > 0) {
            const auto game_count = library->count();
            for (int i = 0; i < 5; ++i) {
                QTest::keyClick(window, Qt::Key_Z);
            }
            QTRY_COMPARE(library->count(), 0);
            QVERIFY(!library->homeGames().isEmpty());
            QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
            QTest::keyClick(window, Qt::Key_Backspace);
            QTRY_COMPARE(library->count(), game_count);
        }
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(controls->hasActiveFocus());
        if (library->count() > 12) {
            QTest::keyClick(window, Qt::Key_Down);
            auto* grid = item("allSoftwareGrid");
            QVERIFY(grid && grid->hasActiveFocus());
            const int columns = grid->property("columns").toInt();
            QVERIFY(columns > 0);
            for (int i = columns; i < library->count(); i += columns) {
                QTest::keyClick(window, Qt::Key_Down);
            }
            QCOMPARE(grid->property("currentIndex").toInt() / columns,
                     (library->count() - 1) / columns);
            QTRY_VERIFY(grid->property("contentY").toDouble() > 0);
            QVERIFY(capture(QStringLiteral("all-software-scroll")));
        }
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(item("homeScreen")->isVisible());

        auto* settings = item("settingsNavigation");
        QVERIFY(settings);
        settings->forceActiveFocus();
        QVERIFY(capture(QStringLiteral("home-utilities")));
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_VERIFY(item("settingsScreen")->isVisible());
        auto* categories = item("settingsCategories");
        auto* values = item("settingsValues");
        auto* footer = item("consoleFooter");
        auto* stage = item("consoleStage");
        QVERIFY(categories && values && footer && stage);
        QVERIFY(BoundsIn(categories, stage).right() <= BoundsIn(values, stage).left());
        QVERIFY(BoundsIn(values, stage).bottom() <= BoundsIn(footer, stage).top());
        QVERIFY(capture(QStringLiteral("settings")));
        QTest::keyClick(window, Qt::Key_Right);
        QTRY_VERIFY(values->hasActiveFocus());
        QVERIFY(capture(QStringLiteral("settings-control")));
        app.appearance()->setTheme(QStringLiteral("light"));
        QVERIFY(capture(QStringLiteral("settings-control-light")));
        app.appearance()->setTheme(QStringLiteral("dark"));
        QTest::keyClick(window, Qt::Key_Escape);
    }

    QSignalSpy validation_errors(&app, &ConsoleApp::error);
    QVERIFY(!app.resetGameSettings(QStringLiteral("../qt-config")));
    QCOMPARE(validation_errors.size(), 1);
    QVERIFY(!library->addFolder(QUrl::fromLocalFile(temp.filePath(QStringLiteral("missing")))));
    QCOMPARE(validation_errors.size(), 2);
    QTest::qWait(200);
    QTest::keyClick(window, Qt::Key_Escape);

    const auto playable = QGuiApplication::platformName() == QStringLiteral("offscreen")
                              ? QString{}
                              : qEnvironmentVariable("LICHII_CONSOLE_TEST_GAME");
    if (!playable.isEmpty()) {
        QSignalSpy errors(&app, &ConsoleApp::error);
        QFile invalid(temp.filePath(QStringLiteral("invalid.nro")));
        QVERIFY(invalid.open(QIODevice::WriteOnly));
        QCOMPARE(invalid.write("invalid"), 7);
        invalid.close();
        QTimer dismiss;
        connect(&dismiss, &QTimer::timeout, this, [] {
            if (auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                message->accept();
            }
        });
        dismiss.start(25);
        app.launch(invalid.fileName());
        QTRY_COMPARE(app.state(), QStringLiteral("idle"));
        QCOMPARE(errors.count(), 1);
        dismiss.stop();
        QTest::qWait(200);
        QTest::keyClick(app.window(), Qt::Key_Escape);

        QVERIFY(app.setSetting(
            QString::fromStdString(UISettings::values.confirm_before_stopping.GetLabel()), 2));
        QVERIFY(app.setSetting(
            QString::fromStdString(UISettings::values.pause_when_in_background.GetLabel()), true));
        QString program_id;
        for (int row = 0; row < library->count(); ++row) {
            const auto entry = library->get(row);
            if (entry.value(QStringLiteral("gamePath")).toString() == playable) {
                program_id = entry.value(QStringLiteral("programId")).toString();
                break;
            }
        }
        app.launch(playable, program_id);
        QTRY_COMPARE_WITH_TIMEOUT(app.state(), QStringLiteral("running"), 30000);
        QTRY_VERIFY(QGuiApplication::focusWindow() &&
                    QGuiApplication::focusWindow() != app.window());
        QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_F1);
        QTRY_COMPARE(app.state(), QStringLiteral("paused"));
        QTRY_COMPARE(QGuiApplication::focusWindow(), app.window());
        app.resume();
        QTRY_COMPARE(app.state(), QStringLiteral("running"));
        QTRY_VERIFY(QGuiApplication::focusWindow() &&
                    QGuiApplication::focusWindow() != app.window());
        app.stop();
        QTRY_COMPARE_WITH_TIMEOUT(app.state(), QStringLiteral("idle"), 20000);
        QTRY_COMPARE(QGuiApplication::focusWindow(), app.window());
    } else {
        QSignalSpy errors(&app, &ConsoleApp::error);
        app.launch(temp.filePath(QStringLiteral("missing.nro")), {});
        QCOMPARE(errors.count(), 1);
        QCOMPARE(app.state(), QStringLiteral("idle"));
    }
    QCOMPARE(RunLibraryChecks(*library, QDir::currentPath()), 0);
}

} // namespace

int RunSmokeTest(ConsoleApp& app) {
    ConsoleSmoke test(app);
    const int result = QTest::qExec(&test, QStringList{QStringLiteral("horizon-console")});
    std::fflush(stdout);
    app.quit();
    return result;
}

int RunSettingsReadback() {
    if (qEnvironmentVariable("XDG_CONFIG_HOME") !=
        QDir::current().filePath(QStringLiteral("config"))) {
        return 1;
    }

    QtConfig global;
    if (Settings::values.speed_limit.GetValue() != 125) {
        return 2;
    }
    QtConfig per_game("0100123456700000", Config::ConfigType::PerGameConfig);
    const auto speed = Settings::values.speed_limit.GetValue();
    Settings::RestoreGlobalState(false);
    if (speed != 165) {
        return 3;
    }
    QSettings console(
        QDir{QString::fromStdString(Common::FS::GetEdenPathString(Common::FS::EdenPath::ConfigDir))}
            .filePath(QStringLiteral("console.ini")), QSettings::IniFormat);
    return console.value(QStringLiteral("Appearance/Theme")) == QStringLiteral("light") &&
                   console.value(QStringLiteral("Appearance/Muted")).toBool()
               ? 0
               : 4;
}

} // namespace Horizon

#include "console_smoke.moc"
