#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace Game {
namespace Map {

class MapCatalog : public QObject {
  Q_OBJECT
public:
  explicit MapCatalog(QObject *parent = nullptr);
  
  static QVariantList availableMaps();
  
  
  Q_INVOKABLE void loadMapsAsync();
  
  bool isLoading() const { return m_loading; }
  const QVariantList& maps() const { return m_maps; }
  
signals:
  void mapLoaded(QVariantMap mapData);
  void allMapsLoaded();
  void loadingChanged(bool loading);
  
private:
  void loadNextMap();
  QVariantMap loadSingleMap(const QString &filePath);
  
  QStringList m_pendingFiles;
  QVariantList m_maps;
  bool m_loading = false;
};

}
}
