#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <vector>

#include "../core/component.h"
#include "formation_combat_geometry.h"
#include "formation_geometry_internal.h"

namespace Game::Systems::FormationCombat {
namespace Detail {

auto layout_revisions_of(const Engine::Core::Entity& entity) noexcept
    -> LayoutRevisions {
  auto const* traversal =
      entity.get_component<Engine::Core::UnitTraversalLayoutStateComponent>();
  auto const* presentation =
      entity.get_component<Engine::Core::FormationPresentationComponent>();
  return {.traversal = traversal != nullptr ? traversal->slot_states_revision : 0U,
          .presentation = presentation != nullptr ? presentation->revision : 0U,
          .has_traversal = traversal != nullptr,
          .has_presentation = presentation != nullptr};
}

void spatialize_layout_into(const Engine::Core::Entity& entity,
                            const FormationLayout& base_layout,
                            FormationLayout& result) {
  result = base_layout;
  thread_local std::vector<SoldierSpatialAnchor> anchors;
  soldier_spatial_anchors_into(entity, base_layout, anchors);
  thread_local std::vector<const SoldierSpatialAnchor*> anchor_by_slot;
  anchor_by_slot.clear();
  for (auto const& anchor : anchors) {
    if (anchor.slot_index >= anchor_by_slot.size()) {
      anchor_by_slot.resize(static_cast<std::size_t>(anchor.slot_index) + 1U, nullptr);
    }
    anchor_by_slot[anchor.slot_index] = &anchor;
  }
  auto apply = [](std::vector<SoldierSlot>& slot_list) {
    for (auto& slot : slot_list) {
      if (slot.index >= anchor_by_slot.size() ||
          anchor_by_slot[slot.index] == nullptr) {
        continue;
      }
      auto const& anchor = *anchor_by_slot[slot.index];
      slot.row = anchor.row;
      slot.col = anchor.col;
      slot.local_x = anchor.local_x;
      slot.local_z = anchor.local_z;
      slot.local_yaw = anchor.local_yaw;
      slot.world_x = anchor.world_x;
      slot.world_z = anchor.world_z;
    }
  };
  apply(result.all_slots);
  apply(result.live_slots);
  apply(result.occupied_slots);
}

auto spatialized_layout_for(const Engine::Core::Entity& entity,
                            const FormationLayout& base_layout,
                            std::uint64_t base_signature) -> const FormationLayout& {
  auto const* transform = entity.get_component<Engine::Core::TransformComponent>();
  LayoutRevisions const revisions = layout_revisions_of(entity);

  float const world_x = transform != nullptr ? transform->position.x : 0.0F;
  float const world_z = transform != nullptr ? transform->position.z : 0.0F;
  float const yaw = transform != nullptr ? transform->rotation.y : 0.0F;

  SpatialLayoutCacheEntry& entry = caches().spatial[cache_key(entity)];
  if (entry.epoch != formation_cache_epoch()) {
    entry = SpatialLayoutCacheEntry{};
    entry.epoch = formation_cache_epoch();
  }
  if (entry.valid && entry.base_signature == base_signature &&
      entry.world_x == world_x && entry.world_z == world_z && entry.yaw == yaw &&
      entry.revisions == revisions) {
    return entry.layout;
  }

  spatialize_layout_into(entity, base_layout, entry.layout);
  entry.base_signature = base_signature;
  entry.world_x = world_x;
  entry.world_z = world_z;
  entry.yaw = yaw;
  entry.revisions = revisions;
  entry.valid = true;
  return entry.layout;
}

} // namespace Detail

namespace {

constexpr std::size_t k_missing_soldier = std::numeric_limits<std::size_t>::max();

struct RootFrame {
  float x{0.0F};
  float z{0.0F};
  float yaw_degrees{0.0F};
  float sin_yaw{0.0F};
  float cos_yaw{1.0F};
};

auto root_frame_of(const Engine::Core::TransformComponent* transform) -> RootFrame {
  RootFrame frame;
  frame.x = transform != nullptr ? transform->position.x : 0.0F;
  frame.z = transform != nullptr ? transform->position.z : 0.0F;
  frame.yaw_degrees = transform != nullptr ? transform->rotation.y : 0.0F;
  float const yaw = transform != nullptr
                        ? transform->rotation.y * std::numbers::pi_v<float> / 180.0F
                        : 0.0F;
  frame.sin_yaw = std::sin(yaw);
  frame.cos_yaw = std::cos(yaw);
  return frame;
}

void index_presentation_by_slot(
    const Engine::Core::FormationPresentationComponent* presentation,
    const FormationLayout& base_layout,
    std::vector<std::size_t>& presentation_by_slot) {
  presentation_by_slot.clear();
  if (presentation == nullptr) {
    return;
  }
  std::uint16_t max_slot = 0U;
  for (auto const& base : base_layout.live_slots) {
    max_slot = std::max(max_slot, base.index);
  }
  presentation_by_slot.assign(static_cast<std::size_t>(max_slot) + 1U,
                              k_missing_soldier);
  for (std::size_t index = 0; index < presentation->soldiers.size(); ++index) {
    auto const& soldier = presentation->soldiers[index];
    if (soldier.alive && soldier.slot_index < presentation_by_slot.size()) {
      presentation_by_slot[soldier.slot_index] = index;
    }
  }
}

void index_traversal_by_slot(
    const Engine::Core::UnitTraversalLayoutStateComponent* traversal,
    std::vector<const Engine::Core::UnitTraversalSlotState*>& traversal_by_slot) {
  traversal_by_slot.clear();
  if (traversal == nullptr) {
    return;
  }
  for (auto const& slot : traversal->slot_states) {
    if (slot.slot_index >= traversal_by_slot.size()) {
      traversal_by_slot.resize(static_cast<std::size_t>(slot.slot_index) + 1U, nullptr);
    }
    if (traversal_by_slot[slot.slot_index] == nullptr) {
      traversal_by_slot[slot.slot_index] = &slot;
    }
  }
}

void apply_presentation_fact(const Engine::Core::FormationSoldierPresentation& soldier,
                             const RootFrame& root,
                             SoldierSpatialAnchor& anchor) {
  anchor.row = soldier.row;
  anchor.col = soldier.col;
  anchor.local_x = soldier.local_x;
  anchor.local_z = soldier.local_z;
  anchor.local_yaw = soldier.local_yaw;
  if (soldier.world_motion_valid) {
    const float dx = soldier.world_x - root.x;
    const float dz = soldier.world_z - root.z;
    anchor.local_x = root.cos_yaw * dx - root.sin_yaw * dz;
    anchor.local_z = root.sin_yaw * dx + root.cos_yaw * dz;
    anchor.local_yaw = soldier.world_yaw - root.yaw_degrees;
  }
  anchor.source = SoldierAnchorSource::PresentationFacts;
}

} // namespace

void soldier_spatial_anchors_into(const Engine::Core::Entity& entity,
                                  const FormationLayout& base_layout,
                                  std::vector<SoldierSpatialAnchor>& result) {
  result.clear();
  result.reserve(base_layout.live_slots.size());
  auto const* traversal =
      entity.get_component<Engine::Core::UnitTraversalLayoutStateComponent>();
  auto const* presentation =
      entity.get_component<Engine::Core::FormationPresentationComponent>();
  RootFrame const root =
      root_frame_of(entity.get_component<Engine::Core::TransformComponent>());
  thread_local std::vector<std::size_t> presentation_by_slot;
  index_presentation_by_slot(presentation, base_layout, presentation_by_slot);
  thread_local std::vector<const Engine::Core::UnitTraversalSlotState*>
      traversal_by_slot;
  index_traversal_by_slot(traversal, traversal_by_slot);

  auto traversal_slot_for =
      [&](std::uint16_t slot_index) -> const Engine::Core::UnitTraversalSlotState* {
    if (traversal == nullptr) {
      return nullptr;
    }
    if (slot_index < traversal->slot_states.size() &&
        traversal->slot_states[slot_index].slot_index == slot_index) {
      return &traversal->slot_states[slot_index];
    }
    return slot_index < traversal_by_slot.size() ? traversal_by_slot[slot_index]
                                                 : nullptr;
  };

  float const frame_sign =
      traversal != nullptr && traversal->about_faced ? -1.0F : 1.0F;
  for (auto const& base : base_layout.live_slots) {
    SoldierSpatialAnchor anchor{
        .slot_index = base.index,
        .row = base.row,
        .col = base.col,
        .local_x = frame_sign * base.local_x,
        .local_z = frame_sign * base.local_z,
        .local_yaw = base.local_yaw,
        .source = SoldierAnchorSource::BaseLayout,
    };
    if (auto const* slot = traversal_slot_for(base.index);
        slot != nullptr && slot->alive) {
      anchor.row = slot->row;
      anchor.col = slot->col;
      anchor.local_x = slot->current_local_x;
      anchor.local_z = slot->current_local_z;
      anchor.source = SoldierAnchorSource::TraversalLayout;
    }
    if (presentation != nullptr && base.index < presentation_by_slot.size()) {
      std::size_t const soldier_index = presentation_by_slot[base.index];
      if (soldier_index != k_missing_soldier) {
        apply_presentation_fact(presentation->soldiers[soldier_index], root, anchor);
      }
    }
    anchor.world_x =
        root.x + root.cos_yaw * anchor.local_x + root.sin_yaw * anchor.local_z;
    anchor.world_z =
        root.z - root.sin_yaw * anchor.local_x + root.cos_yaw * anchor.local_z;
    result.push_back(anchor);
  }
}

auto soldier_spatial_anchors(const Engine::Core::Entity& entity,
                             const FormationLayout& base_layout)
    -> std::vector<SoldierSpatialAnchor> {
  std::vector<SoldierSpatialAnchor> result;
  soldier_spatial_anchors_into(entity, base_layout, result);
  return result;
}

auto soldier_spatial_anchors(const Engine::Core::Entity& entity)
    -> std::vector<SoldierSpatialAnchor> {
  return soldier_spatial_anchors(entity, resolve_layout(entity));
}
auto face_about_in_place(Engine::Core::Entity& entity) -> bool {
  auto* traversal =
      entity.get_component<Engine::Core::UnitTraversalLayoutStateComponent>();
  auto* transform = entity.get_component<Engine::Core::TransformComponent>();
  if (traversal == nullptr || transform == nullptr) {
    return false;
  }

  transform->rotation.y += transform->rotation.y > 0.0F ? -180.0F : 180.0F;
  traversal->about_faced = !traversal->about_faced;
  for (auto& slot : traversal->slot_states) {
    slot.start_local_x = -slot.start_local_x;
    slot.start_local_z = -slot.start_local_z;
    slot.previous_local_x = -slot.previous_local_x;
    slot.previous_local_z = -slot.previous_local_z;
    slot.current_local_x = -slot.current_local_x;
    slot.current_local_z = -slot.current_local_z;
    slot.target_local_x = -slot.target_local_x;
    slot.target_local_z = -slot.target_local_z;
    slot.velocity_x = -slot.velocity_x;
    slot.velocity_z = -slot.velocity_z;
  }
  ++traversal->slot_states_revision;

  if (auto* presentation =
          entity.get_component<Engine::Core::FormationPresentationComponent>()) {
    for (auto& soldier : presentation->soldiers) {
      soldier.local_x = -soldier.local_x;
      soldier.local_z = -soldier.local_z;
      soldier.previous_local_x = -soldier.previous_local_x;
      soldier.previous_local_z = -soldier.previous_local_z;
      soldier.relocation_velocity_x = -soldier.relocation_velocity_x;
      soldier.relocation_velocity_z = -soldier.relocation_velocity_z;
      soldier.contact_offset_x = -soldier.contact_offset_x;
      soldier.contact_offset_z = -soldier.contact_offset_z;
    }
    ++presentation->revision;
  }

  if (auto* casualties =
          entity.get_component<Engine::Core::SoldierCasualtyAnimationComponent>()) {

    for (auto& entry : casualties->entries) {
      entry.local_x = -entry.local_x;
      entry.local_z = -entry.local_z;
      entry.local_yaw += 180.0F;
      entry.launch_velocity_x = -entry.launch_velocity_x;
      entry.launch_velocity_z = -entry.launch_velocity_z;
    }
  }
  return true;
}

} // namespace Game::Systems::FormationCombat
