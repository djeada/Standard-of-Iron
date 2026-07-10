#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector2D>
#include <QVector>

#include <memory>
#include <variant>
#include <vector>

namespace MapEditor {

struct TerrainElement {
  QString type;
  float x = 0.0F;
  float z = 0.0F;
  float radius = 10.0F;
  float width = 10.0F;
  float depth = 10.0F;
  float height = 3.0F;
  float rotation = 0.0F;
  QJsonArray entrances;
  QJsonObject extra_fields;
};

struct WorldPropElement {
  QString type = "firecamp";
  float x = 0.0F;
  float z = 0.0F;
  float scale = 1.0F;
  float rotation = 0.0F;
  float intensity = 1.0F;
  float radius = 3.0F;
  bool persistent = true;
  QJsonObject extra_fields;
};

struct LinearElement {
  QString type;
  QVector2D start;
  QVector2D end;
  float width = 3.0F;
  float height = 0.5F;
  QString style;
  int player_id = 0;
  QString nation;
  QJsonObject extra_fields;
};

inline constexpr float k_min_bridge_height = 0.1F;

[[nodiscard]] auto
compute_min_bridge_width(const QVector2D& bridge_start,
                         const QVector2D& bridge_end,
                         const QVector<LinearElement>& elements) -> float;

struct StructureElement {
  QString type;
  float x = 0.0F;
  float z = 0.0F;
  int player_id = 0;
  int max_population = 150;
  QString nation;
  QJsonObject extra_fields;
  int spawn_order = -1;
};

struct TroopSpawnElement {
  QString type;
  float x = 0.0F;
  float z = 0.0F;
  int player_id = -1;
  int max_population = -1;
  QString nation;
  QString behavior;
  float guard_radius = 10.0F;
  QJsonArray patrol_waypoints;
  QJsonObject extra_fields;
  int spawn_order = -1;
};

struct UndeadZoneElement {
  QString id;
  QString anchor_type = QStringLiteral("magic_shrine");
  float x = 0.0F;
  float z = 0.0F;
  float radius = 8.0F;
  float leash_radius = 14.0F;
  int owner_id = 99;
  int team_id = 99;
  QJsonArray awaken_on;
  QJsonArray waves;
};

struct FogZoneElement {
  float x = 0.0F;
  float z = 0.0F;
  float width = 10.0F;
  float height = 10.0F;
  float density = 0.6F;
};

struct GridSettings {
  int width = 100;
  int height = 100;
  float tile_size = 1.0F;
};

class MapData;

class Command {
public:
  virtual ~Command() = default;
  virtual void execute() = 0;
  virtual void undo() = 0;
  [[nodiscard]] virtual QString description() const { return {}; }
};

class MapData : public QObject {
  Q_OBJECT

public:
  explicit MapData(QObject* parent = nullptr);

  bool load_from_json(const QString& file_path, QString* out_error = nullptr);
  bool save_to_json(const QString& file_path, QString* out_error = nullptr) const;
  [[nodiscard]] QString to_json_string() const;

  [[nodiscard]] QString name() const { return m_name; }
  void set_name(const QString& name);

  [[nodiscard]] const GridSettings& grid() const { return m_grid; }
  void set_grid(const GridSettings& grid);

  [[nodiscard]] const QVector<TerrainElement>& terrain_elements() const {
    return m_terrain;
  }
  void add_terrain_element(const TerrainElement& element);
  void insert_terrain_element(int index, const TerrainElement& element);
  void update_terrain_element(int index, const TerrainElement& element);
  void remove_terrain_element(int index);

  [[nodiscard]] const QVector<WorldPropElement>& world_props() const {
    return m_world_props;
  }
  void add_world_prop(const WorldPropElement& element);
  void insert_world_prop(int index, const WorldPropElement& element);
  void update_world_prop(int index, const WorldPropElement& element);
  void remove_world_prop(int index);

  [[nodiscard]] const QVector<LinearElement>& linear_elements() const {
    return m_linear_elements;
  }
  void add_linear_element(const LinearElement& element);
  void insert_linear_element(int index, const LinearElement& element);
  void update_linear_element(int index, const LinearElement& element);
  void remove_linear_element(int index);

  [[nodiscard]] const QVector<StructureElement>& structures() const {
    return m_structures;
  }
  void add_structure(const StructureElement& element);
  void insert_structure(int index, const StructureElement& element);
  void update_structure(int index, const StructureElement& element);
  void remove_structure(int index);

  [[nodiscard]] const QVector<TroopSpawnElement>& troop_spawns() const {
    return m_troop_spawns;
  }
  void add_troop_spawn(const TroopSpawnElement& element);
  void insert_troop_spawn(int index, const TroopSpawnElement& element);
  void update_troop_spawn(int index, const TroopSpawnElement& element);
  void remove_troop_spawn(int index);

  [[nodiscard]] const QVector<UndeadZoneElement>& undead_zones() const {
    return m_undead_zones;
  }
  void add_undead_zone(const UndeadZoneElement& element);
  void insert_undead_zone(int index, const UndeadZoneElement& element);
  void update_undead_zone(int index, const UndeadZoneElement& element);
  void remove_undead_zone(int index);

  [[nodiscard]] const QVector<FogZoneElement>& fog_zones() const { return m_fog_zones; }
  void add_fog_zone(const FogZoneElement& element);
  void remove_fog_zone(int index);

  void execute_command(std::unique_ptr<Command> cmd);
  void record_command(std::unique_ptr<Command> cmd);
  void undo();
  void redo();
  [[nodiscard]] bool can_undo() const { return !m_undo_stack.empty(); }
  [[nodiscard]] bool can_redo() const { return !m_redo_stack.empty(); }
  [[nodiscard]] QString undo_description() const;
  [[nodiscard]] QString redo_description() const;

  void clear();

  [[nodiscard]] bool is_modified() const { return m_modified; }
  void set_modified(bool modified);

signals:
  void data_changed();
  void modified_changed(bool modified);
  void undo_redo_changed();

private:
  QString m_name;
  GridSettings m_grid;
  QVector<TerrainElement> m_terrain;
  QVector<WorldPropElement> m_world_props;
  QVector<LinearElement> m_linear_elements;
  QVector<StructureElement> m_structures;
  QVector<TroopSpawnElement> m_troop_spawns;
  QVector<UndeadZoneElement> m_undead_zones;
  QVector<FogZoneElement> m_fog_zones;

  QJsonObject m_biome;
  QJsonObject m_camera;
  QJsonObject m_victory;
  QJsonObject m_rain;
  QJsonObject m_extra_root_fields;
  QString m_description;
  QString m_coord_system;
  int m_max_troops_per_player = 2000;
  int m_next_spawn_order = 0;

  struct RawSpawnEntry {
    int spawn_order = -1;
    QJsonObject object;
  };
  QVector<RawSpawnEntry> m_raw_spawns;

  bool m_modified = false;

  std::vector<std::unique_ptr<Command>> m_undo_stack;
  std::vector<std::unique_ptr<Command>> m_redo_stack;

  void parse_terrain_array(const QJsonArray& arr);
  void parse_world_props_array(const QJsonArray& arr);
  void parse_legacy_firecamps_array(const QJsonArray& arr);
  void parse_rivers_array(const QJsonArray& arr);
  void parse_roads_array(const QJsonArray& arr);
  void parse_bridges_array(const QJsonArray& arr);
  void parse_spawns_array(const QJsonArray& arr);
  void parse_undead_zones_array(const QJsonArray& arr);
  void parse_fog_zones_array(const QJsonArray& arr);
  void parse_buildings_array(const QJsonArray& arr);
  void parse_walls_array(const QJsonArray& arr);

  [[nodiscard]] QJsonArray terrain_to_json() const;
  [[nodiscard]] QJsonArray world_props_to_json() const;
  [[nodiscard]] QJsonArray rivers_to_json() const;
  [[nodiscard]] QJsonArray roads_to_json() const;
  [[nodiscard]] QJsonArray bridges_to_json() const;
  [[nodiscard]] QJsonArray buildings_to_json() const;
  [[nodiscard]] QJsonArray walls_to_json() const;
  [[nodiscard]] QJsonObject build_root_json() const;
  [[nodiscard]] QJsonObject structure_to_spawn_json(const StructureElement& elem) const;
  [[nodiscard]] QJsonObject troop_to_spawn_json(const TroopSpawnElement& elem) const;
  [[nodiscard]] QJsonArray undead_zones_to_json() const;
  [[nodiscard]] QJsonArray fog_zones_to_json() const;
};

} // namespace MapEditor

namespace MapEditor {

class AddTerrainCmd : public Command {
public:
  AddTerrainCmd(MapData* data, TerrainElement elem)
      : m_data(data)
      , m_elem(std::move(elem)) {}
  void execute() override {
    m_index = m_data->terrain_elements().size();
    m_data->add_terrain_element(m_elem);
  }
  void undo() override { m_data->remove_terrain_element(m_index); }
  [[nodiscard]] QString description() const override { return "Place " + m_elem.type; }

private:
  MapData* m_data;
  TerrainElement m_elem;
  int m_index = -1;
};

class RemoveTerrainCmd : public Command {
public:
  RemoveTerrainCmd(MapData* data, int index, TerrainElement elem)
      : m_data(data)
      , m_index(index)
      , m_elem(std::move(elem)) {}
  void execute() override { m_data->remove_terrain_element(m_index); }
  void undo() override { m_data->insert_terrain_element(m_index, m_elem); }
  [[nodiscard]] QString description() const override { return "Erase " + m_elem.type; }

private:
  MapData* m_data;
  int m_index;
  TerrainElement m_elem;
};

class UpdateTerrainCmd : public Command {
public:
  UpdateTerrainCmd(MapData* data,
                   int index,
                   TerrainElement before,
                   TerrainElement after,
                   QString desc = "Edit terrain")
      : m_data(data)
      , m_index(index)
      , m_before(std::move(before))
      , m_after(std::move(after))
      , m_desc(std::move(desc)) {}
  void execute() override { m_data->update_terrain_element(m_index, m_after); }
  void undo() override { m_data->update_terrain_element(m_index, m_before); }
  [[nodiscard]] QString description() const override { return m_desc; }

private:
  MapData* m_data;
  int m_index;
  TerrainElement m_before;
  TerrainElement m_after;
  QString m_desc;
};

class AddWorldPropCmd : public Command {
public:
  AddWorldPropCmd(MapData* data, WorldPropElement elem)
      : m_data(data)
      , m_elem(std::move(elem)) {}
  void execute() override {
    m_index = m_data->world_props().size();
    m_data->add_world_prop(m_elem);
  }
  void undo() override { m_data->remove_world_prop(m_index); }
  [[nodiscard]] QString description() const override { return "Place " + m_elem.type; }

private:
  MapData* m_data;
  WorldPropElement m_elem;
  int m_index = -1;
};

class RemoveWorldPropCmd : public Command {
public:
  RemoveWorldPropCmd(MapData* data, int index, WorldPropElement elem)
      : m_data(data)
      , m_index(index)
      , m_elem(std::move(elem)) {}
  void execute() override { m_data->remove_world_prop(m_index); }
  void undo() override { m_data->insert_world_prop(m_index, m_elem); }
  [[nodiscard]] QString description() const override { return "Erase " + m_elem.type; }

private:
  MapData* m_data;
  int m_index;
  WorldPropElement m_elem;
};

class UpdateWorldPropCmd : public Command {
public:
  UpdateWorldPropCmd(MapData* data,
                     int index,
                     WorldPropElement before,
                     WorldPropElement after,
                     QString desc = "Edit prop")
      : m_data(data)
      , m_index(index)
      , m_before(std::move(before))
      , m_after(std::move(after))
      , m_desc(std::move(desc)) {}
  void execute() override { m_data->update_world_prop(m_index, m_after); }
  void undo() override { m_data->update_world_prop(m_index, m_before); }
  [[nodiscard]] QString description() const override { return m_desc; }

private:
  MapData* m_data;
  int m_index;
  WorldPropElement m_before;
  WorldPropElement m_after;
  QString m_desc;
};

class AddLinearCmd : public Command {
public:
  AddLinearCmd(MapData* data, LinearElement elem)
      : m_data(data)
      , m_elem(std::move(elem)) {}
  void execute() override {
    m_index = m_data->linear_elements().size();
    m_data->add_linear_element(m_elem);
  }
  void undo() override { m_data->remove_linear_element(m_index); }
  [[nodiscard]] QString description() const override { return "Place " + m_elem.type; }

private:
  MapData* m_data;
  LinearElement m_elem;
  int m_index = -1;
};

class RemoveLinearCmd : public Command {
public:
  RemoveLinearCmd(MapData* data, int index, LinearElement elem)
      : m_data(data)
      , m_index(index)
      , m_elem(std::move(elem)) {}
  void execute() override { m_data->remove_linear_element(m_index); }
  void undo() override { m_data->insert_linear_element(m_index, m_elem); }
  [[nodiscard]] QString description() const override { return "Erase " + m_elem.type; }

private:
  MapData* m_data;
  int m_index;
  LinearElement m_elem;
};

class UpdateLinearCmd : public Command {
public:
  UpdateLinearCmd(MapData* data,
                  int index,
                  LinearElement before,
                  LinearElement after,
                  QString desc = "Edit element")
      : m_data(data)
      , m_index(index)
      , m_before(std::move(before))
      , m_after(std::move(after))
      , m_desc(std::move(desc)) {}
  void execute() override { m_data->update_linear_element(m_index, m_after); }
  void undo() override { m_data->update_linear_element(m_index, m_before); }
  [[nodiscard]] QString description() const override { return m_desc; }

private:
  MapData* m_data;
  int m_index;
  LinearElement m_before;
  LinearElement m_after;
  QString m_desc;
};

class AddStructureCmd : public Command {
public:
  AddStructureCmd(MapData* data, StructureElement elem)
      : m_data(data)
      , m_elem(std::move(elem)) {}
  void execute() override {
    m_index = m_data->structures().size();
    m_data->add_structure(m_elem);
  }
  void undo() override { m_data->remove_structure(m_index); }
  [[nodiscard]] QString description() const override { return "Place " + m_elem.type; }

private:
  MapData* m_data;
  StructureElement m_elem;
  int m_index = -1;
};

class RemoveStructureCmd : public Command {
public:
  RemoveStructureCmd(MapData* data, int index, StructureElement elem)
      : m_data(data)
      , m_index(index)
      , m_elem(std::move(elem)) {}
  void execute() override { m_data->remove_structure(m_index); }
  void undo() override { m_data->insert_structure(m_index, m_elem); }
  [[nodiscard]] QString description() const override { return "Erase " + m_elem.type; }

private:
  MapData* m_data;
  int m_index;
  StructureElement m_elem;
};

class UpdateStructureCmd : public Command {
public:
  UpdateStructureCmd(MapData* data,
                     int index,
                     StructureElement before,
                     StructureElement after,
                     QString desc = "Edit structure")
      : m_data(data)
      , m_index(index)
      , m_before(std::move(before))
      , m_after(std::move(after))
      , m_desc(std::move(desc)) {}
  void execute() override { m_data->update_structure(m_index, m_after); }
  void undo() override { m_data->update_structure(m_index, m_before); }
  [[nodiscard]] QString description() const override { return m_desc; }

private:
  MapData* m_data;
  int m_index;
  StructureElement m_before;
  StructureElement m_after;
  QString m_desc;
};

class AddTroopSpawnCmd : public Command {
public:
  AddTroopSpawnCmd(MapData* data, TroopSpawnElement elem)
      : m_data(data)
      , m_elem(std::move(elem)) {}
  void execute() override {
    m_index = m_data->troop_spawns().size();
    m_data->add_troop_spawn(m_elem);
  }
  void undo() override { m_data->remove_troop_spawn(m_index); }
  [[nodiscard]] QString description() const override { return "Place " + m_elem.type; }

private:
  MapData* m_data;
  TroopSpawnElement m_elem;
  int m_index = -1;
};

class RemoveTroopSpawnCmd : public Command {
public:
  RemoveTroopSpawnCmd(MapData* data, int index, TroopSpawnElement elem)
      : m_data(data)
      , m_index(index)
      , m_elem(std::move(elem)) {}
  void execute() override { m_data->remove_troop_spawn(m_index); }
  void undo() override { m_data->insert_troop_spawn(m_index, m_elem); }
  [[nodiscard]] QString description() const override { return "Erase " + m_elem.type; }

private:
  MapData* m_data;
  int m_index;
  TroopSpawnElement m_elem;
};

class UpdateTroopSpawnCmd : public Command {
public:
  UpdateTroopSpawnCmd(MapData* data,
                      int index,
                      TroopSpawnElement before,
                      TroopSpawnElement after,
                      QString desc = "Edit troop")
      : m_data(data)
      , m_index(index)
      , m_before(std::move(before))
      , m_after(std::move(after))
      , m_desc(std::move(desc)) {}
  void execute() override { m_data->update_troop_spawn(m_index, m_after); }
  void undo() override { m_data->update_troop_spawn(m_index, m_before); }
  [[nodiscard]] QString description() const override { return m_desc; }

private:
  MapData* m_data;
  int m_index;
  TroopSpawnElement m_before;
  TroopSpawnElement m_after;
  QString m_desc;
};

class AddUndeadZoneCmd : public Command {
public:
  AddUndeadZoneCmd(MapData* data, UndeadZoneElement elem)
      : m_data(data)
      , m_elem(std::move(elem)) {}
  void execute() override {
    m_index = m_data->undead_zones().size();
    m_data->add_undead_zone(m_elem);
  }
  void undo() override { m_data->remove_undead_zone(m_index); }
  [[nodiscard]] QString description() const override {
    return "Place undead zone " + m_elem.anchor_type;
  }

private:
  MapData* m_data;
  UndeadZoneElement m_elem;
  int m_index = -1;
};

class RemoveUndeadZoneCmd : public Command {
public:
  RemoveUndeadZoneCmd(MapData* data, int index, UndeadZoneElement elem)
      : m_data(data)
      , m_index(index)
      , m_elem(std::move(elem)) {}
  void execute() override { m_data->remove_undead_zone(m_index); }
  void undo() override { m_data->insert_undead_zone(m_index, m_elem); }
  [[nodiscard]] QString description() const override {
    return "Erase undead zone " + m_elem.anchor_type;
  }

private:
  MapData* m_data;
  int m_index;
  UndeadZoneElement m_elem;
};

class UpdateUndeadZoneCmd : public Command {
public:
  UpdateUndeadZoneCmd(MapData* data,
                      int index,
                      UndeadZoneElement before,
                      UndeadZoneElement after,
                      QString desc = "Edit undead zone")
      : m_data(data)
      , m_index(index)
      , m_before(std::move(before))
      , m_after(std::move(after))
      , m_desc(std::move(desc)) {}
  void execute() override { m_data->update_undead_zone(m_index, m_after); }
  void undo() override { m_data->update_undead_zone(m_index, m_before); }
  [[nodiscard]] QString description() const override { return m_desc; }

private:
  MapData* m_data;
  int m_index;
  UndeadZoneElement m_before;
  UndeadZoneElement m_after;
  QString m_desc;
};

class ResizeMapCmd : public Command {
public:
  ResizeMapCmd(MapData* data, GridSettings before, GridSettings after)
      : m_data(data)
      , m_before(before)
      , m_after(after) {}
  void execute() override { m_data->set_grid(m_after); }
  void undo() override { m_data->set_grid(m_before); }
  [[nodiscard]] QString description() const override { return "Resize map"; }

private:
  MapData* m_data;
  GridSettings m_before;
  GridSettings m_after;
};

} // namespace MapEditor
