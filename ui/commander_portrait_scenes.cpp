#include "commander_portrait_scenes.h"

#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLFunctions>
#include <QVector3D>

#include "animation/showcase_pose_manifest.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/systems/showcase_routine_system.h"
#include "game/units/factory.h"
#include "game/units/troop_type.h"
#include "game/units/unit.h"
#include "render/graphics_settings.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"
#include "scene/environment_lighting.h"

namespace UI {

namespace {

constexpr int k_warm_target_size = 64;

constexpr float k_body_yaw_degrees = 14.0F;

auto portrait_lighting() -> Render::EnvironmentLightingState {
  Render::EnvironmentLightingState lighting;
  lighting.primary_direction = QVector3D(0.45F, 0.80F, 0.55F);
  lighting.primary_color = QVector3D(1.0F, 0.82F, 0.58F);
  lighting.primary_intensity = 1.15F;
  lighting.sky_color = QVector3D(0.055F, 0.048F, 0.042F);
  lighting.ground_bounce_color = QVector3D(0.12F, 0.08F, 0.05F);
  lighting.ambient_intensity = 0.44F;
  lighting.fog_density = 0.0F;
  lighting.shadow_tint = QVector3D(0.10F, 0.09F, 0.10F);
  lighting.shadow_strength = 0.5F;
  return lighting;
}

void seed_showcase_routine(Engine::Core::ShowcaseRoutineComponent& routine) {
  routine.steps.clear();
  routine.steps.push_back({.move = static_cast<std::uint8_t>(
                               Animation::HumanoidShowcaseMove::TauntDismissive),
                           .duration = 0.0F,
                           .hold_after = 0.0F});
  routine.steps.push_back(
      {.move = static_cast<std::uint8_t>(Animation::HumanoidShowcaseMove::TauntCynical),
       .duration = 0.0F,
       .hold_after = 0.0F});
  routine.index = 0;
  routine.elapsed = 0.0F;
  routine.phase = 0.0F;
  routine.finished = false;
  routine.active = true;
  routine.loop = true;
  routine.loop_from = routine.steps.size() - 1U;
}

} // namespace

auto CommanderPortraitScenes::instance() -> CommanderPortraitScenes& {
  static CommanderPortraitScenes scenes;
  return scenes;
}

CommanderPortraitScenes::~CommanderPortraitScenes() {

  m_entries.clear();
  (void)m_renderer.release();
  (void)m_camera.release();
}

void CommanderPortraitScenes::add_reference() {
  ++m_references;
}

void CommanderPortraitScenes::release_reference() {
  if (m_references > 0) {
    --m_references;
  }
  if (m_references == 0) {
    release_all();
  }
}

void CommanderPortraitScenes::release_all() {
  m_entries.clear();
  m_warmed_signature.clear();
  if (m_renderer != nullptr) {
    m_renderer->shutdown();
    m_renderer.reset();
  }
  m_camera.reset();
  m_renderer_failed = false;
}

auto CommanderPortraitScenes::ensure_renderer() -> bool {
  if (m_renderer_failed) {
    return false;
  }
  if (m_renderer != nullptr) {
    return true;
  }
  auto renderer = std::make_unique<Render::GL::Renderer>(
      Render::GraphicsSettings::instance().backend_kind());
  if (!renderer->initialize()) {
    m_renderer_failed = true;
    return false;
  }
  m_renderer = std::move(renderer);
  m_camera = std::make_unique<Render::GL::Camera>();
  return true;
}

auto CommanderPortraitScenes::renderer() -> Render::GL::Renderer* {
  return ensure_renderer() ? m_renderer.get() : nullptr;
}

auto CommanderPortraitScenes::camera() -> Render::GL::Camera* {
  return ensure_renderer() ? m_camera.get() : nullptr;
}

auto CommanderPortraitScenes::acquire(const QString& troop_type) -> Scene {
  Game::Units::TroopType parsed{};
  if (troop_type.isEmpty() || !Game::Units::try_parse_troop_type(troop_type, parsed)) {
    return {};
  }

  auto existing = m_entries.find(troop_type);
  if (existing != m_entries.end()) {
    return Scene{.world = existing->second.world.get(),
                 .entity = existing->second.entity};
  }

  Entry entry;
  entry.world = std::make_unique<Engine::Core::World>();
  entry.world->add_system(std::make_unique<Game::Systems::ShowcaseRoutineSystem>());

  entry.factory = std::make_unique<Game::Units::UnitFactoryRegistry>();
  Game::Units::register_built_in_units(*entry.factory);

  Game::Units::SpawnParams params;
  params.position = QVector3D(0.0F, 0.0F, 0.0F);
  params.player_id = 1;
  params.spawn_type = Game::Units::spawn_typeFromTroopType(parsed);
  params.ai_controlled = false;
  params.enables_production = false;

  entry.unit = entry.factory->create(parsed, *entry.world, params);
  if (entry.unit == nullptr) {
    return {};
  }
  entry.entity = entry.unit->id();

  auto* entity = entry.world->get_entity(entry.entity);
  if (entity == nullptr) {
    return {};
  }
  if (auto* transform = entity->get_component<Engine::Core::TransformComponent>()) {
    transform->rotation.y = k_body_yaw_degrees;
    transform->desired_yaw = k_body_yaw_degrees;
    transform->has_desired_yaw = true;
  }
  auto* routine = entity->add_component<Engine::Core::ShowcaseRoutineComponent>();
  if (routine != nullptr) {
    seed_showcase_routine(*routine);
  }

  auto inserted = m_entries.emplace(troop_type, std::move(entry));
  return Scene{.world = inserted.first->second.world.get(),
               .entity = inserted.first->second.entity};
}

void CommanderPortraitScenes::warm(const QStringList& troop_types) {
  if (troop_types.isEmpty()) {
    return;
  }
  const QString signature = troop_types.join(QLatin1Char('|'));
  if (signature == m_warmed_signature) {
    return;
  }

  QOpenGLContext* context = QOpenGLContext::currentContext();
  if ((context == nullptr) || !context->isValid()) {
    return;
  }
  if (!ensure_renderer()) {
    return;
  }

  QOpenGLFramebufferObjectFormat format;
  format.setAttachment(QOpenGLFramebufferObject::Depth);
  QOpenGLFramebufferObject target(QSize(k_warm_target_size, k_warm_target_size),
                                  format);
  if (!target.isValid()) {
    return;
  }

  QOpenGLFunctions* functions = context->functions();
  GLint previous_framebuffer = 0;
  functions->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);

  target.bind();
  for (const QString& troop_type : troop_types) {
    const Scene scene = acquire(troop_type);
    if (scene.world == nullptr) {
      continue;
    }

    scene.world->update(1.0F / 60.0F);

    m_camera->look_at(QVector3D(0.35F, 1.35F, 1.15F),
                      QVector3D(0.0F, 1.31F, 0.0F),
                      QVector3D(0.0F, 1.0F, 0.0F));
    m_camera->set_perspective(32.0F, 1.0F, 0.05F, 40.0F);

    m_renderer->set_camera(m_camera.get());
    m_renderer->set_viewport(k_warm_target_size, k_warm_target_size);
    m_renderer->set_environment_lighting(portrait_lighting());
    m_renderer->set_clear_color(0.055F, 0.048F, 0.042F, 1.0F);
    m_renderer->set_local_owner_id(1);
    m_renderer->set_force_full_creature_lod(true);
    m_renderer->update_animation_time(1.0F / 60.0F);

    m_renderer->begin_frame();
    m_renderer->render_world(scene.world);
    m_renderer->end_frame();
  }
  functions->glBindFramebuffer(GL_FRAMEBUFFER,
                               static_cast<GLuint>(previous_framebuffer));

  m_warmed_signature = signature;
}

} // namespace UI
