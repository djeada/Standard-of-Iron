#include "commander_portrait_view.h"

#include <QMatrix4x4>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QVector3D>

#include <algorithm>
#include <chrono>
#include <cmath>

#include "animation/rig/humanoid_proportions.h"
#include "animation/showcase_pose_manifest.h"
#include "commander_portrait_scenes.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/systems/nation_id.h"
#include "render/creature/pipeline/creature_bone_probe.h"
#include "render/creature/runtime_bake_guard.h"
#include "render/gl/primitives.h"
#include "render/humanoid/schema/skeleton_schema.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"
#include "scene/environment_lighting.h"

namespace {

constexpr float k_field_of_view = 32.0F;

constexpr float k_bust_distance = 1.16F;
constexpr QVector3D k_bust_direction = QVector3D(0.30F, 0.09F, 0.95F);

constexpr float k_bust_drop = 0.075F;

constexpr float k_default_focus_height = 1.31F;

constexpr float k_focus_tau = 0.30F;

constexpr float k_max_frame_seconds = 0.1F;

constexpr float k_head_radius = Render::GL::HumanProportions::HEAD_RADIUS;
constexpr float k_cranium_rise = k_head_radius * 0.06F;

// The baked cranium is an ellipsoid whose front reaches just under two local head
// radii. Keep the portrait features a hair above it so depth testing seats them on
// the skin instead of swallowing most of each small mesh.
constexpr float k_face_surface = k_head_radius * 2.02F;
constexpr float k_mouth_surface = k_head_radius * 2.20F;

constexpr QVector3D k_eye_white{0.72F, 0.66F, 0.54F};
constexpr QVector3D k_roman_iris{0.30F, 0.18F, 0.075F};
constexpr QVector3D k_carthage_iris{0.20F, 0.25F, 0.18F};
constexpr QVector3D k_pupil{0.025F, 0.018F, 0.012F};
constexpr QVector3D k_brow{0.13F, 0.075F, 0.035F};
constexpr QVector3D k_mouth{0.16F, 0.055F, 0.035F};
constexpr QVector3D k_lip{0.38F, 0.16F, 0.11F};

auto face_feature_model(const QMatrix4x4& head_world,
                        const QVector3D& centre,
                        const QVector3D& radii,
                        float roll_degrees = 0.0F) -> QMatrix4x4 {
  QMatrix4x4 model = head_world;
  model.translate(centre);
  if (std::abs(roll_degrees) > 1.0e-4F) {
    model.rotate(roll_degrees, 0.0F, 0.0F, 1.0F);
  }
  model.scale(radii.x(), radii.y(), radii.z());
  return model;
}

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

auto move_for_pose(const QString& pose) -> Animation::HumanoidShowcaseMove {
  if (pose == QStringLiteral("dismissive")) {
    return Animation::HumanoidShowcaseMove::TauntDismissive;
  }
  return Animation::HumanoidShowcaseMove::TauntCynical;
}

} // namespace

class CommanderPortraitView::PortraitRenderer
    : public QQuickFramebufferObject::Renderer {
public:
  PortraitRenderer() { UI::CommanderPortraitScenes::instance().add_reference(); }
  ~PortraitRenderer() override {
    release_scene();
    UI::CommanderPortraitScenes::instance().release_reference();
  }

  PortraitRenderer(const PortraitRenderer&) = delete;
  auto operator=(const PortraitRenderer&) -> PortraitRenderer& = delete;
  PortraitRenderer(PortraitRenderer&&) = delete;
  auto operator=(PortraitRenderer&&) -> PortraitRenderer& = delete;

  auto
  createFramebufferObject(const QSize& size) -> QOpenGLFramebufferObject* override {
    m_size = size;
    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::Depth);
    return new QOpenGLFramebufferObject(size, format);
  }

  void synchronize(QQuickFramebufferObject* item) override {
    auto* view = dynamic_cast<CommanderPortraitView*>(item);
    if (view == nullptr) {
      return;
    }
    const QString troop_type = view->troop_type();
    const QString pose = view->pose();
    m_nation = view->nation();
    m_speaking = view->speaking();
    m_talking = view->talking();

    if (troop_type != m_troop_type) {
      m_troop_type = troop_type;
      m_scene_dirty = true;
    }
    if (pose != m_pose) {
      m_pose = pose;
      m_pose_dirty = true;
    }
  }

  void render() override;

private:
  void release_scene();
  auto ensure_scene() -> bool;
  void apply_pose();

  void submit_face(const QMatrix4x4& head_world, float delta);
  [[nodiscard]] auto advance_focus(float delta) -> QVector3D;

  QSize m_size;
  QVector3D m_focus{0.0F, k_default_focus_height, 0.0F};
  QVector3D m_focus_target{0.0F, k_default_focus_height, 0.0F};
  bool m_focus_settled = false;
  QString m_troop_type;
  QString m_nation;
  QString m_pose;
  bool m_speaking = false;
  bool m_talking = false;
  bool m_scene_dirty = true;
  bool m_pose_dirty = true;

  float m_expression_time = 0.0F;
  float m_mouth_open = 0.0F;

  Render::GL::Renderer* m_renderer = nullptr;
  Render::GL::Camera* m_camera = nullptr;
  Engine::Core::World* m_world = nullptr;
  Engine::Core::EntityID m_entity = 0;
  bool m_renderer_failed = false;

  std::chrono::steady_clock::time_point m_last_frame{};
};

void CommanderPortraitView::PortraitRenderer::release_scene() {
  m_focus = QVector3D(0.0F, k_default_focus_height, 0.0F);
  m_focus_target = m_focus;
  m_focus_settled = false;
  m_world = nullptr;
  m_entity = 0;
  m_renderer = nullptr;
  m_camera = nullptr;
}

auto CommanderPortraitView::PortraitRenderer::ensure_scene() -> bool {
  if (m_renderer_failed) {
    return false;
  }
  if (m_scene_dirty) {
    m_entity = 0;
    m_world = nullptr;
  }

  auto& scenes = UI::CommanderPortraitScenes::instance();
  m_renderer = scenes.renderer();
  m_camera = scenes.camera();
  if (m_renderer == nullptr || m_camera == nullptr) {
    m_renderer_failed = true;
    return false;
  }

  if (m_world == nullptr) {
    const auto scene = scenes.acquire(m_troop_type);
    if (scene.world == nullptr) {
      return false;
    }
    m_world = scene.world;
    m_entity = scene.entity;
    m_pose_dirty = true;
  }

  m_scene_dirty = false;
  return true;
}

void CommanderPortraitView::PortraitRenderer::apply_pose() {
  if (!m_pose_dirty || m_world == nullptr) {
    return;
  }
  auto* entity = m_world->get_entity(m_entity);
  if (entity == nullptr) {
    return;
  }
  auto* routine = entity->get_component<Engine::Core::ShowcaseRoutineComponent>();
  if (routine == nullptr) {
    return;
  }

  const auto move = move_for_pose(m_pose);
  routine->steps.clear();

  if (move != Animation::HumanoidShowcaseMove::TauntCynical) {
    routine->steps.push_back({.move = static_cast<std::uint8_t>(move),
                              .duration = 0.0F,
                              .hold_after = 0.0F});
  }
  routine->steps.push_back(
      {.move = static_cast<std::uint8_t>(Animation::HumanoidShowcaseMove::TauntCynical),
       .duration = 0.0F,
       .hold_after = 0.0F});

  routine->index = 0;
  routine->elapsed = 0.0F;
  routine->phase = 0.0F;
  routine->finished = false;
  routine->active = true;
  routine->loop = true;
  routine->loop_from = routine->steps.size() - 1U;
  m_pose_dirty = false;
}

void CommanderPortraitView::PortraitRenderer::submit_face(const QMatrix4x4& head_world,
                                                          float delta) {
  if (m_renderer == nullptr) {
    return;
  }

  m_expression_time += std::max(0.0F, delta);
  float mouth_target = 0.0F;
  if (m_talking) {
    const float syllable = 0.5F + 0.5F * std::sin(m_expression_time * 24.0F);
    const float cadence = 0.5F + 0.5F * std::sin(m_expression_time * 11.0F + 0.8F);
    mouth_target = 0.18F + 0.82F * syllable * (0.55F + 0.45F * cadence);
  }
  const float mouth_tau = m_talking ? 0.035F : 0.070F;
  const float mouth_alpha = delta > 0.0F ? 1.0F - std::exp(-delta / mouth_tau) : 1.0F;
  m_mouth_open += (mouth_target - m_mouth_open) * mouth_alpha;

  float blink = 0.0F;
  const float blink_phase = std::fmod(m_expression_time, 3.7F);
  if (blink_phase > 3.52F) {
    blink = std::sin((blink_phase - 3.52F) / 0.18F * 3.14159265F);
  }
  const float lid_scale = std::max(0.12F, 1.0F - blink * 0.88F);

  auto* sphere = Render::GL::get_unit_sphere();
  if (sphere == nullptr) {
    return;
  }

  const QVector3D iris =
      m_nation == QStringLiteral("carthage") ? k_carthage_iris : k_roman_iris;
  for (float side : {-1.0F, 1.0F}) {
    const float eye_x = side * k_head_radius * 0.48F;
    m_renderer->mesh(
        sphere,
        face_feature_model(head_world,
                           QVector3D(eye_x, k_head_radius * 0.15F, k_face_surface),
                           QVector3D(k_head_radius * 0.27F,
                                     k_head_radius * 0.15F * lid_scale,
                                     k_head_radius * 0.045F)),
        k_eye_white);
    m_renderer->mesh(
        sphere,
        face_feature_model(head_world,
                           QVector3D(eye_x,
                                     k_head_radius * 0.15F,
                                     k_face_surface + k_head_radius * 0.045F),
                           QVector3D(k_head_radius * 0.105F,
                                     k_head_radius * 0.12F * lid_scale,
                                     k_head_radius * 0.024F)),
        iris);
    m_renderer->mesh(
        sphere,
        face_feature_model(head_world,
                           QVector3D(eye_x,
                                     k_head_radius * 0.15F,
                                     k_face_surface + k_head_radius * 0.070F),
                           QVector3D(k_head_radius * 0.045F,
                                     k_head_radius * 0.060F * lid_scale,
                                     k_head_radius * 0.014F)),
        k_pupil);

    const float brow_roll =
        side * (m_pose == QStringLiteral("dismissive") ? 8.0F : -5.0F);
    m_renderer->mesh(
        sphere,
        face_feature_model(
            head_world,
            QVector3D(
                eye_x, k_head_radius * 0.43F, k_face_surface + k_head_radius * 0.015F),
            QVector3D(
                k_head_radius * 0.31F, k_head_radius * 0.055F, k_head_radius * 0.032F),
            brow_roll),
        k_brow);
  }

  const float mouth_height =
      k_head_radius * (0.025F + 0.14F * std::clamp(m_mouth_open, 0.0F, 1.0F));
  m_renderer->mesh(
      sphere,
      face_feature_model(
          head_world,
          QVector3D(0.0F, -k_head_radius * 0.43F, k_mouth_surface),
          QVector3D(k_head_radius * 0.39F, mouth_height, k_head_radius * 0.035F)),
      k_mouth);
  m_renderer->mesh(
      sphere,
      face_feature_model(head_world,
                         QVector3D(0.0F,
                                   -k_head_radius * (0.46F + 0.10F * m_mouth_open),
                                   k_mouth_surface + k_head_radius * 0.040F),
                         QVector3D(k_head_radius * 0.34F,
                                   k_head_radius * 0.035F,
                                   k_head_radius * 0.020F)),
      k_lip);
}

auto CommanderPortraitView::PortraitRenderer::advance_focus(float delta) -> QVector3D {
  if (!m_focus_settled) {
    m_focus = m_focus_target;
    m_focus_settled = true;
    return m_focus;
  }
  float const alpha = delta > 0.0F ? 1.0F - std::exp(-delta / k_focus_tau) : 0.0F;
  m_focus += (m_focus_target - m_focus) * alpha;
  return m_focus;
}

void CommanderPortraitView::PortraitRenderer::render() {
  const auto now = std::chrono::steady_clock::now();
  float delta = 0.0F;
  if (m_last_frame.time_since_epoch().count() != 0) {
    delta = std::min(k_max_frame_seconds,
                     std::chrono::duration<float>(now - m_last_frame).count());
  }
  m_last_frame = now;

  if (!m_speaking) {
    m_expression_time = 0.0F;
    m_mouth_open = 0.0F;
    return;
  }

  Render::Creature::RuntimeBakeAllowScope const allow_bakes;

  if (!ensure_scene()) {
    return;
  }
  apply_pose();

  m_world->update(delta);

  const float aspect = m_size.height() > 0 ? static_cast<float>(m_size.width()) /
                                                 static_cast<float>(m_size.height())
                                           : 1.0F;
  const QVector3D focus = advance_focus(delta);
  m_camera->look_at(focus + (k_bust_direction.normalized() * k_bust_distance),
                    focus,
                    QVector3D(0.0F, 1.0F, 0.0F));
  m_camera->set_perspective(k_field_of_view, aspect, 0.05F, 40.0F);

  m_renderer->set_camera(m_camera);
  m_renderer->set_viewport(m_size.width(), m_size.height());
  m_renderer->set_environment_lighting(portrait_lighting());

  m_renderer->set_clear_color(0.055F, 0.048F, 0.042F, 1.0F);
  m_renderer->set_local_owner_id(1);
  m_renderer->set_force_full_creature_lod(true);
  m_renderer->update_animation_time(delta);
  Render::Creature::Pipeline::BoneProbe head_probe{};
  head_probe.entity_id = static_cast<std::uint32_t>(m_entity);
  head_probe.instance_index = 0U;
  head_probe.bone_index =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head);

  m_renderer->begin_frame();
  {
    Render::Creature::Pipeline::ScopedBoneProbe const probe_scope(&head_probe);
    m_renderer->render_world(m_world);
  }

  if (head_probe.resolved) {
    submit_face(head_probe.world, delta);
    QVector3D const head = head_probe.world.map(QVector3D(0.0F, k_cranium_rise, 0.0F));
    m_focus_target = QVector3D(head.x(), head.y() - k_bust_drop, head.z());
  }
  m_renderer->end_frame();

  update();
}

CommanderPortraitView::CommanderPortraitView() {
  setMirrorVertically(true);
  setTextureFollowsItemSize(true);
}

CommanderPortraitView::~CommanderPortraitView() = default;

auto CommanderPortraitView::createRenderer() const
    -> QQuickFramebufferObject::Renderer* {
  return new PortraitRenderer();
}

void CommanderPortraitView::set_troop_type(const QString& value) {
  if (m_troop_type == value) {
    return;
  }
  m_troop_type = value;
  emit troop_type_changed();
  update();
}

void CommanderPortraitView::set_nation(const QString& value) {
  if (m_nation == value) {
    return;
  }
  m_nation = value;
  emit nation_changed();
  update();
}

void CommanderPortraitView::set_pose(const QString& value) {
  if (m_pose == value) {
    return;
  }
  m_pose = value;
  emit pose_changed();
  update();
}

void CommanderPortraitView::set_speaking(bool value) {
  if (m_speaking == value) {
    return;
  }
  m_speaking = value;
  emit speaking_changed();
  update();
}

void CommanderPortraitView::set_talking(bool value) {
  if (m_talking == value) {
    return;
  }
  m_talking = value;
  emit talking_changed();
  update();
}
