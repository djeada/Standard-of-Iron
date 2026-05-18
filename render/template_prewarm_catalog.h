#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "../game/systems/nation_id.h"
#include "../game/units/spawn_type.h"
#include "anim_key.h"
#include "entity/registry.h"

namespace Render::Creature {
class ArchetypeRegistry;
}

namespace Render::GL {

struct PrewarmProfile {
  std::string renderer_id;
  Game::Units::SpawnType spawn_type{Game::Units::SpawnType::Archer};
  Game::Systems::NationID nation_id{Game::Systems::NationID::RomanRepublic};
  int max_health{1};
  bool is_mounted{false};
  bool is_elephant{false};
  RenderFunc fn;
};

struct PrewarmWorkItem {
  std::size_t profile_index{0};
  int owner_id{0};
  HumanoidLOD lod{HumanoidLOD::Full};
  std::uint8_t variant{0};
  AnimKey anim_key{};
};

struct TemplatePrewarmAnimCatalog {
  std::vector<AnimKey> core_keys;
  std::vector<AnimKey> extra_keys;
};

struct TemplatePrewarmAnimSelection {
  std::vector<AnimKey> selected_core_keys;
  std::vector<AnimKey> selected_extra_keys;
  std::vector<std::uint8_t> variant_values;
  std::size_t expected_template_count{0};
};

auto build_template_prewarm_anim_catalog(const Render::Creature::ArchetypeRegistry&
                                             archetypes) -> TemplatePrewarmAnimCatalog;

auto select_template_prewarm_anim_budget(std::size_t domain_count,
                                         std::size_t target_template_count,
                                         const TemplatePrewarmAnimCatalog& catalog)
    -> TemplatePrewarmAnimSelection;

auto build_template_prewarm_work_items(const std::vector<PrewarmProfile>& profiles,
                                       const std::vector<int>& owner_ids,
                                       const std::vector<std::uint8_t>& variant_values,
                                       const std::vector<AnimKey>& anim_keys)
    -> std::vector<PrewarmWorkItem>;

} // namespace Render::GL
