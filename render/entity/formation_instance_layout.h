#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "animation/individuality_manifest.h"
#include "game/formation/unit_layout.h"
#include "render/gl/humanoid/humanoid_types.h"

namespace Engine::Core {
struct FormationPresentationComponent;
}

namespace Render::Entity {

struct FormationInstance {
  float offset_x{0.0F};
  float offset_z{0.0F};
  float vertical_jitter{0.0F};
  float yaw_offset{0.0F};

  Animation::SoldierIndividuality individuality{};
  std::uint8_t rank_band{0U};
  std::uint8_t row_index{0U};
  std::uint8_t col_index{0U};
  std::uint32_t inst_seed{0};
};

struct FormationInstanceRequest {
  int idx{0};
  int row{0};
  int col{0};
  int rows{0};
  int cols{0};
  int count{0};
  float formation_spacing{0.0F};
  std::uint32_t seed{0};
  bool force_single_soldier{false};
  bool melee_attack{false};
  float animation_time{0.0F};
  Game::Formation::UnitLayoutId unit_layout{Game::Formation::k_invalid_layout};
  float formed_ratio{1.0F};
  Game::Formation::UnitLayoutId blend_from{Game::Formation::k_invalid_layout};
  float blend_ratio{1.0F};

  const Game::Formation::UnitLayoutSystem* soldier_offsets{nullptr};
};

[[nodiscard]] auto
build_formation_instance(const FormationInstanceRequest& request) -> FormationInstance;

struct FormationLayoutCache {
  std::vector<FormationInstance> instances;
  Render::GL::FormationParams formation{};
  Game::Formation::UnitLayoutId unit_layout{Game::Formation::k_invalid_layout};
  int rows{0};
  int cols{0};
  std::uint32_t layout_version{0};
  std::uint32_t seed{0};
  std::uint32_t frame_number{0};
  Game::Formation::UnitLayoutId blend_from{Game::Formation::k_invalid_layout};
  float blend_ratio{1.0F};
  bool valid{false};
};

struct FormationLayoutRequest {
  Render::GL::FormationParams formation{};
  Game::Formation::UnitLayoutId unit_layout{Game::Formation::k_invalid_layout};
  int rows{0};
  int cols{0};
  int total_count{0};
  std::uint32_t seed{0};
  float animation_time{0.0F};
  bool melee_attack{false};
  bool force_single_soldier{false};
  float formed_ratio{1.0F};
  Game::Formation::UnitLayoutId blend_from{Game::Formation::k_invalid_layout};
  float blend_ratio{1.0F};
  std::uint32_t frame_index{0};
  std::uint32_t layout_version{0};

  std::uint32_t max_cache_age{0};

  const Game::Formation::UnitLayoutSystem* soldier_offsets{nullptr};
};

struct FormationLayoutResult {

  std::vector<FormationInstance>* instances{nullptr};
  bool reused_cache{false};

  bool preserve_state_prefix{false};
};

auto resolve_formation_instances(FormationLayoutCache* cache,
                                 std::vector<FormationInstance>& scratch,
                                 const FormationLayoutRequest& request)
    -> FormationLayoutResult;

void apply_authoritative_formation_slots(
    std::span<FormationInstance> instances,
    const Engine::Core::FormationPresentationComponent* presentation,
    Engine::Core::Entity* entity,
    bool force_single_soldier);

} // namespace Render::Entity
