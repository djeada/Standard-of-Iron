#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector3D>

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "app/commander/commander_control_controller.h"
#include "app/commander/commander_mode_coordinator.h"
#include "app/commander/rts_camera_bookmark.h"
#include "app/core/frame_snapshot.h"
#include "render/entity/combat_dust_renderer.h"

namespace Engine::Core {
class Entity;
class CombatHitEvent;
} // namespace Engine::Core

namespace App::Core {
struct ClientContext;
class ClientHost;
class CommanderModeCoordinator;
} // namespace App::Core

namespace App::ViewModels {

class CameraViewModel;
class PlacementViewModel;

class CommanderViewModel : public QObject {
  Q_OBJECT

  Q_PROPERTY(QString control_mode READ control_mode NOTIFY control_mode_changed)
  Q_PROPERTY(QString game_mode READ game_mode NOTIFY game_mode_changed)
  Q_PROPERTY(bool available READ available NOTIFY available_changed)
  Q_PROPERTY(QString mode_state READ mode_state NOTIFY mode_state_changed)

public:
  CommanderViewModel(const App::Core::ClientContext& context,
                     App::Core::ClientHost& host,
                     CameraViewModel& camera,
                     PlacementViewModel& placement,
                     QObject* parent = nullptr);
  ~CommanderViewModel() override;

  Q_INVOKABLE void toggle_mode();
  Q_INVOKABLE void enter_mode();
  Q_INVOKABLE void exit_mode();
  [[nodiscard]] auto control_mode() const -> QString;
  [[nodiscard]] auto game_mode() const -> QString;
  [[nodiscard]] auto mode_state() const -> QString;
  [[nodiscard]] auto available() const -> bool;
  [[nodiscard]] auto active() const -> bool {
    return m_control_mode == PlayerControlMode::Commander;
  }
  [[nodiscard]] auto rpg_mode() const -> bool { return m_game_mode == GameMode::Rpg; }

  Q_INVOKABLE void key_down(int key, int modifiers = 0);
  Q_INVOKABLE void key_up(int key, int modifiers = 0);
  Q_INVOKABLE void primary_action_down();
  Q_INVOKABLE void primary_action_up();
  Q_INVOKABLE void secondary_action_down();
  Q_INVOKABLE void secondary_action_up();
  Q_INVOKABLE void mouse_move(qreal dx, qreal dy);
  Q_INVOKABLE void mouse_look_at(qreal sx, qreal sy, qreal center_sx, qreal center_sy);
  Q_INVOKABLE void center_mouse(qreal center_sx, qreal center_sy);

  Q_INVOKABLE void trigger_aura();
  Q_INVOKABLE void trigger_rally();
  Q_INVOKABLE void dodge();
  Q_INVOKABLE void jump();
  Q_INVOKABLE void cycle_lock_on();
  Q_INVOKABLE void special_action();
  Q_INVOKABLE void vanguard_rush();
  Q_INVOKABLE void second_wind();
  Q_INVOKABLE void toggle_camera_mode();
  Q_INVOKABLE void toggle_weapon_stance();

  Q_INVOKABLE void start_flag_rally();
  Q_INVOKABLE void confirm_flag_rally(qreal sx, qreal sy);
  Q_INVOKABLE void cancel_flag_rally();
  Q_INVOKABLE void begin_barracks_rally();
  Q_INVOKABLE void confirm_barracks_rally(qreal sx, qreal sy);
  Q_INVOKABLE void cancel_barracks_rally();

  Q_INVOKABLE [[nodiscard]] QVariantMap status() const;

  void publish_frame();
  Q_INVOKABLE [[nodiscard]] QVariantList pop_damage_events();

  [[nodiscard]] auto controlled_commander_id() const -> Engine::Core::EntityID {
    return m_controlled_commander_id;
  }
  [[nodiscard]] auto is_placing_rally() const -> bool;
  [[nodiscard]] auto rally_preview_position() const -> const std::optional<QVector3D>& {
    return m_rally_preview;
  }

  void update_rally_preview_at(qreal sx, qreal sy);
  void seed_barracks_rally_preview_from_selection();

  [[nodiscard]] auto find_local_commander() const -> Engine::Core::Entity*;
  [[nodiscard]] auto
  should_render_selected_entity(Engine::Core::EntityID id) const -> bool;

  void update_control_mode(float dt);
  void sample_frame_intent();
  void update_camera_presentation(float dt);
  [[nodiscard]] auto frame_intent() const -> const CommanderFrameIntent& {
    return m_control.frame_intent();
  }

  void restore_direct_control_if_ready();
  void render_effects();
  [[nodiscard]] auto time_effect_scale(float scaled_dt, bool paused) -> float;

  [[nodiscard]] auto record_rpg_hit(const Engine::Core::CombatHitEvent& event) -> bool;
  void reset_for_new_match();
  void notify_availability_changed() { emit available_changed(); }

signals:
  void control_mode_changed();

  void game_mode_changed();
  void mode_state_changed();
  void available_changed();

  void active_camera_requested(Render::GL::Camera* camera);

  void rts_selection_restored();

private:
  enum class PlayerControlMode : std::uint8_t {
    Rts,
    Commander
  };
  enum class GameMode : std::uint8_t {
    Rts,
    Rpg
  };

  struct ModeSignalBatch {
    QString control_mode;
    QString game_mode;
    QString mode_state;
  };
  [[nodiscard]] auto capture_mode_signals() const -> ModeSignalBatch;
  void bookmark_rts_camera();
  void restore_rts_camera();
  void emit_mode_signal_changes(const ModeSignalBatch& before);

  [[nodiscard]] auto controlled_commander_entity() const -> Engine::Core::Entity*;
  void store_rts_selection();
  void select_controlled_commander();
  void restore_rts_selection();
  void reset_input();
  void enter_rts_runtime_mode();
  void enter_commander_runtime_mode();
  void exit_commander_runtime_mode();
  void primary_action();
  void cancel_active_placement();
  void seed_flag_rally_preview_from_view_center();

  const App::Core::ClientContext& m_context;
  App::Core::ClientHost& m_host;
  CameraViewModel& m_camera;
  PlacementViewModel& m_placement;

  CommanderControlController m_control;
  std::unique_ptr<App::Core::CommanderModeCoordinator> m_mode;
  Render::GL::RpgTelegraphRenderer m_telegraphs;

  PlayerControlMode m_control_mode = PlayerControlMode::Rts;
  GameMode m_game_mode = GameMode::Rts;
  Engine::Core::EntityID m_controlled_commander_id = 0;
  std::vector<Engine::Core::EntityID> m_saved_rts_selection_ids;
  std::optional<bool> m_rts_follow_selection_snapshot;
  App::Core::RtsCameraBookmark m_rts_camera_bookmark;
  std::optional<QVector3D> m_rally_preview;

  struct DamageEvent {
    float wx = 0.0F;
    float wy = 0.0F;
    float wz = 0.0F;
    int damage = 0;
    float damage_ratio = 0.0F;
    int lane = 0;
    bool killing_blow = false;
  };
  static constexpr int k_max_damage_events = 96;
  App::Core::Published<QVariantMap> m_status;

  bool m_status_published_empty = false;

  mutable std::mutex m_damage_events_mutex;
  std::vector<DamageEvent> m_damage_events;
  std::uint32_t m_damage_event_sequence = 0;
  float m_hit_stop_timer = 0.0F;
  float m_hit_stop_total = 0.10F;
};

} // namespace App::ViewModels
