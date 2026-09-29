#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "../../core/entity.h"

namespace Game::Systems::Combat {

struct ForeignSoldier {
  Engine::Core::EntityID entity_id{0};
  std::uint16_t slot_index{0};
  float x{0.0F};
  float z{0.0F};
};

class ForeignSoldierGrid {
public:
  void clear();
  void insert(const ForeignSoldier& soldier);
  void gather(Engine::Core::EntityID self,
              std::uint16_t self_slot,
              float x,
              float z,
              float radius,
              std::vector<ForeignSoldier>& out) const;

private:
  std::unordered_map<std::uint64_t, std::vector<ForeignSoldier>> m_cells;
};

} // namespace Game::Systems::Combat
