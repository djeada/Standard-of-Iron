#pragma once

#include <QVector3D>

#include "../ai_types.h"
#include "builder_stall_watch.h"

namespace Game::Systems::AI {

[[nodiscard]] auto expanding_ring_offset(const AIContext& context,
                                         int index,
                                         int per_ring,
                                         float first_radius,
                                         float radius_step) -> QVector3D;

[[nodiscard]] auto planned_settlement_offset(const AIContext& context,
                                             const char* building_type,
                                             int construction_index) -> QVector3D;

struct NodeOnTheGround {
  const ResourceNodeSnapshot* node = nullptr;
  ResourceType resource = ResourceType::Count;
};

[[nodiscard]] auto node_on_the_ground(const AISnapshot& snapshot,
                                      const SourNodes& sour,
                                      const char* building_type,
                                      float world_x,
                                      float world_z) -> NodeOnTheGround;

struct ResolvedSite {
  bool resolved = false;
  float x = 0.0F;
  float z = 0.0F;

  bool exhausted = false;
};

[[nodiscard]] auto find_free_site(const AISnapshot& snapshot,
                                  const AIContext& context,
                                  const char* building_type,
                                  int& construction_counter) -> ResolvedSite;

} // namespace Game::Systems::AI
