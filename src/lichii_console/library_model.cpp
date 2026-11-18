// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <utility>

#include <QBuffer>
#include <QCollator>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QImageReader>
#include <QQuickImageProvider>
#include <QReadWriteLock>
#include <QSaveFile>
#include <QSet>
#include <QtConcurrentRun>

#include "common/fs/path_util.h"
#include "lichii_console/library_model.h"
#include "qt_common/game_list/game_list_p.h"
#include "qt_common/game_list/model.h"
#include "qt_common/util/game.h"

namespace Horizon {

struct LibraryArtwork {
    struct Image {
        QByteArray embedded;
        QString override_path;
    };

    QReadWriteLock lock;
    QHash<QString, Image> images;
};

namespace {

constexpr auto MaxArtworkBytes = 16 * 1024 * 1024;
constexpr auto MaxArtworkPixels = 36'000'000;
constexpr auto MaxArtworkSide = 1024;

QString ProfilePath(Common::FS::EdenPath base, const std::filesystem::path& path) {
    return QString::fromStdString(
        Common::FS::PathToUTF8String(Common::FS::GetEdenPath(base) / path));
}

QString NormalizePath(const QString& path) {
    QString normalized = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
#ifdef _WIN32
    normalized = normalized.toCaseFolded();
#endif
    return normalized;
}

QString PathKey(const QString& path) {
    return QString::fromLatin1(
        QCryptographicHash::hash(NormalizePath(path).toUtf8(), QCryptographicHash::Sha256).toHex());
}

bool IsSystemFolder(const QString& path) {
    return path == QStringLiteral("SDMC") || path == QStringLiteral("UserNAND") ||
           path == QStringLiteral("SysNAND");
}

auto FindFolder(auto& dirs, const QString& path) {
    const auto normalized = NormalizePath(path);
    return std::find_if(dirs.begin(), dirs.end(), [&](const UISettings::GameDir& dir) {
        const auto existing = QString::fromStdString(dir.path);
        return !IsSystemFolder(existing) && NormalizePath(existing) == normalized;
    });
}

QString OverridePath(const QString& preference_key, u64 program_id) {
    if (program_id) {
        return QString::fromStdString(
            Common::FS::PathToUTF8String(QtCommon::Game::CustomArtworkPath(program_id)));
    }
    return ProfilePath(Common::FS::EdenPath::EdenDir,
                       std::filesystem::path("artwork") / (preference_key.toStdString() + ".png"));
}

QImage ReadArtwork(QImageReader& reader, const QSize& requested_size = {}) {
    const QSize source_size = reader.size();
    if (source_size.isEmpty() ||
        static_cast<qint64>(source_size.width()) * source_size.height() > MaxArtworkPixels) {
        return {};
    }
    QSize limit{MaxArtworkSide, MaxArtworkSide};
    if (!requested_size.isEmpty()) {
        limit = {std::min(limit.width(), requested_size.width()),
                 std::min(limit.height(), requested_size.height())};
    }
    if (source_size.width() > limit.width() || source_size.height() > limit.height()) {
        reader.setScaledSize(source_size.scaled(limit, Qt::KeepAspectRatio));
    }
    reader.setAutoTransform(true);
    return reader.read();
}

QImage ReadArtworkFile(const QString& path, const QSize& requested_size = {}) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > MaxArtworkBytes) {
        return {};
    }
    QImageReader reader(&file);
    return ReadArtwork(reader, requested_size);
}

class ArtworkProvider final : public QQuickImageProvider {
public:
    explicit ArtworkProvider(std::shared_ptr<LibraryArtwork> artwork)
        : QQuickImageProvider(QQuickImageProvider::Image, ForceAsynchronousImageLoading),
          data{std::move(artwork)} {}

    QImage requestImage(const QString& id, QSize* size, const QSize& requested_size) override {
        LibraryArtwork::Image entry;
        {
            const QReadLocker lock(&data->lock);
            entry = data->images.value(id.section(QLatin1Char('/'), 0, 0));
        }

        QImage image;
        if (!entry.override_path.isEmpty()) {
            image = ReadArtworkFile(entry.override_path, requested_size);
        }
        if (image.isNull() && !entry.embedded.isEmpty()) {
            QBuffer buffer(&entry.embedded);
            if (buffer.open(QIODevice::ReadOnly)) {
                QImageReader reader(&buffer);
                image = ReadArtwork(reader, requested_size);
            }
        }
        if (size) {
            *size = image.size();
        }
        return image;
    }

private:
    std::shared_ptr<LibraryArtwork> data;
};

} // namespace

ConsoleLibrary::ConsoleLibrary(GameListModel& source_, QObject* parent)
    : QAbstractListModel(parent), source(source_),
      preferences(ProfilePath(Common::FS::EdenPath::ConfigDir, "console.ini"),
                  QSettings::IniFormat),
      artwork_data(std::make_shared<LibraryArtwork>()) {
    preferences.beginGroup(QStringLiteral("Library"));

    refresh_timer.setSingleShot(true);
    refresh_timer.setInterval(300);
    connect(&refresh_timer, &QTimer::timeout, this, &ConsoleLibrary::refresh);
    connect(&missing_folder_watch, &QFileSystemWatcher::directoryChanged, this,
            [this] { refresh_timer.start(); });

    source_update_timer.setSingleShot(true);
    connect(&source_update_timer, &QTimer::timeout, this, &ConsoleLibrary::reload);
    connect(&source, &GameListModel::dataChanged, this, [this](const QModelIndex& first) {
        if (!scanning() && first.column() == GameListModel::COLUMN_NAME) {
            source_update_timer.start();
        }
    });
    connect(&source, &GameListModel::PopulatingStarted, this, [this] {
        source_update_timer.stop();
        setScanning(true);
    });
    connect(&source, &GameListModel::PopulatingCancelled, this, [this] { setScanning(false); });
    connect(&source, &GameListModel::PopulatingCompleted, this, [this] {
        reload();
        setScanning(false);
        emit foldersChanged();
        if (std::exchange(refresh_pending, false)) {
            refresh_timer.start();
        }
    });

    if (source.IsPopulating()) {
        setScanning(true);
    } else {
        reload();
    }
}

int ConsoleLibrary::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : count();
}

QVariant ConsoleLibrary::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= count()) {
        return {};
    }
    return roleValue(games[visible_rows[index.row()]], role);
}

QHash<int, QByteArray> ConsoleLibrary::roleNames() const {
    return {
        {GameKeyRole, "gameKey"},
        {TitleRole, "title"},
        {PublisherRole, "publisher"},
        {VersionRole, "version"},
        {ProgramIdRole, "programId"},
        {GamePathRole, "gamePath"},
        {ArtworkRole, "artwork"},
        {FavoriteRole, "favorite"},
        {LastPlayedRole, "lastPlayed"},
        {FormatRole, "format"},
        {CustomArtworkRole, "customArtwork"},
    };
}

bool ConsoleLibrary::scanning() const { return is_scanning; }

int ConsoleLibrary::count() const { return static_cast<int>(visible_rows.size()); }

QStringList ConsoleLibrary::folders() const {
    QStringList result;
    for (const auto& folder : std::as_const(UISettings::values.game_dirs)) {
        const auto path = QString::fromStdString(folder.path);
        if (!path.isEmpty() && !IsSystemFolder(path)) {
            result.append(path);
        }
    }
    return result;
}

QString ConsoleLibrary::search() const { return search_text; }

QString ConsoleLibrary::sortOrder() const { return sort_order; }

bool ConsoleLibrary::favoritesOnly() const { return favorites_only; }

QVariantList ConsoleLibrary::homeGames() const { return home_games; }

bool ConsoleLibrary::artworkBusy() const { return changing_artwork; }

void ConsoleLibrary::setSearch(const QString& value) {
    if (search_text != value) {
        search_text = value;
        rebuildViews();
        emit searchChanged();
    }
}

void ConsoleLibrary::setSortOrder(const QString& value) {
    if (value == sort_order ||
        (value != QStringLiteral("title") && value != QStringLiteral("recent") &&
         value != QStringLiteral("favorites"))) {
        return;
    }
    sort_order = value;
    rebuildViews();
    emit sortOrderChanged();
}

void ConsoleLibrary::setFavoritesOnly(bool value) {
    if (favorites_only != value) {
        favorites_only = value;
        rebuildViews();
        emit favoritesOnlyChanged();
    }
}

void ConsoleLibrary::refresh() {
    if (fixtures_enabled) {
        return;
    }
    updateMissingFolderWatch();
    if (scanning()) {
        refresh_pending = true;
        return;
    }
    source.RefreshGameDirectory();
}

bool ConsoleLibrary::addFolder(const QUrl& url) {
    if (scanning()) {
        emit error(tr("Wait for the current library scan to finish before editing game folders."));
        return false;
    }
    const QFileInfo info(url.toLocalFile());
    if (!url.isLocalFile() || !info.isDir()) {
        emit error(tr("Choose an existing local folder."));
        return false;
    }
    const QString path = info.absoluteFilePath();
    auto& dirs = UISettings::values.game_dirs;
    if (FindFolder(dirs, path) != dirs.end()) {
        emit error(tr("This folder is already in the game library."));
        return false;
    }
    dirs.append(UISettings::GameDir{path.toStdString(), false, true});
    updateFolderConfiguration();
    return true;
}

bool ConsoleLibrary::removeFolder(const QString& path) {
    if (scanning()) {
        emit error(tr("Wait for the current library scan to finish before editing game folders."));
        return false;
    }
    auto& dirs = UISettings::values.game_dirs;
    const auto found = FindFolder(dirs, path);
    if (found == dirs.end()) {
        return false;
    }
    dirs.erase(found);
    updateFolderConfiguration();
    return true;
}

bool ConsoleLibrary::setDeepScan(const QString& path, bool enabled) {
    if (scanning()) {
        emit error(tr("Wait for the current library scan to finish before editing game folders."));
        return false;
    }
    auto& dirs = UISettings::values.game_dirs;
    const auto found = FindFolder(dirs, path);
    if (found == dirs.end()) {
        return false;
    }
    if (found->deep_scan != enabled) {
        found->deep_scan = enabled;
        updateFolderConfiguration();
    }
    return true;
}

bool ConsoleLibrary::deepScan(const QString& path) const {
    const auto& dirs = UISettings::values.game_dirs;
    const auto found = FindFolder(dirs, path);
    return found != dirs.end() && found->deep_scan;
}

void ConsoleLibrary::updateFolderConfiguration() {
    emit saveConfiguration();
    emit foldersChanged();
    refresh();
}

void ConsoleLibrary::updateMissingFolderWatch() {
    QSet<QString> targets;
    for (const QString& folder : folders()) {
        if (QFileInfo(folder).isDir()) {
            continue;
        }
        QFileInfo current(folder);
        for (QString ancestor = current.absolutePath(); !ancestor.isEmpty();) {
            current.setFile(ancestor);
            if (current.isDir()) {
                targets.insert(ancestor);
                break;
            }
            const QString parent = current.absolutePath();
            if (parent == ancestor) {
                break;
            }
            ancestor = parent;
        }
    }

    const auto current = missing_folder_watch.directories();
    const QSet<QString> watched(current.begin(), current.end());
    const auto removed = (watched - targets).values();
    const auto added = (targets - watched).values();
    if (!removed.isEmpty()) {
        missing_folder_watch.removePaths(removed);
    }
    if (!added.isEmpty()) {
        missing_folder_watch.addPaths(added);
    }
}

void ConsoleLibrary::setScanning(bool active) {
    if (!fixtures_enabled && is_scanning != active) {
        is_scanning = active;
        emit scanningChanged();
    }
}

bool ConsoleLibrary::isFavorite(const Game& game) const {
    if (!game.program_id.isEmpty()) {
        return UISettings::values.favorited_ids.contains(game.program_id.toULongLong(nullptr, 16));
    }
    return preferences.value(QStringLiteral("Favorites/%1").arg(game.preference_key), false)
        .toBool();
}

QDateTime ConsoleLibrary::lastPlayed(const Game& game) const {
    return preferences.value(QStringLiteral("Played/%1").arg(game.preference_key)).toDateTime();
}

bool ConsoleLibrary::savePreferences() {
    preferences.sync();
    if (preferences.status() == QSettings::NoError) {
        return true;
    }
    emit error(tr("Console library preferences could not be saved."));
    return false;
}

bool ConsoleLibrary::setFavorite(const QString& key, bool favorite) {
    const auto found = games_by_key.constFind(key);
    if (found == games_by_key.cend()) {
        return false;
    }
    const auto& game = games[*found];
    if (isFavorite(game) == favorite) {
        return true;
    }
    bool saved = true;
    if (const u64 program_id = game.program_id.toULongLong(nullptr, 16)) {
        auto& favorites = UISettings::values.favorited_ids;
        if (favorite) {
            favorites.append(program_id);
        } else {
            favorites.removeAll(program_id);
        }
        emit saveConfiguration();
    } else {
        const auto preference = QStringLiteral("Favorites/%1").arg(game.preference_key);
        if (favorite) {
            preferences.setValue(preference, true);
        } else {
            preferences.remove(preference);
        }
        saved = savePreferences();
    }
    rebuildViews();
    if (count() > 0) {
        emit dataChanged(index(0), index(count() - 1), {FavoriteRole});
    }
    return saved;
}

void ConsoleLibrary::markPlayed(const QString& path, const QString& program_id) {
    const auto normalized_path = NormalizePath(path);
    const auto id = program_id.toULongLong(nullptr, 16);
    const auto entry = std::find_if(games.cbegin(), games.cend(), [&](const Game& game) {
        return NormalizePath(game.path) == normalized_path &&
               (!id || game.program_id.isEmpty() || game.program_id.toULongLong(nullptr, 16) == id);
    });
    if (entry == games.cend()) {
        return;
    }
    preferences.setValue(QStringLiteral("Played/%1").arg(entry->preference_key),
                         QDateTime::currentDateTimeUtc());
    savePreferences();
    rebuildViews();
    if (count() > 0) {
        emit dataChanged(index(0), index(count() - 1), {LastPlayedRole});
    }
}

void ConsoleLibrary::setArtwork(const QString& key, const QUrl& url) {
    if (!url.isLocalFile()) {
        emit error(tr("Choose a local image file."));
        return;
    }
    changeArtwork(key, url, false);
}

void ConsoleLibrary::resetArtwork(const QString& key) { changeArtwork(key, {}, true); }

void ConsoleLibrary::changeArtwork(const QString& key, const QUrl& url, bool remove) {
    const auto found = games_by_key.constFind(key);
    if (changing_artwork || found == games_by_key.cend()) {
        return;
    }
    const auto& game = games[*found];
    const auto program_id = game.program_id.toULongLong(nullptr, 16);
    const auto preference_key = game.preference_key;
    const auto path = OverridePath(preference_key, program_id);
    const auto selected_file = url.toLocalFile();

    changing_artwork = true;
    emit artworkBusyChanged();
    auto* watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, preference_key] {
        const QString result = watcher->result();
        watcher->deleteLater();
        if (result.isEmpty()) {
            updateArtworkState(preference_key);
        } else {
            emit error(result);
        }
        changing_artwork = false;
        emit artworkBusyChanged();
    });
    watcher->setFuture(QtConcurrent::run([path, program_id, selected_file, remove] {
        if (remove) {
            if (program_id ? QtCommon::Game::RemoveCustomArtwork(program_id)
                           : !QFileInfo::exists(path) || QFile::remove(path)) {
                return QString{};
            }
            return tr("The artwork override could not be removed.");
        }

        const QImage image = ReadArtworkFile(selected_file);
        if (image.isNull()) {
            return tr("Choose a readable image smaller than 16 MB and 36 megapixels.");
        }
        if (program_id) {
            if (QtCommon::Game::SaveCustomArtwork(program_id, image)) {
                return QString{};
            }
        } else if (QDir().mkpath(QFileInfo(path).absolutePath())) {
            QSaveFile file(path);
            if (file.open(QIODevice::WriteOnly) && image.save(&file, "PNG") && file.commit()) {
                return QString{};
            }
        }
        return tr("The artwork override could not be saved.");
    }));
}

void ConsoleLibrary::updateArtworkState(const QString& preference_key) {
    ++artwork_revision;
    for (auto& game : games) {
        if (game.preference_key != preference_key) {
            continue;
        }
        const QReadLocker lock(&artwork_data->lock);
        const auto image = artwork_data->images.constFind(game.key);
        if (image == artwork_data->images.cend()) {
            continue;
        }
        game.custom_artwork = QFileInfo(image->override_path).isFile();
        game.artwork = QUrl{};
        if (game.custom_artwork || !image->embedded.isEmpty()) {
            game.artwork =
                QUrl(QStringLiteral("image://game-art/%1/%2").arg(game.key).arg(artwork_revision));
        }
    }
    rebuildViews();
    if (count() > 0) {
        emit dataChanged(index(0), index(count() - 1), {ArtworkRole, CustomArtworkRole});
    }
}

QQuickImageProvider* ConsoleLibrary::createImageProvider() const {
    return new ArtworkProvider(artwork_data);
}

QVariantMap ConsoleLibrary::get(int index) const {
    return index >= 0 && index < count() ? gameMap(games[visible_rows[index]]) : QVariantMap{};
}

QVariantMap ConsoleLibrary::getByKey(const QString& key) const {
    const auto found = games_by_key.constFind(key);
    return found != games_by_key.cend() ? gameMap(games[*found]) : QVariantMap{};
}

QVariant ConsoleLibrary::roleValue(const Game& game, int role) const {
    switch (role) {
    case GameKeyRole:
        return game.key;
    case TitleRole:
        return game.title;
    case PublisherRole:
        return game.publisher;
    case VersionRole:
        return game.version;
    case ProgramIdRole:
        return game.program_id;
    case GamePathRole:
        return game.path;
    case ArtworkRole:
        return game.artwork;
    case FavoriteRole:
        return isFavorite(game);
    case LastPlayedRole:
        return lastPlayed(game);
    case FormatRole:
        return game.format;
    case CustomArtworkRole:
        return game.custom_artwork;
    default:
        return {};
    }
}

QVariantMap ConsoleLibrary::gameMap(const Game& game) const {
    QVariantMap result;
    const auto roles = roleNames();
    for (auto role = roles.cbegin(); role != roles.cend(); ++role) {
        result.insert(QString::fromLatin1(role.value()), roleValue(game, role.key()));
    }
    return result;
}

void ConsoleLibrary::reload() {
    if (fixtures_enabled) {
        return;
    }

    QList<Game> entries;
    QHash<QString, LibraryArtwork::Image> images;
    ++artwork_revision;
    const auto visit = [&](auto&& self, const QStandardItem* parent) -> void {
        for (int row = 0; row < parent->rowCount(); ++row) {
            const auto* item = parent->child(row, GameListModel::COLUMN_NAME);
            if (!item || item->type() == static_cast<int>(GameListItemType::Favorites)) {
                continue;
            }
            if (item->type() != static_cast<int>(GameListItemType::Game)) {
                self(self, item);
                continue;
            }

            const u64 id = item->data(GameListItemPath::ProgramIdRole).toULongLong();
            Game game;
            game.path = item->data(GameListItemPath::FullPathRole).toString();
            if (game.path.isEmpty()) {
                continue;
            }
            const QString path_key = PathKey(game.path);
            if (id) {
                game.program_id = QStringLiteral("%1").arg(id, 16, 16, QLatin1Char('0')).toUpper();
                game.preference_key = game.program_id;
                game.key = game.program_id + QLatin1Char('-') + path_key;
            } else {
                game.key = game.preference_key = QStringLiteral("path-%1").arg(path_key);
            }
            if (images.contains(game.key)) {
                continue;
            }

            game.title = item->data(GameListItemPath::TitleRole).toString().trimmed();
            if (game.title.isEmpty()) {
                game.title = QFileInfo(game.path).completeBaseName();
            }
            game.publisher = item->data(GameListItemPath::PublisherRole).toString();
            game.version = item->data(GameListItemPath::VersionRole).toString();
            game.format = item->data(GameListItemPath::FileTypeRole).toString();

            const auto path = OverridePath(game.preference_key, id);
            const auto embedded = item->data(GameListItemPath::OriginalArtworkRole).toByteArray();
            game.custom_artwork = QFileInfo(path).isFile();
            if (game.custom_artwork || !embedded.isEmpty()) {
                game.artwork = QUrl(
                    QStringLiteral("image://game-art/%1/%2").arg(game.key).arg(artwork_revision));
            }
            images.insert(game.key, LibraryArtwork::Image{embedded, path});
            entries.append(std::move(game));
        }
    };
    visit(visit, source.invisibleRootItem());
    {
        const QWriteLocker lock(&artwork_data->lock);
        artwork_data->images = std::move(images);
    }
    rebuildViews(std::move(entries));
    updateMissingFolderWatch();
}

void ConsoleLibrary::setFixtureGames(const QVariantList& fixtures) {
    setScanning(false);
    fixtures_enabled = true;
    QList<Game> entries;
    QHash<QString, LibraryArtwork::Image> images;
    ++artwork_revision;
    for (const QVariant& fixture : fixtures) {
        const auto values = fixture.toMap();
        Game game;
        game.path = values.value(QStringLiteral("gamePath")).toString();
        if (game.path.isEmpty()) {
            continue;
        }
        game.program_id = values.value(QStringLiteral("programId")).toString().toUpper();
        game.key = values.value(QStringLiteral("gameKey")).toString();
        if (game.key.isEmpty()) {
            game.key = QStringLiteral("path-%1").arg(PathKey(game.path));
        }
        game.preference_key = game.program_id.isEmpty() ? game.key : game.program_id;
        game.title = values.value(QStringLiteral("title")).toString();
        game.publisher = values.value(QStringLiteral("publisher")).toString();
        game.version = values.value(QStringLiteral("version")).toString();
        game.format = values.value(QStringLiteral("format")).toString();
        game.artwork = values.value(QStringLiteral("artwork")).toUrl();
        const auto path =
            OverridePath(game.preference_key, game.program_id.toULongLong(nullptr, 16));
        const auto embedded = values.value(QStringLiteral("embeddedArtwork")).toByteArray();
        game.custom_artwork = QFileInfo(path).isFile();
        if (game.custom_artwork || !embedded.isEmpty()) {
            game.artwork =
                QUrl(QStringLiteral("image://game-art/%1/%2").arg(game.key).arg(artwork_revision));
        }
        images.insert(game.key, LibraryArtwork::Image{embedded, path});
        entries.append(std::move(game));
    }
    {
        const QWriteLocker lock(&artwork_data->lock);
        artwork_data->images = std::move(images);
    }
    rebuildViews(std::move(entries));
}

void ConsoleLibrary::rebuildViews(std::optional<QList<Game>> replacement) {
    const int previous_count = count();
    if (replacement) {
        beginResetModel();
        games = std::move(*replacement);
        games_by_key.clear();
        for (int row = 0; row < static_cast<int>(games.size()); ++row) {
            games_by_key.insert(games[row].key, row);
        }
    }

    struct SortEntry {
        int row;
        QDateTime played;
        qsizetype recent;
        bool favorite;
    };
    QList<SortEntry> ordered;
    ordered.reserve(games.size());
    const auto& recent_files = UISettings::values.recent_files;
    for (int row = 0; row < static_cast<int>(games.size()); ++row) {
        const auto& game = games[row];
        const auto recent = recent_files.indexOf(game.path);
        ordered.append(
            {row, lastPlayed(game), recent < 0 ? recent_files.size() : recent, isFavorite(game)});
    }
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    const auto title_less = [&](const SortEntry& a, const SortEntry& b) {
        const auto& lhs = games[a.row];
        const auto& rhs = games[b.row];
        const auto cmp = collator.compare(lhs.title, rhs.title);
        return cmp == 0 ? lhs.key < rhs.key : cmp < 0;
    };
    const auto recent_less = [&](const SortEntry& a, const SortEntry& b) {
        if (a.played != b.played) {
            return a.played > b.played;
        }
        if (a.recent != b.recent) {
            return a.recent < b.recent;
        }
        return a.favorite == b.favorite ? title_less(a, b) : a.favorite;
    };

    std::sort(ordered.begin(), ordered.end(), recent_less);
    QVariantList home;
    constexpr qsizetype MaxHomeTiles = 16;
    for (qsizetype i = 0; i < std::min(MaxHomeTiles, ordered.size()); ++i) {
        home.append(gameMap(games[ordered[i].row]));
    }
    if (sort_order != QStringLiteral("recent")) {
        const bool favorite_first = sort_order == QStringLiteral("favorites");
        std::sort(ordered.begin(), ordered.end(), [&](const auto& a, const auto& b) {
            if (favorite_first && a.favorite != b.favorite) {
                return a.favorite;
            }
            return title_less(a, b);
        });
    }

    QList<int> filtered;
    const QString needle = search_text.trimmed();
    for (const auto& entry : ordered) {
        const auto& game = games[entry.row];
        if (favorites_only && !entry.favorite) {
            continue;
        }
        if (!needle.isEmpty() && !game.title.contains(needle, Qt::CaseInsensitive) &&
            !game.publisher.contains(needle, Qt::CaseInsensitive) &&
            !game.program_id.contains(needle, Qt::CaseInsensitive) &&
            !QFileInfo(game.path).fileName().contains(needle, Qt::CaseInsensitive)) {
            continue;
        }
        filtered.append(entry.row);
    }

    if (replacement || filtered != visible_rows) {
        if (!replacement) {
            beginResetModel();
        }
        visible_rows = std::move(filtered);
        endResetModel();
    }
    if (count() != previous_count) {
        emit countChanged();
    }
    if (home_games != home) {
        home_games = std::move(home);
        emit homeGamesChanged();
    }
}

} // namespace Horizon
