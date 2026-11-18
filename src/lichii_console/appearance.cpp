// SPDX-FileCopyrightText: Copyright 2026 Lichii contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cmath>
#include <utility>

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QSettings>
#include <QSoundEffect>
#include <QStringList>
#include <QStyleHints>

#include "appearance.h"

namespace Horizon {
namespace {

constexpr auto FocusIntervalMs = 76;
constexpr auto MaxSoundPackFileBytes = 512 * 1024;
constexpr auto MaxIconPackFileBytes = 2 * 1024 * 1024;
constexpr auto DefaultVolume = 0.35;

const auto FocusSound = QStringLiteral("focus");
const QStringList SoundNames{
    FocusSound,
    QStringLiteral("confirm"),
    QStringLiteral("cancel"),
    QStringLiteral("open"),
    QStringLiteral("options"),
    QStringLiteral("warning"),
    QStringLiteral("home"),
};
const QStringList IconNames{
    QStringLiteral("home"),          QStringLiteral("library"),  QStringLiteral("settings"),
    QStringLiteral("controllers"),   QStringLiteral("profile"),  QStringLiteral("screenshots"),
    QStringLiteral("notifications"), QStringLiteral("power"),    QStringLiteral("appearance"),
    QStringLiteral("about"),         QStringLiteral("folder"),   QStringLiteral("play"),
    QStringLiteral("back"),          QStringLiteral("favorite"), QStringLiteral("search"),
    QStringLiteral("artwork"),       QStringLiteral("game"),     QStringLiteral("check"),
    QStringLiteral("cover"),         QStringLiteral("console"),  QStringLiteral("wifi"),
    QStringLiteral("wifi-off"),      QStringLiteral("desktop"),
};

bool IsTheme(const QString& value) {
    return value == QStringLiteral("dark") || value == QStringLiteral("light") ||
           value == QStringLiteral("system");
}

qreal ClampVolume(qreal value) {
    return std::isfinite(value) ? std::clamp(value, qreal{0}, qreal{1}) : DefaultVolume;
}

QUrl ReadPack(const QSettings& settings, const QString& key) {
    const auto path = settings.value(key).toString();
    return QDir::isAbsolutePath(path) ? QUrl::fromLocalFile(path) : QUrl{};
}

QUrl PackFile(const QUrl& directory, const QString& name, qint64 max_size) {
    if (!directory.isLocalFile()) {
        return {};
    }
    const QFileInfo file{QDir{directory.toLocalFile()}.filePath(name)};
    if (!file.isFile() || !file.isReadable() || file.size() <= 0 || file.size() > max_size) {
        return {};
    }
    return QUrl::fromLocalFile(file.absoluteFilePath());
}

QUrl BundledSound(const QString& name) {
    return QUrl{QStringLiteral("qrc:/horizon/assets/sounds/%1.wav").arg(name)};
}

} // namespace

Appearance::Appearance(QSettings& settings_, QObject* parent)
    : QObject{parent}, settings{settings_},
      current_theme{
          settings.value(QStringLiteral("Appearance/Theme"), QStringLiteral("dark")).toString()},
      sound_volume{ClampVolume(
          settings.value(QStringLiteral("Appearance/SoundVolume"), DefaultVolume).toDouble())},
      sound_muted{settings.value(QStringLiteral("Appearance/Muted"), false).toBool()},
      reduced_motion{settings.value(QStringLiteral("Appearance/ReducedMotion"), false).toBool()},
      icon_directory{ReadPack(settings, QStringLiteral("Appearance/IconPack"))},
      sound_directory{ReadPack(settings, QStringLiteral("Appearance/SoundPack"))} {
    if (!IsTheme(current_theme)) {
        current_theme = QStringLiteral("dark");
    }
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (current_theme == QStringLiteral("system")) {
            emit effectiveThemeChanged();
        }
    });
    ReloadSounds();
}

QString Appearance::theme() const {
    return current_theme;
}

QString Appearance::effectiveTheme() const {
    if (current_theme != QStringLiteral("system")) {
        return current_theme;
    }
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Light
               ? QStringLiteral("light")
               : QStringLiteral("dark");
}

qreal Appearance::soundVolume() const {
    return sound_volume;
}

bool Appearance::muted() const {
    return sound_muted;
}

bool Appearance::reducedMotion() const {
    return reduced_motion;
}

QUrl Appearance::iconPack() const {
    return icon_directory;
}

QUrl Appearance::soundPack() const {
    return sound_directory;
}

void Appearance::setTheme(const QString& value) {
    if (current_theme == value || !IsTheme(value)) {
        return;
    }
    const auto previous = effectiveTheme();
    current_theme = value;
    settings.setValue(QStringLiteral("Appearance/Theme"), value);
    emit themeChanged();
    if (previous != effectiveTheme()) {
        emit effectiveThemeChanged();
    }
}

void Appearance::setSoundVolume(qreal value) {
    value = ClampVolume(value);
    if (sound_volume == value) {
        return;
    }
    sound_volume = value;
    settings.setValue(QStringLiteral("Appearance/SoundVolume"), value);
    for (auto* effect : std::as_const(sounds)) {
        effect->setVolume(static_cast<float>(value * value));
    }
    emit soundVolumeChanged();
}

void Appearance::setMuted(bool value) {
    if (sound_muted == value) {
        return;
    }
    sound_muted = value;
    settings.setValue(QStringLiteral("Appearance/Muted"), value);
    if (value && active_sound) {
        active_sound->stop();
    }
    emit mutedChanged();
}

void Appearance::setReducedMotion(bool value) {
    if (reduced_motion == value) {
        return;
    }
    reduced_motion = value;
    settings.setValue(QStringLiteral("Appearance/ReducedMotion"), value);
    emit reducedMotionChanged();
}

void Appearance::setIconPack(const QUrl& directory) {
    if (SetPack(directory, icon_directory, QStringLiteral("Appearance/IconPack"))) {
        emit iconPackChanged();
    }
}

void Appearance::setSoundPack(const QUrl& directory) {
    if (SetPack(directory, sound_directory, QStringLiteral("Appearance/SoundPack"))) {
        ReloadSounds();
        emit soundPackChanged();
    }
}

bool Appearance::SetPack(const QUrl& directory, QUrl& current, const QString& setting) {
    QUrl canonical;
    if (!directory.isEmpty()) {
        const QFileInfo folder{directory.toLocalFile()};
        if (!directory.isLocalFile() || !folder.isDir() || !folder.isReadable()) {
            emit error(tr("Choose a readable local folder for the asset pack."));
            return false;
        }
        canonical = QUrl::fromLocalFile(folder.canonicalFilePath());
    }
    if (canonical == current) {
        return false;
    }
    current = canonical;
    settings.setValue(setting, canonical.toLocalFile());
    return true;
}

QUrl Appearance::icon(const QString& name, const QString& theme, const QUrl& pack) const {
    if (!IconNames.contains(name)) {
        return {};
    }
    const auto palette =
        theme == QStringLiteral("light") ? QStringLiteral("light") : QStringLiteral("dark");
    const QStringList candidates{
        QStringLiteral("icons/%1/%2.svg").arg(palette, name),
        QStringLiteral("icons/%1/%2.png").arg(palette, name),
        QStringLiteral("icons/%1.svg").arg(name),
        QStringLiteral("icons/%1.png").arg(name),
    };
    for (const auto& candidate : candidates) {
        const auto source = PackFile(pack, candidate, MaxIconPackFileBytes);
        if (!source.isEmpty()) {
            QImageReader reader{source.toLocalFile()};
            const auto dimensions = reader.size();
            if (dimensions.isValid() && dimensions.width() <= 2048 && dimensions.height() <= 2048 &&
                reader.canRead()) {
                return source;
            }
        }
    }
    if (name == QStringLiteral("cover")) {
        return QUrl{QStringLiteral("qrc:/horizon/assets/cover.svg")};
    }
    return QUrl{QStringLiteral("qrc:/horizon/assets/icons/%1/%2.svg").arg(palette, name)};
}

void Appearance::ReloadSounds() {
    active_sound = nullptr;
    for (auto* effect : std::as_const(sounds)) {
        effect->stop();
        effect->deleteLater();
    }
    sounds.clear();
    for (const auto& name : SoundNames) {
        auto* effect = new QSoundEffect{this};
        effect->setVolume(static_cast<float>(sound_volume * sound_volume));
        const auto fallback = BundledSound(name);
        const auto source = PackFile(sound_directory, QStringLiteral("sounds/%1.wav").arg(name),
                                     MaxSoundPackFileBytes);
        connect(effect, &QSoundEffect::statusChanged, this, [effect, fallback] {
            if (effect->status() == QSoundEffect::Error && effect->source() != fallback) {
                effect->setSource(fallback);
            }
        });
        effect->setSource(source.isEmpty() ? fallback : source);
        sounds.insert(name, effect);
    }
}

void Appearance::play(const QString& name) {
    auto* effect = sounds.value(name);
    if (sound_muted || sound_volume == 0 || !effect || !effect->isLoaded()) {
        return;
    }
    const auto playing = active_sound && active_sound->isPlaying();
    if (name == FocusSound) {
        if (playing ||
            (last_focus_sound.isValid() && !last_focus_sound.hasExpired(FocusIntervalMs))) {
            return;
        }
        last_focus_sound.start();
    } else if (playing) {
        if (active_sound == effect) {
            return;
        }
        active_sound->stop();
    }
    active_sound = effect;
    active_sound->play();
}

} // namespace Horizon
