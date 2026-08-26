#ifndef SOI_UI_PREFERENCES_H
#define SOI_UI_PREFERENCES_H

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

#include "../game/render_bridge/camera_speeds.h"

class UiPreferences : public QObject {
  Q_OBJECT

  Q_PROPERTY(qreal uiScale READ ui_scale WRITE set_ui_scale NOTIFY ui_scale_changed)
  Q_PROPERTY(bool reducedMotion READ reduced_motion WRITE set_reduced_motion NOTIFY
                 reduced_motion_changed)
  Q_PROPERTY(bool highContrast READ high_contrast WRITE set_high_contrast NOTIFY
                 high_contrast_changed)
  Q_PROPERTY(QString colorVisionMode READ color_vision_mode WRITE set_color_vision_mode
                 NOTIFY color_vision_mode_changed)
  Q_PROPERTY(bool alwaysShowFocus READ always_show_focus WRITE set_always_show_focus
                 NOTIFY always_show_focus_changed)
  Q_PROPERTY(bool teamPatterns READ team_patterns WRITE set_team_patterns NOTIFY
                 team_patterns_changed)
  Q_PROPERTY(bool effectiveTeamPatterns READ effective_team_patterns NOTIFY
                 team_patterns_changed)
  Q_PROPERTY(bool edgeScrollEnabled READ edge_scroll_enabled WRITE
                 set_edge_scroll_enabled NOTIFY edge_scroll_enabled_changed)
  Q_PROPERTY(qreal edgeScrollSensitivity READ edge_scroll_sensitivity WRITE
                 set_edge_scroll_sensitivity NOTIFY edge_scroll_sensitivity_changed)
  Q_PROPERTY(qreal cameraMotionScale READ camera_motion_scale WRITE
                 set_camera_motion_scale NOTIFY camera_motion_scale_changed)
  Q_PROPERTY(bool damageNumbers READ damage_numbers WRITE set_damage_numbers NOTIFY
                 damage_numbers_changed)
  Q_PROPERTY(QString damageNumberMode READ damage_number_mode WRITE
                 set_damage_number_mode NOTIFY damage_number_mode_changed)
  Q_PROPERTY(QStringList damageNumberModes READ damage_number_modes CONSTANT)
  Q_PROPERTY(bool cameraLegendSeen READ camera_legend_seen WRITE set_camera_legend_seen
                 NOTIFY camera_legend_seen_changed)
  Q_PROPERTY(bool tutorialCompleted READ tutorial_completed WRITE set_tutorial_completed
                 NOTIFY tutorial_completed_changed)
  Q_PROPERTY(qreal screenEffectIntensity READ screen_effect_intensity WRITE
                 set_screen_effect_intensity NOTIFY screen_effect_intensity_changed)
  Q_PROPERTY(qreal commanderLookSensitivityX READ commander_look_sensitivity_x WRITE
                 set_commander_look_sensitivity_x NOTIFY commander_input_changed)
  Q_PROPERTY(qreal commanderLookSensitivityY READ commander_look_sensitivity_y WRITE
                 set_commander_look_sensitivity_y NOTIFY commander_input_changed)
  Q_PROPERTY(bool commanderInvertLookY READ commander_invert_look_y WRITE
                 set_commander_invert_look_y NOTIFY commander_input_changed)
  Q_PROPERTY(bool commanderCameraImpulse READ commander_camera_impulse WRITE
                 set_commander_camera_impulse NOTIFY commander_input_changed)
  Q_PROPERTY(bool commanderHeadBob READ commander_head_bob WRITE set_commander_head_bob
                 NOTIFY commander_input_changed)
  Q_PROPERTY(qreal commanderFieldOfViewScale READ commander_field_of_view_scale WRITE
                 set_commander_field_of_view_scale NOTIFY commander_input_changed)
  Q_PROPERTY(bool commanderGuardIsToggle READ commander_guard_is_toggle WRITE
                 set_commander_guard_is_toggle NOTIFY commander_input_changed)
  Q_PROPERTY(QStringList colorVisionModes READ color_vision_modes CONSTANT)
  Q_PROPERTY(QString displayWindowMode READ display_window_mode WRITE
                 set_display_window_mode NOTIFY display_window_mode_changed)
  Q_PROPERTY(QStringList displayWindowModes READ display_window_modes CONSTANT)
  Q_PROPERTY(bool displayVsync READ display_vsync WRITE set_display_vsync NOTIFY
                 display_vsync_changed)
  Q_PROPERTY(bool showFps READ show_fps WRITE set_show_fps NOTIFY show_fps_changed)
  Q_PROPERTY(qreal cameraPanSpeed READ camera_pan_speed WRITE set_camera_pan_speed
                 NOTIFY camera_speeds_changed)
  Q_PROPERTY(qreal cameraZoomSpeed READ camera_zoom_speed WRITE set_camera_zoom_speed
                 NOTIFY camera_speeds_changed)
  Q_PROPERTY(qreal cameraRotationSpeed READ camera_rotation_speed WRITE
                 set_camera_rotation_speed NOTIFY camera_speeds_changed)
  Q_PROPERTY(qreal minUiScale READ min_ui_scale CONSTANT)
  Q_PROPERTY(qreal maxUiScale READ max_ui_scale CONSTANT)
  Q_PROPERTY(qreal minEdgeScrollSensitivity READ min_edge_scroll_sensitivity CONSTANT)
  Q_PROPERTY(qreal maxEdgeScrollSensitivity READ max_edge_scroll_sensitivity CONSTANT)
  Q_PROPERTY(qreal minCameraSpeedScale READ min_camera_speed_scale CONSTANT)
  Q_PROPERTY(qreal maxCameraSpeedScale READ max_camera_speed_scale CONSTANT)

public:
  static auto instance() -> UiPreferences*;
  static auto create(QQmlEngine* engine, QJSEngine* scriptEngine) -> UiPreferences*;

  [[nodiscard]] auto ui_scale() const -> qreal { return m_ui_scale; }
  [[nodiscard]] auto reduced_motion() const -> bool { return m_reduced_motion; }
  [[nodiscard]] auto high_contrast() const -> bool { return m_high_contrast; }
  [[nodiscard]] auto color_vision_mode() const -> QString {
    return m_color_vision_mode;
  }
  [[nodiscard]] auto always_show_focus() const -> bool { return m_always_show_focus; }
  [[nodiscard]] auto team_patterns() const -> bool { return m_team_patterns; }
  [[nodiscard]] auto edge_scroll_enabled() const -> bool {
    return m_edge_scroll_enabled;
  }
  [[nodiscard]] auto edge_scroll_sensitivity() const -> qreal {
    return m_edge_scroll_sensitivity;
  }
  [[nodiscard]] auto camera_motion_scale() const -> qreal {
    return m_camera_motion_scale;
  }
  [[nodiscard]] auto damage_numbers() const -> bool { return m_damage_numbers; }
  [[nodiscard]] auto damage_number_mode() const -> QString {
    return m_damage_number_mode;
  }
  [[nodiscard]] static auto damage_number_modes() -> QStringList {
    return {QStringLiteral("off"), QStringLiteral("important"), QStringLiteral("all")};
  }
  [[nodiscard]] auto camera_legend_seen() const -> bool { return m_camera_legend_seen; }
  [[nodiscard]] auto tutorial_completed() const -> bool { return m_tutorial_completed; }
  [[nodiscard]] auto screen_effect_intensity() const -> qreal {
    return m_screen_effect_intensity;
  }

  [[nodiscard]] auto commander_look_sensitivity_x() const -> qreal {
    return m_commander_look_sensitivity_x;
  }
  [[nodiscard]] auto commander_look_sensitivity_y() const -> qreal {
    return m_commander_look_sensitivity_y;
  }
  [[nodiscard]] auto commander_invert_look_y() const -> bool {
    return m_commander_invert_look_y;
  }
  [[nodiscard]] auto commander_camera_impulse() const -> bool {
    return m_commander_camera_impulse;
  }
  [[nodiscard]] auto commander_head_bob() const -> bool { return m_commander_head_bob; }
  [[nodiscard]] auto commander_field_of_view_scale() const -> qreal {
    return m_commander_field_of_view_scale;
  }
  [[nodiscard]] auto commander_guard_is_toggle() const -> bool {
    return m_commander_guard_is_toggle;
  }

  [[nodiscard]] auto display_window_mode() const -> QString {
    return m_display_window_mode;
  }
  [[nodiscard]] static auto display_window_modes() -> QStringList {
    return {QStringLiteral("fullscreen"),
            QStringLiteral("borderless"),
            QStringLiteral("windowed")};
  }
  [[nodiscard]] auto display_vsync() const -> bool { return m_display_vsync; }
  [[nodiscard]] auto show_fps() const -> bool { return m_show_fps; }
  [[nodiscard]] auto camera_pan_speed() const -> qreal { return m_camera_pan_speed; }
  [[nodiscard]] auto camera_zoom_speed() const -> qreal { return m_camera_zoom_speed; }
  [[nodiscard]] auto camera_rotation_speed() const -> qreal {
    return m_camera_rotation_speed;
  }

  [[nodiscard]] auto effective_team_patterns() const -> bool;

  [[nodiscard]] static auto color_vision_modes() -> QStringList;
  [[nodiscard]] static auto min_ui_scale() -> qreal;
  [[nodiscard]] static auto max_ui_scale() -> qreal;
  [[nodiscard]] static auto min_edge_scroll_sensitivity() -> qreal;
  [[nodiscard]] static auto max_edge_scroll_sensitivity() -> qreal;
  [[nodiscard]] static auto min_camera_speed_scale() -> qreal {
    return Game::Systems::CameraSpeeds::k_min_scale;
  }
  [[nodiscard]] static auto max_camera_speed_scale() -> qreal {
    return Game::Systems::CameraSpeeds::k_max_scale;
  }

  void set_ui_scale(qreal scale);
  void set_reduced_motion(bool enabled);
  void set_high_contrast(bool enabled);
  void set_color_vision_mode(const QString& mode);
  void set_always_show_focus(bool enabled);
  void set_team_patterns(bool enabled);
  void set_edge_scroll_enabled(bool enabled);
  void set_edge_scroll_sensitivity(qreal sensitivity);
  void set_camera_motion_scale(qreal scale);
  void set_damage_numbers(bool enabled);
  void set_damage_number_mode(const QString& mode);
  void set_camera_legend_seen(bool seen);
  void set_tutorial_completed(bool completed);
  void set_screen_effect_intensity(qreal intensity);
  void set_commander_look_sensitivity_x(qreal scale);
  void set_commander_look_sensitivity_y(qreal scale);
  void set_commander_invert_look_y(bool enabled);
  void set_commander_camera_impulse(bool enabled);
  void set_commander_head_bob(bool enabled);
  void set_commander_field_of_view_scale(qreal scale);
  void set_commander_guard_is_toggle(bool enabled);
  void set_display_window_mode(const QString& mode);
  void set_display_vsync(bool enabled);
  void set_show_fps(bool enabled);
  void set_camera_pan_speed(qreal scale);
  void set_camera_zoom_speed(qreal scale);
  void set_camera_rotation_speed(qreal scale);

  Q_INVOKABLE void reset_to_defaults();

signals:
  void ui_scale_changed();
  void reduced_motion_changed();
  void high_contrast_changed();
  void color_vision_mode_changed();
  void always_show_focus_changed();
  void team_patterns_changed();
  void edge_scroll_enabled_changed();
  void edge_scroll_sensitivity_changed();
  void camera_motion_scale_changed();
  void damage_numbers_changed();
  void damage_number_mode_changed();
  void camera_legend_seen_changed();
  void tutorial_completed_changed();
  void screen_effect_intensity_changed();
  void commander_input_changed();
  void display_window_mode_changed();
  void display_vsync_changed();
  void show_fps_changed();
  void camera_speeds_changed();

private:
  explicit UiPreferences(QObject* parent = nullptr);
  void publish_commander_input_settings() const;
  void publish_camera_speeds() const;

  static UiPreferences* m_instance;

  qreal m_ui_scale;
  bool m_reduced_motion;
  bool m_high_contrast;
  QString m_color_vision_mode;
  bool m_always_show_focus;
  bool m_team_patterns;
  bool m_edge_scroll_enabled;
  qreal m_edge_scroll_sensitivity;
  qreal m_camera_motion_scale;
  qreal m_commander_look_sensitivity_x;
  qreal m_commander_look_sensitivity_y;
  bool m_commander_invert_look_y;
  bool m_commander_camera_impulse;
  bool m_commander_head_bob;
  qreal m_commander_field_of_view_scale;
  bool m_commander_guard_is_toggle;
  bool m_damage_numbers;
  QString m_damage_number_mode;
  bool m_camera_legend_seen;
  bool m_tutorial_completed;
  qreal m_screen_effect_intensity;
  QString m_display_window_mode;
  bool m_display_vsync;
  bool m_show_fps;
  qreal m_camera_pan_speed;
  qreal m_camera_zoom_speed;
  qreal m_camera_rotation_speed;
};

#endif
