#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

namespace Engine::Core {
class World;
}

class SelectionQueryService : public QObject {
  Q_OBJECT

public:
  explicit SelectionQueryService(Engine::Core::World *world,
                                QObject *parent = nullptr);

  [[nodiscard]] QString get_selected_units_command_mode() const;
  [[nodiscard]] QVariantMap get_selected_units_mode_availability() const;

private:
  Engine::Core::World *m_world;
};
