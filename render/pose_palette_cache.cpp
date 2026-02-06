#include "pose_palette_cache.h"

#include "humanoid/rig.h"
#include "template_cache.h"

namespace Render::GL {

void PosePaletteCache::generate() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_palette.clear();

  VariationParams default_variation{};
  default_variation.height_scale = 1.0F;
  default_variation.bulk_scale = 1.0F;
  default_variation.stance_width = 1.0F;
  default_variation.arm_swing_amp = 1.0F;
  default_variation.walk_speed_mult = 1.0F;
  default_variation.posture_slump = 0.0F;
  default_variation.shoulder_tilt = 0.0F;

  constexpr std::uint32_t k_base_seed = 0;
  constexpr float k_cycle = 1.0F;

  constexpr AnimState k_states[] = {AnimState::Idle, AnimState::Move,
                                    AnimState::Run,  AnimState::Construct,
                                    AnimState::Heal, AnimState::Hit};

  for (AnimState state : k_states) {
    bool is_moving = (state == AnimState::Move || state == AnimState::Run);

    for (std::uint8_t frame = 0; frame < k_anim_frame_count; ++frame) {
      float phase = (k_anim_frame_count > 1)
                        ? static_cast<float>(frame) /
                              static_cast<float>(k_anim_frame_count - 1)
                        : 0.0F;
      float time = phase * k_cycle;

      HumanoidPose pose;
      HumanoidRendererBase::compute_locomotion_pose(
          k_base_seed, time, is_moving, default_variation, pose);

      PosePaletteKey key;
      key.state = state;
      key.frame = frame;
      key.is_moving = is_moving;

      PosePaletteEntry entry;
      entry.pose = pose;
      entry.time = time;
      m_palette[key] = entry;
    }
  }

  constexpr AnimState k_combat_states[] = {AnimState::AttackMelee,
                                           AnimState::AttackRanged};
  for (AnimState state : k_combat_states) {
    for (std::uint8_t frame = 0; frame < k_anim_frame_count; ++frame) {
      float phase = (k_anim_frame_count > 1)
                        ? static_cast<float>(frame) /
                              static_cast<float>(k_anim_frame_count - 1)
                        : 0.0F;
      float time = phase * k_cycle;

      HumanoidPose pose;
      HumanoidRendererBase::compute_locomotion_pose(k_base_seed, time, false,
                                                    default_variation, pose);

      PosePaletteKey key;
      key.state = state;
      key.frame = frame;
      key.is_moving = false;

      PosePaletteEntry entry;
      entry.pose = pose;
      entry.time = time;
      m_palette[key] = entry;
    }
  }

  m_generated = true;
}

auto PosePaletteCache::get(const PosePaletteKey &key) const
    -> const PosePaletteEntry * {
  std::lock_guard<std::mutex> lock(m_mutex);
  auto it = m_palette.find(key);
  if (it == m_palette.end()) {
    return nullptr;
  }
  return &it->second;
}

void PosePaletteCache::clear() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_palette.clear();
  m_generated = false;
}

auto PosePaletteCache::size() const -> std::size_t {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_palette.size();
}

} // namespace Render::GL
