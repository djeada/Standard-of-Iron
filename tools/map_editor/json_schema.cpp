#include "json_schema.h"

#include <QJsonArray>
#include <QJsonObject>

#include "element_ops.h"

namespace MapEditor {

namespace {

auto required_field(const QString& key,
                    const QString& type,
                    const QString& description,
                    const QJsonValue& placeholder,
                    const QStringList& allowed = {}) -> JsonFieldSpec {
  JsonFieldSpec spec;
  spec.key = key;
  spec.type = type;
  spec.description = description;
  spec.allowed = allowed;
  spec.required = true;
  spec.placeholder = placeholder;
  return spec;
}

auto optional_field(const QString& key,
                    const QString& type,
                    const QString& default_value,
                    const QString& description,
                    const QJsonValue& placeholder,
                    const QStringList& allowed = {}) -> JsonFieldSpec {
  JsonFieldSpec spec;
  spec.key = key;
  spec.type = type;
  spec.default_value = default_value;
  spec.description = description;
  spec.allowed = allowed;
  spec.placeholder = placeholder;
  return spec;
}

auto grid_x_field() -> JsonFieldSpec {
  return required_field("x", "number", "Grid column (cells, 0 … grid width).", 0);
}

auto grid_z_field() -> JsonFieldSpec {
  return required_field("z", "number", "Grid row (cells, 0 … grid height).", 0);
}

auto nation_field() -> JsonFieldSpec {
  return optional_field("nation",
                        "string",
                        "map default",
                        "Overrides the nation used for this entry's visuals.",
                        QString());
}

const QStringList k_prop_types = {"firecamp",
                                  "tent",
                                  "supply_cart",
                                  "weapon_rack",
                                  "ruins",
                                  "magic_shrine",
                                  "dead_tree",
                                  "boulder",
                                  "pine_tree",
                                  "olive_tree",
                                  "plant",
                                  "iron_ore",
                                  "abandoned_home",
                                  "statue"};

auto terrain_schema(const QString& sub_type) -> JsonSchema {
  const bool is_mountain = sub_type == QStringLiteral("mountain");
  const bool is_lake = sub_type == QStringLiteral("lake");

  JsonSchema schema;
  schema.title = QStringLiteral("Terrain feature");
  schema.summary =
      is_mountain
          ? QStringLiteral("Impassable massif. Mountains never carry entrances.")
          : (is_lake ? QStringLiteral("Water body; units path around it.")
                     : QStringLiteral(
                           "Raised ground. Entrances carve ramps units can climb."));

  schema.fields = {
      required_field("type",
                     "string",
                     "Feature kind.",
                     sub_type.isEmpty() ? QStringLiteral("hill") : sub_type,
                     {"hill", "mountain", "lake"}),
      grid_x_field(),
      grid_z_field(),
      optional_field("radius",
                     "number",
                     "10",
                     "Circular footprint radius in cells. Used when width/depth are "
                     "absent.",
                     10.0),
      optional_field("width",
                     "number",
                     "unset",
                     "Elliptical footprint along x. Set together with depth.",
                     10.0),
      optional_field("depth",
                     "number",
                     "unset",
                     "Elliptical footprint along z. Set together with width.",
                     10.0),
      optional_field("height",
                     "number",
                     is_mountain ? "8" : "3",
                     "Peak height in world units. Lakes ignore it.",
                     is_mountain ? 8.0 : 3.0),
      optional_field(
          "rotation", "number", "0", "Footprint rotation in degrees, clockwise.", 0.0),
  };

  if (!is_mountain) {
    JsonFieldSpec entrances =
        optional_field("entrances",
                       "array",
                       "[]",
                       "Ramp openings: [{\"x\": 0, \"z\": 0, \"radius\": 2}]. Easier "
                       "to paint in the projection panel.",
                       QJsonArray{});
    schema.fields.append(entrances);
  }

  return schema;
}

auto world_prop_schema(const QString& sub_type) -> JsonSchema {
  const bool is_firecamp = sub_type == QStringLiteral("firecamp");

  JsonSchema schema;
  schema.title = QStringLiteral("World prop");
  schema.summary =
      is_firecamp
          ? QStringLiteral("Firecamps light their surroundings and can burn out.")
          : QStringLiteral("Decorative or blocking prop placed on the ground.");

  schema.fields = {
      required_field("type",
                     "string",
                     "Prop kind.",
                     sub_type.isEmpty() ? QStringLiteral("firecamp") : sub_type,
                     k_prop_types),
      grid_x_field(),
      grid_z_field(),
  };

  if (is_firecamp) {
    schema.fields.append(optional_field(
        "intensity", "number", "1.0", "Light strength multiplier.", 1.0));
    schema.fields.append(
        optional_field("radius", "number", "3.0", "Lit radius in cells.", 3.0));
    schema.fields.append(optional_field("persistent",
                                        "bool",
                                        "true",
                                        "false lets the fire burn out during play.",
                                        true));
  } else {
    schema.fields.append(
        optional_field("scale", "number", "1.0", "Uniform model scale.", 1.0));
    schema.fields.append(
        optional_field("rotation", "number", "0", "Yaw in degrees, clockwise.", 0.0));
  }

  return schema;
}

auto linear_schema(const QString& sub_type) -> JsonSchema {
  JsonSchema schema;
  schema.title = QStringLiteral("Linear feature");
  schema.summary = QStringLiteral(
      "Segment between two grid points. Bridges must span the river bank to bank; "
      "walls are forced to stay axis aligned.");

  schema.fields = {
      required_field("type",
                     "string",
                     "Segment kind.",
                     sub_type.isEmpty() ? QStringLiteral("road") : sub_type,
                     {"river", "road", "bridge", "wall"}),
      required_field("start", "[x, z]", "Start point in grid cells.", QJsonArray{0, 0}),
      required_field("end", "[x, z]", "End point in grid cells.", QJsonArray{0, 0}),
      optional_field("width", "number", "3.0", "Segment width in cells.", 3.0),
  };

  if (sub_type == QStringLiteral("bridge")) {
    schema.fields.append(optional_field(
        "height", "number", "0.5", "Deck height; raised to 0.1 minimum on save.", 0.5));
  }
  if (sub_type == QStringLiteral("road") || sub_type.isEmpty()) {
    schema.fields.append(optional_field("style",
                                        "string",
                                        "default",
                                        "Road surface variant.",
                                        QStringLiteral("default")));
  }
  if (sub_type == QStringLiteral("road") || sub_type == QStringLiteral("river") ||
      sub_type.isEmpty()) {
    schema.fields.append(
        optional_field("waypoints",
                       "[[x, z], ...]",
                       "none",
                       "Intermediate points; the runtime walks start, every "
                       "waypoint, then end as one chain of segments.",
                       QJsonArray{QJsonArray{0, 0}}));
  }
  if (sub_type == QStringLiteral("wall") || sub_type.isEmpty()) {
    schema.fields.append(
        optional_field("player_id", "integer", "0", "Owning player; 0 is neutral.", 0));
    schema.fields.append(nation_field());
  }

  return schema;
}

auto structure_schema(const QString& sub_type) -> JsonSchema {
  JsonSchema schema;
  schema.title = QStringLiteral("Structure");
  schema.summary = QStringLiteral(
      "Pre-placed building. Player 0 is neutral; 1+ belong to the matching player "
      "slot in the mission.");

  schema.fields = {
      required_field("type",
                     "string",
                     "Building kind.",
                     sub_type.isEmpty() ? QStringLiteral("barracks") : sub_type,
                     {"barracks",
                      "village",
                      "defense_tower",
                      "home",
                      "marketplace",
                      "temple",
                      "wall_gate"}),
      grid_x_field(),
      grid_z_field(),
      optional_field("player_id", "integer", "0", "Owning player; 0 is neutral.", 0),
      optional_field("rotation",
                     "number",
                     "0",
                     "Yaw in degrees. A wall gate must face along the wall it "
                     "closes: 0 spans x, 90 spans z.",
                     0),
      optional_field("max_population",
                     "integer",
                     "100",
                     "Population this building supports.",
                     150),
      nation_field(),
  };

  return schema;
}

auto troop_schema(const QString& sub_type) -> JsonSchema {
  JsonSchema schema;
  schema.title = QStringLiteral("Troop spawn");
  schema.summary = QStringLiteral(
      "A unit placed at map start. guard/hold/patrol keep the unit under scenario "
      "control; anything else hands it to the strategic AI.");

  schema.fields = {
      required_field("type",
                     "string",
                     "Unit kind, e.g. archer, swordsman, catapult.",
                     sub_type.isEmpty() ? QStringLiteral("archer") : sub_type),
      grid_x_field(),
      grid_z_field(),
      optional_field(
          "player_id", "integer", "unset", "Owning player; omit for neutral.", 1),
      optional_field("max_population",
                     "integer",
                     "unset",
                     "Caps how many of this unit the owner may hold.",
                     -1),
      nation_field(),
      optional_field("behavior",
                     "string",
                     "strategic",
                     "Scenario behaviour for this unit.",
                     QStringLiteral("guard"),
                     {"guard", "hold", "patrol", "strategic"}),
      optional_field("guard_radius",
                     "number",
                     "10",
                     "How far a guard unit chases before returning.",
                     10.0),
      optional_field("patrol_waypoints",
                     "array",
                     "[]",
                     "Patrol route: [{\"x\": 0, \"z\": 0}, …].",
                     QJsonArray{}),
  };

  return schema;
}

auto wildlife_area_schema() -> JsonSchema {
  JsonSchema schema;
  schema.title = QStringLiteral("Wildlife range");
  schema.summary = QStringLiteral(
      "Ground a species is anchored to. Groups spawn somewhere inside the circle "
      "and roam out from there. A map that authors no range at all still gets one "
      "picked for it at load time, off the roads and away from the player bases.");

  schema.fields = {
      optional_field("species",
                     "string",
                     "sheep",
                     "Which population this range belongs to.",
                     QStringLiteral("sheep"),
                     QStringList{QStringLiteral("sheep"),
                                 QStringLiteral("wolves"),
                                 QStringLiteral("birds")}),
      grid_x_field(),
      grid_z_field(),
      optional_field("radius",
                     "number",
                     "14",
                     "How far from the centre a group may be anchored, in cells.",
                     14.0),
  };
  return schema;
}

auto undead_zone_schema() -> JsonSchema {
  JsonSchema schema;
  schema.title = QStringLiteral("Undead zone");
  schema.summary = QStringLiteral(
      "Dormant undead encounter anchored to a prop. Waves spawn when a trigger "
      "fires; units are leashed to the anchor. Every zone also raises a magic "
      "shrine at its centre - the sepulcher's capturable barracks.");

  schema.fields = {
      optional_field("id",
                     "string",
                     "undead_zone_N",
                     "Identifier missions reference in objectives.",
                     QStringLiteral("zone_1")),
      optional_field("anchor_type",
                     "string",
                     "ruins",
                     "Prop the zone is built around.",
                     QStringLiteral("magic_shrine"),
                     k_prop_types),
      grid_x_field(),
      grid_z_field(),
      optional_field("radius", "number", "8", "Awaken radius in cells.", 8.0),
      optional_field("leash_radius",
                     "number",
                     "max(radius, 14)",
                     "How far spawned undead may roam from the anchor.",
                     14.0),
      optional_field("owner_id", "integer", "99", "Owning player slot.", 99),
      optional_field(
          "team_id", "integer", "99", "Team slot; shared by all undead.", 99),
      optional_field(
          "fog_density", "number", "zone default", "Fog thickness over the zone.", 0.6),
      optional_field("wave_timeout",
                     "number",
                     "zone default",
                     "Seconds before the next wave is released.",
                     30.0),
      optional_field("awaken_on",
                     "array",
                     "[\"unit_enters_radius\"]",
                     "Trigger names that wake the zone.",
                     QJsonArray{QStringLiteral("unit_enters_radius")}),
      optional_field(
          "waves",
          "array",
          "default waves",
          "[{\"trigger\": \"initial\", \"units\": {\"skeleton_swordsman\": "
          "2}}]",
          QJsonArray{QJsonObject{{"trigger", "initial"},
                                 {"units", QJsonObject{{"skeleton_swordsman", 2}}}}}),
  };

  return schema;
}

} // namespace

auto JsonSchema::find(const QString& key) const -> const JsonFieldSpec* {
  for (const JsonFieldSpec& field : fields) {
    if (field.key == key) {
      return &field;
    }
  }
  return nullptr;
}

auto schema_for_element(int element_kind, const QString& sub_type) -> JsonSchema {
  const QString normalized = sub_type.trimmed().toLower();
  switch (static_cast<ElementKind>(element_kind)) {
  case ElementKind::Terrain:
    return terrain_schema(normalized);
  case ElementKind::WorldProp:
    return world_prop_schema(normalized);
  case ElementKind::Linear:
    return linear_schema(normalized);
  case ElementKind::Structure:
    return structure_schema(normalized);
  case ElementKind::TroopSpawn:
    return troop_schema(normalized);
  case ElementKind::UndeadZone:
    return undead_zone_schema();
  case ElementKind::WildlifeArea:
    return wildlife_area_schema();
  }
  return {};
}

auto schema_for_biome() -> JsonSchema {
  JsonSchema schema;
  schema.title = QStringLiteral("Biome");
  schema.summary = QStringLiteral(
      "Ground and vegetation look. Setting ground_type applies a full preset; every "
      "other key overrides one value of that preset. Colours are [r, g, b] in 0…1.");

  schema.fields = {
      optional_field(
          "ground_type",
          "string",
          "forest_mud",
          "Preset applied before the overrides below.",
          QStringLiteral("forest_mud"),
          {"forest_mud", "grass_dry", "soil_rocky", "alpine_mix", "soil_fertile"}),
      optional_field("seed", "integer", "preset", "Scatter randomisation seed.", 0),
      optional_field("patch_density", "number", "preset", "Grass patch coverage.", 1.0),
      optional_field(
          "patch_jitter", "number", "preset", "Random offset of patches.", 0.5),
      optional_field("blade_height",
                     "[min, max]",
                     "preset",
                     "Grass blade height range.",
                     QJsonArray{0.2, 0.5}),
      optional_field("blade_width",
                     "[min, max]",
                     "preset",
                     "Grass blade width range.",
                     QJsonArray{0.02, 0.05}),
      optional_field("background_blade_density",
                     "number",
                     "preset",
                     "Density of the distant grass layer.",
                     1.0),
      optional_field("background_sway_variance",
                     "number",
                     "preset",
                     "Sway variation of the distant layer.",
                     0.5),
      optional_field("background_scatter_radius",
                     "number",
                     "preset",
                     "Radius the distant layer scatters over.",
                     1.0),
      optional_field("sway_strength", "number", "preset", "Wind sway amount.", 0.5),
      optional_field("sway_speed", "number", "preset", "Wind sway speed.", 1.0),
      optional_field("height_noise",
                     "[amp, freq]",
                     "preset",
                     "Ground height noise amplitude and frequency.",
                     QJsonArray{0.1, 0.05}),
      optional_field("grass_primary",
                     "[r, g, b]",
                     "preset",
                     "Main grass colour.",
                     QJsonArray{0.35, 0.45, 0.2}),
      optional_field("grass_secondary",
                     "[r, g, b]",
                     "preset",
                     "Secondary grass colour.",
                     QJsonArray{0.3, 0.4, 0.18}),
      optional_field("grass_dry",
                     "[r, g, b]",
                     "preset",
                     "Dry grass colour.",
                     QJsonArray{0.5, 0.45, 0.25}),
      optional_field("soil_color",
                     "[r, g, b]",
                     "preset",
                     "Exposed soil colour.",
                     QJsonArray{0.3, 0.22, 0.15}),
      optional_field("rock_low",
                     "[r, g, b]",
                     "preset",
                     "Rock colour at low slopes.",
                     QJsonArray{0.4, 0.4, 0.42}),
      optional_field("rock_high",
                     "[r, g, b]",
                     "preset",
                     "Rock colour at high slopes.",
                     QJsonArray{0.55, 0.55, 0.58}),
      optional_field("snow_color",
                     "[r, g, b]",
                     "preset",
                     "Snow colour where coverage applies.",
                     QJsonArray{0.9, 0.93, 0.96}),
      optional_field("snow_coverage", "number", "preset", "Snow amount, 0…1.", 0.0),
      optional_field(
          "moisture_level", "number", "preset", "Wetness of the ground, 0…1.", 0.5),
      optional_field(
          "crack_intensity", "number", "preset", "Dry cracking strength.", 0.0),
      optional_field(
          "rock_exposure", "number", "preset", "How much rock breaks through.", 0.3),
      optional_field(
          "grass_saturation", "number", "preset", "Grass colour saturation.", 1.0),
      optional_field(
          "soil_roughness", "number", "preset", "Soil specular roughness.", 0.8),
      optional_field(
          "plant_density", "number", "preset", "Scattered plant count.", 1.0),
      optional_field("spawn_edge_padding",
                     "number",
                     "preset",
                     "Cells kept clear of scatter around spawns.",
                     2.0),
      optional_field("terrain_macro_noise_scale",
                     "number",
                     "preset",
                     "Large-scale ground noise scale.",
                     0.05),
      optional_field("terrain_detail_noise_scale",
                     "number",
                     "preset",
                     "Fine ground noise scale.",
                     0.5),
      optional_field(
          "terrain_soil_height", "number", "preset", "Height soil blends in at.", 0.5),
      optional_field("terrain_soil_sharpness",
                     "number",
                     "preset",
                     "Sharpness of the soil blend.",
                     1.0),
      optional_field("terrain_rock_threshold",
                     "number",
                     "preset",
                     "Slope where rock takes over.",
                     0.6),
      optional_field("terrain_rock_sharpness",
                     "number",
                     "preset",
                     "Sharpness of the rock blend.",
                     1.0),
      optional_field("terrain_ambient_boost",
                     "number",
                     "preset",
                     "Extra ambient light on the ground.",
                     0.0),
      optional_field("terrain_rock_detail_strength",
                     "number",
                     "preset",
                     "Rock detail normal strength.",
                     1.0),
      optional_field("ground_irregularity_enabled",
                     "bool",
                     "preset",
                     "Enables the irregular ground edge.",
                     true),
      optional_field("irregularity_scale",
                     "number",
                     "preset",
                     "Scale of the irregular edge noise.",
                     0.1),
      optional_field("irregularity_amplitude",
                     "number",
                     "preset",
                     "Amplitude of the irregular edge noise.",
                     0.5),
  };

  return schema;
}

} // namespace MapEditor
