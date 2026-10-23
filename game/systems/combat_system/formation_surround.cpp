#include "formation_surround.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>

#include "formation_local_frame.h"

namespace Game::Systems::Combat {
namespace {

constexpr float k_pi = std::numbers::pi_v<float>;
constexpr float k_lone_half_width = k_pi * 0.75F;
constexpr int k_spread_passes = 12;
constexpr std::size_t k_max_rings = 6U;
constexpr float k_shoulder_spacing = 0.92F;
constexpr float k_ring_gap_spacing = 1.0F;

auto wrap_angle(float angle) -> float {
  while (angle > k_pi) {
    angle -= 2.0F * k_pi;
  }
  while (angle <= -k_pi) {
    angle += 2.0F * k_pi;
  }
  return angle;
}

void spread_sectors(std::vector<SurroundSector*>& ring) {
  if (ring.size() < 2U) {
    return;
  }
  for (int pass = 0; pass < k_spread_passes; ++pass) {
    for (std::size_t i = 0; i < ring.size(); ++i) {
      auto& left = *ring[i];
      auto& right = *ring[(i + 1U) % ring.size()];
      float gap = right.center - left.center;
      while (gap < 0.0F) {
        gap += 2.0F * k_pi;
      }
      float const need = left.half_width + right.half_width;
      if (gap < need) {
        float const push = (need - gap) * 0.5F;
        left.center = wrap_angle(left.center - push);
        right.center = wrap_angle(right.center + push);
      }
    }
  }
}

struct RankedSlot {
  const FormationCombat::SoldierSlot* slot{nullptr};
  float forward{0.0F};
  float angle{0.0F};
};

} // namespace

void SurroundBoard::enlist(Engine::Core::EntityID target,
                           Engine::Core::EntityID squad,
                           float approach,
                           int soldiers) {
  m_claims.push_back({.target = target,
                      .squad = squad,
                      .approach = approach,
                      .weight = static_cast<float>(std::max(1, soldiers))});
}

void SurroundBoard::allot() {
  std::sort(m_claims.begin(), m_claims.end(), [](auto const& a, auto const& b) {
    if (a.target != b.target) {
      return a.target < b.target;
    }
    if (a.approach != b.approach) {
      return a.approach < b.approach;
    }
    return a.squad < b.squad;
  });
  std::vector<SurroundSector*> ring;
  for (std::size_t begin = 0; begin < m_claims.size();) {
    std::size_t end = begin;
    float total = 0.0F;
    while (end < m_claims.size() && m_claims[end].target == m_claims[begin].target) {
      total += m_claims[end].weight;
      ++end;
    }
    ring.clear();
    for (std::size_t i = begin; i < end; ++i) {
      auto& claim = m_claims[i];
      claim.sector.center = claim.approach;
      claim.sector.half_width =
          std::min(k_lone_half_width, k_pi * claim.weight / std::max(total, 1.0F));
      ring.push_back(&claim.sector);
    }
    spread_sectors(ring);
    begin = end;
  }
}

auto SurroundBoard::sector_of(Engine::Core::EntityID target,
                              Engine::Core::EntityID squad) const
    -> std::optional<SurroundSector> {
  for (auto const& claim : m_claims) {
    if (claim.target == target && claim.squad == squad) {
      return claim.sector;
    }
  }
  return std::nullopt;
}

auto surround_approach(float target_x,
                       float target_z,
                       float squad_x,
                       float squad_z) -> float {
  float const dx = squad_x - target_x;
  float const dz = squad_z - target_z;
  if (dx * dx + dz * dz < 1.0e-8F) {
    return 0.0F;
  }
  return std::atan2(dz, dx);
}

void plan_surround(const SurroundInputs& in, std::vector<SurroundSpot>& spots) {
  auto const& layout = in.layout;
  spots.assign(layout.all_slots.size(), SurroundSpot{});
  if (layout.live_slots.size() < 2U) {
    return;
  }
  auto const [toward_x, toward_z] = world_to_local(in.actor, in.target_x, in.target_z);
  float const toward_length = std::hypot(toward_x, toward_z);
  float const unit_x = toward_length > 1.0e-4F ? toward_x / toward_length : 0.0F;
  float const unit_z = toward_length > 1.0e-4F ? toward_z / toward_length : 1.0F;

  thread_local std::vector<RankedSlot> ranked;
  ranked.clear();
  for (auto const& slot : layout.live_slots) {
    float const local_x = in.frame_sign * slot.local_x;
    float const local_z = in.frame_sign * slot.local_z;
    float world_x = 0.0F;
    float world_z = 0.0F;
    auto const* previous =
        slot.index < in.previous.size() ? &in.previous[slot.index] : nullptr;
    if (previous != nullptr && previous->alive && previous->world_motion_valid) {
      world_x = previous->world_x;
      world_z = previous->world_z;
    } else {
      QVector3D const world = local_to_world(in.actor, local_x, local_z);
      world_x = world.x();
      world_z = world.z();
    }
    ranked.push_back(
        {.slot = &slot,
         .forward = local_x * unit_x + local_z * unit_z,
         .angle = wrap_angle(std::atan2(world_z - in.target_z, world_x - in.target_x) -
                             in.sector.center)});
  }

  float const spacing_step =
      std::max(layout.spacing * k_shoulder_spacing, layout.body_radius * 2.1F);
  float const ring_gap =
      std::max(layout.spacing * k_ring_gap_spacing, layout.body_radius * 2.0F);
  std::stable_sort(ranked.begin(), ranked.end(), [&](auto const& a, auto const& b) {
    float const row_a = std::round(a.forward / std::max(0.05F, layout.spacing * 0.5F));
    float const row_b = std::round(b.forward / std::max(0.05F, layout.spacing * 0.5F));
    if (row_a != row_b) {
      return row_a > row_b;
    }
    return a.slot->index < b.slot->index;
  });

  bool const full_circle = in.sector.half_width >= k_pi - 1.0e-3F;
  std::size_t placed = 0U;
  for (std::size_t ring = 0U; ring < k_max_rings && placed < ranked.size(); ++ring) {
    float const radius = in.first_ring_radius + static_cast<float>(ring) * ring_gap;
    float const arc = 2.0F * in.sector.half_width * radius;
    auto const capacity = static_cast<std::size_t>(
        std::max(1.0F, std::floor(arc / spacing_step) + (full_circle ? 0.0F : 1.0F)));
    std::size_t const members = std::min(capacity, ranked.size() - placed);
    auto const first = ranked.begin() + static_cast<std::ptrdiff_t>(placed);
    auto const last = first + static_cast<std::ptrdiff_t>(members);
    std::stable_sort(
        first, last, [](auto const& a, auto const& b) { return a.angle < b.angle; });
    float const step =
        std::min(spacing_step / std::max(radius, 0.05F),
                 2.0F * in.sector.half_width / static_cast<float>(members));
    for (std::size_t j = 0U; j < members; ++j) {
      auto const& member = *(first + static_cast<std::ptrdiff_t>(j));
      float const angle =
          in.sector.center +
          (static_cast<float>(j) - (static_cast<float>(members) - 1.0F) * 0.5F) * step;
      float const world_x = in.target_x + std::cos(angle) * radius;
      float const world_z = in.target_z + std::sin(angle) * radius;
      auto const [local_x, local_z] = world_to_local(in.actor, world_x, world_z);
      auto& spot = spots[member.slot->index];
      spot.active = true;
      spot.ring = static_cast<std::uint8_t>(ring);
      spot.local_x = local_x;
      spot.local_z = local_z;
    }
    placed += members;
  }
}

} // namespace Game::Systems::Combat
