#pragma once

#include <QPoint>
#include <Qt>

class QQuickWindow;

namespace Engine::Core {
class Entity;
class World;
using EntityID = unsigned int;
} // namespace Engine::Core

namespace Render::GL {
class Camera;
}

class CommanderControlController {
public:
  struct InputState {
    bool forward = false;
    bool backward = false;
    bool left = false;
    bool right = false;
    bool turn_left = false;
    bool turn_right = false;
    bool run = false;
    bool primary_action = false;
    bool secondary_action = false;
    float primary_action_scan_cooldown = 0.0F;
  };

  void reset();
  void set_view_yaw(float yaw);
  void set_view_pitch(float pitch);
  [[nodiscard]] float view_yaw() const;
  [[nodiscard]] float view_pitch() const;
  [[nodiscard]] InputState &input();
  [[nodiscard]] const InputState &input() const;

  void key_down(int key);
  void key_up(int key);
  void primary_action_down();
  void primary_action_up();
  void secondary_action_down();
  void secondary_action_up();
  void mouse_move(qreal dx, qreal dy);
  void mouse_look_at(qreal sx, qreal sy, qreal center_sx, qreal center_sy,
                     QQuickWindow *window);
  void center_mouse(qreal center_sx, qreal center_sy, QQuickWindow *window);
  void poll_mouse_look(QQuickWindow *window);
  [[nodiscard]] Engine::Core::Entity *
  controlled_commander(Engine::Core::World &world,
                       Engine::Core::EntityID commander_id,
                       int local_owner_id) const;
  [[nodiscard]] Engine::Core::EntityID
  find_primary_target(Engine::Core::World &world,
                      Engine::Core::EntityID commander_id,
                      int local_owner_id) const;
  [[nodiscard]] bool primary_action(Engine::Core::World &world,
                                    Engine::Core::EntityID commander_id,
                                    int local_owner_id);
  void release_guard(Engine::Core::World &world,
                     Engine::Core::EntityID commander_id, int local_owner_id);
  [[nodiscard]] bool update(Engine::Core::World &world,
                            Engine::Core::EntityID commander_id,
                            int local_owner_id, Render::GL::Camera &camera,
                            float dt);

private:
  void update_camera(Engine::Core::Entity &commander,
                     Render::GL::Camera &camera);
  InputState m_input;
  float m_view_yaw = 0.0F;
  float m_view_pitch = 0.0F;
  QPoint m_mouse_center;
  bool m_mouse_center_valid = false;
  QPoint m_last_mouse_global;
  bool m_last_mouse_valid = false;
  bool m_mouse_warp_supported = false;
  bool m_mouse_recentering = false;
};
