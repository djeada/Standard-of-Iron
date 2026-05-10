#pragma once

#include <QObject>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>

namespace Game::Map {

class MapCatalog : public QObject {
  Q_OBJECT
public:
  explicit MapCatalog(QObject *parent = nullptr);

  static auto available_maps() -> QVariantList;

  Q_INVOKABLE void load_maps_async();

  [[nodiscard]] auto isLoading() const -> bool { return m_loading; }
  [[nodiscard]] auto maps() const -> const QVariantList & { return m_maps; }

signals:
  void map_loaded(QVariantMap map_data);
  void all_maps_loaded();
  void loading_changed(bool loading);

private:
  void load_next_map();
  static auto load_single_map(const QString &file_path) -> QVariantMap;
  void ensure_campaign_map_paths_loaded();

  QStringList m_pending_files;
  QVariantList m_maps;
  QSet<QString> m_campaign_map_paths;
  bool m_campaign_map_paths_loaded = false;
  bool m_loading = false;
};

} // namespace Game::Map
