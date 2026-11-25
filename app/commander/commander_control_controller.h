#pragma once

#include <QPoint>
#include <QVector3D>
#include <Qt>

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "app/commander/commander_camera_rig.h"
#include "app/commander/commander_frame_intent.h"
#include "app/commander/commander_look.h"
#include "app/commander/commander_presentation.h"
#include "app/core/player_feedback.h"
#include "game/commander/commander_abilities.h"
#include "game/commander/commander_heading.h"
#include "game/commander/commander_input_port.h"

class QQuickWindow;

namespace Engine::Core {
class Entity;
class CommanderComponent;
class World;
class TransformComponent;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Render::GL {
class Camera;
}

namespace Game::Commander {
class CommanderRuntime;
} // namespace Game::Commander

using Game::Commander::k_commander_rest_view_pitch_degrees;

class CommanderControlController {
public:
  CommanderControlController();
  ~CommanderControlController();

  using InputState = Game::Commander::CommanderHeldInput;

  void reset();

  void release_for_rally();
  void release_all_input();
  void release_pointer();
  [[nodiscard]] auto pointer_captured() const -> bool;
  void set_view_yaw(float yaw);
  void set_view_pitch(float pitch);
  [[nodiscard]] float view_yaw() const;
  [[nodiscard]] float view_pitch() const;
  [[nodiscard]] InputState& input();
  [[nodiscard]] const InputState& input() const;

  void key_down(int key);
  [[nodiscard]] auto queued_intent_count(Engine::Core::World& world,
                                         Engine::Core::EntityID commander_id,
                                         int local_owner_id) const -> int;
  void key_up(int key);
  void primary_action_down();
  void primary_action_up();
  void request_heavy_action();
  void secondary_action_down();
  void secondary_action_up();
  void mouse_move(qreal dx, qreal dy);
  void mouse_look_at(
      qreal sx, qreal sy, qreal center_sx, qreal center_sy, QQuickWindow* window);
  void center_mouse(qreal center_sx, qreal center_sy, QQuickWindow* window);
  void poll_mouse_look(QQuickWindow* window);
  void set_latency_probe(Game::Commander::CommanderLatencyProbe* probe);
  void set_feedback_bus(App::Core::PlayerFeedbackBus* bus);
  auto sample_frame_intent(QQuickWindow* window) -> CommanderFrameIntent;
  [[nodiscard]] auto frame_intent() const -> const CommanderFrameIntent& {
    return m_look.frame_intent();
  }
  void request_dodge();
  void request_dodge(const QVector3D& world_direction);
  void request_jump();
  void special_action();
  void request_vanguard_rush();
  void request_second_wind();
  void toggle_close_camera_mode(Engine::Core::World& world,
                                Engine::Core::EntityID commander_id,
                                int local_owner_id) const;
  void toggle_weapon_stance(Engine::Core::World& world,
                            Engine::Core::EntityID commander_id,
                            int local_owner_id);
  void cycle_lock_on_target(Engine::Core::World& world,
                            Engine::Core::EntityID commander_id,
                            int local_owner_id);
  void toggle_lock_on_target(Engine::Core::World& world,
                             Engine::Core::EntityID commander_id,
                             int local_owner_id);
  void switch_lock_on_target(Engine::Core::World& world,
                             Engine::Core::EntityID commander_id,
                             int local_owner_id,
                             int direction);
  void note_refusal(const char* action);
  [[nodiscard]] auto last_refusal() const -> const Game::Commander::CommanderRefusal&;
  [[nodiscard]] Engine::Core::EntityID locked_target_id() const;
  [[nodiscard]] Engine::Core::EntityID focus_target_id() const;
  [[nodiscard]] bool is_dodge_rolling() const;
  [[nodiscard]] Engine::Core::Entity*
  controlled_commander(Engine::Core::World& world,
                       Engine::Core::EntityID commander_id,
                       int local_owner_id) const;

  [[nodiscard]] Engine::Core::EntityID
  find_primary_target(Engine::Core::World& world,
                      Engine::Core::EntityID commander_id,
                      int local_owner_id,
                      float extra_reach = 0.0F);
  [[nodiscard]] bool update(Engine::Core::World& world,
                            Engine::Core::EntityID commander_id,
                            int local_owner_id,
                            Render::GL::Camera& camera,
                            float dt);
  [[nodiscard]] bool update_simulation(Engine::Core::World& world,
                                       Engine::Core::EntityID commander_id,
                                       int local_owner_id,
                                       float dt);
  void update_camera_presentation(Engine::Core::World& world,
                                  Engine::Core::EntityID commander_id,
                                  Render::GL::Camera& camera,
                                  float dt);
  void snap_presentation_pose();
  [[nodiscard]] auto
  presentation_pose() const -> const Engine::Core::PresentationPose& {
    return m_presentation.pose();
  }

  void set_presentation_trace_enabled(bool enabled);
  [[nodiscard]] auto presentation_trace_enabled() const -> bool {
    return m_trace_enabled;
  }
  [[nodiscard]] auto
  presentation_trace() const -> const Game::Commander::CommanderPresentationTrace& {
    return m_trace;
  }
  [[nodiscard]] auto input_edges() const -> Game::Commander::CommanderInputTrace;
  [[nodiscard]] auto
  camera_trace() const -> const Game::Commander::CommanderCameraTrace& {
    return m_camera_rig.trace();
  }

  // The game command system owns the runtime for a match. Unbound preview
  // worlds use the same runtime with a directly supplied input frame.
  void bind_runtime(std::shared_ptr<Game::Commander::CommanderRuntime> runtime);
  void publish_runtime_tick(Engine::Core::World& world,
                            Engine::Core::EntityID commander_id,
                            float dt);

private:
  [[nodiscard]] auto capture_input(Engine::Core::EntityID commander_id)
      -> Game::Command::CommanderInputFrame;
  [[nodiscard]] auto update_impl(Engine::Core::World& world,
                                 Engine::Core::EntityID commander_id,
                                 int owner,
                                 Render::GL::Camera* camera,
                                 float dt) -> bool;
  void update_camera(Engine::Core::World& world,
                     Engine::Core::Entity& commander,
                     Render::GL::Camera& camera,
                     float dt);
  [[nodiscard]] auto look_sensitivity_scale() const -> float;

  Game::Commander::CommanderLatencyProbe* m_latency_probe = nullptr;
  Game::Core::PlayerFeedbackBus* m_feedback = nullptr;
  std::unique_ptr<Game::Commander::CommanderRuntime> m_preview_runtime;
  std::shared_ptr<Game::Commander::CommanderRuntime> m_match_runtime;
  Game::Commander::CommanderRuntime* m_runtime;
  Game::Commander::CommanderInputPort m_input_port;
  App::Core::CommanderLook m_look;
  App::Core::CommanderCameraRig m_camera_rig;
  App::Core::CommanderPresentation m_presentation;
  bool m_trace_enabled = false;
  Game::Commander::CommanderPresentationTrace m_trace;
};
