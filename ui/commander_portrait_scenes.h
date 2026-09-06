#pragma once

#include <QString>
#include <QStringList>

#include <cstdint>
#include <map>
#include <memory>

#include "game/core/entity_id.h"

namespace Engine::Core {
class World;
}

namespace Game::Units {
class Unit;
class UnitFactoryRegistry;
} // namespace Game::Units

namespace Render::GL {
class Camera;
class Renderer;
} // namespace Render::GL

namespace UI {

class CommanderPortraitScenes {
public:
  struct Scene {
    Engine::Core::World* world{nullptr};
    Engine::Core::EntityID entity{0};
  };

  static auto instance() -> CommanderPortraitScenes&;

  CommanderPortraitScenes(const CommanderPortraitScenes&) = delete;
  auto operator=(const CommanderPortraitScenes&) -> CommanderPortraitScenes& = delete;
  CommanderPortraitScenes(CommanderPortraitScenes&&) = delete;
  auto operator=(CommanderPortraitScenes&&) -> CommanderPortraitScenes& = delete;

  void add_reference();
  void release_reference();

  [[nodiscard]] auto renderer() -> Render::GL::Renderer*;
  [[nodiscard]] auto camera() -> Render::GL::Camera*;

  [[nodiscard]] auto acquire(const QString& troop_type) -> Scene;

  void warm(const QStringList& troop_types);

  [[nodiscard]] auto warmed_signature() const -> QString { return m_warmed_signature; }

private:
  CommanderPortraitScenes() = default;
  ~CommanderPortraitScenes();

  struct Entry {
    std::unique_ptr<Engine::Core::World> world;
    std::unique_ptr<Game::Units::UnitFactoryRegistry> factory;
    std::unique_ptr<Game::Units::Unit> unit;
    Engine::Core::EntityID entity{0};
  };

  [[nodiscard]] auto ensure_renderer() -> bool;
  void release_all();

  static constexpr std::uint32_t k_scene_entity_block = 256U;

  std::unique_ptr<Render::GL::Renderer> m_renderer;
  std::unique_ptr<Render::GL::Camera> m_camera;
  std::map<QString, Entry> m_entries;
  QString m_warmed_signature;
  int m_references{0};
  std::uint32_t m_next_entity_block{0};
  bool m_renderer_failed{false};
};

} // namespace UI
