// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <optional>
#include <QCoreApplication>
#include <QVariantMap>
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "lichii_console/console_settings.h"
#include "qt_common/config/qt_config.h"
#include "qt_common/config/shared_translation.h"
#include "qt_common/config/uisettings.h"

namespace Horizon {
namespace {

struct SettingRow {
    const char* category;
    const char* label;
    Settings::BasicSetting& setting;
    bool global_only = false;
};

const SettingRow rows[] = {
    {"general", QT_TRANSLATE_NOOP("HorizonSettings", "Limit emulation speed"), Settings::values.use_speed_limit},
    {"general", QT_TRANSLATE_NOOP("HorizonSettings", "Speed limit (%)"), Settings::values.speed_limit},
    {"general", QT_TRANSLATE_NOOP("HorizonSettings", "Confirm before stopping"), UISettings::values.confirm_before_stopping, true},
    {"graphics", QT_TRANSLATE_NOOP("HorizonSettings", "Graphics API"), Settings::values.renderer_backend},
    {"graphics", QT_TRANSLATE_NOOP("HorizonSettings", "Resolution scale"), Settings::values.resolution_setup},
    {"graphics", QT_TRANSLATE_NOOP("HorizonSettings", "GPU accuracy"), Settings::values.gpu_accuracy},
    {"graphics", QT_TRANSLATE_NOOP("HorizonSettings", "Aspect ratio"), Settings::values.aspect_ratio},
    {"graphics", QT_TRANSLATE_NOOP("HorizonSettings", "Scaling filter"), Settings::values.scaling_filter},
    {"graphics", QT_TRANSLATE_NOOP("HorizonSettings", "Asynchronous shaders"), Settings::values.use_asynchronous_shaders},
    {"cpu", QT_TRANSLATE_NOOP("HorizonSettings", "Multicore emulation"), Settings::values.use_multi_core},
    {"cpu", QT_TRANSLATE_NOOP("HorizonSettings", "CPU accuracy"), Settings::values.cpu_accuracy},
    {"audio", QT_TRANSLATE_NOOP("HorizonSettings", "Game volume (%)"), Settings::values.volume},
    {"audio", QT_TRANSLATE_NOOP("HorizonSettings", "Guest audio mode"), Settings::values.sound_index},
    {"system", QT_TRANSLATE_NOOP("HorizonSettings", "Console mode"), Settings::values.use_docked_mode},
    {"system", QT_TRANSLATE_NOOP("HorizonSettings", "Guest language"), Settings::values.language_index},
    {"system", QT_TRANSLATE_NOOP("HorizonSettings", "Guest region"), Settings::values.region_index},
    {"system", QT_TRANSLATE_NOOP("HorizonSettings", "Guest time zone"), Settings::values.time_zone_index},
    {"interface", QT_TRANSLATE_NOOP("HorizonSettings", "Pause when unfocused"), UISettings::values.pause_when_in_background},
    {"interface", QT_TRANSLATE_NOOP("HorizonSettings", "Mute when unfocused"), UISettings::values.mute_when_in_background},
    {"advanced", QT_TRANSLATE_NOOP("HorizonSettings", "Disk shader cache"), Settings::values.use_disk_shader_cache},
    {"advanced", QT_TRANSLATE_NOOP("HorizonSettings", "Filesystem access logging"), Settings::values.enable_fs_access_log},
};

ConfigurationShared::ComboboxTranslations Choices(const Settings::BasicSetting& setting) {
    const auto choices = ConfigurationShared::ComboboxEnumeration(nullptr);
    const auto it = choices->find(setting.EnumIndex());
    if (it == choices->end()) {
        return {};
    }
    auto options = it->second;
    if (&setting == &Settings::values.renderer_backend) {
        std::erase_if(options, [](const auto& option) {
            switch (static_cast<Settings::RendererBackend>(option.first)) {
            case Settings::RendererBackend::Vulkan:
                return false;
#ifdef HAS_OPENGL
            case Settings::RendererBackend::OpenGL_GLSL:
                return false;
#endif
            default:
                return true;
            }
        });
    }
    return options;
}

std::filesystem::path OverridePath(const QString& program_id) {
    return Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir) / "custom" /
           fmt::format("{:016X}.ini", ParseProgramId(program_id));
}

} // namespace

qulonglong ParseProgramId(const QString& program_id) {
    if (program_id.size() != 16 ||
        std::ranges::any_of(program_id, [](QChar digit) {
            const QChar lower = digit.toLower();
            return !(digit >= u'0' && digit <= u'9') && !(lower >= u'a' && lower <= u'f');
        })) {
        return 0;
    }
    return program_id.toULongLong(nullptr, 16);
}

QVariantList ReadEmulatorSettings(const QString& category, const QString& program_id) {
    const bool game = !program_id.isEmpty();
    if (game && !ParseProgramId(program_id)) {
        return {};
    }

    SCOPE_EXIT {
        if (game) {
            Settings::RestoreGlobalState(false);
        }
    };
    std::optional<QtConfig> game_config;
    if (game && Common::FS::Exists(OverridePath(program_id))) {
        game_config.emplace(program_id.toUpper().toStdString(), Config::ConfigType::PerGameConfig);
    }

    QVariantList result;
    for (const auto& [group, label, setting, global_only] : rows) {
        if (category != QLatin1String(group) || (game && (global_only || !setting.Switchable()))) {
            continue;
        }
        const auto value =
            QString::fromStdString(game ? setting.ToString() : setting.ToStringGlobal());
        QVariantMap row{
            {QStringLiteral("key"), QString::fromStdString(setting.GetLabel())},
            {QStringLiteral("label"), QCoreApplication::translate("HorizonSettings", label)},
            {QStringLiteral("inherited"), game && setting.UsingGlobal()},
        };
        if (setting.TypeId() == "bool") {
            row.insert(QStringLiteral("type"), QStringLiteral("toggle"));
            row.insert(QStringLiteral("value"), value == QStringLiteral("true"));
        } else if (setting.IsEnum()) {
            const auto choices = Choices(setting);
            if (choices.empty()) {
                continue;
            }
            int index = -1;
            QVariantList options;
            for (const auto& [number, name] : choices) {
                if (value.toUInt() == number) {
                    index = static_cast<int>(options.size());
                }
                options.push_back(name);
            }
            row.insert(QStringLiteral("type"), QStringLiteral("choice"));
            row.insert(QStringLiteral("value"), index);
            row.insert(QStringLiteral("options"), options);
        } else {
            row.insert(QStringLiteral("type"), QStringLiteral("slider"));
            row.insert(QStringLiteral("value"), value.toInt());
            row.insert(QStringLiteral("min"), QString::fromStdString(setting.MinVal()).toInt());
            row.insert(QStringLiteral("max"), QString::fromStdString(setting.MaxVal()).toInt());
            row.insert(QStringLiteral("step"), 1);
        }
        result.push_back(row);
    }
    return result;
}

bool WriteEmulatorSetting(QtConfig& config, const QString& key, const QVariant& value,
                          const QString& program_id) {
    const auto it = std::ranges::find_if(rows, [&](const SettingRow& row) {
        return row.setting.GetLabel() == key.toStdString();
    });
    const bool game = !program_id.isEmpty();
    if (it == std::end(rows) ||
        (game && (!ParseProgramId(program_id) || it->global_only || !it->setting.Switchable()))) {
        return false;
    }

    auto& setting = it->setting;
    std::string serialized;
    const bool inherit = game && value.isNull();
    if (!inherit) {
        if (setting.TypeId() == "bool") {
            serialized = value.toBool() ? "true" : "false";
        } else {
            bool valid = false;
            const int number = value.toInt(&valid);
            if (!valid) {
                return false;
            }
            if (setting.IsEnum()) {
                const auto choices = Choices(setting);
                if (number < 0 || static_cast<size_t>(number) >= choices.size()) {
                    return false;
                }
                serialized = std::to_string(choices[number].first);
            } else {
                const auto minimum = std::stoi(setting.MinVal());
                const auto maximum = std::stoi(setting.MaxVal());
                serialized = std::to_string(std::clamp(number, minimum, maximum));
            }
        }
    }

    if (game) {
        SCOPE_EXIT { Settings::RestoreGlobalState(false); };
        QtConfig game_config(program_id.toUpper().toStdString(), Config::ConfigType::PerGameConfig);
        setting.SetGlobal(inherit);
        if (!inherit) {
            setting.LoadString(serialized);
        }
        game_config.SaveAllValues();
    } else {
        setting.LoadString(serialized);
        config.SaveAllValues();
    }
    return true;
}

bool RemoveEmulatorOverrides(const QString& program_id) {
    if (!ParseProgramId(program_id)) {
        return false;
    }
    const auto path = OverridePath(program_id);
    return !Common::FS::Exists(path) || Common::FS::RemoveFile(path);
}

} // namespace Horizon
