#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "../../core/component.h"
#include "../../core/entity.h"
#include "../formation_combat_geometry.h"

namespace Game::Systems::Combat {

struct SurroundSector {
  float center{0.0F};
  float half_width{0.0F};
};

class SurroundBoard {
public:
  void clear() { m_claims.clear(); }
  void enlist(Engine::Core::EntityID target,
              Engine::Core::EntityID squad,
              float approach,
              int soldiers);
  void allot();
  [[nodiscard]] auto
  sector_of(Engine::Core::EntityID target,
            Engine::Core::EntityID squad) const -> std::optional<SurroundSector>;

private:
  struct Claim {
    Engine::Core::EntityID target{0U};
    Engine::Core::EntityID squad{0U};
    float approach{0.0F};
    float weight{0.0F};
    SurroundSector sector;
  };
  std::vector<Claim> m_claims;
};

struct SurroundSpot {
  bool active{false};
  std::uint8_t ring{0U};
  float local_x{0.0F};
  float local_z{0.0F};
};

struct SurroundInputs {
  const Engine::Core::TransformComponent& actor;
  const FormationCombat::FormationLayout& layout;
  const std::vector<Engine::Core::FormationSoldierPresentation>& previous;
  SurroundSector sector;
  float target_x{0.0F};
  float target_z{0.0F};
  float first_ring_radius{0.0F};
  float frame_sign{1.0F};
};

inline constexpr float k_surround_reach = 7.0F;

[[nodiscard]] auto surround_approach(float target_x,
                                     float target_z,
                                     float squad_x,
                                     float squad_z) -> float;

void plan_surround(const SurroundInputs& in, std::vector<SurroundSpot>& spots);

} // namespace Game::Systems::Combat
