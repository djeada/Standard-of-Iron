#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "app/session/skirmish_loader.h"
#include "core/world.h"
#include "game/core/ambient_session.h"
#include "game/map/base_options.h"
#include "game/map/map_catalog.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/systems/build_site.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/default_content.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"

namespace {

constexpr float k_farm_reach = 42.0F;
constexpr float k_lattice_step = 2.0F;
constexpr int k_plots_every_base_needs = 2;
constexpr int k_plots_every_seat_needs = 3;

auto skirmish_map_paths() -> QStringList {
  QStringList paths;
  for (const QVariant& entry : Game::Map::MapCatalog::available_maps()) {
    const QString path = entry.toMap().value(QStringLiteral("path")).toString();
    if (!path.isEmpty()) {
      paths.append(path);
    }
  }
  return paths;
}

auto seat_everyone(const Game::Map::MapDefinition& def) -> QVariantList {
  std::set<int> seats;
  for (const auto& option : Game::Map::collect_base_options(def)) {
    if (option.default_player_id > 0) {
      seats.insert(option.default_player_id);
    }
  }
  QVariantList configs;
  for (const int seat : seats) {
    QVariantMap config;
    config["player_id"] = seat;
    config["team_id"] = seat;
    config["colorHex"] = QStringLiteral("#808080");
    config["isHuman"] = seat == 1;
    config["nationId"] = seat % 2 == 1 ? QStringLiteral("roman_republic")
                                       : QStringLiteral("carthaginian_empire");
    configs.append(config);
  }
  return configs;
}

auto verdict_name(Game::Systems::GroundVerdict verdict) -> const char* {
  switch (verdict) {
  case Game::Systems::GroundVerdict::Clear:
    return "clear";
  case Game::Systems::GroundVerdict::Occupied:
    return "occupied";
  case Game::Systems::GroundVerdict::Impassable:
    return "impassable";
  case Game::Systems::GroundVerdict::Water:
    return "water";
  case Game::Systems::GroundVerdict::Uneven:
    return "uneven";
  case Game::Systems::GroundVerdict::OffMap:
    return "off-map";
  }
  return "?";
}

constexpr std::array<const char*, 6> k_skirmish_maps = {
    "map_amber_delta.json",
    "map_copper_canyons.json",
    "map_forest.json",
    "map_mountain.json",
    "map_rivers.json",
    "map_spanish_grove.json",
};

class SkirmishFarmlandTest : public ::testing::TestWithParam<const char*> {
protected:
  void SetUp() override {
    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    Game::Systems::initialize_default_content(nations);
    Game::Systems::OwnerRegistry::instance().clear();
    Game::Systems::GlobalStatsRegistry::instance().clear();
  }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    Game::Map::VisibilityService::instance().reset();
    Game::Systems::GlobalStatsRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().clear();
  }
};

} // namespace

TEST(SkirmishFarmlandCatalogTest, EveryShippedSkirmishMapIsMeasured) {
  QStringList measured;
  for (const char* name : k_skirmish_maps) {
    measured.append(QString::fromLatin1(name));
  }
  for (const QString& path : skirmish_map_paths()) {
    const QString name = path.section(QLatin1Char('/'), -1);
    EXPECT_TRUE(measured.contains(name))
        << name.toStdString()
        << " is offered in skirmish but k_skirmish_maps does not measure its bases";
  }
}

TEST_P(SkirmishFarmlandTest, EveryBaseHasRoomForFieldsWithinAShortWalk) {
  const QStringList paths{
      QStringLiteral("assets/maps/%1").arg(QString::fromLatin1(GetParam()))};

  const auto farm = Game::Systems::BuildingCollisionRegistry::get_building_size("farm");
  const float spacing_x = farm.width + 0.8F;
  const float spacing_z = farm.depth + 0.8F;

  for (const QString& path : paths) {
    Game::Map::MapDefinition def;
    QString error;
    ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(path, def, &error))
        << path.toStdString() << ": " << error.toStdString();

    Engine::Core::World world;
    Render::GL::Renderer renderer{Render::ShaderQuality::None};
    Render::GL::Camera camera;
    App::Core::SkirmishLoader loader{world, renderer, camera};
    Game::Systems::register_runtime_systems(world);
    int selected_player_id = 1;
    const auto result =
        loader.start(path, seat_everyone(def), 1, true, selected_player_id);
    ASSERT_TRUE(result.ok) << path.toStdString() << ": "
                           << result.error_message.toStdString();

    for (const auto& base : Game::Map::collect_base_options(def)) {
      const int needed = base.default_player_id > 0 ? k_plots_every_seat_needs
                                                    : k_plots_every_base_needs;
      std::vector<QVector3D> plots;
      std::map<std::string, int> refusals;
      const int reach_steps = static_cast<int>(k_farm_reach / k_lattice_step);
      std::vector<std::pair<float, QVector3D>> candidates;
      for (int ix = -reach_steps; ix <= reach_steps; ++ix) {
        for (int iz = -reach_steps; iz <= reach_steps; ++iz) {
          const float dx = static_cast<float>(ix) * k_lattice_step;
          const float dz = static_cast<float>(iz) * k_lattice_step;
          const float distance = std::hypot(dx, dz);
          if (distance > k_farm_reach) {
            continue;
          }
          candidates.emplace_back(
              distance,
              QVector3D(base.position.x() + dx, 0.0F, base.position.z() + dz));
        }
      }
      const auto* height_map =
          Game::Session::services_for(world).terrain->get_height_map();
      const auto footprint_walkable = [&](const QVector3D& site) {
        if (height_map == nullptr) {
          return true;
        }
        const float tile = height_map->get_tile_size();
        const float half = farm.width * 0.5F;
        for (const auto& [ox, oz] : {std::pair{0.0F, 0.0F},
                                     std::pair{-half, -half},
                                     std::pair{half, -half},
                                     std::pair{-half, half},
                                     std::pair{half, half}}) {
          const int gx = static_cast<int>(std::lround(
              (site.x() + ox) / tile + (height_map->get_width() * 0.5F - 0.5F)));
          const int gz = static_cast<int>(std::lround(
              (site.z() + oz) / tile + (height_map->get_height() * 0.5F - 0.5F)));
          if (!height_map->is_walkable(gx, gz)) {
            return false;
          }
        }
        return true;
      };
      struct NearProp {
        float x;
        float z;
        float radius;
      };
      std::vector<NearProp> near_props;
      const auto& terrain = *Game::Session::services_for(world).terrain;
      for (const auto& prop : terrain.world_props()) {
        if (!Game::Map::is_solid_world_prop_type(prop.type)) {
          continue;
        }
        const auto [px, pz] = terrain.world_prop_world_xz(prop);
        if (std::hypot(px - base.position.x(), pz - base.position.z()) <
            k_farm_reach + farm.width) {
          near_props.push_back(
              {px, pz, Game::Map::world_prop_ground_radius(prop.type, prop.scale)});
        }
      }
      const auto prop_in_the_way = [&](const QVector3D& site) {
        const float half = (farm.width * 0.5F) + 0.35F;
        return std::any_of(
            near_props.begin(), near_props.end(), [&](const NearProp& prop) {
              return std::abs(prop.x - site.x()) < half + prop.radius &&
                     std::abs(prop.z - site.z()) < half + prop.radius;
            });
      };
      std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
      });
      for (const auto& [distance, site] : candidates) {
        if (static_cast<int>(plots.size()) >= needed) {
          break;
        }
        const bool overlaps =
            std::any_of(plots.begin(), plots.end(), [&](const QVector3D& plot) {
              return std::abs(plot.x() - site.x()) < spacing_x &&
                     std::abs(plot.z() - site.z()) < spacing_z;
            });
        if (overlaps) {
          continue;
        }
        if (!footprint_walkable(site)) {
          ++refusals["unwalkable"];
          continue;
        }
        if (prop_in_the_way(site)) {
          ++refusals["prop"];
          continue;
        }
        const auto verdict =
            Game::Systems::assess_ground(world, "farm", site.x(), site.z());
        if (verdict == Game::Systems::GroundVerdict::Clear) {
          plots.push_back(site);
        } else {
          ++refusals[verdict_name(verdict)];
        }
      }

      std::string why;
      for (const auto& [reason, count] : refusals) {
        why += " " + reason + "=" + std::to_string(count);
      }
      EXPECT_GE(static_cast<int>(plots.size()), needed)
          << path.toStdString() << " base " << base.key.toStdString() << " has "
          << plots.size() << " farm plot(s) within " << k_farm_reach
          << " m; a player starting there cannot feed a household. Refusals:" << why;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(ShippedSkirmishMaps,
                         SkirmishFarmlandTest,
                         ::testing::ValuesIn(k_skirmish_maps),
                         [](const ::testing::TestParamInfo<const char*>& info) {
                           std::string name = info.param;
                           name = name.substr(4, name.size() - 9);
                           return name;
                         });
