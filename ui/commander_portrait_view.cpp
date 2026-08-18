#include "commander_portrait_view.h"

#include <QMatrix4x4>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QPointF>
#include <QVector3D>
#include <QtMath>

#include <algorithm>
#include <chrono>
#include <cmath>

#include "animation/rig/humanoid_proportions.h"
#include "animation/showcase_pose_manifest.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/systems/nation_id.h"
#include "game/systems/showcase_routine_system.h"
#include "game/units/factory.h"
#include "game/units/troop_type.h"
#include "game/units/unit.h"
#include "render/creature/pipeline/creature_bone_probe.h"
#include "render/graphics_settings.h"
#include "render/humanoid/skeleton.h"
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

constexpr float k_body_yaw_degrees = 14.0F;

constexpr float k_max_frame_seconds = 0.1F;

constexpr float k_head_radius = Render::GL::HumanProportions::HEAD_RADIUS;
constexpr float k_cranium_rise = k_head_radius * 0.06F;

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
  PortraitRenderer() = default;
  ~PortraitRenderer() override { release_scene(); }

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
    m_item = view;
    const QString troop_type = view->troop_type();
    const QString pose = view->pose();
    m_speaking = view->speaking();

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

  void measure_face_anchor(const QMatrix4x4& head_world);
  void post_face_anchor();
  [[nodiscard]] auto advance_focus(float delta) -> QVector3D;

  QSize m_size;
  CommanderPortraitView* m_item = nullptr;
  CommanderPortraitView::FaceAnchor m_face{};
  CommanderPortraitView::FaceAnchor m_published{};
  QVector3D m_focus{0.0F, k_default_focus_height, 0.0F};
  QVector3D m_focus_target{0.0F, k_default_focus_height, 0.0F};
  bool m_focus_settled = false;
  QString m_troop_type;
  QString m_pose;
  bool m_speaking = false;
  bool m_scene_dirty = true;
  bool m_pose_dirty = true;

  std::unique_ptr<Render::GL::Renderer> m_renderer;
  std::unique_ptr<Render::GL::Camera> m_camera;
  std::unique_ptr<Engine::Core::World> m_world;
  std::unique_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<Game::Units::Unit> m_unit;
  Engine::Core::EntityID m_entity = 0;
  bool m_renderer_failed = false;

  std::chrono::steady_clock::time_point m_last_frame{};
};

void CommanderPortraitView::PortraitRenderer::release_scene() {
  m_focus = QVector3D(0.0F, k_default_focus_height, 0.0F);
  m_focus_target = m_focus;
  m_focus_settled = false;
  m_unit.reset();
  m_world.reset();
  m_factory.reset();
  m_entity = 0;
  if (m_renderer != nullptr) {
    m_renderer->shutdown();
    m_renderer.reset();
  }
  m_camera.reset();
}

auto CommanderPortraitView::PortraitRenderer::ensure_scene() -> bool {
  if (m_renderer_failed) {
    return false;
  }
  if (m_scene_dirty) {
    m_unit.reset();
    m_entity = 0;
    m_world.reset();
  }

  Game::Units::TroopType troop_type{};
  if (!Game::Units::try_parse_troop_type(m_troop_type, troop_type)) {
    return false;
  }

  if (m_renderer == nullptr) {
    m_renderer = std::make_unique<Render::GL::Renderer>(
        Render::GraphicsSettings::instance().features().shader_quality);
    if (!m_renderer->initialize()) {
      m_renderer.reset();
      m_renderer_failed = true;
      return false;
    }
    m_camera = std::make_unique<Render::GL::Camera>();
  }

  if (m_world == nullptr) {

    m_world = std::make_unique<Engine::Core::World>();
    m_world->add_system(std::make_unique<Game::Systems::ShowcaseRoutineSystem>());

    m_factory = std::make_unique<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);

    Game::Units::SpawnParams params;
    params.position = QVector3D(0.0F, 0.0F, 0.0F);
    params.player_id = 1;
    params.spawn_type = Game::Units::spawn_typeFromTroopType(troop_type);
    params.ai_controlled = false;
    params.enables_production = false;

    m_unit = m_factory->create(troop_type, *m_world, params);
    if (m_unit == nullptr) {
      return false;
    }
    m_entity = m_unit->id();

    auto* entity = m_world->get_entity(m_entity);
    if (entity == nullptr) {
      return false;
    }
    if (auto* transform = entity->get_component<Engine::Core::TransformComponent>()) {
      transform->rotation.y = k_body_yaw_degrees;
      transform->desired_yaw = k_body_yaw_degrees;
      transform->has_desired_yaw = true;
    }
    entity->add_component<Engine::Core::ShowcaseRoutineComponent>();
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

void CommanderPortraitView::PortraitRenderer::measure_face_anchor(
    const QMatrix4x4& head_world) {
  m_face = {};
  if (m_camera == nullptr || m_size.width() <= 0 || m_size.height() <= 0) {
    return;
  }

  const auto width = static_cast<qreal>(m_size.width());
  const auto height = static_cast<qreal>(m_size.height());

  const QVector3D centre = head_world.map(QVector3D(0.0F, k_cranium_rise, 0.0F));
  const QVector3D above =
      head_world.map(QVector3D(0.0F, k_cranium_rise + k_head_radius, 0.0F));

  QPointF centre_screen;
  QPointF above_screen;
  if (!m_camera->world_to_screen(centre, width, height, centre_screen) ||
      !m_camera->world_to_screen(above, width, height, above_screen)) {
    return;
  }

  const qreal dx = above_screen.x() - centre_screen.x();
  const qreal dy = above_screen.y() - centre_screen.y();
  const qreal up_length = std::hypot(dx, dy);
  if (!(up_length > 0.5)) {
    return;
  }

  const QVector3D origin = head_world.map(QVector3D(0.0F, 0.0F, 0.0F));
  auto axis = [&](const QVector3D& local) {
    QVector3D const world = head_world.map(local) - origin;
    return world.lengthSquared() > 1.0e-12F ? world.normalized() : world;
  };
  const QVector3D head_right = axis(QVector3D(1.0F, 0.0F, 0.0F));
  const QVector3D head_up = axis(QVector3D(0.0F, 1.0F, 0.0F));
  const QVector3D head_forward = axis(QVector3D(0.0F, 0.0F, 1.0F));

  QVector3D view_dir = centre - m_camera->get_position();
  if (view_dir.lengthSquared() <= 1.0e-12F) {
    return;
  }
  view_dir.normalize();

  m_face.valid = true;
  m_face.x = centre_screen.x() / width;
  m_face.y = centre_screen.y() / height;
  m_face.radius = up_length / height;

  m_face.roll = qRadiansToDegrees(std::atan2(dx, -dy));
  m_face.turn = QVector3D::dotProduct(head_right, view_dir);
  m_face.tilt = QVector3D::dotProduct(head_up, view_dir);
  m_face.facing = -QVector3D::dotProduct(head_forward, view_dir);
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

void CommanderPortraitView::PortraitRenderer::post_face_anchor() {
  if (m_item == nullptr) {
    return;
  }

  constexpr qreal k_epsilon = 1.0e-4;
  auto same = [](qreal lhs, qreal rhs) {
    return std::abs(lhs - rhs) < k_epsilon;
  };
  if (m_published.valid == m_face.valid && same(m_published.x, m_face.x) &&
      same(m_published.y, m_face.y) && same(m_published.radius, m_face.radius) &&
      same(m_published.roll, m_face.roll) && same(m_published.turn, m_face.turn) &&
      same(m_published.tilt, m_face.tilt) && same(m_published.facing, m_face.facing)) {
    return;
  }
  m_published = m_face;

  auto* item = m_item;
  const auto anchor = m_face;
  QMetaObject::invokeMethod(
      item,
      [item, anchor]() { item->publish_face_anchor(anchor); },
      Qt::QueuedConnection);
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

    m_face = {};
    post_face_anchor();
    release_scene();
    return;
  }

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

  m_renderer->set_camera(m_camera.get());
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
    m_renderer->render_world(m_world.get());
  }
  m_renderer->end_frame();

  if (head_probe.resolved) {
    measure_face_anchor(head_probe.world);
  } else {
    m_face = {};
  }
  if (head_probe.resolved) {
    QVector3D const head = head_probe.world.map(QVector3D(0.0F, k_cranium_rise, 0.0F));
    m_focus_target = QVector3D(head.x(), head.y() - k_bust_drop, head.z());
  }
  post_face_anchor();

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

void CommanderPortraitView::publish_face_anchor(const FaceAnchor& anchor) {
  constexpr qreal k_epsilon = 1.0e-4;
  auto same = [](qreal lhs, qreal rhs) {
    return std::abs(lhs - rhs) < k_epsilon;
  };

  if (m_face.valid == anchor.valid && same(m_face.x, anchor.x) &&
      same(m_face.y, anchor.y) && same(m_face.radius, anchor.radius) &&
      same(m_face.roll, anchor.roll) && same(m_face.turn, anchor.turn) &&
      same(m_face.tilt, anchor.tilt) && same(m_face.facing, anchor.facing)) {
    return;
  }
  m_face = anchor;
  emit face_anchor_changed();
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
