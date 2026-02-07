#include "template_cache.h"

#include "gl/humanoid/animation/animation_inputs.h"
#include <algorithm>
#include <cmath>

namespace Render::GL {

namespace {
constexpr float k_anim_cycle_seconds = 1.0F;
constexpr std::size_t k_combat_phase_slots = 7;
constexpr std::size_t k_frame_slots = k_anim_frame_count;
constexpr std::size_t k_idle_base = 0;
constexpr std::size_t k_move_base = k_idle_base + 1;
constexpr std::size_t k_run_base = k_move_base + k_frame_slots;
constexpr std::size_t k_attack_melee_base = k_run_base + k_frame_slots;
constexpr std::size_t k_attack_ranged_base =
    k_attack_melee_base + (k_combat_phase_slots * k_frame_slots);
constexpr std::size_t k_construct_base =
    k_attack_ranged_base + (k_combat_phase_slots * k_frame_slots);
constexpr std::size_t k_heal_base = k_construct_base + k_frame_slots;
constexpr std::size_t k_hit_base = k_heal_base + k_frame_slots;
constexpr std::size_t k_anim_dense_state_slot_count =
    k_hit_base + k_frame_slots;

static_assert(k_anim_dense_state_slot_count ==
              TemplateCache::k_dense_anim_state_slots);

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

inline auto clamp_phase_index(CombatAnimPhase phase) -> std::size_t {
  auto idx = static_cast<std::size_t>(phase);
  return std::min<std::size_t>(idx, k_combat_phase_slots - 1);
}

inline auto clamp_frame_index(std::uint8_t frame) -> std::size_t {
  if (k_frame_slots == 0) {
    return 0;
  }
  return std::min<std::size_t>(frame, k_frame_slots - 1);
}

inline auto dense_anim_state_slot_index(AnimState state, CombatAnimPhase phase,
                                        std::uint8_t frame) -> std::size_t {
  const auto frame_idx = clamp_frame_index(frame);
  const auto phase_idx = clamp_phase_index(phase);
  switch (state) {
  case AnimState::Idle:
    return k_idle_base;
  case AnimState::Move:
    return k_move_base + frame_idx;
  case AnimState::Run:
    return k_run_base + frame_idx;
  case AnimState::AttackMelee:
    return k_attack_melee_base + (phase_idx * k_frame_slots) + frame_idx;
  case AnimState::AttackRanged:
    return k_attack_ranged_base + (phase_idx * k_frame_slots) + frame_idx;
  case AnimState::Construct:
    return k_construct_base + frame_idx;
  case AnimState::Heal:
    return k_heal_base + frame_idx;
  case AnimState::Hit:
    return k_hit_base + frame_idx;
  }
  return k_idle_base;
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

std::size_t TemplateCache::DenseDomainKeyHash::operator()(
    const DenseDomainKey &key) const noexcept {
  std::size_t h = std::hash<std::string>()(key.renderer_id);
  h ^=
      static_cast<std::size_t>(key.owner_id) + 0x9e3779b9 + (h << 6) + (h >> 2);
  h ^= static_cast<std::size_t>(key.lod) + 0x9e3779b9 + (h << 6) + (h >> 2);
  h ^= static_cast<std::size_t>(key.mount_lod) + 0x9e3779b9 + (h << 6) +
       (h >> 2);
  return h;
}

void TemplateRecorder::reset(std::size_t reserve_hint) {
  m_commands.clear();
  if (reserve_hint > m_commands.capacity()) {
    m_commands.reserve(reserve_hint);
  }
}

void TemplateRecorder::mesh(Mesh *mesh, const QMatrix4x4 &model,
                            const QVector3D &color, Texture *texture,
                            float alpha, int material_id) {
  if (mesh == nullptr) {
    return;
  }
  m_commands.emplace_back();
  RecordedMeshCmd &cmd = m_commands.back();
  cmd.mesh = mesh;
  cmd.texture = texture;
  cmd.shader = get_current_shader();
  cmd.local_model = model;
  cmd.color = color;
  cmd.alpha = alpha;
  cmd.material_id = material_id;
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

auto TemplateCache::dense_slot_index(std::uint8_t variant,
                                     const AnimKey &anim_key) -> std::size_t {
  const std::size_t variant_slot =
      std::min<std::size_t>(variant, k_dense_variant_slots - 1);
  const std::size_t attack_slot = std::min<std::size_t>(
      anim_key.attack_variant, k_dense_attack_variant_slots - 1);
  const std::size_t anim_slot = dense_anim_state_slot_index(
      anim_key.state, anim_key.combat_phase, anim_key.frame);

  return ((variant_slot * k_dense_attack_variant_slots) + attack_slot) *
             k_dense_anim_state_slots +
         anim_slot;
}

auto TemplateCache::get_dense_domain_handle(
    const std::string &renderer_id, std::uint32_t owner_id, std::uint8_t lod,
    std::uint8_t mount_lod) -> DenseDomainHandle {
  std::lock_guard<std::mutex> lock(m_mutex);
  DenseDomainKey key{renderer_id, owner_id, lod, mount_lod};
  auto it = m_dense_domain_lookup.find(key);
  if (it != m_dense_domain_lookup.end()) {
    return DenseDomainHandle{it->second};
  }

  const std::size_t id = m_dense_domains.size();
  DenseDomainEntry entry;
  entry.key = key;
  entry.template_slots.assign(k_dense_anim_slot_count, nullptr);
  m_dense_domains.push_back(std::move(entry));
  m_dense_domain_lookup.emplace(std::move(key), id);
  return DenseDomainHandle{id};
}

auto TemplateCache::set_dense_slot(DenseDomainHandle domain,
                                   std::size_t dense_slot,
                                   const PoseTemplate *tpl) -> void {
  if (!domain.is_valid() || domain.value >= m_dense_domains.size() ||
      dense_slot >= k_dense_anim_slot_count) {
    return;
  }
  m_dense_domains[domain.value].template_slots[dense_slot] = tpl;
}

void TemplateCache::clear_dense_slot_for_key(const TemplateKey &key) {
  DenseDomainKey domain_key{key.renderer_id, key.owner_id, key.lod,
                            key.mount_lod};
  auto domain_it = m_dense_domain_lookup.find(domain_key);
  if (domain_it == m_dense_domain_lookup.end()) {
    return;
  }
  if (domain_it->second >= m_dense_domains.size()) {
    return;
  }

  AnimKey anim_key{};
  anim_key.state = key.state;
  anim_key.combat_phase = key.combat_phase;
  anim_key.frame = key.frame;
  anim_key.attack_variant = key.attack_variant;
  const std::size_t dense_slot = dense_slot_index(key.variant, anim_key);
  if (dense_slot >= k_dense_anim_slot_count) {
    return;
  }

  m_dense_domains[domain_it->second].template_slots[dense_slot] = nullptr;
}

auto TemplateCache::get_or_build_dense(
    DenseDomainHandle domain, std::size_t dense_slot, const TemplateKey &key,
    const std::function<PoseTemplate()> &builder) -> const PoseTemplate * {
  const bool use_dense =
      domain.is_valid() && dense_slot < k_dense_anim_slot_count;

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (use_dense && domain.value < m_dense_domains.size()) {
      const PoseTemplate *dense_hit =
          m_dense_domains[domain.value].template_slots[dense_slot];
      if (dense_hit != nullptr) {
        return dense_hit;
      }
    }

    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
      m_lru.splice(m_lru.begin(), m_lru, it->second.lru_it);
      if (use_dense) {
        set_dense_slot(domain, dense_slot, &it->second.tpl);
      }
      return &it->second.tpl;
    }
  }

  PoseTemplate built = builder();

  std::lock_guard<std::mutex> lock(m_mutex);
  if (use_dense && domain.value < m_dense_domains.size()) {
    const PoseTemplate *dense_hit =
        m_dense_domains[domain.value].template_slots[dense_slot];
    if (dense_hit != nullptr) {
      return dense_hit;
    }
  }

  auto it = m_cache.find(key);
  if (it != m_cache.end()) {
    m_lru.splice(m_lru.begin(), m_lru, it->second.lru_it);
    if (use_dense) {
      set_dense_slot(domain, dense_slot, &it->second.tpl);
    }
    return &it->second.tpl;
  }

  while (m_cache.size() >= m_max_entries && !m_lru.empty()) {
    evict_lru();
  }

  m_lru.push_front(key);
  CacheEntry entry{std::move(built), m_lru.begin()};
  auto [ins_it, inserted] = m_cache.emplace(key, std::move(entry));

  if (!inserted) {
    m_lru.pop_front();
    m_lru.splice(m_lru.begin(), m_lru, ins_it->second.lru_it);
  }

  const PoseTemplate *result = &ins_it->second.tpl;
  if (use_dense) {
    set_dense_slot(domain, dense_slot, result);
  }
  return result;
}

void TemplateCache::clear() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_cache.clear();
  m_lru.clear();
  m_dense_domain_lookup.clear();
  m_dense_domains.clear();
}

void TemplateCache::evict_lru() {
  const TemplateKey oldest_key = m_lru.back();
  clear_dense_slot_for_key(oldest_key);
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
