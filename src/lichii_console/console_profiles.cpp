// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <QQmlApplicationEngine>
#include <QQuickImageProvider>
#include <QRegularExpression>
#include <QSaveFile>

#include "common/fs/path_util.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/hle/service/acc/profile_manager.h"
#include "lichii_console/console_app.h"
#include "qt_common/qt_common.h"

namespace Horizon {
namespace {

QString AvatarPath(const QString& id) {
    const auto path = Common::FS::GetEdenPath(Common::FS::EdenPath::NANDDir) /
                      "system/save/8000000000000010/su/avators";
    return QDir{QString::fromStdString(Common::FS::PathToUTF8String(path))}.filePath(
        id + QStringLiteral(".jpg"));
}

QString ProfileId(const Common::UUID& uuid) {
    return QString::fromStdString(uuid.FormattedString());
}

std::optional<size_t> FindProfile(const QString& id) {
    const auto& manager = QtCommon::system->GetProfileManager();
    for (size_t i = 0; i < manager.GetUserCount(); ++i) {
        const auto user = manager.GetUser(i);
        if (user && ProfileId(*user) == id) {
            return i;
        }
    }
    return std::nullopt;
}

QUrl AvatarUrl(const QString& id, unsigned revision) {
    if (id.isEmpty() || !QFileInfo::exists(AvatarPath(id))) {
        return {};
    }
    return QUrl{QStringLiteral("image://profile-avatar/%1?%2").arg(id).arg(revision)};
}

QImage ReadAvatar(const QString& path) {
    if (path.isEmpty() || QFileInfo(path).size() > 16 * 1024 * 1024) {
        return {};
    }
    QImageReader reader{path};
    reader.setAutoTransform(true);
    const auto size = reader.size();
    if (!size.isValid() || qint64(size.width()) * size.height() > 36'000'000) {
        return {};
    }
    if (size.width() > 512 || size.height() > 512) {
        reader.setScaledSize(size.scaled(512, 512, Qt::KeepAspectRatio));
    }
    auto image = reader.read();
    const auto side = std::min(image.width(), image.height());
    return image.isNull()
               ? QImage{}
               : image.copy((image.width() - side) / 2, (image.height() - side) / 2, side, side);
}

class ProfileAvatarProvider final : public QQuickImageProvider {
public:
    ProfileAvatarProvider() : QQuickImageProvider(Image) {}

    QImage requestImage(const QString& key, QSize* size, const QSize& requested) override {
        static const QRegularExpression uuid{
            QStringLiteral("^[a-fA-F0-9]{8}(?:-[a-fA-F0-9]{4}){3}-[a-fA-F0-9]{12}$")};
        const auto id = key.section(QLatin1Char('?'), 0, 0);
        if (!uuid.match(id).hasMatch()) {
            return {};
        }
        const auto image = ReadAvatar(AvatarPath(id));
        if (image.isNull()) {
            return {};
        }
        const int side = requested.isValid()
                             ? std::clamp(std::min(requested.width(), requested.height()), 1, 256)
                             : 256;
        QImage circle{side, side, QImage::Format_ARGB32_Premultiplied};
        circle.fill(Qt::transparent);
        QPainter painter{&circle};
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(
            QBrush{image.scaled(side, side, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)});
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(circle.rect());
        if (size) {
            *size = circle.size();
        }
        return circle;
    }
};

} // namespace

void ConsoleApp::registerProfileImages() {
    engine->addImageProvider(QStringLiteral("profile-avatar"), new ProfileAvatarProvider);
}

QUrl ConsoleApp::profileAvatar() const {
    const auto& manager = QtCommon::system->GetProfileManager();
    const auto user = manager.GetUser(Settings::values.current_user.GetValue());
    return user ? AvatarUrl(ProfileId(*user), avatar_revision) : QUrl{};
}

QVariantList ConsoleApp::profiles() const {
    const auto& manager = QtCommon::system->GetProfileManager();
    QVariantList profiles;
    const auto current = static_cast<size_t>(Settings::values.current_user.GetValue());
    for (size_t i = 0; i < manager.GetUserCount(); ++i) {
        Service::Account::ProfileBase profile{};
        if (!manager.GetProfileBase(i, profile)) {
            continue;
        }
        const auto id = ProfileId(profile.user_uuid);
        const auto name = QString::fromUtf8(reinterpret_cast<const char*>(profile.username.data()),
                                            profile.username.size())
                              .section(QChar{}, 0, 0);
        profiles.push_back(QVariantMap{{QStringLiteral("id"), id},
                                       {QStringLiteral("name"), name},
                                       {QStringLiteral("active"), i == current},
                                       {QStringLiteral("avatar"), AvatarUrl(id, avatar_revision)}});
    }
    return profiles;
}

QUrl ConsoleApp::avatarFolder() const {
    return QUrl::fromLocalFile(
        preferences.value(QStringLiteral("profiles/avatarFolder")).toString());
}

void ConsoleApp::setAvatarFolder(const QUrl& url) {
    if (!url.isLocalFile() || !QFileInfo(url.toLocalFile()).isDir()) {
        return;
    }
    preferences.setValue(QStringLiteral("profiles/avatarFolder"), url.toLocalFile());
    preferences.sync();
    emit settingsChanged();
}

bool ConsoleApp::saveProfileName(const QString& id, const QString& name) {
    if (session != State::Idle || desktop_visible) {
        return false;
    }
    const auto utf8 = name.trimmed().toUtf8();
    if (utf8.isEmpty() || utf8.contains('\0') ||
        utf8.size() > static_cast<qsizetype>(Service::Account::profile_username_size)) {
        return false;
    }
    auto& manager = QtCommon::system->GetProfileManager();
    if (id.isEmpty()) {
        if (manager.CreateNewUser(Common::UUID::MakeRandom(), utf8.toStdString()).IsError()) {
            return false;
        }
        manager.WriteUserSaveFile();
    } else {
        Service::Account::ProfileBase profile{};
        if (!manager.GetProfileBase(FindProfile(id), profile)) {
            return false;
        }
        profile.username.fill(0);
        std::copy(utf8.cbegin(), utf8.cend(), profile.username.begin());
        if (!manager.SetProfileBase(profile.user_uuid, profile)) {
            return false;
        }
    }
    emit settingsChanged();
    return true;
}

bool ConsoleApp::saveProfileAvatar(const QString& id, const QUrl& source) {
    if (session != State::Idle || desktop_visible || !FindProfile(id)) {
        return false;
    }
    auto path = source.toLocalFile();
    if (source.scheme() == QStringLiteral("qrc") &&
        source.path().startsWith(QStringLiteral("/horizon/assets/avatars/"))) {
        path = QLatin1Char(':') + source.path();
    }
    const auto image = ReadAvatar(path);
    const auto target = AvatarPath(id);
    if (image.isNull() || !QDir{}.mkpath(QFileInfo(target).absolutePath())) {
        return false;
    }
    QImage avatar{256, 256, QImage::Format_RGB32};
    avatar.fill(QColor{QStringLiteral("#dfe4e5")});
    QPainter painter{&avatar};
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(avatar.rect(), image);
    painter.end();
    QSaveFile file{target};
    if (!file.open(QIODevice::WriteOnly) || !avatar.save(&file, "JPG", 95) || !file.commit()) {
        return false;
    }
    ++avatar_revision;
    emit settingsChanged();
    return true;
}

bool ConsoleApp::activateProfile(const QString& id) {
    const auto index = FindProfile(id);
    return index && setSetting(QStringLiteral("current_user"), static_cast<int>(*index));
}

} // namespace Horizon
