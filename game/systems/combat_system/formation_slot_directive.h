#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <unordered_map>
#include <vector>

#include "../../core/component.h"
#include "../../core/world.h"
#include "../formation_combat_geometry.h"
#include "../navigation/pathfinding.h"
#include "formation_foreign_grid.h"
#include "formation_structure_facade.h"

namespace Game::Systems::Combat {

class LayoutCache {
public:
  void begin_tick() {
    m_index.clear();
    m_used = 0U;
  }

  auto
  for_entity(Engine::Core::Entity& entity) -> const FormationCombat::FormationLayout& {
    auto const [entry, inserted] = m_index.try_emplace(entity.get_id(), m_used);
    if (inserted) {
      if (m_used == m_pool.size()) {
        m_pool.emplace_back();
      }
      FormationCombat::resolve_layout_into(entity, m_pool[m_used]);
      ++m_used;
    }
    return m_pool[entry->second];
  }

private:
  std::unordered_map<Engine::Core::EntityID, std::size_t> m_index;
  std::deque<FormationCombat::FormationLayout> m_pool;
  std::size_t m_used{0U};
};

struct ForeignFighters {
  ForeignSoldierGrid grid;
  std::vector<ForeignSoldier> scratch;
};

struct DamageCarrier {
  const Engine::Core::FormationContactFront* front{nullptr};
  std::optional<std::uint16_t> attacker_slot;
};

struct EntityFrame {
  Engine::Core::World& world;
  Engine::Core::Entity& entity;
  const Engine::Core::UnitComponent& unit;
  const FormationCombat::FormationLayout& layout;
  LayoutCache& layouts;
  ForeignFighters& foreign;
  Engine::Core::FormationPresentationComponent& presentation;
  const Engine::Core::TransformComponent* actor{nullptr};
  const Engine::Core::FormationContactComponent* contact{nullptr};
  const Engine::Core::UnitTraversalLayoutStateComponent* traversal{nullptr};
  Engine::Core::SquadReformComponent* reform{nullptr};
  Engine::Core::Entity* display_opponent{nullptr};
  Engine::Core::EntityID display_target{0U};
  bool target_alive{false};
  bool melee_ordered{false};
  bool in_melee_contact{false};
  float combat_motion_time{0.0F};
  StructureFacadeFrame structure;
  std::vector<DamageCarrier> damage_carriers;
  float squad_speed{0.0F};
  Pathfinding::Passability passability{Pathfinding::Passability::Light};
  bool mounted{false};
  float frame_sign{1.0F};
  float delta_time{0.0F};
};

[[nodiscard]] auto publish_soldiers(const EntityFrame& frame) -> bool;

} // namespace Game::Systems::Combat
