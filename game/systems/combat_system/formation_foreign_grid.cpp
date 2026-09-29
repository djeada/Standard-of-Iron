#include "formation_foreign_grid.h"

#include <cmath>

namespace Game::Systems::Combat {
namespace {

constexpr float k_foreign_soldier_cell = 0.75F;

}

void ForeignSoldierGrid::clear() {
  m_cells.clear();
}

namespace {

auto cell_of(float value) -> int {
  return static_cast<int>(std::floor(value / k_foreign_soldier_cell));
}

auto cell_key(int x, int z) -> std::uint64_t {
  return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32U) |
         static_cast<std::uint64_t>(static_cast<std::uint32_t>(z));
}

} // namespace

void ForeignSoldierGrid::insert(const ForeignSoldier& soldier) {
  m_cells[cell_key(cell_of(soldier.x), cell_of(soldier.z))].push_back(soldier);
}

void ForeignSoldierGrid::gather(Engine::Core::EntityID self,
                                std::uint16_t self_slot,
                                float x,
                                float z,
                                float radius,
                                std::vector<ForeignSoldier>& out) const {
  out.clear();
  if (m_cells.empty()) {
    return;
  }
  int const reach = static_cast<int>(std::ceil(radius / k_foreign_soldier_cell));
  int const cx = cell_of(x);
  int const cz = cell_of(z);
  for (int ix = cx - reach; ix <= cx + reach; ++ix) {
    for (int iz = cz - reach; iz <= cz + reach; ++iz) {
      auto const found = m_cells.find(cell_key(ix, iz));
      if (found == m_cells.end()) {
        continue;
      }
      for (auto const& soldier : found->second) {
        bool const is_self =
            soldier.entity_id == self && soldier.slot_index == self_slot;
        if (!is_self && std::hypot(soldier.x - x, soldier.z - z) < radius) {
          out.push_back(soldier);
        }
      }
    }
  }
}

} // namespace Game::Systems::Combat
