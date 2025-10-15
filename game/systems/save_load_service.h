#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVariantList>

#include <memory>

namespace Engine {
namespace Core {
class World;
}
} // namespace Engine

namespace Game {
namespace Systems {

class SaveStorage;

class SaveLoadService {
public:
  SaveLoadService();
  ~SaveLoadService();

  bool saveGameToSlot(Engine::Core::World &world, const QString &slotName,
                      const QString &title, const QString &mapName,
                      const QJsonObject &metadata = {},
                      const QByteArray &screenshot = QByteArray());

  bool loadGameFromSlot(Engine::Core::World &world, const QString &slotName);

  QVariantList getSaveSlots() const;

  bool deleteSaveSlot(const QString &slotName);

  QString getLastError() const { return m_lastError; }

  void clearError() { m_lastError.clear(); }

  QJsonObject getLastMetadata() const { return m_lastMetadata; }
  QString getLastTitle() const { return m_lastTitle; }
  QByteArray getLastScreenshot() const { return m_lastScreenshot; }

  void openSettings();

  void exitGame();

private:
  QString getSavesDirectory() const;
  QString getDatabasePath() const;
  void ensureSavesDirectoryExists() const;

  mutable QString m_lastError;
  QJsonObject m_lastMetadata;
  QString m_lastTitle;
  QByteArray m_lastScreenshot;
  std::unique_ptr<SaveStorage> m_storage;
};

} // namespace Systems
} // namespace Game
