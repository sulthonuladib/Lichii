// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <memory>
#include <QAbstractItemModelTester>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QQuickImageProvider>
#include <QSettings>
#include <QSignalSpy>
#include <QtTest/QTest>

#include "common/fs/path_util.h"
#include "lichii_console/library_model.h"
#include "lichii_console/library_smoke.h"

namespace Horizon {

int RunLibraryChecks(ConsoleLibrary& library, const QString& temp_path) {
    const QString config = QString::fromStdString(Common::FS::PathToUTF8String(
        Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir) / "console.ini"));
    const QString data = QString::fromStdString(
        Common::FS::PathToUTF8String(Common::FS::GetEdenPath(Common::FS::EdenPath::EdenDir)));
    const QString prefix = QDir::cleanPath(temp_path) + QLatin1Char('/');
    if (!config.startsWith(prefix) || !data.startsWith(prefix)) {
        qCritical("Library test requires an isolated profile");
        return 1;
    }

    const QAbstractItemModelTester tester(&library,
                                          QAbstractItemModelTester::FailureReportingMode::Fatal);
    const auto check = [](bool condition, const char* message) {
        if (!condition) {
            qCritical("Library test failed: %s", message);
        }
        return condition;
    };

    const auto folder = QDir(temp_path).filePath(QStringLiteral("spare-library"));
    if (!check(QTest::qWaitFor([&library] { return !library.scanning(); }),
               "finish the initial library scan") ||
        !check(QDir().mkpath(folder) && library.addFolder(QUrl::fromLocalFile(folder)),
               "add a real local library folder") ||
        !check(QTest::qWaitFor([&library] { return !library.scanning(); }) &&
                   library.folders().contains(folder),
               "refresh after adding a folder") ||
        !check(QDir().rmdir(folder), "remove the directory on disk") ||
        !check(QTest::qWaitFor([&library] { return !library.scanning(); }) &&
                   library.removeFolder(folder),
               "remove an unavailable folder from the library configuration")) {
        return 1;
    }

    const auto first_path = QDir(temp_path).filePath(QStringLiteral("first.nro"));
    const auto second_path = QDir(temp_path).filePath(QStringLiteral("second.nro"));
    const auto mock_title = QStringLiteral("River Pilot");
    const auto alternate_title = QStringLiteral("Alpine Loop");
    const QString title_role = QStringLiteral("title");
    const QString key_role = QStringLiteral("gameKey");
    const QString artwork_role = QStringLiteral("artwork");
    const QString custom_role = QStringLiteral("customArtwork");

    QImage embedded(16, 16, QImage::Format_RGB32);
    embedded.fill(Qt::blue);
    QByteArray embedded_bytes;
    QBuffer bytes(&embedded_bytes);
    bytes.open(QIODevice::WriteOnly);
    if (!check(embedded.save(&bytes, "PNG"), "generate embedded image")) {
        return 1;
    }
    const QVariantList fixtures{
        QVariantMap{
            {title_role, mock_title},
            {QStringLiteral("gamePath"), first_path},
            {QStringLiteral("embeddedArtwork"), embedded_bytes},
        },
        QVariantMap{
            {title_role, alternate_title},
            {QStringLiteral("gamePath"), second_path},
            {QStringLiteral("programId"), QStringLiteral("0100000000007700")},
            {QStringLiteral("publisher"), QStringLiteral("Library Fixtures")},
        },
    };
    library.setSearch({});
    library.setFavoritesOnly(false);
    library.setSortOrder(QStringLiteral("title"));
    library.setFixtureGames(fixtures);
    const QString key = library.get(1).value(key_role).toString();
    if (!check(library.count() == 2 && !key.isEmpty(), "populate two distinct fixtures") ||
        !check(library.get(0).value(title_role) == alternate_title, "sort by title")) {
        return 1;
    }

    library.setSearch(QStringLiteral("Fixtures"));
    if (!check(library.count() == 1 && library.homeGames().size() == 2,
               "filter by publisher while keeping HOME independent") ||
        !check(library.setFavorite(key, true), "favorite a title without a program ID")) {
        return 1;
    }
    library.setSearch({});
    library.setFavoritesOnly(true);
    if (!check(library.count() == 1 && library.get(0).value(key_role) == key, "favorites filter")) {
        return 1;
    }
    library.setFavoritesOnly(false);
    library.markPlayed(first_path);
    library.setSortOrder(QStringLiteral("recent"));
    if (!check(library.get(0).value(key_role) == key, "sort by last successful launch")) {
        return 1;
    }

    QSettings saved(config, QSettings::IniFormat);
    saved.sync();
    if (!check(saved.value(QStringLiteral("Library/Favorites/%1").arg(key)).toBool(),
               "read the saved favorite from the profile") ||
        !check(saved.value(QStringLiteral("Library/Played/%1").arg(key)).toDateTime().isValid(),
               "read the saved launch timestamp from the profile")) {
        return 1;
    }

    const auto missing_image = QDir(temp_path).filePath(QStringLiteral("missing.png"));
    const QSignalSpy errors(&library, &ConsoleLibrary::error);
    library.setArtwork(key, QUrl::fromLocalFile(missing_image));
    if (!check(QTest::qWaitFor([&library] { return !library.artworkBusy(); }) &&
                   errors.count() == 1,
               "reject a nonexistent image without removing the existing artwork")) {
        return 1;
    }
    const auto image_path = QDir(temp_path).filePath(QStringLiteral("cover.png"));
    QImage replacement(32, 16, QImage::Format_RGB32);
    replacement.fill(Qt::red);
    if (!check(replacement.save(image_path), "write test artwork")) {
        return 1;
    }
    library.setArtwork(key, QUrl::fromLocalFile(image_path));
    if (!check(QTest::qWaitFor([&library] { return !library.artworkBusy(); }) &&
                   library.getByKey(key).value(custom_role).toBool(),
               "apply a custom image asynchronously")) {
        return 1;
    }

    const std::unique_ptr<QQuickImageProvider> provider(library.createImageProvider());
    const auto rendered = [&] {
        const QString id = library.getByKey(key).value(artwork_role).toUrl().path().mid(1);
        return provider->requestImage(id, nullptr, QSize{64, 64});
    };
    const auto matches = [](const QImage& actual, const QImage& expected) {
        return actual.size() == expected.size() &&
               actual.pixelColor(0, 0) == expected.pixelColor(0, 0);
    };
    if (!check(matches(rendered(), replacement), "provide the custom image") ||
        !check(QFile::remove(image_path), "delete the imported source image")) {
        return 1;
    }
    library.setFixtureGames(fixtures);
    if (!check(library.getByKey(key).value(custom_role).toBool() &&
                   matches(rendered(), replacement),
               "restore the managed image when reloading the library")) {
        return 1;
    }
    library.resetArtwork(key);
    if (!check(QTest::qWaitFor([&library] { return !library.artworkBusy(); }) &&
                   !library.getByKey(key).value(custom_role).toBool() &&
                   matches(rendered(), embedded),
               "clear the override and restore embedded artwork")) {
        return 1;
    }
    return 0;
}

} // namespace Horizon
