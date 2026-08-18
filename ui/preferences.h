#ifndef SOI_UI_PREFERENCES_H
#define SOI_UI_PREFERENCES_H

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

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
  Q_PROPERTY(bool cameraLegendSeen READ camera_legend_seen WRITE set_camera_legend_seen
                 NOTIFY camera_legend_seen_changed)
  Q_PROPERTY(bool tutorialCompleted READ tutorial_completed WRITE set_tutorial_completed
                 NOTIFY tutorial_completed_changed)
  Q_PROPERTY(qreal screenEffectIntensity READ screen_effect_intensity WRITE
                 set_screen_effect_intensity NOTIFY screen_effect_intensity_changed)
  Q_PROPERTY(QStringList colorVisionModes READ color_vision_modes CONSTANT)
  Q_PROPERTY(qreal minUiScale READ min_ui_scale CONSTANT)
  Q_PROPERTY(qreal maxUiScale READ max_ui_scale CONSTANT)
  Q_PROPERTY(qreal minEdgeScrollSensitivity READ min_edge_scroll_sensitivity CONSTANT)
  Q_PROPERTY(qreal maxEdgeScrollSensitivity READ max_edge_scroll_sensitivity CONSTANT)

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
  [[nodiscard]] auto camera_legend_seen() const -> bool { return m_camera_legend_seen; }
  [[nodiscard]] auto tutorial_completed() const -> bool { return m_tutorial_completed; }
  [[nodiscard]] auto screen_effect_intensity() const -> qreal {
    return m_screen_effect_intensity;
  }

  [[nodiscard]] auto effective_team_patterns() const -> bool;

  [[nodiscard]] static auto color_vision_modes() -> QStringList;
  [[nodiscard]] static auto min_ui_scale() -> qreal;
  [[nodiscard]] static auto max_ui_scale() -> qreal;
  [[nodiscard]] static auto min_edge_scroll_sensitivity() -> qreal;
  [[nodiscard]] static auto max_edge_scroll_sensitivity() -> qreal;

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
  void set_camera_legend_seen(bool seen);
  void set_tutorial_completed(bool completed);
  void set_screen_effect_intensity(qreal intensity);

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
  void camera_legend_seen_changed();
  void tutorial_completed_changed();
  void screen_effect_intensity_changed();

private:
  explicit UiPreferences(QObject* parent = nullptr);

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
  bool m_damage_numbers;
  bool m_camera_legend_seen;
  bool m_tutorial_completed;
  qreal m_screen_effect_intensity;
};

#endif
