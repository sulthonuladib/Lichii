// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include "lichii_console/appearance.h"
#include "lichii_console/library_model.h"

class MainWindow;
class QQmlApplicationEngine;
class QQuickWindow;

namespace Horizon {

class ConsoleApp final : public QObject {
    Q_OBJECT
    Q_PROPERTY(Horizon::ConsoleLibrary* library READ library CONSTANT)
    Q_PROPERTY(Horizon::Appearance* appearance READ appearance CONSTANT)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString profileName READ profileName NOTIFY settingsChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(bool gamepadHints READ gamepadHints NOTIFY gamepadHintsChanged)
    Q_PROPERTY(bool confirmSouth READ confirmSouth NOTIFY settingsChanged)
    Q_PROPERTY(QString optionsKey READ optionsKey NOTIFY settingsChanged)
    Q_PROPERTY(QVariantMap gamepadPrompts READ gamepadPrompts NOTIFY navigationPromptsChanged)
    Q_PROPERTY(bool networkKnown READ networkKnown NOTIFY hostStatusChanged)
    Q_PROPERTY(bool networkOnline READ networkOnline NOTIFY hostStatusChanged)
    Q_PROPERTY(int batteryLevel READ batteryLevel NOTIFY hostStatusChanged)
    Q_PROPERTY(bool batteryCharging READ batteryCharging NOTIFY hostStatusChanged)

public:
    explicit ConsoleApp(MainWindow& desktop);
    ~ConsoleApp() override;

    bool initialize(bool fullscreen);
    QQuickWindow* window() const;
    ConsoleLibrary* library();
    Appearance* appearance();

    QString state() const;
    QString profileName() const;
    QString version() const;
    bool gamepadHints() const;
    bool confirmSouth() const;
    QString optionsKey() const;
    QVariantMap gamepadPrompts() const;
    bool networkKnown() const;
    bool networkOnline() const;
    int batteryLevel() const;
    bool batteryCharging() const;

    Q_INVOKABLE void launch(const QString& game_path, const QString& program_id = {});
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void quit();
    Q_INVOKABLE void openDesktop();
    Q_INVOKABLE QVariantList settings(const QString& category, const QString& program_id = {});
    Q_INVOKABLE bool setSetting(const QString& key, const QVariant& value,
                                const QString& program_id = {});
    Q_INVOKABLE bool resetGameSettings(const QString& program_id);
    Q_INVOKABLE void openSaves(const QString& program_id, const QString& game_path = {});
    Q_INVOKABLE QUrl dataFolder(const QString& kind) const;
    Q_INVOKABLE void openData(const QString& kind);
    Q_INVOKABLE void configureController();
    Q_INVOKABLE void manageProfiles();
    Q_INVOKABLE void configureGame(const QString& program_id, const QString& game_path);

signals:
    void stateChanged();
    void settingsChanged();
    void gamepadHintsChanged();
    void navigationPromptsChanged();
    void hostStatusChanged();
    void error(const QString& message);

private:
    enum class State { Idle, Loading, Running, Paused, Stopping };

    bool eventFilter(QObject* watched, QEvent* event) override;
    void setState(State next);
    void openHome();
    void showConsole();
    void showGame();
    void pollController();
    void refreshHostStatus();
    void sendNavigationKey(int key);
    void setGamepadHints(bool enabled);
    void configureDesktopSection(const QString& section);

    MainWindow& desktop;
    QSettings preferences;
    ConsoleLibrary library_model;
    Appearance appearance_model;
    std::unique_ptr<QQmlApplicationEngine> engine;
    QQuickWindow* console_window = nullptr;
    QTimer controller_timer;
    QTimer host_status_timer;
    State session = State::Idle;
    bool desktop_visible = false;
    bool exiting = false;
    bool exit_requested = false;
    bool home_requested = false;
    bool gamepad_hints = false;
    bool sending_gamepad_key = false;
    unsigned controller_id = 0;
    bool network_known = false;
    bool network_online = false;
    int battery_level = -1;
    bool battery_charging = false;
    unsigned held_buttons = 0;
    int held_direction = 0;
    qint64 next_repeat = 0;
};

#ifdef LICHII_CONSOLE_TESTS
int RunSmokeTest(ConsoleApp& app);
int RunSettingsReadback();
#endif

} // namespace Horizon
