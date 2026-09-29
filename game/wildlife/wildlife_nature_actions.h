#pragma once

#include "nature_ai.h"
#include "wildlife_census.h"
#include "wildlife_group.h"
#include "wildlife_predation.h"

namespace Engine::Core {
class World;
}

namespace Game::Wildlife {

class WildlifeNatureActions final : public NatureActions {
public:
  WildlifeNatureActions(Engine::Core::World& world,
                        WildlifeCensus& census,
                        WildlifePredation& predation,
                        WildlifeStats& stats);

  void move_to(const NatureContext& ctx, float world_x, float world_z) override;
  [[nodiscard]] auto bypass_around_obstacle(const NatureContext& ctx,
                                            float prey_x,
                                            float prey_z,
                                            float standoff)
      -> std::optional<std::pair<float, float>> override;
  void halt(const NatureContext& ctx) override;
  void face_toward(const NatureContext& ctx, float world_x, float world_z) override;
  void set_travel_speed(const NatureContext& ctx, bool urgent) override;
  void alert_herd(std::uint16_t group_id, float duration) override;
  void mark_hostile(const NatureContext& ctx,
                    Engine::Core::EntityID foe_id,
                    bool rally_pack) override;
  void note(const NatureContext& ctx, NatureEvent event) override;
  auto pick_open_point(std::uint32_t& rng,
                       float origin_x,
                       float origin_z,
                       float min_radius,
                       float max_radius,
                       float& out_x,
                       float& out_z) -> bool override;
  auto nearest_prey(const NatureContext& ctx, float radius) -> PreyRef override;
  auto nearest_quarry(const NatureContext& ctx, float radius) -> PreyRef override;
  auto locate(const NatureContext& ctx,
              Engine::Core::EntityID entity_id) -> PreyRef override;
  auto claim_pack_slot(const NatureContext& ctx,
                       const PreyRef& prey) -> PackSlot override;
  auto nearest_pack_hunter(float world_x,
                           float world_z,
                           float radius) -> ThreatQuery override;
  auto bite(const NatureContext& ctx, const PreyRef& prey) -> bool override;

private:
  Engine::Core::World& m_world;
  WildlifeCensus& m_census;
  WildlifePredation& m_predation;
  WildlifeStats& m_stats;
};

} // namespace Game::Wildlife
