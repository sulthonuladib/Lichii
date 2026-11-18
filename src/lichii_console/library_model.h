// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <QAbstractListModel>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QHash>
#include <QSettings>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

class GameListModel;
class QQuickImageProvider;

namespace Horizon {

struct LibraryArtwork;

class ConsoleLibrary final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QStringList folders READ folders NOTIFY foldersChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY searchChanged)
    Q_PROPERTY(QString sortOrder READ sortOrder WRITE setSortOrder NOTIFY sortOrderChanged)
    Q_PROPERTY(
        bool favoritesOnly READ favoritesOnly WRITE setFavoritesOnly NOTIFY favoritesOnlyChanged)
    Q_PROPERTY(QVariantList homeGames READ homeGames NOTIFY homeGamesChanged)
    Q_PROPERTY(bool artworkBusy READ artworkBusy NOTIFY artworkBusyChanged)

public:
    explicit ConsoleLibrary(GameListModel& source, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool scanning() const;
    int count() const;
    QStringList folders() const;
    QString search() const;
    QString sortOrder() const;
    bool favoritesOnly() const;
    QVariantList homeGames() const;
    bool artworkBusy() const;

    void setSearch(const QString& value);
    void setSortOrder(const QString& value);
    void setFavoritesOnly(bool value);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool addFolder(const QUrl& url);
    Q_INVOKABLE bool removeFolder(const QString& path);
    Q_INVOKABLE bool setDeepScan(const QString& path, bool enabled);
    Q_INVOKABLE bool deepScan(const QString& path) const;
    Q_INVOKABLE bool setFavorite(const QString& key, bool favorite);
    Q_INVOKABLE void setArtwork(const QString& key, const QUrl& url);
    Q_INVOKABLE void resetArtwork(const QString& key);
    Q_INVOKABLE QVariantMap get(int index) const;
    Q_INVOKABLE QVariantMap getByKey(const QString& key) const;

    void markPlayed(const QString& path, const QString& program_id = {});
    QQuickImageProvider* createImageProvider() const;
    void setFixtureGames(const QVariantList& games);

signals:
    void scanningChanged();
    void countChanged();
    void foldersChanged();
    void searchChanged();
    void sortOrderChanged();
    void favoritesOnlyChanged();
    void homeGamesChanged();
    void artworkBusyChanged();
    void saveConfiguration();
    void error(const QString& message);

private:
    enum Roles {
        GameKeyRole = Qt::UserRole + 1,
        TitleRole,
        PublisherRole,
        VersionRole,
        ProgramIdRole,
        GamePathRole,
        ArtworkRole,
        FavoriteRole,
        LastPlayedRole,
        FormatRole,
        CustomArtworkRole,
    };

    struct Game {
        QString key;
        QString preference_key;
        QString title;
        QString publisher;
        QString version;
        QString program_id;
        QString path;
        QString format;
        QUrl artwork;
        bool custom_artwork = false;
    };

    void reload();
    void updateMissingFolderWatch();
    void setScanning(bool active);
    void rebuildViews(std::optional<QList<Game>> replacement = {});
    void updateFolderConfiguration();
    void updateArtworkState(const QString& preference_key);
    void changeArtwork(const QString& key, const QUrl& url, bool remove);
    bool savePreferences();
    bool isFavorite(const Game& game) const;
    QDateTime lastPlayed(const Game& game) const;
    QVariant roleValue(const Game& game, int role) const;
    QVariantMap gameMap(const Game& game) const;

    GameListModel& source;
    QSettings preferences;
    std::shared_ptr<LibraryArtwork> artwork_data;
    QFileSystemWatcher missing_folder_watch;
    QTimer refresh_timer;
    QTimer source_update_timer;
    QList<Game> games;
    QHash<QString, int> games_by_key;
    QList<int> visible_rows;
    QVariantList home_games;
    QString search_text;
    QString sort_order = QStringLiteral("title");
    bool favorites_only = false;
    bool is_scanning = false;
    bool refresh_pending = false;
    bool changing_artwork = false;
    bool fixtures_enabled = false;
    quint64 artwork_revision = 0;
};

} // namespace Horizon
