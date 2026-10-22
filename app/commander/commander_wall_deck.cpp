#include "app/commander/commander_wall_deck.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "game/core/component_core.h"
#include "game/core/component_gameplay.h"
#include "game/core/wall_walk_geometry.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/owner_registry.h"
#include "game/units/spawn_type.h"

namespace App::Core::WallDeck {

namespace {

namespace WW = Game::Systems::WallWalk;

constexpr float k_deck_half_span = 1.05F;
constexpr float k_deck_walk_inner = WW::k_deck_inner_edge + 0.30F;
constexpr float k_deck_walk_outer = WW::k_deck_outer_edge - 0.08F;
constexpr float k_access_start = WW::k_deck_outer_edge - 0.20F;
constexpr float k_stair_half_width = WW::k_stair_half_width + 0.06F;
constexpr float k_stair_top = WW::k_deck_outer_edge + WW::k_stair_run;
constexpr float k_stair_landing = 0.35F;
constexpr float k_ladder_half_width = WW::k_ladder_half_width + 0.10F;
constexpr float k_ladder_top = WW::k_deck_outer_edge + WW::k_ladder_run;
constexpr float k_ladder_landing = 0.40F;

struct Frame {
  float across{0.0F};
  float along{0.0F};
  float inward_x{0.0F};
  float inward_z{0.0F};
};

auto frame_of(const Node& node, float x, float z) -> Frame {
  auto inward_x = static_cast<float>(node.inner_x);
  auto inward_z = static_cast<float>(node.inner_z);
  float const length = std::hypot(inward_x, inward_z);
  inward_x /= length;
  inward_z /= length;
  float const dx = x - node.x;
  float const dz = z - node.z;
  return {.across = (dx * inward_x) + (dz * inward_z),
          .along = (dx * -inward_z) + (dz * inward_x),
          .inward_x = inward_x,
          .inward_z = inward_z};
}

auto ramp_elevation(float across, float top) -> float {
  return WW::k_deck_height *
         std::clamp((top - across) / (top - WW::k_deck_outer_edge), 0.0F, 1.0F);
}

auto node_surface(const Node& node, float x, float z) -> std::optional<Surface> {
  if (node.inner_x == 0 && node.inner_z == 0) {
    return std::nullopt;
  }
  Frame const frame = frame_of(node, x, z);
  std::optional<Surface> best;
  auto offer = [&best, &node](float elevation, bool access) {
    if (!best.has_value() || elevation > best->elevation) {
      best = Surface{.elevation = elevation, .node_id = node.id, .on_access = access};
    }
  };
  if (std::abs(frame.along) <= k_deck_half_span && frame.across >= k_deck_walk_inner &&
      frame.across <= k_deck_walk_outer) {
    offer(WW::k_deck_height, false);
  }
  if (node.stair && std::abs(frame.along) <= k_stair_half_width &&
      frame.across >= k_access_start && frame.across <= k_stair_top + k_stair_landing) {
    offer(ramp_elevation(frame.across, k_stair_top), true);
  } else if (node.ladder && std::abs(frame.along) <= k_ladder_half_width &&
             frame.across >= k_access_start &&
             frame.across <= k_ladder_top + k_ladder_landing) {
    offer(ramp_elevation(frame.across, k_ladder_top), true);
  }
  return best;
}

} // namespace

auto surface_at(const std::vector<Node>& nodes,
                float x,
                float z) -> std::optional<Surface> {
  std::optional<Surface> best;
  for (auto const& node : nodes) {
    if (auto const surface = node_surface(node, x, z);
        surface.has_value() &&
        (!best.has_value() || surface->elevation > best->elevation)) {
      best = surface;
    }
  }
  return best;
}

auto nearest_deck_point(const std::vector<Node>& nodes,
                        float x,
                        float z) -> std::optional<Placement> {
  std::optional<Placement> best;
  float best_distance = std::numeric_limits<float>::max();
  for (auto const& node : nodes) {
    if (node.inner_x == 0 && node.inner_z == 0) {
      continue;
    }
    Frame const frame = frame_of(node, x, z);
    float const along = std::clamp(frame.along, -k_deck_half_span, k_deck_half_span);
    float const across = std::clamp(frame.across, k_deck_walk_inner, k_deck_walk_outer);
    float const px = node.x + (frame.inward_x * across) - (frame.inward_z * along);
    float const pz = node.z + (frame.inward_z * across) + (frame.inward_x * along);
    float const distance = std::hypot(px - x, pz - z);
    if (distance < best_distance) {
      best_distance = distance;
      best = Placement{.x = px,
                       .z = pz,
                       .surface = {.elevation = WW::k_deck_height, .node_id = node.id}};
    }
  }
  return best;
}

auto gather(Engine::Core::World& world, int owner_id, float x, float z, float reach)
    -> std::vector<Node> {
  std::vector<Node> nodes;
  auto const& owners = Game::Session::session_for(world).owners();
  for (auto [id, unit, transform] :
       world.view<const Engine::Core::UnitComponent,
                  const Engine::Core::TransformComponent>()) {
    if (unit.spawn_type != Game::Units::SpawnType::WallSegment || unit.health <= 0 ||
        world.has<Engine::Core::PendingRemovalComponent>(id)) {
      continue;
    }
    if (std::abs(transform.position.x - x) > reach ||
        std::abs(transform.position.z - z) > reach) {
      continue;
    }
    auto const* wall = world.try_get<Engine::Core::WallSegmentComponent>(id);
    if (wall == nullptr || (wall->inner_x == 0 && wall->inner_z == 0)) {
      continue;
    }
    bool const friendly =
        unit.owner_id == owner_id || owners.are_allies(unit.owner_id, owner_id);
    nodes.push_back({.id = id,
                     .x = transform.position.x,
                     .z = transform.position.z,
                     .inner_x = wall->inner_x,
                     .inner_z = wall->inner_z,
                     .stair = friendly && wall->has_stair,
                     .ladder = wall->ladder ==
                               Engine::Core::WallSegmentComponent::Ladder::Standing});
  }
  return nodes;
}

} // namespace App::Core::WallDeck
