#include "template_prewarm_catalog.h"

#include <algorithm>
#include <unordered_set>

#include "../game/core/component.h"
#include "creature/archetype_registry.h"
#include "creature/pose_intent.h"

namespace Render::GL {

namespace {

using Render::Creature::PoseIntent;

constexpr CombatAnimPhase k_attack_phases[] = {CombatAnimPhase::Idle,
                                               CombatAnimPhase::Advance,
                                               CombatAnimPhase::WindUp,
                                               CombatAnimPhase::Strike,
                                               CombatAnimPhase::Impact,
                                               CombatAnimPhase::Recover,
                                               CombatAnimPhase::Reposition};

void push_anim_key(std::vector<AnimKey>& keys,
                   PoseIntent state,
                   CombatAnimPhase phase,
                   std::uint8_t frame,
                   std::uint8_t attack_variant) {
  AnimKey key{};
  key.state = state;
  key.combat_phase = phase;
  key.frame = frame;
  key.attack_variant = attack_variant;
  keys.push_back(key);
}

void add_state_frames(std::vector<AnimKey>& keys, PoseIntent state, int frame_step) {
  int const step = std::max(1, frame_step);
  for (int frame = 0; frame < static_cast<int>(k_anim_frame_count); frame += step) {
    push_anim_key(
        keys, state, CombatAnimPhase::Idle, static_cast<std::uint8_t>(frame), 0);
  }
}

void add_attack_frames(std::vector<AnimKey>& keys,
                       PoseIntent state,
                       int frame_step,
                       std::uint8_t variant_count) {
  int const step = std::max(1, frame_step);
  variant_count = std::max<std::uint8_t>(variant_count, 1U);
  for (std::uint8_t attack_variant = 0; attack_variant < variant_count;
       ++attack_variant) {
    for (CombatAnimPhase const phase : k_attack_phases) {
      for (int frame = 0; frame < static_cast<int>(k_anim_frame_count); frame += step) {
        push_anim_key(
            keys, state, phase, static_cast<std::uint8_t>(frame), attack_variant);
      }
    }
  }
}

void add_core_attack_frames(std::vector<AnimKey>& keys, PoseIntent state) {
  for (CombatAnimPhase const phase : k_attack_phases) {
    push_anim_key(keys, state, phase, 0, 0);
  }
}

auto prewarm_attack_family_for_spawn(Game::Units::SpawnType spawn_type,
                                     PoseIntent state) noexcept
    -> Engine::Core::CombatAttackFamily {
  switch (state) {
  case PoseIntent::AttackMelee: {
    auto const family = Engine::Core::resolve_combat_attack_family(
        spawn_type, Engine::Core::AttackComponent::CombatMode::Melee);
    return family == Engine::Core::CombatAttackFamily::Spear
               ? Engine::Core::CombatAttackFamily::None
               : family;
  }
  case PoseIntent::AttackSpear: {
    auto const family = Engine::Core::resolve_combat_attack_family(
        spawn_type, Engine::Core::AttackComponent::CombatMode::Melee);
    return family == Engine::Core::CombatAttackFamily::Spear
               ? family
               : Engine::Core::CombatAttackFamily::None;
  }
  case PoseIntent::AttackRanged:
    return Engine::Core::resolve_combat_attack_family(
        spawn_type, Engine::Core::AttackComponent::CombatMode::Ranged);
  default:
    return Engine::Core::CombatAttackFamily::None;
  }
}

} // namespace

auto build_template_prewarm_anim_catalog(const Render::Creature::ArchetypeRegistry&
                                             archetypes) -> TemplatePrewarmAnimCatalog {
  TemplatePrewarmAnimCatalog catalog;
  catalog.core_keys.reserve(192);

  push_anim_key(catalog.core_keys, PoseIntent::Idle, CombatAnimPhase::Idle, 0, 0);
  push_anim_key(catalog.core_keys, PoseIntent::Walk, CombatAnimPhase::Idle, 0, 0);
  push_anim_key(catalog.core_keys, PoseIntent::Run, CombatAnimPhase::Idle, 0, 0);
  push_anim_key(catalog.core_keys, PoseIntent::Hold, CombatAnimPhase::Idle, 0, 0);
  push_anim_key(catalog.core_keys, PoseIntent::Construct, CombatAnimPhase::Idle, 0, 0);
  push_anim_key(catalog.core_keys, PoseIntent::Healing, CombatAnimPhase::Idle, 0, 0);
  push_anim_key(
      catalog.core_keys, PoseIntent::HitReaction, CombatAnimPhase::Idle, 0, 0);
  push_anim_key(catalog.core_keys, PoseIntent::Dying, CombatAnimPhase::Idle, 0, 0);
  push_anim_key(catalog.core_keys, PoseIntent::Dead, CombatAnimPhase::Idle, 0, 0);
  add_core_attack_frames(catalog.core_keys, PoseIntent::AttackMelee);
  add_core_attack_frames(catalog.core_keys, PoseIntent::AttackSpear);
  add_core_attack_frames(catalog.core_keys, PoseIntent::AttackRanged);

  std::vector<AnimKey> full_keys;
  full_keys.reserve(1024);
  push_anim_key(full_keys, PoseIntent::Idle, CombatAnimPhase::Idle, 0, 0);
  add_state_frames(full_keys, PoseIntent::Walk, 1);
  add_state_frames(full_keys, PoseIntent::Run, 1);
  add_state_frames(full_keys, PoseIntent::Hold, 1);
  add_state_frames(full_keys, PoseIntent::Construct, 1);
  add_state_frames(full_keys, PoseIntent::Healing, 1);
  add_state_frames(full_keys, PoseIntent::HitReaction, 1);
  add_state_frames(full_keys, PoseIntent::Dying, 1);
  push_anim_key(full_keys, PoseIntent::Dead, CombatAnimPhase::Idle, 0, 0);

  auto const sword_variant_count = std::max(
      archetypes.clip_variant_count(
          Render::Creature::ArchetypeRegistry::k_humanoid_base,
          Render::Creature::AnimationStateId::AttackSword),
      archetypes.clip_variant_count(Render::Creature::ArchetypeRegistry::k_rider_base,
                                    Render::Creature::AnimationStateId::AttackSword));
  auto const spear_variant_count = std::max(
      archetypes.clip_variant_count(
          Render::Creature::ArchetypeRegistry::k_humanoid_base,
          Render::Creature::AnimationStateId::AttackSpear),
      archetypes.clip_variant_count(Render::Creature::ArchetypeRegistry::k_rider_base,
                                    Render::Creature::AnimationStateId::AttackSpear));
  auto const ranged_variant_count = archetypes.clip_variant_count(
      Render::Creature::ArchetypeRegistry::k_humanoid_base,
      Render::Creature::AnimationStateId::AttackBow);
  add_attack_frames(full_keys, PoseIntent::AttackMelee, 1, sword_variant_count);
  add_attack_frames(full_keys, PoseIntent::AttackSpear, 1, spear_variant_count);
  add_attack_frames(full_keys, PoseIntent::AttackRanged, 1, ranged_variant_count);

  std::unordered_set<AnimKey, AnimKeyHash> core_key_set;
  core_key_set.reserve(catalog.core_keys.size() * 2U);
  for (auto const& key : catalog.core_keys) {
    core_key_set.insert(key);
  }

  catalog.extra_keys.reserve(full_keys.size());
  for (auto const& key : full_keys) {
    if (core_key_set.find(key) == core_key_set.end()) {
      catalog.extra_keys.push_back(key);
    }
  }

  return catalog;
}

auto select_template_prewarm_anim_budget(std::size_t domain_count,
                                         std::size_t target_template_count,
                                         const TemplatePrewarmAnimCatalog& catalog)
    -> TemplatePrewarmAnimSelection {
  TemplatePrewarmAnimSelection selection;

  std::size_t variant_count = k_template_variant_count;
  std::size_t const core_anim_count = catalog.core_keys.size();
  std::size_t const full_anim_count =
      catalog.core_keys.size() + catalog.extra_keys.size();
  std::size_t const core_per_variant = domain_count * core_anim_count;
  if (core_per_variant > 0) {
    std::size_t const max_variants_for_core = target_template_count / core_per_variant;
    variant_count =
        std::clamp<std::size_t>(max_variants_for_core, 1, k_template_variant_count);
  }

  // Keep at least four visual variants in the prewarm set; cache sizing is clamped
  // later.
  variant_count = std::max<std::size_t>(variant_count, 4U);

  std::size_t anim_count_budget =
      target_template_count / std::max<std::size_t>(1, domain_count * variant_count);
  anim_count_budget = std::max<std::size_t>(anim_count_budget, 1);

  std::size_t const anim_count =
      std::max(core_anim_count, std::min(full_anim_count, anim_count_budget));

  selection.variant_values.reserve(variant_count);
  for (std::size_t i = 0; i < variant_count; ++i) {
    std::size_t idx = 0;
    if (variant_count > 1) {
      idx = (i * (k_template_variant_count - 1)) / (variant_count - 1);
    }
    selection.variant_values.push_back(static_cast<std::uint8_t>(idx));
  }

  std::size_t const core_take = std::min(core_anim_count, anim_count);
  selection.selected_core_keys.reserve(core_take);
  selection.selected_core_keys.insert(selection.selected_core_keys.end(),
                                      catalog.core_keys.begin(),
                                      catalog.core_keys.begin() +
                                          static_cast<std::ptrdiff_t>(core_take));

  if (anim_count > core_take) {
    std::size_t const extra_take =
        std::min<std::size_t>(anim_count - core_take, catalog.extra_keys.size());
    selection.selected_extra_keys.reserve(extra_take);
    selection.selected_extra_keys.insert(selection.selected_extra_keys.end(),
                                         catalog.extra_keys.begin(),
                                         catalog.extra_keys.begin() +
                                             static_cast<std::ptrdiff_t>(extra_take));
  }

  std::size_t const selected_anim_total =
      selection.selected_core_keys.size() + selection.selected_extra_keys.size();
  selection.expected_template_count =
      domain_count * selection.variant_values.size() * selected_anim_total;
  return selection;
}

auto build_template_prewarm_work_items(const std::vector<PrewarmProfile>& profiles,
                                       const std::vector<int>& owner_ids,
                                       const std::vector<std::uint8_t>& variant_values,
                                       const std::vector<AnimKey>& anim_keys)
    -> std::vector<PrewarmWorkItem> {
  std::vector<PrewarmWorkItem> items;
  items.reserve(profiles.size() * owner_ids.size() * 2U * variant_values.size() *
                anim_keys.size());

  constexpr HumanoidLOD lods[] = {HumanoidLOD::Full, HumanoidLOD::Minimal};
  for (std::size_t profile_idx = 0; profile_idx < profiles.size(); ++profile_idx) {
    auto const spawn_type = profiles[profile_idx].spawn_type;
    for (int const owner_id : owner_ids) {
      for (HumanoidLOD const lod : lods) {
        for (std::uint8_t const variant : variant_values) {
          for (auto const& anim_key : anim_keys) {
            auto const attack_family =
                prewarm_attack_family_for_spawn(spawn_type, anim_key.state);
            if (Render::Creature::is_attack_pose_intent(anim_key.state) &&
                attack_family == Engine::Core::CombatAttackFamily::None) {
              continue;
            }

            PrewarmWorkItem item;
            item.profile_index = profile_idx;
            item.owner_id = owner_id;
            item.lod = lod;
            item.variant = variant;
            item.anim_key = anim_key;
            item.anim_key.attack_family = attack_family;
            items.push_back(item);
          }
        }
      }
    }
  }

  return items;
}

} // namespace Render::GL
