#pragma once

#include <map>
#include <memory>

#include "../core/entity_id.h"
#include "../core/system.h"

class QJsonArray;

namespace Game::Commander {
class CommanderRuntime;
}

namespace Game::Command {

struct CommanderActionRecord;
struct CommanderInputFrame;

class CommandSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;
  void clear_commander_controls();

  [[nodiscard]] auto
  commander_runtime(int owner) -> std::shared_ptr<Game::Commander::CommanderRuntime>;
  [[nodiscard]] auto capture_checkpoint() const -> QJsonArray;
  auto restore_checkpoint(Engine::Core::World& world, const QJsonArray& state) -> bool;

private:
  struct CommanderControl {
    std::shared_ptr<Game::Commander::CommanderRuntime> runtime;
    Engine::Core::EntityID entity = 0;
    bool applied_input = false;
  };

  void apply_action(Engine::Core::World& world,
                    int owner,
                    const CommanderActionRecord& action);
  void apply_input(Engine::Core::World& world,
                   int owner,
                   const CommanderInputFrame& frame,
                   float dt);

  std::map<int, CommanderControl> m_commanders;
};

} // namespace Game::Command
