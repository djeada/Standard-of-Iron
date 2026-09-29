#include "serialization.h"

#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <qfiledevice.h>
#include <qglobal.h>
#include <qjsonarray.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "../core/component_economy.h"
#include "../core/death_sequence.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../formation/army_formation_registry.h"
#include "../map/biome_settings.h"
#include "../map/terrain.h"
#include "../map/terrain_service.h"
#include "../session/deterministic_rng.h"
#include "../session/session_context.h"
#include "../session/simulation_clock.h"
#include "../systems/owner_registry.h"
#include "entity_codec.h"

namespace Engine::Core {

auto Serialization::serialize_entity(const Entity* entity) -> QJsonObject {
  QJsonObject entity_obj;
  entity_obj["id"] = static_cast<qint64>(entity->get_id());

  EntityCodec::write_core(entity, entity_obj);
  write_movement(entity, entity_obj);
  EntityCodec::write_combat(entity, entity_obj);
  EntityCodec::write_ability(entity, entity_obj);
  EntityCodec::write_commander(entity, entity_obj);
  EntityCodec::write_status(entity, entity_obj);
  EntityCodec::write_structures(entity, entity_obj);
  EntityCodec::write_economy(entity, entity_obj);

  return entity_obj;
}

void Serialization::deserialize_entity(Entity* entity, const QJsonObject& json) {
  EntityCodec::read_core(entity, json);
  read_movement(entity, json);
  EntityCodec::read_combat(entity, json);
  EntityCodec::read_ability(entity, json);
  EntityCodec::read_commander(entity, json);
  EntityCodec::read_status(entity, json);
  EntityCodec::read_structures(entity, json);
  EntityCodec::read_economy(entity, json);
}

auto Serialization::read_capture_stamp(const QJsonDocument& doc) -> CaptureStamp {
  const QJsonObject world_obj = doc.object();
  if (!world_obj.contains(QLatin1String("captureTick"))) {
    return {};
  }
  return {.present = true,
          .tick = static_cast<std::uint64_t>(
              world_obj[QLatin1String("captureTick")].toVariant().toULongLong()),
          .rng_draws = static_cast<std::uint64_t>(
              world_obj[QLatin1String("captureRngDraws")].toVariant().toULongLong())};
}

namespace {

void write_terrain(QJsonObject& world_obj,
                   const Game::Session::SessionContext& session) {
  const auto& terrain_service = session.terrain();
  if (terrain_service.is_initialized() &&
      (terrain_service.get_height_map() != nullptr)) {
    world_obj["terrain"] =
        Serialization::serialize_terrain(terrain_service.get_height_map(),
                                         terrain_service.biome_settings(),
                                         terrain_service.road_segments(),
                                         terrain_service.world_props(),
                                         terrain_service.authored_world_props());
  }
}

void read_entities(World* world, const QJsonObject& world_obj) {
  auto entities_array = world_obj["entities"].toArray();
  for (const auto value : entities_array) {
    auto entity_obj = value.toObject();
    const auto entity_id =
        static_cast<EntityID>(entity_obj["id"].toVariant().toULongLong());
    auto* entity = entity_id == NULL_ENTITY ? world->create_entity()
                                            : world->create_entity_with_id(entity_id);
    if (entity != nullptr) {
      Serialization::deserialize_entity(entity, entity_obj);
    }
  }

  if (world_obj.contains("nextEntityId")) {
    const auto next_id =
        static_cast<EntityID>(world_obj["nextEntityId"].toVariant().toULongLong());
    world->set_next_entity_id(next_id);
  }
}

void read_terrain(Game::Session::SessionContext& session,
                  const QJsonObject& world_obj) {
  if (!world_obj.contains("terrain")) {
    return;
  }

  const auto terrain_obj = world_obj["terrain"].toObject();
  const int width = terrain_obj["width"].toInt(50);
  const int height = terrain_obj["height"].toInt(50);
  const float tile_size = static_cast<float>(terrain_obj["tile_size"].toDouble(1.0));

  Game::Map::BiomeSettings biome;
  std::vector<Game::Map::RoadSegment> roads;
  std::vector<Game::Map::WorldProp> world_props;
  std::vector<Game::Map::WorldProp> authored_world_props;

  auto temp_height_map =
      std::make_unique<Game::Map::TerrainHeightMap>(width, height, tile_size);
  Serialization::deserialize_terrain(temp_height_map.get(),
                                     biome,
                                     roads,
                                     world_props,
                                     authored_world_props,
                                     terrain_obj);

  auto& terrain_service = session.terrain();
  terrain_service.restore_from_serialized(width,
                                          height,
                                          tile_size,
                                          temp_height_map->get_height_data(),
                                          temp_height_map->getTerrainTypes(),
                                          temp_height_map->get_river_segments(),
                                          roads,
                                          temp_height_map->get_bridges(),
                                          biome,
                                          world_props,
                                          authored_world_props,
                                          temp_height_map->get_lakes(),
                                          temp_height_map->hill_navigation());
}

} // namespace

auto Serialization::serialize_world(const World* world) -> QJsonDocument {
  QJsonObject world_obj;
  QJsonArray entities_array;

  world->for_each_entity([&entities_array](Entity& entity) {
    if (entity.get_component<ConstructionPreviewComponent>() != nullptr) {
      return;
    }

    if (is_collapsing_structure(entity)) {
      return;
    }
    entities_array.append(serialize_entity(&entity));
  });

  auto& session = Game::Session::session_for(*world);
  world_obj["entities"] = entities_array;
  world_obj["nextEntityId"] = static_cast<qint64>(world->get_next_entity_id());
  world_obj["schemaVersion"] = 2;
  world_obj["captureTick"] = static_cast<qint64>(session.clock().tick());
  world_obj["captureRngDraws"] = static_cast<qint64>(session.rng().draw_count());
  world_obj["owner_registry"] = session.owners().to_json();
  world_obj["army_formations"] = session.army_formations().to_json();
  write_terrain(world_obj, session);

  return QJsonDocument(world_obj);
}

void Serialization::deserialize_world(World* world, const QJsonDocument& doc) {
  auto world_obj = doc.object();
  read_entities(world, world_obj);

  auto& session = Game::Session::session_for(*world);
  if (world_obj.contains("owner_registry")) {
    session.owners().from_json(world_obj["owner_registry"].toObject());
  }

  session.army_formations().from_json(world_obj["army_formations"].toObject());

  read_terrain(session, world_obj);
}

auto Serialization::save_to_file(const QString& filename,
                                 const QJsonDocument& doc) -> bool {
  QFile file(filename);
  if (!file.open(QIODevice::WriteOnly)) {
    qWarning() << "Could not open file for writing:" << filename;
    return false;
  }
  file.write(doc.toJson());
  return true;
}

auto Serialization::load_from_file(const QString& filename) -> QJsonDocument {
  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "Could not open file for reading:" << filename;
    return {};
  }
  const QByteArray data = file.readAll();
  return QJsonDocument::fromJson(data);
}

} // namespace Engine::Core
