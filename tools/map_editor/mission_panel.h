#pragma once

#include <QWidget>

#include "map_data.h"
#include "mission_data.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QTableWidget;

namespace MapEditor {

class MissionPanel : public QWidget {
  Q_OBJECT

public:
  MissionPanel(MissionData* mission_data, MapData* map_data, QWidget* parent = nullptr);

  void refresh();

signals:
  void map_path_changed(const QString& map_path);
  void validate_requested();
  void launch_game_requested();
  void launch_arena_requested();

private:
  void setup_ui();
  void sync_identity();
  void sync_player();
  void sync_weather();
  void refresh_ai_table();
  void refresh_forces_table();
  void refresh_condition_tables();
  void refresh_waves_table();
  void refresh_phases_table();
  void refresh_fog_table();

  void edit_ai(int row);
  void remove_ai(int row);
  void edit_force(int row);
  void remove_force(int row);
  void edit_condition(const QString& key, QTableWidget* table, int row);
  void remove_condition(const QString& key, QTableWidget* table, int row);
  void edit_wave(int row);
  void remove_wave(int row);
  void edit_phase(int row);
  void remove_phase(int row);
  void edit_fog_zone(int row);
  void remove_fog_zone(int row);

  MissionData* m_mission_data = nullptr;
  MapData* m_map_data = nullptr;
  bool m_refreshing = false;

  QLineEdit* m_id_edit = nullptr;
  QLineEdit* m_title_edit = nullptr;
  QPlainTextEdit* m_summary_edit = nullptr;
  QPlainTextEdit* m_intro_edit = nullptr;
  QLineEdit* m_map_path_edit = nullptr;
  QComboBox* m_player_nation = nullptr;
  QComboBox* m_player_faction = nullptr;
  QComboBox* m_player_color = nullptr;
  QSpinBox* m_player_gold = nullptr;
  QSpinBox* m_player_food = nullptr;
  QCheckBox* m_ambient_undead = nullptr;

  QTableWidget* m_ai_table = nullptr;
  QTableWidget* m_forces_table = nullptr;
  QTableWidget* m_victory_table = nullptr;
  QTableWidget* m_defeat_table = nullptr;
  QTableWidget* m_optional_table = nullptr;
  QTableWidget* m_waves_table = nullptr;
  QTableWidget* m_phases_table = nullptr;

  QCheckBox* m_weather_enabled = nullptr;
  QComboBox* m_weather_type = nullptr;
  QDoubleSpinBox* m_weather_intensity = nullptr;
  QDoubleSpinBox* m_weather_cycle = nullptr;
  QDoubleSpinBox* m_weather_active = nullptr;
  QDoubleSpinBox* m_weather_fade = nullptr;
  QDoubleSpinBox* m_weather_wind = nullptr;
  QTableWidget* m_fog_table = nullptr;
};

} // namespace MapEditor
