#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace Game::Map {

class MapCatalog : public QObject {
  Q_OBJECT
public:
  explicit MapCatalog(QObject *parent = nullptr);

  static auto availableMaps() -> QVariantList;

  Q_INVOKABLE void loadMapsAsync();

  [[nodiscard]] auto isLoading() const -> bool { return m_loading; }
  [[nodiscard]] auto maps() const -> const QVariantList & { return m_maps; }

signals:
  void mapLoaded(QVariantMap mapData);
  void allMapsLoaded();
  void loadingChanged(bool loading);

private:
  void loadNextMap();
  static auto loadSingleMap(const QString &filePath) -> QVariantMap;

  QStringList m_pendingFiles;
  QVariantList m_maps;
  bool m_loading = false;
};

} // namespace Game::Map
