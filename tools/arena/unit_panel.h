#pragma once

#include <QString>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSlider;
class QSpinBox;

class UnitPanel : public QWidget {
  Q_OBJECT

public:
  explicit UnitPanel(QWidget* parent = nullptr);

  void set_animation_paused(bool paused);
  void set_selection_summary(const QString& summary);
  [[nodiscard]] auto selected_owner_id() const -> int;
  [[nodiscard]] auto selected_nation_id() const -> QString;
  [[nodiscard]] auto selected_unit_type_id() const -> QString;

signals:
  void spawn_units_requested(int count);
  void clear_units_requested();
  void spawn_owner_selected(int owner_id);
  void nation_selected(const QString& nation_id);
  void unit_type_selected(const QString& unit_type);
  void spawn_individuals_per_unit_changed(int count);
  void spawn_rider_visibility_changed(bool visible);
  void apply_visual_overrides_requested();
  void spawn_opposing_batch_requested(int count);
  void spawn_mirror_match_requested(int count);
  void reset_arena_requested();
  void load_scenario_requested(const QString& scenario_id);
  void animation_selected(const QString& animation_name);
  void play_animation_requested();
  void animation_paused_toggled(bool paused);
  void move_selected_unit_requested();
  void movement_speed_changed(float speed);
  void skeleton_debug_toggled(bool enabled);
  void combat_debug_toggled(bool enabled);
  void attack_scrub_toggled(bool enabled);
  void attack_scrub_phase_changed(float phase);

private:
  void apply_special_unit_defaults(const QString& unit_id);
  void populate_nation_options();
  void populate_unit_options(const QString& nation_id,
                             const QString& preferred_unit_type = QString());

  QCheckBox* m_pause_checkbox = nullptr;
  QComboBox* m_owner_box = nullptr;
  QComboBox* m_nation_box = nullptr;
  QComboBox* m_unit_box = nullptr;
  QComboBox* m_scenario_box = nullptr;
  QSpinBox* m_spawn_count_box = nullptr;
  QSpinBox* m_individuals_per_unit_box = nullptr;
  QCheckBox* m_render_rider_checkbox = nullptr;
  QCheckBox* m_combat_debug_checkbox = nullptr;
  QCheckBox* m_attack_scrub_checkbox = nullptr;
  QSlider* m_attack_scrub_slider = nullptr;
  QDoubleSpinBox* m_attack_scrub_spin = nullptr;
  QLabel* m_scenario_description_label = nullptr;
  QLabel* m_selection_summary_label = nullptr;
  bool m_special_unit_option_active = false;
  int m_saved_manual_individuals_per_unit = 0;
  bool m_saved_manual_rider_visible = true;
};
