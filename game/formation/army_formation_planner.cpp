#include "army_formation_planner.h"

#include <QCoreApplication>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_map>

#include "../core/world.h"
#include "../systems/navigation/nav_grid.h"
#include "army_formation_registry.h"
#include "formation_assignment.h"
#include "formation_frame.h"
#include "formation_line_layout.h"
#include "formation_silhouette.h"
#include "formation_slot_adjust.h"
#include "formation_slot_fitter.h"

namespace Game::Formation {

using planning::Bounds;
using planning::k_deg_to_rad;
using planning::lane_for;

namespace {

using MemberIndex = std::unordered_map<EntityID, const ArmyFormationMember*>;

class Hasher {
public:
  void mix(std::uint64_t value) {
    m_state ^= value + 0x9e3779b97f4a7c15ULL + (m_state << 6U) + (m_state >> 2U);
  }

  void mix_float(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    mix(static_cast<std::uint64_t>(bits));
  }

  void mix(const std::string& value) {
    for (char const character : value) {
      mix(static_cast<std::uint64_t>(static_cast<unsigned char>(character)));
    }
    mix(value.size());
  }

  [[nodiscard]] auto value() const -> std::uint64_t { return m_state; }

private:
  std::uint64_t m_state{0xcbf29ce484222325ULL};
};

auto index_members(const std::vector<ArmyFormationMember>& members) -> MemberIndex {
  MemberIndex by_id;
  by_id.reserve(members.size());
  for (const auto& member : members) {
    by_id.emplace(member.entity_id, &member);
  }
  return by_id;
}

auto troop_kind_key(const ArmyFormationMember& member) -> std::uint64_t {
  Hasher hasher;
  hasher.mix(static_cast<std::uint64_t>(member.troop_type));
  hasher.mix(static_cast<std::uint64_t>(member.individuals));
  hasher.mix(static_cast<std::uint64_t>(member.heavy));
  return hasher.value();
}

void hash_members(Hasher& hasher, const std::vector<ArmyFormationMember>& members) {
  for (const auto& member : members) {
    hasher.mix(member.entity_id);
    hasher.mix(static_cast<std::uint64_t>(member.troop_type));
    hasher.mix(static_cast<std::uint64_t>(member.roles));
    hasher.mix(member.doctrine);
    hasher.mix_float(member.footprint);

    hasher.mix_float(member.current_position.x());
    hasher.mix_float(member.current_position.z());
  }
}

void hash_options(Hasher& hasher, const ArmyFormationOptions& options) {
  hasher.mix(static_cast<std::uint64_t>(options.flank_preference));
  hasher.mix(static_cast<std::uint64_t>(options.ranged_placement));
  hasher.mix(static_cast<std::uint64_t>(options.mixed_policy));
  hasher.mix(static_cast<std::uint64_t>(options.movement_policy));
  hasher.mix_float(options.frontage_scale);
  hasher.mix_float(options.depth_scale);
  hasher.mix_float(options.spacing_scale);
  hasher.mix(static_cast<std::uint64_t>(options.reserve_rows));
  hasher.mix(static_cast<std::uint64_t>(options.preserve_member_order));
  hasher.mix(static_cast<std::uint64_t>(options.doctrine_locked));
}

auto displacement_score(const ArmyFormationPlan& plan,
                        const QVector3D& requested_anchor) -> float {
  float const pitch = std::max(1.0F, plan.slot_spacing);
  QVector3D const moved(plan.anchor.x() - requested_anchor.x(),
                        0.0F,
                        plan.anchor.z() - requested_anchor.z());
  return static_cast<float>(plan.blocked_count * 4 + plan.adjusted_count) +
         plan.displacement / pitch + (plan.narrowed ? 1.0F : 0.0F) +
         moved.length() / pitch * 0.5F;
}

auto anchor_shifts(const ArmyFormationPlan& best,
                   float facing) -> std::array<QVector3D, 6> {
  float const yaw = facing * k_deg_to_rad;
  QVector3D const forward(std::sin(yaw), 0.0F, std::cos(yaw));
  QVector3D const lateral(std::cos(yaw), 0.0F, -std::sin(yaw));
  float const depth = std::max(best.depth, best.slot_spacing);
  float const frontage = std::max(best.frontage, best.slot_spacing);
  return {forward * (-0.25F * depth),
          forward * (-0.5F * depth),
          lateral * (-0.25F * frontage),
          lateral * (0.25F * frontage),
          forward * (0.25F * depth),
          forward * (0.5F * depth)};
}

struct LocalSlotOvershoot {
  float width{0.0F};
  float depth{0.0F};
};

auto overshoot_of(const std::vector<FormationSlot>& slot_list,
                  const DoctrineIntentTemplate& tmpl,
                  float requested_frontage) -> LocalSlotOvershoot {
  Bounds measured;
  for (const auto& slot : slot_list) {
    measured.expand(slot.local_offset);
  }
  return {tmpl.max_frontage > 0.1F && requested_frontage <= 0.01F
              ? measured.width() - tmpl.max_frontage
              : 0.0F,
          tmpl.max_depth > 0.1F ? measured.depth() - tmpl.max_depth : 0.0F};
}

auto tightest_line_layout(const std::vector<ArmyFormationMember>& shaped,
                          const DoctrineIntentTemplate& tmpl,
                          const ArmyFormationRequest& request,
                          float spacing) -> planning::LineLayout {
  constexpr int k_bound_attempts = 6;
  int row_cap_override = 0;
  planning::LineLayout best;
  best.slot_spacing = spacing;
  float best_excess = std::numeric_limits<float>::max();
  for (int attempt = 0; attempt < k_bound_attempts; ++attempt) {
    auto candidate = planning::plan_line_layout(shaped,
                                                tmpl,
                                                request.options,
                                                spacing,
                                                request.frontage,
                                                request.facing,
                                                row_cap_override);
    auto const over = overshoot_of(candidate.slot_list, tmpl, request.frontage);
    float const excess = std::max(0.0F, over.width) + std::max(0.0F, over.depth);
    int const current = std::max(1, candidate.row_cap_used);
    if (excess < best_excess) {
      best_excess = excess;
      best = std::move(candidate);
    }
    if (excess <= 0.0F) {
      break;
    }
    int const next = over.width > over.depth
                         ? std::max(1, current - std::max(1, current / 5))
                         : current + std::max(1, current / 5);
    if (next == current) {
      break;
    }
    row_cap_override = next;
  }
  return best;
}

auto slot_extents_of(const std::vector<FormationSlot>& slot_list,
                     const MemberIndex& by_id) -> planning::SlotExtents {
  planning::SlotExtents extents;
  extents.half_width.reserve(slot_list.size());
  extents.half_depth.reserve(slot_list.size());
  for (const auto& slot : slot_list) {
    auto const it = by_id.find(slot.occupant);
    extents.half_width.push_back(it == by_id.end() ? 0.5F : it->second->half_width);
    extents.half_depth.push_back(it == by_id.end() ? 0.5F : it->second->half_depth);
  }
  return extents;
}

auto slot_kind_keys(const std::vector<FormationSlot>& slot_list,
                    const MemberIndex& by_id) -> std::vector<std::uint64_t> {
  std::vector<std::uint64_t> keys(slot_list.size(), 0U);
  for (std::size_t i = 0; i < slot_list.size(); ++i) {
    auto const found = by_id.find(slot_list[i].occupant);
    if (found != by_id.end()) {
      keys[i] = troop_kind_key(*found->second);
    }
  }
  return keys;
}

void record_member_footprints(ArmyFormationLayout& layout,
                              const MemberIndex& by_id,
                              const DoctrineIntentTemplate& tmpl) {
  auto const count = layout.slot_list.size();
  layout.slot_clearance.assign(count, layout.slot_spacing * 0.5F);
  layout.slot_half_width.assign(count, layout.slot_spacing * 0.5F);
  layout.slot_half_depth.assign(count, layout.slot_spacing * 0.5F);
  layout.slot_files.assign(count, 0);
  for (std::size_t i = 0; i < count; ++i) {
    auto& slot = layout.slot_list[i];
    auto const found = by_id.find(slot.occupant);
    if (found == by_id.end()) {
      slot.half_width = layout.slot_half_width[i];
      slot.half_depth = layout.slot_half_depth[i];
      continue;
    }
    layout.slot_files[i] = tmpl.unit_files_aspect > 0.0F ? found->second->files : 0;
    layout.slot_clearance[i] =
        std::min(found->second->half_width, found->second->half_depth);
    layout.slot_half_width[i] = found->second->half_width;
    layout.slot_half_depth[i] = found->second->half_depth;
    slot.half_width = found->second->half_width;
    slot.half_depth = found->second->half_depth;
    slot.heavy = found->second->heavy;
  }
}

void record_assignment_inputs(ArmyFormationLayout& layout,
                              const MemberIndex& by_id,
                              const ArmyFormationRequest& request,
                              const ArmyFormation* previous_group) {
  layout.assign_by_distance =
      request.assign_nearest &&
      !(request.preserve_previous_slots && request.group_id != k_invalid_group &&
        previous_group != nullptr);
  layout.slot_start.assign(layout.slot_list.size(), QVector3D());
  layout.slot_kind.assign(layout.slot_list.size(), 0U);
  for (std::size_t i = 0; i < layout.slot_list.size(); ++i) {
    auto const found = by_id.find(layout.slot_list[i].occupant);
    if (found == by_id.end()) {
      continue;
    }
    layout.slot_start[i] = found->second->current_position;
    layout.slot_kind[i] = troop_kind_key(*found->second);
  }
}

void arrange_members(ArmyFormationLayout& layout,
                     const MemberIndex& members_by_id,
                     const ArmyFormationRequest& request,
                     const DoctrineIntentTemplate& tmpl,
                     const ArmyFormation* previous_group) {
  planning::regularize_silhouette(
      layout.slot_list,
      slot_extents_of(layout.slot_list, members_by_id),
      {request.intent, request.frontage, layout.spacing, request.options, tmpl});

  if (request.preserve_previous_slots && request.group_id != k_invalid_group &&
      previous_group != nullptr) {
    planning::keep_previous_occupants(layout.slot_list,
                                      slot_kind_keys(layout.slot_list, members_by_id),
                                      previous_group->slot_list);
  }

  record_member_footprints(layout, members_by_id, tmpl);
  planning::separate_footprints(layout.slot_list,
                                layout.slot_half_width,
                                layout.slot_half_depth,
                                layout.footprint_gap);
  planning::recentre_on_centroid(layout.slot_list);
  record_assignment_inputs(layout, members_by_id, request, previous_group);

  Bounds bounds;
  for (const auto& slot : layout.slot_list) {
    bounds.expand(slot.local_offset);
  }
  layout.frontage = bounds.width();
  layout.depth = bounds.depth();
  layout.valid = true;
}

auto reject(ArmyFormationLayout& layout, std::string reason) -> ArmyFormationLayout {
  layout.rejection_reason = std::move(reason);
  return std::move(layout);
}

} // namespace

auto ArmyFormationPlanner::fit_to_ground(
    const ArmyFormationLayout& first_layout,
    const std::vector<ArmyFormationMember>& members,
    const ArmyFormationRequest& request,
    const ArmyFormation* previous_group) -> ArmyFormationPlan {
  constexpr int k_attempts = 4;
  constexpr float k_narrowing = 0.62F;

  ArmyFormationPlan best = place(first_layout, request);
  if (!best.valid || best.keeps_shape() || !request.resolve_terrain) {
    return best;
  }
  QVector3D const requested_anchor = best.anchor;
  auto adopt_if_better = [&](ArmyFormationPlan&& plan) {
    if (displacement_score(plan, requested_anchor) <
        displacement_score(best, requested_anchor)) {
      best = std::move(plan);
    }
  };

  if (request.allow_anchor_shift) {
    for (const auto& shift : anchor_shifts(best, request.facing)) {
      ArmyFormationRequest shifted = request;
      shifted.anchor = request.anchor + shift;
      auto plan = place(first_layout, shifted);
      if (!plan.valid) {
        continue;
      }
      adopt_if_better(std::move(plan));
      if (best.keeps_shape()) {
        return best;
      }
    }
  }

  ArmyFormationRequest attempt = request;
  float current = request.frontage > 0.01F ? request.frontage : best.frontage;
  for (int index = 1; index < k_attempts; ++index) {
    attempt.frontage = std::max(1.0F, current * k_narrowing);
    if (attempt.frontage >= current - 0.01F) {
      break;
    }
    current = attempt.frontage;
    auto plan = place(build_layout(members, attempt, previous_group), attempt);
    if (!plan.valid) {
      break;
    }
    plan.narrowed = true;
    adopt_if_better(std::move(plan));
    if (best.keeps_shape()) {
      break;
    }
  }
  return best;
}

auto ArmyFormationPlanner::plan(const std::vector<ArmyFormationMember>& members,
                                const ArmyFormationRequest& request,
                                const ArmyFormation* previous_group)
    -> ArmyFormationPlan {
  return fit_to_ground(
      build_layout(members, request, previous_group), members, request, previous_group);
}

auto ArmyFormationPlanner::plan(Engine::Core::World& world,
                                const ArmyFormationRequest& request)
    -> ArmyFormationPlan {
  const ArmyFormation* previous =
      request.group_id != k_invalid_group
          ? ArmyFormationRegistry::for_world(world).find(request.group_id)
          : nullptr;
  const auto members = collect_members(world, request.members);
  return plan(members, request, previous);
}

auto ArmyFormationPlanner::plan(const std::vector<ArmyFormationMember>& members,
                                const ArmyFormationRequest& request)
    -> ArmyFormationPlan {
  return plan(members, request, nullptr);
}

auto ArmyFormationPlanner::resolve_movement_policy(
    MovementPolicy requested, const DoctrineIntentTemplate& tmpl) -> MovementPolicy {
  if (requested != MovementPolicy::DoctrineDefault) {
    return requested;
  }
  return tmpl.default_movement == MovementPolicy::DoctrineDefault
             ? MovementPolicy::ReformAtDestination
             : tmpl.default_movement;
}

auto ArmyFormationPlanner::layout_from_reference(const ArmyFormation& formation)
    -> ArmyFormationLayout {
  ArmyFormationLayout layout;
  layout.doctrine = formation.doctrine;
  layout.intent = formation.intent;
  layout.spacing = formation.spacing;
  layout.slot_spacing = formation.slot_spacing;
  layout.footprint_gap = lane_for(formation.spacing) * 0.5F;
  layout.movement_policy = formation.options.movement_policy;
  layout.slot_list = formation.reference_slots;
  Bounds bounds;
  for (const auto& slot : layout.slot_list) {
    bounds.expand(slot.local_offset);
    layout.slot_clearance.push_back(std::min(slot.half_width, slot.half_depth));
    layout.slot_half_width.push_back(slot.half_width);
    layout.slot_half_depth.push_back(slot.half_depth);
    layout.slot_files.push_back(0);
  }
  layout.frontage = bounds.width();
  layout.depth = bounds.depth();
  layout.valid = !layout.slot_list.empty();
  if (!layout.valid) {
    layout.rejection_reason = "The formation has no reference shape.";
  }
  return layout;
}

void ArmyFormationPlanner::fold_onto_reference(
    ArmyFormationPlan& plan, const std::vector<FormationSlot>& reference) {
  planning::fold_onto_reference(plan.slot_list, reference);
}

auto ArmyFormationPlanner::min_cost_assignment(
    const std::vector<std::vector<float>>& cost) -> std::vector<int> {
  return planning::min_cost_assignment(cost);
}

auto ArmyFormationPlanner::plan_local_slots(
    const std::vector<ArmyFormationMember>& members,
    const DoctrineIntentTemplate& tmpl,
    const ArmyFormationOptions& options,
    float spacing,
    float requested_frontage,
    float facing,
    float* slot_spacing_out,
    int row_cap_override,
    int* row_cap_used) -> std::vector<FormationSlot> {
  auto layout = planning::plan_line_layout(
      members, tmpl, options, spacing, requested_frontage, facing, row_cap_override);
  if (members.empty()) {
    return std::move(layout.slot_list);
  }
  if (row_cap_used != nullptr) {
    *row_cap_used = layout.row_cap_used;
  }
  if (slot_spacing_out != nullptr) {
    *slot_spacing_out = layout.slot_spacing;
  }
  return std::move(layout.slot_list);
}

auto ArmyFormationPlanner::footprints_overlap(const FormationSlot& a,
                                              const FormationSlot& b,
                                              float gap) -> bool {
  auto const axes = planning::frame_axes(a.facing);
  QVector3D const offset = b.world_position - a.world_position;
  return planning::local_overlap(QVector3D::dotProduct(offset, axes.lateral),
                                 QVector3D::dotProduct(offset, axes.depth),
                                 a.half_width,
                                 a.half_depth,
                                 b.half_width,
                                 b.half_depth,
                                 gap);
}

auto ArmyFormationPlanner::first_overlap(const std::vector<FormationSlot>& slot_list,
                                         float gap) -> std::pair<int, int> {
  for (std::size_t i = 0; i < slot_list.size(); ++i) {
    if (slot_list[i].status == SlotStatus::Blocked) {
      continue;
    }
    for (std::size_t j = i + 1; j < slot_list.size(); ++j) {
      if (slot_list[j].status == SlotStatus::Blocked) {
        continue;
      }
      if (footprints_overlap(slot_list[i], slot_list[j], gap)) {
        return {static_cast<int>(i), static_cast<int>(j)};
      }
    }
  }
  return {-1, -1};
}

auto ArmyFormationPlanner::layout_signature(
    const std::vector<ArmyFormationMember>& members,
    const ArmyFormationRequest& request,
    const ArmyFormation* previous_group) -> std::uint64_t {
  Hasher hasher;
  hash_members(hasher, members);

  hasher.mix_float(request.facing);
  hasher.mix(static_cast<std::uint64_t>(request.intent));
  for (const auto& member : members) {
    hasher.mix(static_cast<std::uint64_t>(member.individuals));
  }
  hasher.mix(request.doctrine);
  hasher.mix_float(request.spacing);
  hasher.mix_float(request.frontage);
  hasher.mix(request.group_id);
  hasher.mix(static_cast<std::uint64_t>(request.preserve_previous_slots));
  hasher.mix(static_cast<std::uint64_t>(request.assign_nearest));
  hash_options(hasher, request.options);

  if (request.preserve_previous_slots && request.group_id != k_invalid_group &&
      previous_group != nullptr) {
    for (const auto& slot : previous_group->slot_list) {
      hasher.mix(slot.occupant);
      hasher.mix(static_cast<std::uint64_t>(slot.id));
    }
  }
  return hasher.value();
}

auto ArmyFormationPlanner::build_layout(const std::vector<ArmyFormationMember>& members,
                                        const ArmyFormationRequest& request,
                                        const ArmyFormation* previous_group)
    -> ArmyFormationLayout {
  ArmyFormationLayout layout;
  layout.intent = request.intent;
  layout.signature = layout_signature(members, request, previous_group);

  if (members.empty()) {
    return reject(layout,
                  QCoreApplication::translate(
                      "Formation", "No units eligible for formation placement.")
                      .toStdString());
  }

  layout.doctrine = resolve_doctrine(members, request);
  const auto& doctrine = DoctrineRegistry::instance().get_or_neutral(layout.doctrine);
  auto const reason = DoctrineRegistry::instance().availability_reason(
      layout.doctrine,
      request.intent,
      combined_roles(members),
      static_cast<int>(members.size()));
  if (!reason.empty()) {
    return reject(layout, reason);
  }
  const auto* tmpl = doctrine.resolve_template(request.intent);
  if (tmpl == nullptr) {
    return reject(
        layout,
        QCoreApplication::translate(
            "Formation", "This doctrine has no template for the chosen formation.")
            .toStdString());
  }

  float const spacing =
      std::max(0.2F,
               request.spacing * tmpl->spacing_scale *
                   std::clamp(request.options.spacing_scale, 0.4F, 2.5F));
  layout.spacing = spacing;
  layout.footprint_gap = lane_for(spacing) * 0.5F;
  layout.movement_policy =
      resolve_movement_policy(request.options.movement_policy, *tmpl);

  std::vector<ArmyFormationMember> shaped = members;
  for (auto& member : shaped) {
    shape_member_for_intent(member, tmpl->unit_files_aspect);
  }
  auto line_layout = tightest_line_layout(shaped, *tmpl, request, spacing);
  layout.slot_list = std::move(line_layout.slot_list);
  layout.slot_spacing = line_layout.slot_spacing;
  if (layout.slot_list.empty()) {
    return reject(layout, "The formation template produced no slot_list.");
  }

  arrange_members(layout, index_members(shaped), request, *tmpl, previous_group);
  return layout;
}

namespace {

void begin_plan_from_layout(ArmyFormationPlan& plan,
                            const ArmyFormationLayout& layout,
                            const ArmyFormationRequest& request) {
  plan.anchor = request.anchor;
  plan.facing = request.facing;
  plan.intent = layout.intent;
  plan.doctrine = layout.doctrine;
  plan.spacing = layout.spacing;
  plan.slot_spacing = layout.slot_spacing;
  plan.frontage = layout.frontage;
  plan.depth = layout.depth;
  plan.footprint_gap = layout.footprint_gap;
  plan.movement_policy = layout.movement_policy;
}

void copy_layout_slots(ArmyFormationPlan& plan, const ArmyFormationLayout& layout) {
  plan.slot_list = layout.slot_list;
  plan.slot_clearance = layout.slot_clearance;
  plan.slot_half_width = layout.slot_half_width;
  plan.slot_half_depth = layout.slot_half_depth;
  plan.slot_files = layout.slot_files;
  plan.slot_clearance.resize(plan.slot_list.size(), layout.slot_spacing * 0.5F);
  plan.slot_half_width.resize(plan.slot_list.size(), layout.slot_spacing * 0.5F);
  plan.slot_half_depth.resize(plan.slot_list.size(), layout.slot_spacing * 0.5F);
  plan.slot_files.resize(plan.slot_list.size(), 0);
}

auto walkable_anchor(const ArmyFormationRequest& request) -> QVector3D {
  return request.resolve_terrain &&
                 !Game::Systems::NavGrid::is_world_position_walkable(request.anchor)
             ? Game::Systems::NavGrid::snap_to_walkable_ground(request.anchor, 15)
             : request.anchor;
}

auto front_to_back_slot_order(const std::vector<FormationSlot>& placed)
    -> std::vector<std::size_t> {
  std::vector<std::size_t> ordered;
  ordered.reserve(placed.size());
  for (std::size_t i = 0; i < placed.size(); ++i) {
    ordered.push_back(i);
  }
  std::stable_sort(
      ordered.begin(), ordered.end(), [&placed](std::size_t ia, std::size_t ib) {
        const auto* a = &placed[ia];
        const auto* b = &placed[ib];
        if (a->local_offset.z() != b->local_offset.z()) {
          return a->local_offset.z() > b->local_offset.z();
        }
        return std::abs(a->local_offset.x()) < std::abs(b->local_offset.x());
      });
  return ordered;
}

void fit_slot_to_ground(ArmyFormationPlan& plan,
                        std::size_t index,
                        const ArmyFormationRequest& request,
                        const QVector3D& anchor,
                        planning::SlotTerrainFitter& fitter) {
  auto* slot = &plan.slot_list[index];
  QVector3D const rotated = planning::rotate_offset(slot->local_offset, request.facing);
  QVector3D const ideal(anchor.x() + rotated.x(), anchor.y(), anchor.z() + rotated.z());
  SlotStatus status = SlotStatus::Valid;
  slot->world_position = fitter.fit(ideal,
                                    plan.slot_half_width[index],
                                    plan.slot_half_depth[index],
                                    slot->heavy,
                                    status);
  slot->status = status;
  slot->facing = request.facing + slot->local_facing;
  if (status == SlotStatus::Adjusted) {
    plan.displacement += (slot->world_position - ideal).length();
    ++plan.adjusted_count;
  } else if (status == SlotStatus::Blocked) {
    ++plan.blocked_count;
  }
}

} // namespace

auto ArmyFormationPlanner::place(const ArmyFormationLayout& layout,
                                 const ArmyFormationRequest& request)
    -> ArmyFormationPlan {
  ArmyFormationPlan plan;
  begin_plan_from_layout(plan, layout, request);
  if (!layout.valid) {
    plan.rejection_reason = layout.rejection_reason;
    return plan;
  }
  copy_layout_slots(plan, layout);

  QVector3D const anchor = walkable_anchor(request);
  plan.anchor = anchor;
  planning::SlotTerrainFitter fitter(layout.slot_spacing,
                                     layout.footprint_gap,
                                     request.facing,
                                     anchor,
                                     request.resolve_terrain,
                                     plan.slot_list.size());

  if (layout.assign_by_distance && layout.slot_start.size() == plan.slot_list.size() &&
      layout.slot_kind.size() == plan.slot_list.size()) {
    planning::assign_nearest_troops(
        plan.slot_list,
        plan.slot_files,
        {layout.slot_start, layout.slot_kind, anchor, request.facing});
  }

  for (auto const index : front_to_back_slot_order(plan.slot_list)) {
    fit_slot_to_ground(plan, index, request, anchor, fitter);
  }

  if (plan.blocked_count == static_cast<int>(plan.slot_list.size())) {
    plan.rejection_reason =
        QCoreApplication::translate(
            "Formation", "No part of this formation fits on the chosen ground.")
            .toStdString();
    return plan;
  }
  plan.valid = true;
  return plan;
}

auto ArmyFormationPlanner::scatter_offsets(int count,
                                           float spacing) -> std::vector<QVector3D> {
  std::vector<QVector3D> offsets;
  if (count <= 0) {
    return offsets;
  }
  offsets.reserve(static_cast<std::size_t>(count));
  int const side = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
  for (int i = 0; i < count; ++i) {
    float const column = static_cast<float>(i % side);
    float const row = static_cast<float>(i / side);
    float const half = static_cast<float>(side - 1) * 0.5F;
    offsets.emplace_back((column - half) * spacing, 0.0F, (row - half) * spacing);
  }
  return offsets;
}

auto ArmyFormationPlanner::scatter_layout(
    const std::vector<ArmyFormationMember>& members,
    float spacing) -> ArmyFormationLayout {
  ArmyFormationLayout layout;
  layout.spacing = spacing;
  layout.slot_spacing = spacing;
  layout.doctrine = k_neutral_doctrine;
  auto const offsets = scatter_offsets(static_cast<int>(members.size()), spacing);
  layout.slot_list.reserve(members.size());
  Bounds bounds;
  for (std::size_t i = 0; i < members.size() && i < offsets.size(); ++i) {
    FormationSlot slot;
    slot.id = static_cast<int>(i);
    slot.occupant = members[i].entity_id;
    slot.local_offset = offsets[i];
    layout.slot_list.push_back(slot);
    bounds.expand(offsets[i]);
  }
  layout.frontage = bounds.width();
  layout.depth = bounds.depth();
  layout.valid = !layout.slot_list.empty();
  if (!layout.valid) {
    layout.rejection_reason = "No members to scatter.";
  }
  return layout;
}

} // namespace Game::Formation
