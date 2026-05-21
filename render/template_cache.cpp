#include "template_cache.h"

#include "gl/humanoid/animation/animation_inputs.h"

namespace Render::GL {

namespace {

constexpr float k_anim_cycle_seconds = 1.0F;

inline auto frame_to_phase(std::uint8_t frame) -> float {
  if (k_anim_frame_count <= 1U) {
    return 0.0F;
  }
  return static_cast<float>(frame) / static_cast<float>(k_anim_frame_count - 1U);
}

} // namespace

void TemplateRecorder::reset(std::size_t reserve_hint) {
  m_commands.clear();
  if (reserve_hint > m_commands.capacity()) {
    m_commands.reserve(reserve_hint);
  }
}

void TemplateRecorder::mesh(Mesh* mesh,
                            const QMatrix4x4& model,
                            const QVector3D& color,
                            Texture* texture,
                            float alpha,
                            int material_id) {
  if (mesh == nullptr) {
    return;
  }
  m_commands.emplace_back();
  RecordedMeshCmd& cmd = m_commands.back();
  cmd.mesh = mesh;
  cmd.texture = texture;
  cmd.shader = get_current_shader();
  cmd.material = (cmd.shader == nullptr) ? m_current_material : nullptr;
  cmd.local_model = model;
  cmd.color = color;
  cmd.alpha = alpha;
  cmd.material_id = material_id;
}

void TemplateRecorder::banner(Mesh* mesh_obj,
                              const QMatrix4x4& model,
                              const QVector3D& color,
                              const QVector3D& trim_color,
                              Texture* texture,
                              float alpha,
                              int material_id) {
  (void)trim_color;
  this->mesh(mesh_obj, model, color, texture, alpha, material_id);
}

void TemplateRecorder::part(Mesh* mesh,
                            Material* material,
                            const QMatrix4x4& model,
                            const QVector3D& color,
                            Texture* texture,
                            float alpha,
                            int material_id) {
  if (mesh == nullptr) {
    return;
  }
  m_commands.emplace_back();
  RecordedMeshCmd& cmd = m_commands.back();
  cmd.mesh = mesh;
  cmd.texture = texture;
  cmd.shader = nullptr;
  cmd.material = material;
  cmd.local_model = model;
  cmd.color = color;
  cmd.alpha = alpha;
  cmd.material_id = material_id;
}

auto make_animation_inputs(const AnimKey& key) -> AnimationInputs {
  AnimationInputs anim{};
  anim.time = frame_to_phase(key.frame) * k_anim_cycle_seconds;
  anim.is_attacking = false;
  anim.is_melee = false;
  anim.is_in_hold_mode = false;
  anim.is_exiting_hold = false;
  anim.hold_exit_progress = 0.0F;
  anim.hold_entry_progress = 0.0F;
  anim.combat_phase = CombatAnimPhase::Idle;
  anim.combat_phase_progress = 0.0F;
  anim.attack_family = key.attack_family;
  anim.attack_variant = key.attack_variant;
  anim.finisher_attack = key.finisher_attack;
  anim.is_hit_reacting = false;
  anim.hit_reaction_intensity = 0.0F;
  anim.is_healing = false;
  anim.healing_target_dx = 0.0F;
  anim.healing_target_dz = 0.0F;
  anim.is_constructing = false;
  anim.construction_progress = 0.0F;
  anim.is_dying = false;
  anim.is_dead = false;
  anim.death_progress = 0.0F;
  anim.death_variant = 0;

  float const phase = frame_to_phase(key.frame);

  switch (key.state) {
  case Render::Creature::PoseIntent::Walk:
    anim.movement_state = Render::Creature::MovementAnimationState::Walk;
    break;
  case Render::Creature::PoseIntent::Run:
    anim.movement_state = Render::Creature::MovementAnimationState::Run;
    break;
  case Render::Creature::PoseIntent::Hold:
    anim.is_in_hold_mode = true;
    anim.hold_entry_progress = phase;
    break;
  case Render::Creature::PoseIntent::AttackMelee:
    anim.is_attacking = true;
    anim.is_melee = true;
    anim.combat_phase = key.combat_phase;
    anim.combat_phase_progress = phase;
    if (anim.attack_family == Engine::Core::CombatAttackFamily::None) {
      anim.attack_family = Engine::Core::CombatAttackFamily::Sword;
    }
    break;
  case Render::Creature::PoseIntent::AttackSpear:
    anim.is_attacking = true;
    anim.is_melee = true;
    anim.combat_phase = key.combat_phase;
    anim.combat_phase_progress = phase;
    if (anim.attack_family == Engine::Core::CombatAttackFamily::None) {
      anim.attack_family = Engine::Core::CombatAttackFamily::Spear;
    }
    break;
  case Render::Creature::PoseIntent::AttackRanged:
    anim.is_attacking = true;
    anim.is_melee = false;
    anim.combat_phase = key.combat_phase;
    anim.combat_phase_progress = phase;
    if (anim.attack_family == Engine::Core::CombatAttackFamily::None) {
      anim.attack_family = Engine::Core::CombatAttackFamily::Bow;
    }
    break;
  case Render::Creature::PoseIntent::Cast:
    anim.is_attacking = true;
    anim.is_melee = false;
    anim.is_casting = true;
    anim.cast_kind = Render::GL::CastVisualKind::Fireball;
    anim.combat_phase = key.combat_phase;
    anim.combat_phase_progress = phase;
    if (anim.attack_family == Engine::Core::CombatAttackFamily::None) {
      anim.attack_family = Engine::Core::CombatAttackFamily::Bow;
    }
    break;
  case Render::Creature::PoseIntent::Construct:
    anim.is_constructing = true;
    anim.construction_progress = phase;
    break;
  case Render::Creature::PoseIntent::Healing:
    anim.is_healing = true;
    break;
  case Render::Creature::PoseIntent::HitReaction:
    anim.is_hit_reacting = true;
    anim.hit_reaction_intensity = 1.0F - phase;
    break;
  case Render::Creature::PoseIntent::Dying:
    anim.is_dying = true;
    anim.death_progress = phase;
    break;
  case Render::Creature::PoseIntent::Dead:
    anim.is_dead = true;
    anim.death_progress = 1.0F;
    break;
  case Render::Creature::PoseIntent::Idle:
  case Render::Creature::PoseIntent::RidingIdle:
  case Render::Creature::PoseIntent::RidingCharge:
  case Render::Creature::PoseIntent::RidingReining:
  case Render::Creature::PoseIntent::RidingBowShot:
  case Render::Creature::PoseIntent::Count:
  default:
    anim.movement_state = Render::Creature::MovementAnimationState::Idle;
    break;
  }

  if (anim.is_attacking && anim.combat_phase == CombatAnimPhase::Idle) {
    anim.combat_phase = CombatAnimPhase::Strike;
  }

  return anim;
}

} // namespace Render::GL
