// SPDX-FileCopyrightText: Copyright 2026 Lichii contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QString>
#include <QUrl>

class QSettings;
class QSoundEffect;

namespace Horizon {

class Appearance final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString effectiveTheme READ effectiveTheme NOTIFY effectiveThemeChanged)
    Q_PROPERTY(qreal soundVolume READ soundVolume WRITE setSoundVolume NOTIFY soundVolumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(
        bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged)
    Q_PROPERTY(QUrl iconPack READ iconPack WRITE setIconPack NOTIFY iconPackChanged)
    Q_PROPERTY(QUrl soundPack READ soundPack WRITE setSoundPack NOTIFY soundPackChanged)

public:
    explicit Appearance(QSettings& settings, QObject* parent = nullptr);

    QString theme() const;
    QString effectiveTheme() const;
    qreal soundVolume() const;
    bool muted() const;
    bool reducedMotion() const;
    QUrl iconPack() const;
    QUrl soundPack() const;

    void setTheme(const QString& value);
    void setSoundVolume(qreal value);
    void setMuted(bool value);
    void setReducedMotion(bool value);
    void setIconPack(const QUrl& directory);
    void setSoundPack(const QUrl& directory);

    Q_INVOKABLE QUrl icon(const QString& name, const QString& theme, const QUrl& pack) const;
    Q_INVOKABLE void play(const QString& name);

signals:
    void themeChanged();
    void effectiveThemeChanged();
    void soundVolumeChanged();
    void mutedChanged();
    void reducedMotionChanged();
    void iconPackChanged();
    void soundPackChanged();
    void error(const QString& message);

private:
    bool SetPack(const QUrl& directory, QUrl& current, const QString& setting);
    void ReloadSounds();

    QSettings& settings;
    QString current_theme;
    qreal sound_volume;
    bool sound_muted;
    bool reduced_motion;
    QUrl icon_directory;
    QUrl sound_directory;
    QHash<QString, QSoundEffect*> sounds;
    QSoundEffect* active_sound{};
    QElapsedTimer last_focus_sound;
};

} // namespace Horizon
