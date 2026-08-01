#include "army_formation_planner.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <unordered_map>

#include "../core/component.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../systems/command_service.h"
#include "../systems/nation_registry.h"
#include "../units/spawn_type.h"
#include "../units/troop_config.h"
#include "army_formation_registry.h"
#include "troop_role_registry.h"
#include "unit_layout_resolver.h"

namespace Game::Formation {

namespace {

constexpr float k_pi = std::numbers::pi_v<float>;
constexpr float k_deg_to_rad = k_pi / 180.0F;
constexpr float k_rad_to_deg = 180.0F / k_pi;

struct Bounds {
  bool valid{false};
  float min_x{0.0F};
  float max_x{0.0F};
  float min_z{0.0F};
  float max_z{0.0F};

  void expand(const QVector3D& point) {
    if (!valid) {
      valid = true;
      min_x = max_x = point.x();
      min_z = max_z = point.z();
      return;
    }
    min_x = std::min(min_x, point.x());
    max_x = std::max(max_x, point.x());
    min_z = std::min(min_z, point.z());
    max_z = std::max(max_z, point.z());
  }

  [[nodiscard]] auto width() const -> float { return valid ? max_x - min_x : 0.0F; }
  [[nodiscard]] auto depth() const -> float { return valid ? max_z - min_z : 0.0F; }
  [[nodiscard]] auto half_width() const -> float {
    return valid ? std::max(std::abs(min_x), std::abs(max_x)) : 0.0F;
  }
};

auto signed_noise(std::uint64_t seed) -> float {
  auto value = static_cast<std::uint32_t>(seed ^ (seed >> 32U));
  value ^= value >> 16U;
  value *= 0x7feb352dU;
  value ^= value >> 15U;
  value *= 0x846ca68bU;
  value ^= value >> 16U;
  return (static_cast<float>(value & 0xFFFFU) / 32767.5F) - 1.0F;
}

auto balanced_rows(int total, int max_per_row, int min_per_row) -> std::vector<int> {
  std::vector<int> rows;
  if (total <= 0) {
    return rows;
  }
  int const capped = std::max(1, max_per_row);
  if (total <= capped) {
    rows.push_back(total);
    return rows;
  }
  int const row_count = (total + capped - 1) / capped;
  int const base = total / row_count;
  int const extra = total % row_count;
  int assigned = 0;
  for (int r = 0; r < row_count; ++r) {
    int size = base + (r < extra ? 1 : 0);
    size = std::clamp(size, std::max(1, std::min(min_per_row, capped)), capped);
    size = std::min(size, total - assigned);
    if (size <= 0) {
      break;
    }
    rows.push_back(size);
    assigned += size;
  }
  while (assigned < total && !rows.empty()) {
    rows.back() += total - assigned;
    assigned = total;
  }
  return rows;
}

auto member_spacing(const ArmyFormationMember& member, float base_spacing) -> float {
  return std::max(base_spacing, member.footprint * 2.0F + 0.5F) * 1.0F;
}

auto max_member_spacing(const std::vector<const ArmyFormationMember*>& members,
                        float base_spacing) -> float {
  float spacing = base_spacing;
  for (const auto* member : members) {
    spacing = std::max(spacing, member_spacing(*member, base_spacing));
  }
  return spacing;
}

struct AssignedLine {
  const DoctrineLineRule* rule{nullptr};
  std::vector<const ArmyFormationMember*> members;
};

auto assign_lines(const DoctrineIntentTemplate& tmpl,
                  const std::vector<ArmyFormationMember>& members,
                  bool preserve_order) -> std::vector<AssignedLine> {
  std::vector<AssignedLine> lines;
  lines.reserve(tmpl.lines.size());
  for (const auto& rule : tmpl.lines) {
    lines.push_back({&rule, {}});
  }

  for (const auto& member : members) {
    bool matched = false;
    for (auto& line : lines) {
      if (line.rule != nullptr && line.rule->matches(member.roles)) {
        line.members.push_back(&member);
        matched = true;
        break;
      }
    }
    if (!matched && !lines.empty()) {
      lines.back().members.push_back(&member);
    }
  }

  for (auto& line : lines) {
    if (preserve_order) {
      continue;
    }
    std::stable_sort(line.members.begin(),
                     line.members.end(),
                     [](const ArmyFormationMember* a, const ArmyFormationMember* b) {
                       if (a->troop_type != b->troop_type) {
                         return static_cast<int>(a->troop_type) <
                                static_cast<int>(b->troop_type);
                       }
                       if (a->current_position.x() != b->current_position.x()) {
                         return a->current_position.x() < b->current_position.x();
                       }
                       return a->entity_id < b->entity_id;
                     });
  }
  return lines;
}

void emit_centre_block(const DoctrineLineRule& rule,
                       const std::vector<const ArmyFormationMember*>& members,
                       float base_spacing,
                       float& cursor_z,
                       int& next_slot_id,
                       std::vector<FormationSlot>& out,
                       Bounds& all_bounds,
                       Bounds& body_bounds) {
  if (members.empty()) {
    return;
  }

  float const spacing = max_member_spacing(members, base_spacing);
  float const lateral_step = spacing * rule.lateral_spacing_scale;
  float const depth_step = spacing * rule.depth_spacing_scale;
  int const max_per_row =
      std::max(1, std::min(rule.max_per_row, static_cast<int>(members.size())));
  int const min_per_row = std::max(1, std::min(rule.min_per_row, max_per_row));
  auto const rows =
      balanced_rows(static_cast<int>(members.size()), max_per_row, min_per_row);
  float const front_z = cursor_z + rule.front_offset_scale * depth_step;

  std::size_t index = 0;
  float rear_z = front_z;
  for (std::size_t row = 0; row < rows.size() && index < members.size(); ++row) {
    int const in_row = rows[row];
    float const row_z = front_z - static_cast<float>(row) * depth_step;
    rear_z = std::min(rear_z, row_z);
    float const echelon =
        static_cast<float>(row) * lateral_step * rule.row_echelon_scale;
    float const stagger =
        (row % 2U == 1U) ? lateral_step * rule.row_stagger_scale : 0.0F;

    for (int col = 0; col < in_row && index < members.size(); ++col) {
      const auto* member = members[index];
      float const centred_col =
          static_cast<float>(col) - (static_cast<float>(in_row) - 1.0F) * 0.5F;
      float const lateral_noise =
          signed_noise(member->entity_id * 2654435761ULL + row * 97ULL +
                       static_cast<std::uint64_t>(col) * 17ULL) *
          lateral_step * rule.lateral_jitter_scale;
      float const depth_noise =
          signed_noise(member->entity_id * 2246822519ULL + row * 53ULL +
                       static_cast<std::uint64_t>(col) * 29ULL) *
          depth_step * rule.depth_jitter_scale;

      FormationSlot slot;
      slot.id = next_slot_id++;
      slot.role = rule.role;
      slot.local_offset =
          QVector3D(centred_col * lateral_step + echelon + stagger + lateral_noise,
                    0.0F,
                    row_z + depth_noise);
      slot.facing = 0.0F;
      slot.rank = static_cast<int>(row);
      slot.file = col;
      slot.occupant = member->entity_id;
      out.push_back(slot);
      all_bounds.expand(slot.local_offset);
      body_bounds.expand(slot.local_offset);
      ++index;
    }
  }

  if (rule.consumes_depth) {
    cursor_z = rear_z - depth_step * rule.line_gap_scale;
  }
}

void emit_split_flanks(const DoctrineLineRule& rule,
                       const std::vector<const ArmyFormationMember*>& members,
                       float base_spacing,
                       float front_anchor_z,
                       float body_half_width,
                       FlankPreference preference,
                       int& next_slot_id,
                       std::vector<FormationSlot>& out,
                       Bounds& all_bounds) {
  if (members.empty()) {
    return;
  }

  std::vector<const ArmyFormationMember*> sorted = members;
  std::stable_sort(sorted.begin(),
                   sorted.end(),
                   [](const ArmyFormationMember* a, const ArmyFormationMember* b) {
                     if (a->current_position.x() != b->current_position.x()) {
                       return a->current_position.x() < b->current_position.x();
                     }
                     return a->entity_id < b->entity_id;
                   });

  float right_weight = rule.right_side_weight;
  switch (preference) {
  case FlankPreference::StrongLeft:
    right_weight = 0.35F;
    break;
  case FlankPreference::StrongRight:
    right_weight = 0.65F;
    break;
  case FlankPreference::Split:
    right_weight = 0.5F;
    break;
  case FlankPreference::Balanced:
    break;
  }

  auto const total = static_cast<int>(sorted.size());
  int right_count =
      static_cast<int>(std::lround(static_cast<float>(total) * right_weight));
  right_count = std::clamp(right_count, 0, total);
  int left_count = total - right_count;
  if (total > 1) {
    left_count = std::max(1, left_count);
    right_count = std::max(1, total - left_count);
    left_count = total - right_count;
  }

  std::vector<const ArmyFormationMember*> const left(sorted.begin(),
                                                     sorted.begin() + left_count);
  std::vector<const ArmyFormationMember*> const right(sorted.begin() + left_count,
                                                      sorted.end());

  auto emit_side = [&](const std::vector<const ArmyFormationMember*>& side,
                       float side_sign,
                       ArmyRole role) {
    if (side.empty()) {
      return;
    }
    float const spacing = max_member_spacing(side, base_spacing);
    float const lateral_step = spacing * rule.lateral_spacing_scale;
    float const depth_step = spacing * rule.depth_spacing_scale;
    int const max_per_row =
        std::max(1, std::min(rule.max_per_row, static_cast<int>(side.size())));
    int const min_per_row = std::max(1, std::min(rule.min_per_row, max_per_row));
    auto const rows =
        balanced_rows(static_cast<int>(side.size()), max_per_row, min_per_row);
    float const flank_centre =
        side_sign * (body_half_width + spacing * rule.flank_gap_scale);

    std::size_t index = 0;
    for (std::size_t row = 0; row < rows.size() && index < side.size(); ++row) {
      int const in_row = rows[row];
      float const row_z = front_anchor_z + rule.front_offset_scale * depth_step -
                          static_cast<float>(row) * depth_step;
      float const echelon =
          side_sign * static_cast<float>(row) * lateral_step * rule.row_echelon_scale;

      for (int col = 0; col < in_row && index < side.size(); ++col) {
        const auto* member = side[index];
        float const centred_col =
            static_cast<float>(col) - (static_cast<float>(in_row) - 1.0F) * 0.5F;
        float const lateral_noise =
            signed_noise(member->entity_id * 1597334677ULL + row * 41ULL +
                         static_cast<std::uint64_t>(col) * 11ULL) *
            lateral_step * rule.lateral_jitter_scale;
        float const depth_noise =
            signed_noise(member->entity_id * 3812015801ULL + row * 73ULL +
                         static_cast<std::uint64_t>(col) * 31ULL) *
            depth_step * rule.depth_jitter_scale;

        FormationSlot slot;
        slot.id = next_slot_id++;
        slot.role = role;
        slot.local_offset = QVector3D(
            flank_centre + centred_col * lateral_step + echelon + lateral_noise,
            0.0F,
            row_z + centred_col * depth_step * rule.flank_forward_step_scale +
                depth_noise);
        slot.facing = 0.0F;
        slot.rank = static_cast<int>(row);
        slot.file = col;
        slot.occupant = member->entity_id;
        out.push_back(slot);
        all_bounds.expand(slot.local_offset);
        ++index;
      }
    }
  };

  emit_side(left, -1.0F, ArmyRole::LeftFlank);
  emit_side(right, 1.0F, ArmyRole::RightFlank);
}

void recentre(std::vector<FormationSlot>& slot_list) {
  if (slot_list.empty()) {
    return;
  }
  Bounds bounds;
  for (const auto& slot : slot_list) {
    bounds.expand(slot.local_offset);
  }
  if (!bounds.valid) {
    return;
  }
  float const cx = (bounds.min_x + bounds.max_x) * 0.5F;
  float const cz = (bounds.min_z + bounds.max_z) * 0.5F;
  for (auto& slot : slot_list) {
    slot.local_offset.setX(slot.local_offset.x() - cx);
    slot.local_offset.setY(0.0F);
    slot.local_offset.setZ(slot.local_offset.z() - cz);
  }
}

void apply_reserve_rows(std::vector<FormationSlot>& slot_list,
                        int reserve_rows,
                        float spacing) {
  if (reserve_rows <= 0 || slot_list.empty()) {
    return;
  }

  std::vector<FormationSlot*> ordered;
  ordered.reserve(slot_list.size());
  for (auto& slot : slot_list) {
    ordered.push_back(&slot);
  }
  std::sort(ordered.begin(), ordered.end(), [](const auto* a, const auto* b) {
    return a->local_offset.z() < b->local_offset.z();
  });

  int bands = 0;
  float previous = ordered.front()->local_offset.z();
  std::size_t count = 0;
  for (auto* slot : ordered) {
    if (std::abs(slot->local_offset.z() - previous) > spacing * 0.5F) {
      ++bands;
      previous = slot->local_offset.z();
      if (bands >= reserve_rows) {
        break;
      }
    }
    ++count;
  }

  for (std::size_t i = 0; i < count; ++i) {
    auto* slot = ordered[i];
    slot->local_offset.setZ(slot->local_offset.z() - spacing * 1.15F);
    if (slot->role == ArmyRole::Centre || slot->role == ArmyRole::Vanguard ||
        slot->role == ArmyRole::Screen) {
      slot->role = ArmyRole::Reserve;
    }
  }
}

void scale_to_frontage(std::vector<FormationSlot>& slot_list,
                       float requested_frontage,
                       float frontage_scale,
                       float depth_scale) {
  if (slot_list.empty()) {
    return;
  }
  Bounds bounds;
  for (const auto& slot : slot_list) {
    bounds.expand(slot.local_offset);
  }
  float lateral_scale = frontage_scale;
  if (requested_frontage > 0.01F && bounds.width() > 0.01F) {
    lateral_scale = requested_frontage / bounds.width();
  }
  lateral_scale = std::clamp(lateral_scale, 0.25F, 6.0F);
  float const depth_multiplier = std::clamp(depth_scale, 0.25F, 6.0F);
  for (auto& slot : slot_list) {
    slot.local_offset.setX(slot.local_offset.x() * lateral_scale);
    slot.local_offset.setZ(slot.local_offset.z() * depth_multiplier);
  }
}

auto rotate_offset(const QVector3D& local, float yaw_degrees) -> QVector3D {
  float const yaw = yaw_degrees * k_deg_to_rad;
  float const sin_yaw = std::sin(yaw);
  float const cos_yaw = std::cos(yaw);
  return {local.x() * cos_yaw + local.z() * sin_yaw,
          local.y(),
          -local.x() * sin_yaw + local.z() * cos_yaw};
}

class SlotTerrainFitter {
public:
  SlotTerrainFitter(float spacing, bool enabled)
      : m_min_separation_sq(spacing * spacing * 0.36F)
      , m_enabled(enabled) {}

  auto fit(const QVector3D& ideal, SlotStatus& status) -> QVector3D {
    if (!m_enabled) {
      claim(ideal);
      status = SlotStatus::Valid;
      return ideal;
    }

    if (is_free(ideal) &&
        Game::Systems::CommandService::is_world_position_walkable(ideal)) {
      claim(ideal);
      status = SlotStatus::Valid;
      return ideal;
    }

    constexpr int k_rings = 6;
    constexpr int k_samples = 12;
    float const step = std::sqrt(m_min_separation_sq) * 1.05F;
    for (int ring = 1; ring <= k_rings; ++ring) {
      float const radius = step * static_cast<float>(ring);
      for (int sample = 0; sample < k_samples; ++sample) {
        float const angle =
            (static_cast<float>(sample) / static_cast<float>(k_samples)) * 2.0F * k_pi +
            static_cast<float>(ring) * 0.37F;
        QVector3D const candidate(ideal.x() + std::cos(angle) * radius,
                                  ideal.y(),
                                  ideal.z() + std::sin(angle) * radius);
        if (!is_free(candidate)) {
          continue;
        }
        if (!Game::Systems::CommandService::is_world_position_walkable(candidate)) {
          continue;
        }
        claim(candidate);
        status = SlotStatus::Adjusted;
        return candidate;
      }
    }

    status = SlotStatus::Blocked;
    return ideal;
  }

private:
  [[nodiscard]] auto is_free(const QVector3D& point) const -> bool {
    for (const auto& claimed : m_claimed) {
      float const dx = claimed.x() - point.x();
      float const dz = claimed.z() - point.z();
      if ((dx * dx + dz * dz) < m_min_separation_sq) {
        return false;
      }
    }
    return true;
  }

  void claim(const QVector3D& point) { m_claimed.push_back(point); }

  std::vector<QVector3D> m_claimed;
  float m_min_separation_sq{1.0F};
  bool m_enabled{true};
};

auto doctrine_for_entity(Engine::Core::World& world,
                         EntityID id) -> FormationDoctrineId {
  auto* entity = world.get_entity(id);
  if (entity == nullptr) {
    return k_neutral_doctrine;
  }
  const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return k_neutral_doctrine;
  }
  const auto* nation =
      Game::Systems::NationRegistry::instance().get_nation(unit->nation_id);
  if (nation != nullptr && !nation->doctrine.empty()) {
    return nation->doctrine;
  }
  return default_doctrine_for_nation(unit->nation_id);
}

} // namespace

auto ArmyFormationPlan::slot_for(EntityID entity) const -> const FormationSlot* {
  auto it =
      std::find_if(slot_list.begin(), slot_list.end(), [entity](const auto& slot) {
        return slot.occupant == entity;
      });
  return it == slot_list.end() ? nullptr : &(*it);
}

auto ArmyFormationPlan::placed_count() const -> int {
  return static_cast<int>(
      std::count_if(slot_list.begin(), slot_list.end(), [](const auto& s) {
        return s.status != SlotStatus::Blocked;
      }));
}

auto ArmyFormationPlanner::collect_members(Engine::Core::World& world,
                                           const std::vector<EntityID>& entities)
    -> std::vector<ArmyFormationMember> {
  std::vector<ArmyFormationMember> members;
  members.reserve(entities.size());

  for (auto const id : entities) {
    auto* entity = world.get_entity(id);
    if (entity == nullptr) {
      continue;
    }
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (unit == nullptr || transform == nullptr) {
      continue;
    }
    auto troop = Game::Units::spawn_typeToTroopType(unit->spawn_type);
    if (!troop.has_value()) {
      continue;
    }

    ArmyFormationMember member;
    member.entity_id = id;
    member.troop_type = *troop;
    member.roles = TroopRoleRegistry::instance().roles(*troop);
    member.current_position =
        QVector3D(transform->position.x, transform->position.y, transform->position.z);
    member.footprint =
        Game::Units::TroopConfig::instance().get_selection_ring_size(*troop);
    member.doctrine = doctrine_for_entity(world, id);
    members.push_back(member);
  }

  return members;
}

auto ArmyFormationPlanner::combined_roles(
    const std::vector<ArmyFormationMember>& members) -> RoleTagSet {
  RoleTagSet set = 0U;
  for (const auto& member : members) {
    set |= member.roles;
  }
  return set;
}

auto ArmyFormationPlanner::resolve_doctrine(
    const std::vector<ArmyFormationMember>& members,
    const ArmyFormationRequest& request) -> FormationDoctrineId {
  if (!request.doctrine.empty() && request.options.doctrine_locked) {
    return request.doctrine;
  }
  if (members.empty()) {
    return request.doctrine.empty() ? k_neutral_doctrine : request.doctrine;
  }

  switch (request.options.mixed_policy) {
  case MixedDoctrinePolicy::CompositeByRole:
    return k_neutral_doctrine;

  case MixedDoctrinePolicy::CommanderDoctrine: {
    for (const auto& member : members) {
      if (has_role(member.roles, RoleTag::Command)) {
        return member.doctrine;
      }
    }
    break;
  }

  case MixedDoctrinePolicy::SeparateContingents:
  case MixedDoctrinePolicy::MajorityDoctrine:
    break;
  }

  std::unordered_map<FormationDoctrineId, int> counts;
  for (const auto& member : members) {
    ++counts[member.doctrine];
  }
  FormationDoctrineId best = members.front().doctrine;
  int best_count = 0;
  for (const auto& entry : counts) {
    if (entry.second > best_count ||
        (entry.second == best_count && entry.first < best)) {
      best = entry.first;
      best_count = entry.second;
    }
  }
  if (!request.doctrine.empty() && counts.count(request.doctrine) != 0U) {
    return request.doctrine;
  }
  return best;
}

auto ArmyFormationPlanner::plan_local_slots(
    const std::vector<ArmyFormationMember>& members,
    const DoctrineIntentTemplate& tmpl,
    const ArmyFormationOptions& options,
    float spacing,
    float requested_frontage) -> std::vector<FormationSlot> {
  std::vector<FormationSlot> slot_list;
  if (members.empty()) {
    return slot_list;
  }
  slot_list.reserve(members.size());

  auto lines = assign_lines(tmpl, members, options.preserve_member_order);

  Bounds all_bounds;
  Bounds body_bounds;
  float cursor_z = 0.0F;
  int next_slot_id = 0;

  for (const auto& line : lines) {
    if (line.rule == nullptr || line.rule->placement == LinePlacement::SplitFlanks) {
      continue;
    }
    emit_centre_block(*line.rule,
                      line.members,
                      spacing,
                      cursor_z,
                      next_slot_id,
                      slot_list,
                      all_bounds,
                      body_bounds);
  }

  float body_half_width = body_bounds.half_width();

  FlankPreference preference = options.flank_preference;
  if (preference == FlankPreference::Balanced) {
    preference = tmpl.default_flank;
  }

  for (const auto& line : lines) {
    if (line.rule == nullptr || line.rule->placement != LinePlacement::SplitFlanks) {
      continue;
    }
    if (!body_bounds.valid) {
      DoctrineLineRule collapsed = *line.rule;
      collapsed.placement = LinePlacement::CentreBlock;
      collapsed.consumes_depth = true;
      collapsed.front_offset_scale = 0.0F;
      emit_centre_block(collapsed,
                        line.members,
                        spacing,
                        cursor_z,
                        next_slot_id,
                        slot_list,
                        all_bounds,
                        body_bounds);
      body_half_width = body_bounds.half_width();
      continue;
    }
    emit_split_flanks(*line.rule,
                      line.members,
                      spacing,
                      body_bounds.max_z,
                      body_half_width,
                      preference,
                      next_slot_id,
                      slot_list,
                      all_bounds);
  }

  int const reserve_rows =
      options.reserve_rows >= 0 ? options.reserve_rows : tmpl.reserve_rows;
  apply_reserve_rows(slot_list, reserve_rows, spacing);

  scale_to_frontage(slot_list,
                    requested_frontage,
                    tmpl.frontage_scale * options.frontage_scale,
                    tmpl.depth_scale * options.depth_scale);
  recentre(slot_list);

  return slot_list;
}

auto ArmyFormationPlanner::make_member(EntityID entity_id,
                                       Game::Units::TroopType troop_type,
                                       const QVector3D& position,
                                       const FormationDoctrineId& doctrine)
    -> ArmyFormationMember {
  ArmyFormationMember member;
  member.entity_id = entity_id;
  member.troop_type = troop_type;
  member.roles = TroopRoleRegistry::instance().roles(troop_type);
  member.current_position = position;
  member.footprint =
      Game::Units::TroopConfig::instance().get_selection_ring_size(troop_type);
  member.doctrine = doctrine.empty() ? k_neutral_doctrine : doctrine;
  return member;
}

auto ArmyFormationPlanner::plan(Engine::Core::World& world,
                                const ArmyFormationRequest& request)
    -> ArmyFormationPlan {
  return plan(collect_members(world, request.members), request);
}

auto ArmyFormationPlanner::plan(const std::vector<ArmyFormationMember>& members,
                                const ArmyFormationRequest& request)
    -> ArmyFormationPlan {
  ArmyFormationPlan plan;
  plan.anchor = request.anchor;
  plan.facing = request.facing;
  plan.intent = request.intent;

  if (members.empty()) {
    plan.rejection_reason = "No units eligible for formation placement.";
    return plan;
  }

  plan.doctrine = resolve_doctrine(members, request);
  const auto& doctrine = DoctrineRegistry::instance().get_or_neutral(plan.doctrine);

  auto const roles = combined_roles(members);
  auto const reason = DoctrineRegistry::instance().availability_reason(
      plan.doctrine, request.intent, roles, static_cast<int>(members.size()));
  if (!reason.empty()) {
    plan.rejection_reason = reason;
    return plan;
  }

  const auto* tmpl = doctrine.resolve_template(request.intent);
  if (tmpl == nullptr) {
    plan.rejection_reason = "This doctrine has no template for the chosen formation.";
    return plan;
  }

  float const spacing =
      std::max(0.2F,
               request.spacing * tmpl->spacing_scale *
                   std::clamp(request.options.spacing_scale, 0.4F, 2.5F));
  plan.spacing = spacing;

  plan.slot_list =
      plan_local_slots(members, *tmpl, request.options, spacing, request.frontage);
  if (plan.slot_list.empty()) {
    plan.rejection_reason = "The formation template produced no slot_list.";
    return plan;
  }

  if (request.preserve_previous_slots && request.group_id != k_invalid_group) {
    const auto* previous = ArmyFormationRegistry::instance().find(request.group_id);
    if (previous != nullptr) {
      std::vector<FormationSlot> reordered = plan.slot_list;
      std::vector<bool> claimed(reordered.size(), false);
      std::unordered_map<EntityID, std::size_t> desired;
      for (const auto& old_slot : previous->slot_list) {
        if (old_slot.occupant == 0U) {
          continue;
        }
        auto const index = static_cast<std::size_t>(old_slot.id);
        if (index < reordered.size()) {
          desired.emplace(old_slot.occupant, index);
        }
      }
      std::vector<EntityID> unassigned;
      std::vector<EntityID> occupants(reordered.size(), 0U);
      for (const auto& slot : plan.slot_list) {
        auto it = desired.find(slot.occupant);
        if (it != desired.end() && !claimed[it->second]) {
          claimed[it->second] = true;
          occupants[it->second] = slot.occupant;
        } else {
          unassigned.push_back(slot.occupant);
        }
      }
      std::size_t next = 0;
      for (std::size_t i = 0; i < occupants.size(); ++i) {
        if (occupants[i] != 0U) {
          continue;
        }
        if (next < unassigned.size()) {
          occupants[i] = unassigned[next++];
        }
      }
      for (std::size_t i = 0; i < reordered.size(); ++i) {
        reordered[i].occupant = occupants[i];
      }
      plan.slot_list = reordered;
    }
  }

  Bounds bounds;
  for (const auto& slot : plan.slot_list) {
    bounds.expand(slot.local_offset);
  }
  plan.frontage = bounds.width();
  plan.depth = bounds.depth();

  SlotTerrainFitter fitter(spacing, request.resolve_terrain);
  QVector3D const anchor =
      request.resolve_terrain
          ? Game::Systems::CommandService::snap_to_walkable_ground(request.anchor, 15)
          : request.anchor;
  plan.anchor = anchor;

  std::vector<FormationSlot*> ordered;
  ordered.reserve(plan.slot_list.size());
  for (auto& slot : plan.slot_list) {
    ordered.push_back(&slot);
  }
  std::stable_sort(ordered.begin(), ordered.end(), [](const auto* a, const auto* b) {
    if (a->local_offset.z() != b->local_offset.z()) {
      return a->local_offset.z() > b->local_offset.z();
    }
    return std::abs(a->local_offset.x()) < std::abs(b->local_offset.x());
  });

  for (auto* slot : ordered) {
    QVector3D const rotated = rotate_offset(slot->local_offset, request.facing);
    QVector3D const ideal(
        anchor.x() + rotated.x(), anchor.y(), anchor.z() + rotated.z());
    SlotStatus status = SlotStatus::Valid;
    slot->world_position = fitter.fit(ideal, status);
    slot->status = status;
    slot->facing = request.facing;
    if (status == SlotStatus::Blocked) {
      ++plan.blocked_count;
    } else if (status == SlotStatus::Adjusted) {
      ++plan.adjusted_count;
    }
  }

  if (plan.blocked_count == static_cast<int>(plan.slot_list.size())) {
    plan.rejection_reason = "No part of this formation fits on the chosen ground.";
    return plan;
  }

  plan.valid = true;
  return plan;
}

} // namespace Game::Formation
