#pragma once

#include <QString>

#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace Engine::Core {
class World;
}

class PlayerDefeatWatcher {
public:
  struct Defeat {
    int owner_id = 0;
    bool ally = false;
    QString owner_name;
    QString commander_name;
  };

  using Announce = std::function<void(const Defeat&)>;

  void reset();

  void update(Engine::Core::World& world,
              int local_owner_id,
              float dt_seconds,
              const Announce& announce);

private:
  struct OwnerState {
    bool seen_alive = false;
    bool announced = false;
    QString commander_name;
  };

  std::unordered_map<int, OwnerState> m_owners;
  float m_accumulator = 0.0F;
};
