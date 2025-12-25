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

  static auto availableMaps() -> QVariantList;

  Q_INVOKABLE void load_maps_async();

  [[nodiscard]] auto isLoading() const -> bool { return m_loading; }
  [[nodiscard]] auto maps() const -> const QVariantList & { return m_maps; }

signals:
  void map_loaded(QVariantMap mapData);
  void all_maps_loaded();
  void loading_changed(bool loading);

private:
  void load_next_map();
  static auto loadSingleMap(const QString &filePath) -> QVariantMap;
  void ensure_campaign_map_paths_loaded();

  QStringList m_pendingFiles;
  QVariantList m_maps;
  QSet<QString> m_campaign_map_paths;
  bool m_campaign_map_paths_loaded = false;
  bool m_loading = false;
};

} 
