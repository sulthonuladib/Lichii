// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cmath>
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
#include <QNetworkInformation>
#include <QQmlApplicationEngine>
#include <QQuickImageProvider>
#include <QQuickWindow>
#include <QScopedValueRollback>
#include <SDL3/SDL.h>

#include "common/fs/path_util.h"
#include "common/scm_rev.h"
#include "common/settings.h"
#include "core/hle/service/acc/profile_manager.h"
#include "core/hle/service/am/applet_manager.h"
#include "lichii/bootmanager.h"
#include "lichii/game/game_list.h"
#include "lichii/main_window.h"
#include "lichii_console/console_app.h"
#include "lichii_console/console_settings.h"
#include "qt_common/config/qt_config.h"
#include "qt_common/game_list/model.h"
#include "qt_common/qt_common.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace Horizon {
namespace {

const QStringList& OptionsKeys() {
    static const QStringList keys{QStringLiteral("Menu"), QStringLiteral("F3"),
                                  QStringLiteral("F10")};
    return keys;
}

QString FaceButtonLabel(SDL_Gamepad* pad, SDL_GamepadButton button, const QString& fallback) {
    if (!pad) {
        return fallback;
    }
    switch (SDL_GetGamepadButtonLabel(pad, button)) {
    case SDL_GAMEPAD_BUTTON_LABEL_A: return QStringLiteral("A");
    case SDL_GAMEPAD_BUTTON_LABEL_B: return QStringLiteral("B");
    case SDL_GAMEPAD_BUTTON_LABEL_X: return QStringLiteral("X");
    case SDL_GAMEPAD_BUTTON_LABEL_Y: return QStringLiteral("Y");
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS: return QStringLiteral("×");
    case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE: return QStringLiteral("○");
    case SDL_GAMEPAD_BUTTON_LABEL_SQUARE: return QStringLiteral("□");
    case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE: return QStringLiteral("△");
    default: return fallback;
    }
}

QString ProfilePath(Common::FS::EdenPath path) {
    return QString::fromStdString(Common::FS::GetEdenPathString(path));
}

QString ProfileNameAt(size_t index) {
    Service::Account::ProfileBase profile{};
    if (!QtCommon::system->GetProfileManager().GetProfileBase(index, profile)) {
        return {};
    }
    const auto* const name = reinterpret_cast<const char*>(profile.username.data());
    return QString::fromUtf8(name, static_cast<qsizetype>(profile.username.size()))
        .section(QChar{}, 0, 0);
}

} // namespace

ConsoleApp::ConsoleApp(MainWindow& desktop_)
    : desktop{desktop_},
      preferences{QDir{ProfilePath(Common::FS::EdenPath::ConfigDir)}
                      .filePath(QStringLiteral("console.ini")), QSettings::IniFormat},
      library_model{*static_cast<GameListModel*>(desktop.game_list->GetModel())},
      appearance_model{preferences} {
    connect(&library_model, &ConsoleLibrary::error, this, &ConsoleApp::error);
    connect(&appearance_model, &Appearance::error, this, &ConsoleApp::error);
    connect(&library_model, &ConsoleLibrary::saveConfiguration, this,
            [this] { desktop.config->SaveAllValues(); });
    connect(&desktop, &MainWindow::EmulationPausedChanged, this, [this](bool paused) {
        if (desktop.emulation_running && session != State::Loading) {
            setState(paused ? State::Paused : State::Running);
        }
    });
    connect(&desktop, &MainWindow::EmulationStopping, this,
            [this] { setState(State::Stopping); });
    connect(&desktop, &MainWindow::EmulationEnded, this, [this] {
        QTimer::singleShot(0, this, [this] {
            if (desktop.boot_in_progress || desktop.emulation_running) {
                return;
            }
            setState(State::Idle);
            if (exit_requested) {
                quit();
            } else if (home_requested || !desktop_visible) {
                home_requested = false;
                openHome();
            }
        });
    });
    QNetworkInformation::loadDefaultBackend();
    if (auto* information = QNetworkInformation::instance()) {
        connect(information, &QNetworkInformation::reachabilityChanged, this,
                [this] { refreshHostStatus(); });
    }
    refreshHostStatus();
    connect(&host_status_timer, &QTimer::timeout, this, &ConsoleApp::refreshHostStatus);
    host_status_timer.start(30000);
}

ConsoleApp::~ConsoleApp() {
    qApp->removeEventFilter(this);
}

bool ConsoleApp::initialize(bool fullscreen) {
    engine = std::make_unique<QQmlApplicationEngine>();
    engine->setInitialProperties({{QStringLiteral("backend"), QVariant::fromValue(this)}});
    engine->addImageProvider(QStringLiteral("game-art"), library_model.createImageProvider());
    engine->load(QUrl(QStringLiteral("qrc:/horizon/qml/Main.qml")));
    if (engine->rootObjects().isEmpty()) {
        qCritical("Lichii console: the console interface could not be loaded");
        return false;
    }
    console_window = qobject_cast<QQuickWindow*>(engine->rootObjects().constFirst());
    if (!console_window) {
        qCritical("Lichii console: the console interface root is not a window");
        return false;
    }
    if (fullscreen || preferences.value(QStringLiteral("window/fullscreen"), false).toBool()) {
        console_window->showFullScreen();
    } else {
        console_window->show();
    }
    console_window->raise();
    console_window->requestActivate();
    QApplication::setQuitOnLastWindowClosed(false);
    qApp->installEventFilter(this);
    connect(&controller_timer, &QTimer::timeout, this, &ConsoleApp::pollController);
    controller_timer.start(16);
    return true;
}

QQuickWindow* ConsoleApp::window() const {
    return console_window;
}

ConsoleLibrary* ConsoleApp::library() {
    return &library_model;
}

Appearance* ConsoleApp::appearance() {
    return &appearance_model;
}

QString ConsoleApp::state() const {
    switch (session) {
    case State::Idle: return QStringLiteral("idle");
    case State::Loading: return QStringLiteral("loading");
    case State::Running: return QStringLiteral("running");
    case State::Paused: return QStringLiteral("paused");
    case State::Stopping: return QStringLiteral("stopping");
    }
    Q_UNREACHABLE();
}

QString ConsoleApp::profileName() const {
    const auto name = ProfileNameAt(static_cast<size_t>(Settings::values.current_user.GetValue()));
    return name.isEmpty() ? tr("Player") : name;
}

QString ConsoleApp::version() const {
    return QString::fromLatin1(Common::g_build_version);
}

bool ConsoleApp::gamepadHints() const {
    return gamepad_hints;
}

bool ConsoleApp::confirmSouth() const {
    return preferences.value(QStringLiteral("navigation/confirmSouth"), false).toBool();
}

QString ConsoleApp::optionsKey() const {
    const auto& keys = OptionsKeys();
    const auto key = preferences.value(QStringLiteral("navigation/optionsKey"), keys.first()).toString();
    return keys.contains(key) ? key : keys.first();
}

QVariantMap ConsoleApp::gamepadPrompts() const {
    auto* pad = SDL_GetGamepadFromID(controller_id);
    QString options = tr("Start");
    QString home = tr("Guide");
    switch (pad ? SDL_GetGamepadType(pad) : SDL_GAMEPAD_TYPE_UNKNOWN) {
    case SDL_GAMEPAD_TYPE_XBOXONE:
        options = tr("Menu");
        [[fallthrough]];
    case SDL_GAMEPAD_TYPE_XBOX360:
        home = tr("Xbox");
        break;
    case SDL_GAMEPAD_TYPE_PS4:
    case SDL_GAMEPAD_TYPE_PS5:
        options = tr("Options");
        [[fallthrough]];
    case SDL_GAMEPAD_TYPE_PS3:
        home = tr("PS");
        break;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        options = QStringLiteral("+");
        home = tr("Home");
        break;
    default:
        break;
    }
    const auto east = FaceButtonLabel(pad, SDL_GAMEPAD_BUTTON_EAST, tr("East"));
    const auto south = FaceButtonLabel(pad, SDL_GAMEPAD_BUTTON_SOUTH, tr("South"));
    const bool south_confirms = confirmSouth();
    return {{QStringLiteral("confirm"), south_confirms ? south : east},
            {QStringLiteral("back"), south_confirms ? east : south},
            {QStringLiteral("options"), options},
            {QStringLiteral("home"), home}};
}

bool ConsoleApp::networkKnown() const {
    return network_known;
}

bool ConsoleApp::networkOnline() const {
    return network_online;
}

int ConsoleApp::batteryLevel() const {
    return battery_level;
}

bool ConsoleApp::batteryCharging() const {
    return battery_charging;
}

void ConsoleApp::refreshHostStatus() {
    bool known = false;
    bool online = false;
    if (const auto* information = QNetworkInformation::instance();
        information && information->supports(QNetworkInformation::Feature::Reachability)) {
        const auto reachability = information->reachability();
        known = reachability != QNetworkInformation::Reachability::Unknown;
        online = reachability == QNetworkInformation::Reachability::Online;
    }

    int level = -1;
    bool charging = false;
#ifdef _WIN32
    SYSTEM_POWER_STATUS power{};
    if (GetSystemPowerStatus(&power) && !(power.BatteryFlag & 128)) {
        if (power.BatteryLifePercent != 255) {
            level = power.BatteryLifePercent;
        }
        charging = power.ACLineStatus == 1;
    }
#elif defined(__linux__)
    const QDir supplies{QStringLiteral("/sys/class/power_supply")};
    for (const auto& name : supplies.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QDir supply{supplies.filePath(name)};
        QFile type{supply.filePath(QStringLiteral("type"))};
        if (!type.open(QIODevice::ReadOnly) ||
            type.readAll().trimmed() != QByteArrayLiteral("Battery")) {
            continue;
        }
        QFile capacity{supply.filePath(QStringLiteral("capacity"))};
        if (!capacity.open(QIODevice::ReadOnly)) {
            continue;
        }
        bool valid = false;
        const int value = capacity.readAll().trimmed().toInt(&valid);
        if (!valid) {
            continue;
        }
        level = std::clamp(value, 0, 100);
        QFile status{supply.filePath(QStringLiteral("status"))};
        if (status.open(QIODevice::ReadOnly)) {
            const auto state = status.readAll().trimmed();
            charging = state == QByteArrayLiteral("Charging") || state == QByteArrayLiteral("Full");
        }
        break;
    }
#endif

    if (known == network_known && online == network_online && level == battery_level &&
        charging == battery_charging) {
        return;
    }
    network_known = known;
    network_online = online;
    battery_level = level;
    battery_charging = charging;
    emit hostStatusChanged();
}

void ConsoleApp::setGamepadHints(bool enabled) {
    if (gamepad_hints != enabled) {
        gamepad_hints = enabled;
        emit gamepadHintsChanged();
        emit navigationPromptsChanged();
    }
}

void ConsoleApp::setState(State next) {
    if (session != next) {
        session = next;
        emit stateChanged();
    }
}

void ConsoleApp::launch(const QString& game_path, const QString& program_id) {
    if (session != State::Idle || desktop_visible) {
        return;
    }
    if (!QFileInfo::exists(game_path)) {
        emit error(tr("The game file could not be found. Refresh your library and try again."));
        return;
    }

    setState(State::Loading);
    QTimer::singleShot(0, this, [this, game_path, program_id] {
        showGame();
        auto parameters = desktop.ApplicationAppletParameters();
        parameters.program_id = ParseProgramId(program_id);
        desktop.BootGame(game_path, parameters);
        if (!desktop.emulation_running) {
            Settings::RestoreGlobalState(QtCommon::system->IsPoweredOn());
            desktop.hide();
            showConsole();
            setState(State::Idle);
            emit error(tr("Lichii could not start this game. Check its file, firmware, and keys."));
            return;
        }
        const auto id = QtCommon::system->GetApplicationProcessProgramID();
        library_model.markPlayed(game_path, id ? QStringLiteral("%1").arg(id, 16, 16, u'0').toUpper()
                                               : program_id);
        setState(State::Running);
    });
}

void ConsoleApp::pause() {
    if (session == State::Running && QtCommon::emu_thread) {
        desktop.auto_paused = false;
        desktop.OnPauseGame();
    }
}

void ConsoleApp::resume() {
    if (session != State::Paused || !QtCommon::emu_thread) {
        return;
    }
    showGame();
    desktop.OnStartGame();
}

void ConsoleApp::stop() {
    if (session != State::Running && session != State::Paused) {
        return;
    }
    showGame();
    desktop.OnStopGame();
    if (desktop.emulation_running && (session == State::Running || session == State::Paused)) {
        exit_requested = false;
        home_requested = false;
        if (session == State::Paused) {
            showConsole();
        }
    }
}

void ConsoleApp::quit() {
    if (session == State::Loading) {
        return;
    }
    if (session != State::Idle) {
        exit_requested = true;
        stop();
        return;
    }
    exiting = true;
    desktop.close();
    QCoreApplication::exit(0);
}

void ConsoleApp::openDesktop() {
    if (session != State::Idle) {
        emit error(tr("Stop the game before switching to desktop mode."));
        return;
    }
    desktop_visible = true;
    library_model.refresh();
    showGame();
}

void ConsoleApp::openHome() {
    if (desktop_visible && desktop.emulation_running) {
        return;
    }
    if (desktop_visible) {
        desktop_visible = false;
        library_model.refresh();
    } else if (session == State::Running) {
        pause();
    } else if (session != State::Paused && session != State::Idle) {
        return;
    }
    if (session == State::Idle) {
        desktop.hide();
    }
    showConsole();
}

void ConsoleApp::showConsole() {
    if (!console_window) {
        return;
    }
    const bool over_game = session == State::Running || session == State::Paused;
    console_window->setTransientParent(over_game ? desktop.windowHandle() : nullptr);
    console_window->show();
    console_window->raise();
    console_window->requestActivate();
}

void ConsoleApp::showGame() {
    console_window->hide();
    desktop.show();
    desktop.raise();
    if (desktop.render_window->isWindow() && desktop.render_window->isVisible()) {
        desktop.render_window->activateWindow();
    } else {
        desktop.activateWindow();
    }
}

QVariantList ConsoleApp::settings(const QString& category, const QString& program_id) {
    if (session != State::Idle && !program_id.isEmpty()) {
        return {};
    }

    auto rows = ReadEmulatorSettings(category, program_id);
    for (auto& row : rows) {
        auto map = row.toMap();
        map.insert(QStringLiteral("enabled"), session == State::Idle);
        row = map;
    }
    if (!program_id.isEmpty()) {
        return rows;
    }

    const auto choice = [&](const QString& key, const QString& name, int value,
                            const QVariantList& options) {
        rows.push_back(QVariantMap{{QStringLiteral("key"), key},
                                   {QStringLiteral("label"), name},
                                   {QStringLiteral("type"), QStringLiteral("choice")},
                                   {QStringLiteral("value"), value},
                                   {QStringLiteral("options"), options},
                                   {QStringLiteral("enabled"), session == State::Idle}});
    };
    if (category == QStringLiteral("controllers")) {
        choice(QStringLiteral("controller_confirm"), tr("Confirm button"),
               confirmSouth() ? 1 : 0,
               {tr("East"), tr("South")});
        const auto& keys = OptionsKeys();
        QVariantList key_choices;
        for (const auto& key : keys) {
            key_choices.push_back(key);
        }
        choice(QStringLiteral("options_key"), tr("Software options key"),
               static_cast<int>(keys.indexOf(optionsKey())), key_choices);
    } else if (category == QStringLiteral("users")) {
        QVariantList profiles;
        const auto count = QtCommon::system->GetProfileManager().GetUserCount();
        for (size_t i = 0; i < count; ++i) {
            profiles.push_back(ProfileNameAt(i));
        }
        choice(QStringLiteral("current_user"), tr("Active local profile"),
               Settings::values.current_user.GetValue(), profiles);
    } else if (category == QStringLiteral("interface")) {
        rows.push_back(QVariantMap{
            {QStringLiteral("key"), QStringLiteral("start_fullscreen")},
            {QStringLiteral("label"), tr("Start Console in fullscreen")},
            {QStringLiteral("type"), QStringLiteral("toggle")},
            {QStringLiteral("value"), preferences.value(QStringLiteral("window/fullscreen"), false)},
            {QStringLiteral("enabled"), session == State::Idle},
            {QStringLiteral("timing"), tr("Applies when Lichii restarts")},
        });
    }
    return rows;
}

bool ConsoleApp::setSetting(const QString& key, const QVariant& value, const QString& program_id) {
    if (session != State::Idle || desktop_visible) {
        emit error(tr("Stop the game before changing emulator settings."));
        return false;
    }
    if (program_id.isEmpty() && key == QStringLiteral("controller_confirm")) {
        bool valid{};
        const auto index = value.toInt(&valid);
        if (!valid || index < 0 || index > 1) {
            return false;
        }
        preferences.setValue(QStringLiteral("navigation/confirmSouth"), index == 1);
        preferences.sync();
    } else if (program_id.isEmpty() && key == QStringLiteral("options_key")) {
        const auto& keys = OptionsKeys();
        bool valid{};
        const auto index = value.toInt(&valid);
        if (!valid || index < 0 || index >= keys.size()) {
            return false;
        }
        preferences.setValue(QStringLiteral("navigation/optionsKey"), keys[index]);
        preferences.sync();
    } else if (program_id.isEmpty() && key == QStringLiteral("start_fullscreen")) {
        preferences.setValue(QStringLiteral("window/fullscreen"), value.toBool());
        preferences.sync();
    } else if (program_id.isEmpty() && key == QStringLiteral("current_user")) {
        bool valid{};
        const auto index = value.toInt(&valid);
        if (!valid || index < 0 || static_cast<size_t>(index) >=
                                        QtCommon::system->GetProfileManager().GetUserCount()) {
            return false;
        }
        Settings::values.current_user = index;
        desktop.config->SaveAllValues();
    } else if (!WriteEmulatorSetting(*desktop.config, key, value, program_id)) {
        emit error(tr("The setting could not be changed."));
        return false;
    }
    if (preferences.status() != QSettings::NoError) {
        emit error(tr("Console preferences could not be saved."));
        return false;
    }
    emit settingsChanged();
    if (key == QStringLiteral("controller_confirm")) {
        emit navigationPromptsChanged();
    }
    return true;
}

bool ConsoleApp::resetGameSettings(const QString& program_id) {
    if (session != State::Idle || !RemoveEmulatorOverrides(program_id)) {
        emit error(tr("Game settings could not be reset. Stop the game and try again."));
        return false;
    }
    emit settingsChanged();
    return true;
}

void ConsoleApp::openSaves(const QString& program_id, const QString& game_path) {
    const auto id = ParseProgramId(program_id);
    if (!id) {
        emit error(tr("This game has no program ID for save lookup."));
        return;
    }
    desktop.OpenGameFolder(id, GameListOpenTarget::SaveData, game_path.toStdString(), false);
}

QUrl ConsoleApp::dataFolder(const QString& kind) const {
    using Common::FS::EdenPath;
    auto path = EdenPath::EdenDir;
    if (kind == QStringLiteral("screenshots")) {
        path = EdenPath::ScreenshotsDir;
    } else if (kind == QStringLiteral("log")) {
        path = EdenPath::LogDir;
    } else if (kind == QStringLiteral("saves")) {
        path = EdenPath::SaveDir;
    }
    return QUrl::fromLocalFile(ProfilePath(path));
}

void ConsoleApp::openData(const QString& kind) {
    if (!QDesktopServices::openUrl(dataFolder(kind))) {
        emit error(tr("The folder could not be opened."));
    }
}

void ConsoleApp::configureDesktopSection(const QString& section) {
    if (session != State::Idle || desktop_visible) {
        emit error(tr("Stop the game before opening configuration."));
        return;
    }
    desktop.Configure(section);
    emit settingsChanged();
    console_window->requestActivate();
}

void ConsoleApp::configureController() {
    configureDesktopSection(QStringLiteral("controllers"));
}

void ConsoleApp::manageProfiles() {
    configureDesktopSection(QStringLiteral("users"));
}

void ConsoleApp::configureGame(const QString& program_id, const QString& game_path) {
    if (session != State::Idle || desktop_visible) {
        emit error(tr("Stop the game before opening its configuration."));
        return;
    }
    const auto id = ParseProgramId(program_id);
    if (!id || !QFileInfo::exists(game_path)) {
        emit error(tr("Game properties are unavailable for this entry."));
        return;
    }
    desktop.OpenPerGameConfiguration(id, game_path.toStdString());
    emit settingsChanged();
    console_window->requestActivate();
}

bool ConsoleApp::eventFilter(QObject* watched, QEvent* event) {
    if (exiting) {
        return false;
    }
    if (!sending_gamepad_key &&
        (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride ||
         event->type() == QEvent::MouseButtonPress || event->type() == QEvent::TouchBegin)) {
        setGamepadHints(false);
    }
    if (event->type() == QEvent::Close && watched == &desktop) {
        event->ignore();
        if (!desktop.emulation_running && session == State::Idle) {
            openHome();
        } else {
            home_requested = true;
            stop();
        }
        return true;
    }
    if (event->type() == QEvent::KeyPress && QApplication::activeModalWidget() == nullptr) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_F1 && !key->isAutoRepeat() &&
            QGuiApplication::focusWindow() != console_window &&
            (session == State::Running || session == State::Paused ||
                                           (desktop_visible && !desktop.emulation_running))) {
            openHome();
            return true;
        }
    }
    return false;
}

void ConsoleApp::sendNavigationKey(int key) {
    setGamepadHints(true);
    const QScopedValueRollback guard{sending_gamepad_key, true};
    QKeyEvent press{QEvent::KeyPress, key, Qt::NoModifier};
    QKeyEvent release{QEvent::KeyRelease, key, Qt::NoModifier};
    QCoreApplication::sendEvent(console_window, &press);
    QCoreApplication::sendEvent(console_window, &release);
}

void ConsoleApp::pollController() {
    if (QApplication::activeModalWidget() ||
        QGuiApplication::applicationState() != Qt::ApplicationActive) {
        held_direction = 0;
        return;
    }
    auto* pad = SDL_GetGamepadFromID(controller_id);
    if (!pad || !SDL_GamepadConnected(pad)) {
        controller_id = 0;
        setGamepadHints(false);
        int count = 0;
        auto* devices = SDL_GetGamepads(&count);
        for (int i = 0; i < count; ++i) {
            if (auto* candidate = SDL_GetGamepadFromID(devices[i])) {
                pad = candidate;
                controller_id = devices[i];
                break;
            }
        }
        SDL_free(devices);
        held_buttons = 0;
        held_direction = 0;
        if (!pad || !SDL_GamepadConnected(pad)) {
            return;
        }
    }
    const auto is_down = [pad](SDL_GamepadButton button) {
        return SDL_GetGamepadButton(pad, button);
    };
    const bool south = confirmSouth();
    const unsigned buttons =
        unsigned(is_down(south ? SDL_GAMEPAD_BUTTON_SOUTH : SDL_GAMEPAD_BUTTON_EAST)) |
        (unsigned(is_down(south ? SDL_GAMEPAD_BUTTON_EAST : SDL_GAMEPAD_BUTTON_SOUTH)) << 1) |
        (unsigned(is_down(SDL_GAMEPAD_BUTTON_GUIDE)) << 2) |
        (unsigned(is_down(SDL_GAMEPAD_BUTTON_START)) << 3);
    const auto pressed = buttons & ~held_buttons;
    held_buttons = buttons;

    const bool console_active = QGuiApplication::focusWindow() == console_window;
    if (pressed & 4) {
        if (console_active) {
            sendNavigationKey(Qt::Key_F1);
        } else {
            setGamepadHints(true);
            openHome();
        }
    }
    if (!console_active) {
        held_direction = 0;
        return;
    }
    if (pressed & 1) {
        sendNavigationKey(Qt::Key_Return);
    } else if (pressed & 2) {
        sendNavigationKey(Qt::Key_Escape);
    } else if (pressed & 8) {
        sendNavigationKey(Qt::Key_Menu);
    }

    constexpr int threshold = 16000;
    const int x = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
    const int y = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
    int direction = 0;
    if (is_down(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) {
        direction = Qt::Key_Left;
    } else if (is_down(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) {
        direction = Qt::Key_Right;
    } else if (is_down(SDL_GAMEPAD_BUTTON_DPAD_UP)) {
        direction = Qt::Key_Up;
    } else if (is_down(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
        direction = Qt::Key_Down;
    } else if (std::abs(x) > std::abs(y) && std::abs(x) > threshold) {
        direction = x < 0 ? Qt::Key_Left : Qt::Key_Right;
    } else if (std::abs(y) > threshold) {
        direction = y < 0 ? Qt::Key_Up : Qt::Key_Down;
    }
    const auto now = static_cast<qint64>(SDL_GetTicks());
    if (direction && (direction != held_direction || now >= next_repeat)) {
        sendNavigationKey(direction);
        next_repeat = now + (direction == held_direction ? 95 : 315);
    }
    held_direction = direction;
}

} // namespace Horizon
