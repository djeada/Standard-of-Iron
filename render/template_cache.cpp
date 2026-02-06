#include "template_cache.h"

#include "gl/humanoid/animation/animation_inputs.h"
#include <algorithm>
#include <cmath>

namespace Render::GL {

namespace {
constexpr float k_anim_cycle_seconds = 1.0F;

inline auto clamp01(float v) -> float { return std::clamp(v, 0.0F, 1.0F); }

inline auto phase_to_frame(float phase) -> std::uint8_t {
  float clamped = clamp01(phase);
  int idx = static_cast<int>(clamped * k_anim_frame_count);
  if (idx >= static_cast<int>(k_anim_frame_count)) {
    idx = static_cast<int>(k_anim_frame_count - 1);
  }
  return static_cast<std::uint8_t>(idx);
}

inline auto frame_to_phase(std::uint8_t frame) -> float {
  if (k_anim_frame_count == 0) {
    return 0.0F;
  }
  return static_cast<float>(frame) / static_cast<float>(k_anim_frame_count - 1);
}

inline auto time_phase(float t) -> float {
  if (k_anim_cycle_seconds <= 0.0F) {
    return 0.0F;
  }
  float wrapped = std::fmod(t, k_anim_cycle_seconds);
  if (wrapped < 0.0F) {
    wrapped += k_anim_cycle_seconds;
  }
  return wrapped / k_anim_cycle_seconds;
}
} // namespace

std::size_t TemplateKeyHash::operator()(const TemplateKey &key) const noexcept {
  std::size_t h = std::hash<std::string>()(key.renderer_id);
  h ^=
      static_cast<std::size_t>(key.owner_id) + 0x9e3779b9 + (h << 6) + (h >> 2);
  h ^= static_cast<std::size_t>(key.lod) + 0x9e3779b9 + (h << 6) + (h >> 2);
  h ^= static_cast<std::size_t>(key.mount_lod) + 0x9e3779b9 + (h << 6) +
       (h >> 2);
  h ^= static_cast<std::size_t>(key.variant) + 0x9e3779b9 + (h << 6) + (h >> 2);
  h ^= static_cast<std::size_t>(key.attack_variant) + 0x9e3779b9 + (h << 6) +
       (h >> 2);
  h ^= static_cast<std::size_t>(key.state) + 0x9e3779b9 + (h << 6) + (h >> 2);
  h ^= static_cast<std::size_t>(key.combat_phase) + 0x9e3779b9 + (h << 6) +
       (h >> 2);
  h ^= static_cast<std::size_t>(key.frame) + 0x9e3779b9 + (h << 6) + (h >> 2);
  return h;
}

void TemplateRecorder::reset() { m_commands.clear(); }

void TemplateRecorder::mesh(Mesh *mesh, const QMatrix4x4 &model,
                            const QVector3D &color, Texture *texture,
                            float alpha, int material_id) {
  if (mesh == nullptr) {
    return;
  }
  RecordedMeshCmd cmd;
  cmd.mesh = mesh;
  cmd.texture = texture;
  cmd.shader = get_current_shader();
  cmd.local_model = model;
  cmd.color = color;
  cmd.alpha = alpha;
  cmd.material_id = material_id;
  m_commands.push_back(std::move(cmd));
}

auto TemplateCache::get_or_build(const TemplateKey &key,
                                 const std::function<PoseTemplate()> &builder)
    -> const PoseTemplate * {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {

      m_lru.splice(m_lru.begin(), m_lru, it->second.lru_it);
      return &it->second.tpl;
    }
  }

  PoseTemplate built = builder();

  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_cache.find(key);
  if (it != m_cache.end()) {
    m_lru.splice(m_lru.begin(), m_lru, it->second.lru_it);
    return &it->second.tpl;
  }

  while (m_cache.size() >= m_max_entries && !m_lru.empty()) {
    evict_lru();
  }

  m_lru.push_front(key);
  CacheEntry entry{std::move(built), m_lru.begin()};
  auto [ins_it, inserted] = m_cache.emplace(key, std::move(entry));
  return &ins_it->second.tpl;
}

void TemplateCache::clear() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_cache.clear();
  m_lru.clear();
}

void TemplateCache::evict_lru() {

  const auto &oldest_key = m_lru.back();
  m_cache.erase(oldest_key);
  m_lru.pop_back();
}

auto make_anim_key(const AnimationInputs &anim, float phase_offset,
                   std::uint8_t attack_variant) -> AnimKey {
  AnimKey key{};

  if (anim.is_hit_reacting) {
    key.state = AnimState::Hit;
    key.frame = phase_to_frame(1.0F - clamp01(anim.hit_reaction_intensity));
    key.combat_phase = CombatAnimPhase::Idle;
    key.attack_variant = 0;
    return key;
  }

  if (anim.is_constructing) {
    key.state = AnimState::Construct;
    key.frame = phase_to_frame(anim.construction_progress);
    key.combat_phase = CombatAnimPhase::Idle;
    key.attack_variant = 0;
    return key;
  }

  if (anim.is_healing) {
    key.state = AnimState::Heal;
    key.frame = phase_to_frame(time_phase(anim.time + phase_offset));
    key.combat_phase = CombatAnimPhase::Idle;
    key.attack_variant = 0;
    return key;
  }

  if (anim.is_attacking) {
    key.state =
        anim.is_melee ? AnimState::AttackMelee : AnimState::AttackRanged;
    key.combat_phase = anim.combat_phase;
    key.attack_variant = attack_variant;
    float phase = anim.combat_phase_progress;
    if (phase <= 0.0F) {
      phase = time_phase(anim.time + phase_offset);
    } else {
      phase = clamp01(phase + phase_offset);
    }
    key.frame = phase_to_frame(phase);
    return key;
  }

  if (anim.is_running) {
    key.state = AnimState::Run;
    key.frame = phase_to_frame(time_phase(anim.time + phase_offset));
    key.combat_phase = CombatAnimPhase::Idle;
    key.attack_variant = 0;
    return key;
  }

  if (anim.is_moving) {
    key.state = AnimState::Move;
    key.frame = phase_to_frame(time_phase(anim.time + phase_offset));
    key.combat_phase = CombatAnimPhase::Idle;
    key.attack_variant = 0;
    return key;
  }

  key.state = AnimState::Idle;
  key.frame = 0;
  key.combat_phase = CombatAnimPhase::Idle;
  key.attack_variant = 0;
  return key;
}

auto make_animation_inputs(const AnimKey &key) -> AnimationInputs {
  AnimationInputs anim{};
  anim.time = frame_to_phase(key.frame) * k_anim_cycle_seconds;
  anim.is_moving = false;
  anim.is_running = false;
  anim.is_attacking = false;
  anim.is_melee = false;
  anim.is_in_hold_mode = false;
  anim.is_exiting_hold = false;
  anim.hold_exit_progress = 0.0F;
  anim.combat_phase = CombatAnimPhase::Idle;
  anim.combat_phase_progress = 0.0F;
  anim.attack_variant = key.attack_variant;
  anim.is_hit_reacting = false;
  anim.hit_reaction_intensity = 0.0F;
  anim.is_healing = false;
  anim.healing_target_dx = 0.0F;
  anim.healing_target_dz = 0.0F;
  anim.is_constructing = false;
  anim.construction_progress = 0.0F;

  float phase = frame_to_phase(key.frame);

  switch (key.state) {
  case AnimState::Move:
    anim.is_moving = true;
    break;
  case AnimState::Run:
    anim.is_moving = true;
    anim.is_running = true;
    break;
  case AnimState::AttackMelee:
    anim.is_attacking = true;
    anim.is_melee = true;
    anim.combat_phase = key.combat_phase;
    anim.combat_phase_progress = phase;
    break;
  case AnimState::AttackRanged:
    anim.is_attacking = true;
    anim.is_melee = false;
    anim.combat_phase = key.combat_phase;
    anim.combat_phase_progress = phase;
    break;
  case AnimState::Construct:
    anim.is_constructing = true;
    anim.construction_progress = phase;
    break;
  case AnimState::Heal:
    anim.is_healing = true;
    break;
  case AnimState::Hit:
    anim.is_hit_reacting = true;
    anim.hit_reaction_intensity = 1.0F - phase;
    break;
  case AnimState::Idle:
  default:
    break;
  }

  if (anim.is_attacking && anim.combat_phase == CombatAnimPhase::Idle) {
    anim.combat_phase = CombatAnimPhase::Strike;
  }

  return anim;
}

} // namespace Render::GL
