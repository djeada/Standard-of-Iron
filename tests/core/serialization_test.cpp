#include "core/component.h"
#include "core/entity.h"
#include "core/serialization.h"
#include "core/world.h"
#include "systems/nation_id.h"
#include "systems/owner_registry.h"
#include "units/spawn_type.h"
#include "units/troop_type.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>
#include <gtest/gtest.h>

using namespace Engine::Core;

class SerializationTest : public ::testing::Test {
protected:
  void SetUp() override { world = std::make_unique<World>(); }

  void TearDown() override { world.reset(); }

  std::unique_ptr<World> world;
};

TEST_F(SerializationTest, EntitySerializationBasic) {
  auto *entity = world->create_entity();
  ASSERT_NE(entity, nullptr);

  auto entity_id = entity->get_id();

  QJsonObject json = Serialization::serialize_entity(entity);

  EXPECT_TRUE(json.contains("id"));
  EXPECT_EQ(json["id"].toVariant().toULongLong(),
            static_cast<qulonglong>(entity_id));
}

TEST_F(SerializationTest, TransformComponentSerialization) {
  auto *entity = world->create_entity();
  auto *transform = entity->add_component<TransformComponent>();

  transform->position.x = 10.5F;
  transform->position.y = 20.3F;
  transform->position.z = 30.1F;
  transform->rotation.x = 0.5F;
  transform->rotation.y = 1.0F;
  transform->rotation.z = 1.5F;
  transform->scale.x = 2.0F;
  transform->scale.y = 2.5F;
  transform->scale.z = 3.0F;
  transform->has_desired_yaw = true;
  transform->desired_yaw = 45.0F;

  QJsonObject json = Serialization::serialize_entity(entity);

  ASSERT_TRUE(json.contains("transform"));
  QJsonObject transform_obj = json["transform"].toObject();

  EXPECT_FLOAT_EQ(transform_obj["pos_x"].toDouble(), 10.5);
  EXPECT_FLOAT_EQ(transform_obj["pos_y"].toDouble(), 20.3);
  EXPECT_FLOAT_EQ(transform_obj["pos_z"].toDouble(), 30.1);
  EXPECT_FLOAT_EQ(transform_obj["rot_x"].toDouble(), 0.5);
  EXPECT_FLOAT_EQ(transform_obj["rot_y"].toDouble(), 1.0);
  EXPECT_FLOAT_EQ(transform_obj["rot_z"].toDouble(), 1.5);
  EXPECT_FLOAT_EQ(transform_obj["scale_x"].toDouble(), 2.0);
  EXPECT_FLOAT_EQ(transform_obj["scale_y"].toDouble(), 2.5);
  EXPECT_FLOAT_EQ(transform_obj["scale_z"].toDouble(), 3.0);
  EXPECT_TRUE(transform_obj["has_desired_yaw"].toBool());
  EXPECT_FLOAT_EQ(transform_obj["desired_yaw"].toDouble(), 45.0);
}

TEST_F(SerializationTest, TransformComponentRoundTrip) {
  auto *original_entity = world->create_entity();
  auto *transform = original_entity->add_component<TransformComponent>();
  transform->position.x = 15.0F;
  transform->position.y = 25.0F;
  transform->position.z = 35.0F;
  transform->rotation.x = 1.0F;
  transform->rotation.y = 2.0F;
  transform->rotation.z = 3.0F;
  transform->scale.x = 1.5F;
  transform->scale.y = 2.5F;
  transform->scale.z = 3.5F;
  transform->has_desired_yaw = true;
  transform->desired_yaw = 90.0F;

  QJsonObject json = Serialization::serialize_entity(original_entity);

  auto *new_entity = world->create_entity();
  Serialization::deserialize_entity(new_entity, json);

  auto *deserialized = new_entity->get_component<TransformComponent>();
  ASSERT_NE(deserialized, nullptr);
  EXPECT_FLOAT_EQ(deserialized->position.x, 15.0F);
  EXPECT_FLOAT_EQ(deserialized->position.y, 25.0F);
  EXPECT_FLOAT_EQ(deserialized->position.z, 35.0F);
  EXPECT_FLOAT_EQ(deserialized->rotation.x, 1.0F);
  EXPECT_FLOAT_EQ(deserialized->rotation.y, 2.0F);
  EXPECT_FLOAT_EQ(deserialized->rotation.z, 3.0F);
  EXPECT_FLOAT_EQ(deserialized->scale.x, 1.5F);
  EXPECT_FLOAT_EQ(deserialized->scale.y, 2.5F);
  EXPECT_FLOAT_EQ(deserialized->scale.z, 3.5F);
  EXPECT_TRUE(deserialized->has_desired_yaw);
  EXPECT_FLOAT_EQ(deserialized->desired_yaw, 90.0F);
}

TEST_F(SerializationTest, UnitComponentSerialization) {
  auto *entity = world->create_entity();
  auto *unit = entity->add_component<UnitComponent>();

  unit->health = 80;
  unit->max_health = 100;
  unit->speed = 5.5F;
  unit->vision_range = 15.0F;
  unit->spawn_type = Game::Units::SpawnType::Archer;
  unit->owner_id = 1;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;

  QJsonObject json = Serialization::serialize_entity(entity);

  ASSERT_TRUE(json.contains("unit"));
  QJsonObject unit_obj = json["unit"].toObject();

  EXPECT_EQ(unit_obj["health"].toInt(), 80);
  EXPECT_EQ(unit_obj["max_health"].toInt(), 100);
  EXPECT_FLOAT_EQ(unit_obj["speed"].toDouble(), 5.5);
  EXPECT_FLOAT_EQ(unit_obj["vision_range"].toDouble(), 15.0);
  EXPECT_EQ(unit_obj["unit_type"].toString(), QString("archer"));
  EXPECT_EQ(unit_obj["owner_id"].toInt(), 1);
  EXPECT_EQ(unit_obj["nation_id"].toString(), QString("roman_republic"));
}

TEST_F(SerializationTest, UnitComponentRoundTrip) {
  auto *original_entity = world->create_entity();
  auto *unit = original_entity->add_component<UnitComponent>();
  unit->health = 75;
  unit->max_health = 150;
  unit->speed = 7.5F;
  unit->vision_range = 20.0F;
  unit->spawn_type = Game::Units::SpawnType::Spearman;
  unit->owner_id = 2;
  unit->nation_id = Game::Systems::NationID::Carthage;

  QJsonObject json = Serialization::serialize_entity(original_entity);

  auto *new_entity = world->create_entity();
  Serialization::deserialize_entity(new_entity, json);

  auto *deserialized = new_entity->get_component<UnitComponent>();
  ASSERT_NE(deserialized, nullptr);
  EXPECT_EQ(deserialized->health, 75);
  EXPECT_EQ(deserialized->max_health, 150);
  EXPECT_FLOAT_EQ(deserialized->speed, 7.5F);
  EXPECT_FLOAT_EQ(deserialized->vision_range, 20.0F);
  EXPECT_EQ(deserialized->spawn_type, Game::Units::SpawnType::Spearman);
  EXPECT_EQ(deserialized->owner_id, 2);
  EXPECT_EQ(deserialized->nation_id, Game::Systems::NationID::Carthage);
}

TEST_F(SerializationTest, MovementComponentSerialization) {
  auto *entity = world->create_entity();
  auto *movement = entity->add_component<MovementComponent>();

  movement->has_target = true;
  movement->target_x = 50.0F;
  movement->target_y = 60.0F;
  movement->goal_x = 55.0F;
  movement->goal_y = 65.0F;
  movement->vx = 1.5F;
  movement->vz = 2.0F;
  movement->path_pending = false;
  movement->pending_request_id = 42;
  movement->repath_cooldown = 1.0F;
  movement->last_goal_x = 45.0F;
  movement->last_goal_y = 55.0F;
  movement->time_since_last_path_request = 0.5F;

  movement->path.emplace_back(10.0F, 20.0F);
  movement->path.emplace_back(30.0F, 40.0F);

  QJsonObject json = Serialization::serialize_entity(entity);

  ASSERT_TRUE(json.contains("movement"));
  QJsonObject movement_obj = json["movement"].toObject();

  EXPECT_TRUE(movement_obj["has_target"].toBool());
  EXPECT_FLOAT_EQ(movement_obj["target_x"].toDouble(), 50.0);
  EXPECT_FLOAT_EQ(movement_obj["target_y"].toDouble(), 60.0);
  EXPECT_FLOAT_EQ(movement_obj["goal_x"].toDouble(), 55.0);
  EXPECT_FLOAT_EQ(movement_obj["goal_y"].toDouble(), 65.0);
  EXPECT_FLOAT_EQ(movement_obj["vx"].toDouble(), 1.5);
  EXPECT_FLOAT_EQ(movement_obj["vz"].toDouble(), 2.0);
  EXPECT_FALSE(movement_obj["path_pending"].toBool());
  EXPECT_EQ(movement_obj["pending_request_id"].toVariant().toULongLong(),
            42ULL);

  ASSERT_TRUE(movement_obj.contains("path"));
  QJsonArray path_array = movement_obj["path"].toArray();
  ASSERT_EQ(path_array.size(), 2);

  QJsonObject waypoint1 = path_array[0].toObject();
  EXPECT_FLOAT_EQ(waypoint1["x"].toDouble(), 10.0);
  EXPECT_FLOAT_EQ(waypoint1["y"].toDouble(), 20.0);

  QJsonObject waypoint2 = path_array[1].toObject();
  EXPECT_FLOAT_EQ(waypoint2["x"].toDouble(), 30.0);
  EXPECT_FLOAT_EQ(waypoint2["y"].toDouble(), 40.0);
}

TEST_F(SerializationTest, AttackComponentSerialization) {
  auto *entity = world->create_entity();
  auto *attack = entity->add_component<AttackComponent>();

  attack->range = 10.0F;
  attack->damage = 25;
  attack->cooldown = 2.0F;
  attack->time_since_last = 0.5F;
  attack->melee_range = 2.0F;
  attack->melee_damage = 15;
  attack->melee_cooldown = 1.5F;
  attack->preferred_mode = AttackComponent::CombatMode::Ranged;
  attack->current_mode = AttackComponent::CombatMode::Ranged;
  attack->can_melee = true;
  attack->can_ranged = true;
  attack->max_height_difference = 5.0F;
  attack->in_melee_lock = false;
  attack->melee_lock_target_id = 0;

  QJsonObject json = Serialization::serialize_entity(entity);

  ASSERT_TRUE(json.contains("attack"));
  QJsonObject attack_obj = json["attack"].toObject();

  EXPECT_FLOAT_EQ(attack_obj["range"].toDouble(), 10.0);
  EXPECT_EQ(attack_obj["damage"].toInt(), 25);
  EXPECT_FLOAT_EQ(attack_obj["cooldown"].toDouble(), 2.0);
  EXPECT_FLOAT_EQ(attack_obj["time_since_last"].toDouble(), 0.5);
  EXPECT_FLOAT_EQ(attack_obj["melee_range"].toDouble(), 2.0);
  EXPECT_EQ(attack_obj["melee_damage"].toInt(), 15);
  EXPECT_FLOAT_EQ(attack_obj["melee_cooldown"].toDouble(), 1.5);
  EXPECT_EQ(attack_obj["preferred_mode"].toString(), QString("ranged"));
  EXPECT_EQ(attack_obj["current_mode"].toString(), QString("ranged"));
  EXPECT_TRUE(attack_obj["can_melee"].toBool());
  EXPECT_TRUE(attack_obj["can_ranged"].toBool());
  EXPECT_FLOAT_EQ(attack_obj["max_height_difference"].toDouble(), 5.0);
  EXPECT_FALSE(attack_obj["in_melee_lock"].toBool());
}

TEST_F(SerializationTest, EntityDeserializationRoundTrip) {
  auto *original_entity = world->create_entity();
  auto *transform = original_entity->add_component<TransformComponent>();
  transform->position.x = 100.0F;
  transform->position.y = 200.0F;
  transform->position.z = 300.0F;

  auto *unit = original_entity->add_component<UnitComponent>();
  unit->health = 75;
  unit->max_health = 100;
  unit->speed = 6.0F;

  QJsonObject json = Serialization::serialize_entity(original_entity);

  auto *new_entity = world->create_entity();
  Serialization::deserialize_entity(new_entity, json);

  auto *deserialized_transform =
      new_entity->get_component<TransformComponent>();
  ASSERT_NE(deserialized_transform, nullptr);
  EXPECT_FLOAT_EQ(deserialized_transform->position.x, 100.0F);
  EXPECT_FLOAT_EQ(deserialized_transform->position.y, 200.0F);
  EXPECT_FLOAT_EQ(deserialized_transform->position.z, 300.0F);

  auto *deserialized_unit = new_entity->get_component<UnitComponent>();
  ASSERT_NE(deserialized_unit, nullptr);
  EXPECT_EQ(deserialized_unit->health, 75);
  EXPECT_EQ(deserialized_unit->max_health, 100);
  EXPECT_FLOAT_EQ(deserialized_unit->speed, 6.0F);
}

TEST_F(SerializationTest, DeserializationWithMissingFields) {
  QJsonObject json;
  json["id"] = 1;

  QJsonObject unit_obj;
  unit_obj["health"] = 50;
  json["unit"] = unit_obj;

  auto *entity = world->create_entity();
  Serialization::deserialize_entity(entity, json);

  auto *unit = entity->get_component<UnitComponent>();
  ASSERT_NE(unit, nullptr);
  EXPECT_EQ(unit->health, 50);
  EXPECT_EQ(unit->max_health, Defaults::kUnitDefaultHealth);
}

TEST_F(SerializationTest, DeserializationWithMalformedJSON) {
  QJsonObject json;
  json["id"] = 1;

  QJsonObject transform_obj;
  transform_obj["pos_x"] = "not_a_number";
  json["transform"] = transform_obj;

  auto *entity = world->create_entity();

  EXPECT_NO_THROW({ Serialization::deserialize_entity(entity, json); });

  auto *transform = entity->get_component<TransformComponent>();
  ASSERT_NE(transform, nullptr);
  EXPECT_FLOAT_EQ(transform->position.x, 0.0F);
}

TEST_F(SerializationTest, WorldSerializationRoundTrip) {
  auto *entity1 = world->create_entity();
  auto *transform1 = entity1->add_component<TransformComponent>();
  transform1->position.x = 10.0F;

  auto *entity2 = world->create_entity();
  auto *transform2 = entity2->add_component<TransformComponent>();
  transform2->position.x = 20.0F;

  QJsonDocument doc = Serialization::serialize_world(world.get());

  ASSERT_TRUE(doc.isObject());
  QJsonObject world_obj = doc.object();
  EXPECT_TRUE(world_obj.contains("entities"));
  EXPECT_TRUE(world_obj.contains("nextEntityId"));
  EXPECT_TRUE(world_obj.contains("schemaVersion"));

  auto new_world = std::make_unique<World>();
  Serialization::deserialize_world(new_world.get(), doc);

  const auto &entities = new_world->get_entities();
  EXPECT_EQ(entities.size(), 2UL);
}

TEST_F(SerializationTest, SaveAndLoadFromFile) {
  auto *entity = world->create_entity();
  auto *transform = entity->add_component<TransformComponent>();
  transform->position.x = 42.0F;
  transform->position.y = 43.0F;
  transform->position.z = 44.0F;

  QJsonDocument doc = Serialization::serialize_world(world.get());

  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());
  QString filename = temp_file.fileName();
  temp_file.close();

  EXPECT_TRUE(Serialization::save_to_file(filename, doc));

  QJsonDocument loaded_doc = Serialization::load_from_file(filename);
  EXPECT_FALSE(loaded_doc.isNull());

  auto new_world = std::make_unique<World>();
  Serialization::deserialize_world(new_world.get(), loaded_doc);

  const auto &entities = new_world->get_entities();
  EXPECT_EQ(entities.size(), 1UL);

  if (!entities.empty()) {
    auto *loaded_entity = entities.begin()->second.get();
    auto *loaded_transform = loaded_entity->get_component<TransformComponent>();
    ASSERT_NE(loaded_transform, nullptr);
    EXPECT_FLOAT_EQ(loaded_transform->position.x, 42.0F);
    EXPECT_FLOAT_EQ(loaded_transform->position.y, 43.0F);
    EXPECT_FLOAT_EQ(loaded_transform->position.z, 44.0F);
  }
}

TEST_F(SerializationTest, ProductionComponentSerialization) {
  auto *entity = world->create_entity();
  auto *production = entity->add_component<ProductionComponent>();

  production->in_progress = true;
  production->build_time = 10.0F;
  production->time_remaining = 5.0F;
  production->produced_count = 3;
  production->max_units = 10;
  production->product_type = Game::Units::TroopType::Archer;
  production->rally_x = 100.0F;
  production->rally_z = 200.0F;
  production->rally_set = true;
  production->villager_cost = 2;
  production->production_queue.push_back(Game::Units::TroopType::Spearman);
  production->production_queue.push_back(Game::Units::TroopType::Archer);

  QJsonObject json = Serialization::serialize_entity(entity);

  ASSERT_TRUE(json.contains("production"));
  QJsonObject prod_obj = json["production"].toObject();

  EXPECT_TRUE(prod_obj["in_progress"].toBool());
  EXPECT_FLOAT_EQ(prod_obj["build_time"].toDouble(), 10.0);
  EXPECT_FLOAT_EQ(prod_obj["time_remaining"].toDouble(), 5.0);
  EXPECT_EQ(prod_obj["produced_count"].toInt(), 3);
  EXPECT_EQ(prod_obj["max_units"].toInt(), 10);
  EXPECT_EQ(prod_obj["product_type"].toString(), QString("archer"));
  EXPECT_FLOAT_EQ(prod_obj["rally_x"].toDouble(), 100.0);
  EXPECT_FLOAT_EQ(prod_obj["rally_z"].toDouble(), 200.0);
  EXPECT_TRUE(prod_obj["rally_set"].toBool());
  EXPECT_EQ(prod_obj["villager_cost"].toInt(), 2);

  ASSERT_TRUE(prod_obj.contains("queue"));
  QJsonArray queue = prod_obj["queue"].toArray();
  EXPECT_EQ(queue.size(), 2);
  EXPECT_EQ(queue[0].toString(), QString("spearman"));
  EXPECT_EQ(queue[1].toString(), QString("archer"));
}

TEST_F(SerializationTest, PatrolComponentSerialization) {
  auto *entity = world->create_entity();
  auto *patrol = entity->add_component<PatrolComponent>();

  patrol->current_waypoint = 1;
  patrol->patrolling = true;
  patrol->waypoints.emplace_back(10.0F, 20.0F);
  patrol->waypoints.emplace_back(30.0F, 40.0F);
  patrol->waypoints.emplace_back(50.0F, 60.0F);

  QJsonObject json = Serialization::serialize_entity(entity);

  ASSERT_TRUE(json.contains("patrol"));
  QJsonObject patrol_obj = json["patrol"].toObject();

  EXPECT_EQ(patrol_obj["current_waypoint"].toInt(), 1);
  EXPECT_TRUE(patrol_obj["patrolling"].toBool());

  ASSERT_TRUE(patrol_obj.contains("waypoints"));
  QJsonArray waypoints = patrol_obj["waypoints"].toArray();
  EXPECT_EQ(waypoints.size(), 3);

  QJsonObject wp0 = waypoints[0].toObject();
  EXPECT_FLOAT_EQ(wp0["x"].toDouble(), 10.0);
  EXPECT_FLOAT_EQ(wp0["y"].toDouble(), 20.0);
}

TEST_F(SerializationTest, PatrolComponentRoundTrip) {
  auto *original_entity = world->create_entity();
  auto *patrol = original_entity->add_component<PatrolComponent>();
  patrol->current_waypoint = 2;
  patrol->patrolling = true;
  patrol->waypoints.emplace_back(15.0F, 25.0F);
  patrol->waypoints.emplace_back(35.0F, 45.0F);

  QJsonObject json = Serialization::serialize_entity(original_entity);

  auto *new_entity = world->create_entity();
  Serialization::deserialize_entity(new_entity, json);

  auto *deserialized = new_entity->get_component<PatrolComponent>();
  ASSERT_NE(deserialized, nullptr);
  EXPECT_EQ(deserialized->current_waypoint, 2UL);
  EXPECT_TRUE(deserialized->patrolling);
  EXPECT_EQ(deserialized->waypoints.size(), 2UL);
  EXPECT_FLOAT_EQ(deserialized->waypoints[0].first, 15.0F);
  EXPECT_FLOAT_EQ(deserialized->waypoints[0].second, 25.0F);
}

TEST_F(SerializationTest, MovementComponentRoundTrip) {
  auto *original_entity = world->create_entity();
  auto *movement = original_entity->add_component<MovementComponent>();
  movement->has_target = true;
  movement->target_x = 100.0F;
  movement->target_y = 200.0F;
  movement->goal_x = 150.0F;
  movement->goal_y = 250.0F;
  movement->vx = 1.5F;
  movement->vz = 2.5F;
  movement->path.emplace_back(10.0F, 20.0F);
  movement->path.emplace_back(30.0F, 40.0F);

  QJsonObject json = Serialization::serialize_entity(original_entity);

  auto *new_entity = world->create_entity();
  Serialization::deserialize_entity(new_entity, json);

  auto *deserialized = new_entity->get_component<MovementComponent>();
  ASSERT_NE(deserialized, nullptr);
  EXPECT_TRUE(deserialized->has_target);
  EXPECT_FLOAT_EQ(deserialized->target_x, 100.0F);
  EXPECT_FLOAT_EQ(deserialized->target_y, 200.0F);
  EXPECT_FLOAT_EQ(deserialized->goal_x, 150.0F);
  EXPECT_FLOAT_EQ(deserialized->goal_y, 250.0F);
  EXPECT_FLOAT_EQ(deserialized->vx, 1.5F);
  EXPECT_FLOAT_EQ(deserialized->vz, 2.5F);
  EXPECT_EQ(deserialized->path.size(), 2UL);
}

TEST_F(SerializationTest, AttackComponentRoundTrip) {
  auto *original_entity = world->create_entity();
  auto *attack = original_entity->add_component<AttackComponent>();
  attack->range = 15.0F;
  attack->damage = 30;
  attack->cooldown = 2.5F;
  attack->melee_range = 3.0F;
  attack->melee_damage = 20;
  attack->preferred_mode = AttackComponent::CombatMode::Ranged;
  attack->current_mode = AttackComponent::CombatMode::Melee;
  attack->can_melee = true;
  attack->can_ranged = true;
  attack->in_melee_lock = true;
  attack->melee_lock_target_id = 42;

  QJsonObject json = Serialization::serialize_entity(original_entity);

  auto *new_entity = world->create_entity();
  Serialization::deserialize_entity(new_entity, json);

  auto *deserialized = new_entity->get_component<AttackComponent>();
  ASSERT_NE(deserialized, nullptr);
  EXPECT_FLOAT_EQ(deserialized->range, 15.0F);
  EXPECT_EQ(deserialized->damage, 30);
  EXPECT_FLOAT_EQ(deserialized->cooldown, 2.5F);
  EXPECT_FLOAT_EQ(deserialized->melee_range, 3.0F);
  EXPECT_EQ(deserialized->melee_damage, 20);
  EXPECT_EQ(deserialized->preferred_mode, AttackComponent::CombatMode::Ranged);
  EXPECT_EQ(deserialized->current_mode, AttackComponent::CombatMode::Melee);
  EXPECT_TRUE(deserialized->can_melee);
  EXPECT_TRUE(deserialized->can_ranged);
  EXPECT_TRUE(deserialized->in_melee_lock);
  EXPECT_EQ(deserialized->melee_lock_target_id, 42U);
}

TEST_F(SerializationTest, ProductionComponentRoundTrip) {
  auto *original_entity = world->create_entity();
  auto *production = original_entity->add_component<ProductionComponent>();
  production->in_progress = true;
  production->build_time = 15.0F;
  production->time_remaining = 7.5F;
  production->produced_count = 5;
  production->max_units = 20;
  production->product_type = Game::Units::TroopType::Spearman;
  production->rally_x = 150.0F;
  production->rally_z = 250.0F;
  production->rally_set = true;
  production->villager_cost = 3;
  production->production_queue.push_back(Game::Units::TroopType::Archer);

  QJsonObject json = Serialization::serialize_entity(original_entity);

  auto *new_entity = world->create_entity();
  Serialization::deserialize_entity(new_entity, json);

  auto *deserialized = new_entity->get_component<ProductionComponent>();
  ASSERT_NE(deserialized, nullptr);
  EXPECT_TRUE(deserialized->in_progress);
  EXPECT_FLOAT_EQ(deserialized->build_time, 15.0F);
  EXPECT_FLOAT_EQ(deserialized->time_remaining, 7.5F);
  EXPECT_EQ(deserialized->produced_count, 5);
  EXPECT_EQ(deserialized->max_units, 20);
  EXPECT_EQ(deserialized->product_type, Game::Units::TroopType::Spearman);
  EXPECT_FLOAT_EQ(deserialized->rally_x, 150.0F);
  EXPECT_FLOAT_EQ(deserialized->rally_z, 250.0F);
  EXPECT_TRUE(deserialized->rally_set);
  EXPECT_EQ(deserialized->villager_cost, 3);
  EXPECT_EQ(deserialized->production_queue.size(), 1UL);
  EXPECT_EQ(deserialized->production_queue[0], Game::Units::TroopType::Archer);
}

TEST_F(SerializationTest, RenderableComponentSerialization) {
  auto *entity = world->create_entity();
  auto *renderable =
      entity->add_component<RenderableComponent>("mesh.obj", "texture.png");

  renderable->mesh_path = "models/archer.obj";
  renderable->texture_path = "textures/archer_diffuse.png";
  renderable->renderer_id = "archer_renderer";
  renderable->visible = true;
  renderable->mesh = RenderableComponent::MeshKind::Capsule;
  renderable->color = {0.8F, 0.2F, 0.5F};

  QJsonObject json = Serialization::serialize_entity(entity);

  ASSERT_TRUE(json.contains("renderable"));
  QJsonObject renderable_obj = json["renderable"].toObject();

  EXPECT_EQ(renderable_obj["mesh_path"].toString(),
            QString("models/archer.obj"));
  EXPECT_EQ(renderable_obj["texture_path"].toString(),
            QString("textures/archer_diffuse.png"));
  EXPECT_EQ(renderable_obj["renderer_id"].toString(),
            QString("archer_renderer"));
  EXPECT_TRUE(renderable_obj["visible"].toBool());
  EXPECT_EQ(renderable_obj["mesh"].toInt(),
            static_cast<int>(RenderableComponent::MeshKind::Capsule));

  ASSERT_TRUE(renderable_obj.contains("color"));
  QJsonArray color = renderable_obj["color"].toArray();
  EXPECT_EQ(color.size(), 3);
  EXPECT_FLOAT_EQ(color[0].toDouble(), 0.8);
  EXPECT_FLOAT_EQ(color[1].toDouble(), 0.2);
  EXPECT_FLOAT_EQ(color[2].toDouble(), 0.5);
}

TEST_F(SerializationTest, RenderableComponentRoundTrip) {
  auto *original_entity = world->create_entity();
  auto *renderable = original_entity->add_component<RenderableComponent>(
      "test.obj", "test.png");
  renderable->mesh_path = "models/building.obj";
  renderable->texture_path = "textures/building.png";
  renderable->visible = false;
  renderable->mesh = RenderableComponent::MeshKind::Quad;
  renderable->color = {1.0F, 0.5F, 0.25F};

  QJsonObject json = Serialization::serialize_entity(original_entity);

  auto *new_entity = world->create_entity();
  Serialization::deserialize_entity(new_entity, json);

  auto *deserialized = new_entity->get_component<RenderableComponent>();
  ASSERT_NE(deserialized, nullptr);
  EXPECT_EQ(deserialized->mesh_path, "models/building.obj");
  EXPECT_EQ(deserialized->texture_path, "textures/building.png");
  EXPECT_FALSE(deserialized->visible);
  EXPECT_EQ(deserialized->mesh, RenderableComponent::MeshKind::Quad);
  EXPECT_FLOAT_EQ(deserialized->color[0], 1.0F);
  EXPECT_FLOAT_EQ(deserialized->color[1], 0.5F);
  EXPECT_FLOAT_EQ(deserialized->color[2], 0.25F);
}

TEST_F(SerializationTest, AttackTargetComponentSerialization) {
  auto *entity = world->create_entity();
  auto *attack_target = entity->add_component<AttackTargetComponent>();

  attack_target->target_id = 42;
  attack_target->should_chase = true;

  QJsonObject json = Serialization::serialize_entity(entity);

  ASSERT_TRUE(json.contains("attack_target"));
  QJsonObject attack_target_obj = json["attack_target"].toObject();

  EXPECT_EQ(attack_target_obj["target_id"].toVariant().toULongLong(), 42ULL);
  EXPECT_TRUE(attack_target_obj["should_chase"].toBool());
}

TEST_F(SerializationTest, AttackTargetComponentRoundTrip) {
  auto *original_entity = world->create_entity();
  auto *attack_target = original_entity->add_component<AttackTargetComponent>();
  attack_target->target_id = 123;
  attack_target->should_chase = false;

  QJsonObject json = Serialization::serialize_entity(original_entity);

  auto *new_entity = world->create_entity();
  Serialization::deserialize_entity(new_entity, json);

  auto *deserialized = new_entity->get_component<AttackTargetComponent>();
  ASSERT_NE(deserialized, nullptr);
  EXPECT_EQ(deserialized->target_id, 123U);
  EXPECT_FALSE(deserialized->should_chase);
}

TEST_F(SerializationTest, BuildingComponentSerialization) {
  auto *entity = world->create_entity();
  entity->add_component<BuildingComponent>();

  QJsonObject json = Serialization::serialize_entity(entity);

  ASSERT_TRUE(json.contains("building"));
  EXPECT_TRUE(json["building"].toBool());
}

TEST_F(SerializationTest, BuildingComponentRoundTrip) {
  auto *original_entity = world->create_entity();
  original_entity->add_component<BuildingComponent>();

  QJsonObject json = Serialization::serialize_entity(original_entity);

  auto *new_entity = world->create_entity();
  Serialization::deserialize_entity(new_entity, json);

  auto *deserialized = new_entity->get_component<BuildingComponent>();
  ASSERT_NE(deserialized, nullptr);
}

TEST_F(SerializationTest, AIControlledComponentSerialization) {
  auto *entity = world->create_entity();
  entity->add_component<AIControlledComponent>();

  QJsonObject json = Serialization::serialize_entity(entity);

  ASSERT_TRUE(json.contains("aiControlled"));
  EXPECT_TRUE(json["aiControlled"].toBool());
}

TEST_F(SerializationTest, AIControlledComponentRoundTrip) {
  auto *original_entity = world->create_entity();
  original_entity->add_component<AIControlledComponent>();

  QJsonObject json = Serialization::serialize_entity(original_entity);

  auto *new_entity = world->create_entity();
  Serialization::deserialize_entity(new_entity, json);

  auto *deserialized = new_entity->get_component<AIControlledComponent>();
  ASSERT_NE(deserialized, nullptr);
}

TEST_F(SerializationTest, CaptureComponentSerialization) {
  auto *entity = world->create_entity();
  auto *capture = entity->add_component<CaptureComponent>();

  capture->capturing_player_id = 2;
  capture->capture_progress = 7.5F;
  capture->required_time = 15.0F;
  capture->is_being_captured = true;

  QJsonObject json = Serialization::serialize_entity(entity);

  ASSERT_TRUE(json.contains("capture"));
  QJsonObject capture_obj = json["capture"].toObject();

  EXPECT_EQ(capture_obj["capturing_player_id"].toInt(), 2);
  EXPECT_FLOAT_EQ(capture_obj["capture_progress"].toDouble(), 7.5);
  EXPECT_FLOAT_EQ(capture_obj["required_time"].toDouble(), 15.0);
  EXPECT_TRUE(capture_obj["is_being_captured"].toBool());
}

TEST_F(SerializationTest, CaptureComponentRoundTrip) {
  auto *original_entity = world->create_entity();
  auto *capture = original_entity->add_component<CaptureComponent>();
  capture->capturing_player_id = 3;
  capture->capture_progress = 10.0F;
  capture->required_time = 20.0F;
  capture->is_being_captured = false;

  QJsonObject json = Serialization::serialize_entity(original_entity);

  auto *new_entity = world->create_entity();
  Serialization::deserialize_entity(new_entity, json);

  auto *deserialized = new_entity->get_component<CaptureComponent>();
  ASSERT_NE(deserialized, nullptr);
  EXPECT_EQ(deserialized->capturing_player_id, 3);
  EXPECT_FLOAT_EQ(deserialized->capture_progress, 10.0F);
  EXPECT_FLOAT_EQ(deserialized->required_time, 20.0F);
  EXPECT_FALSE(deserialized->is_being_captured);
}

TEST_F(SerializationTest, CompleteEntityWithAllComponents) {
  auto *entity = world->create_entity();

  auto *transform = entity->add_component<TransformComponent>();
  transform->position.x = 50.0F;
  transform->position.y = 10.0F;
  transform->position.z = 30.0F;

  auto *renderable =
      entity->add_component<RenderableComponent>("mesh.obj", "tex.png");
  renderable->visible = true;

  auto *unit = entity->add_component<UnitComponent>();
  unit->health = 100;
  unit->max_health = 100;

  auto *movement = entity->add_component<MovementComponent>();
  movement->has_target = true;
  movement->target_x = 100.0F;

  auto *attack = entity->add_component<AttackComponent>();
  attack->damage = 25;

  auto *attack_target = entity->add_component<AttackTargetComponent>();
  attack_target->target_id = 99;

  entity->add_component<BuildingComponent>();

  auto *production = entity->add_component<ProductionComponent>();
  production->in_progress = true;

  entity->add_component<AIControlledComponent>();

  auto *capture = entity->add_component<CaptureComponent>();
  capture->is_being_captured = true;

  auto *hold_mode = entity->add_component<HoldModeComponent>();
  hold_mode->active = true;

  auto *healer = entity->add_component<HealerComponent>();
  healer->healing_amount = 10;

  auto *catapult = entity->add_component<CatapultLoadingComponent>();
  catapult->state = CatapultLoadingComponent::LoadingState::Idle;

  QJsonObject json = Serialization::serialize_entity(entity);

  EXPECT_TRUE(json.contains("transform"));
  EXPECT_TRUE(json.contains("renderable"));
  EXPECT_TRUE(json.contains("unit"));
  EXPECT_TRUE(json.contains("movement"));
  EXPECT_TRUE(json.contains("attack"));
  EXPECT_TRUE(json.contains("attack_target"));
  EXPECT_TRUE(json.contains("building"));
  EXPECT_TRUE(json.contains("production"));
  EXPECT_TRUE(json.contains("aiControlled"));
  EXPECT_TRUE(json.contains("capture"));
  EXPECT_TRUE(json.contains("hold_mode"));
  EXPECT_TRUE(json.contains("healer"));
  EXPECT_TRUE(json.contains("catapult_loading"));

  auto *new_entity = world->create_entity();
  Serialization::deserialize_entity(new_entity, json);

  EXPECT_NE(new_entity->get_component<TransformComponent>(), nullptr);
  EXPECT_NE(new_entity->get_component<RenderableComponent>(), nullptr);
  EXPECT_NE(new_entity->get_component<UnitComponent>(), nullptr);
  EXPECT_NE(new_entity->get_component<MovementComponent>(), nullptr);
  EXPECT_NE(new_entity->get_component<AttackComponent>(), nullptr);
  EXPECT_NE(new_entity->get_component<AttackTargetComponent>(), nullptr);
  EXPECT_NE(new_entity->get_component<BuildingComponent>(), nullptr);
  EXPECT_NE(new_entity->get_component<ProductionComponent>(), nullptr);
  EXPECT_NE(new_entity->get_component<AIControlledComponent>(), nullptr);
  EXPECT_NE(new_entity->get_component<CaptureComponent>(), nullptr);
  EXPECT_NE(new_entity->get_component<HoldModeComponent>(), nullptr);
  EXPECT_NE(new_entity->get_component<HealerComponent>(), nullptr);
  EXPECT_NE(new_entity->get_component<CatapultLoadingComponent>(), nullptr);
}

TEST_F(SerializationTest, EmptyWorldSerialization) {
  QJsonDocument doc = Serialization::serialize_world(world.get());

  ASSERT_TRUE(doc.isObject());
  QJsonObject world_obj = doc.object();

  EXPECT_TRUE(world_obj.contains("entities"));
  QJsonArray entities = world_obj["entities"].toArray();
  EXPECT_EQ(entities.size(), 0);
}

TEST_F(SerializationTest, HoldModeComponentSerialization) {
  auto *entity = world->create_entity();
  auto *hold_mode = entity->add_component<HoldModeComponent>();

  hold_mode->active = false;
  hold_mode->exit_cooldown = 1.5F;
  hold_mode->stand_up_duration = 3.0F;

  QJsonObject json = Serialization::serialize_entity(entity);

  ASSERT_TRUE(json.contains("hold_mode"));
  QJsonObject hold_mode_obj = json["hold_mode"].toObject();

  EXPECT_FALSE(hold_mode_obj["active"].toBool());
  EXPECT_FLOAT_EQ(hold_mode_obj["exit_cooldown"].toDouble(), 1.5);
  EXPECT_FLOAT_EQ(hold_mode_obj["stand_up_duration"].toDouble(), 3.0);
}

TEST_F(SerializationTest, HoldModeComponentRoundTrip) {
  auto *original_entity = world->create_entity();
  auto *hold_mode = original_entity->add_component<HoldModeComponent>();
  hold_mode->active = true;
  hold_mode->exit_cooldown = 2.5F;
  hold_mode->stand_up_duration = 4.0F;

  QJsonObject json = Serialization::serialize_entity(original_entity);

  auto *new_entity = world->create_entity();
  Serialization::deserialize_entity(new_entity, json);

  auto *deserialized = new_entity->get_component<HoldModeComponent>();
  ASSERT_NE(deserialized, nullptr);
  EXPECT_TRUE(deserialized->active);
  EXPECT_FLOAT_EQ(deserialized->exit_cooldown, 2.5F);
  EXPECT_FLOAT_EQ(deserialized->stand_up_duration, 4.0F);
}

TEST_F(SerializationTest, HealerComponentSerialization) {
  auto *entity = world->create_entity();
  auto *healer = entity->add_component<HealerComponent>();

  healer->healing_range = 12.0F;
  healer->healing_amount = 10;
  healer->healing_cooldown = 3.0F;
  healer->time_since_last_heal = 1.0F;

  QJsonObject json = Serialization::serialize_entity(entity);

  ASSERT_TRUE(json.contains("healer"));
  QJsonObject healer_obj = json["healer"].toObject();

  EXPECT_FLOAT_EQ(healer_obj["healing_range"].toDouble(), 12.0);
  EXPECT_EQ(healer_obj["healing_amount"].toInt(), 10);
  EXPECT_FLOAT_EQ(healer_obj["healing_cooldown"].toDouble(), 3.0);
  EXPECT_FLOAT_EQ(healer_obj["time_since_last_heal"].toDouble(), 1.0);
}

TEST_F(SerializationTest, HealerComponentRoundTrip) {
  auto *original_entity = world->create_entity();
  auto *healer = original_entity->add_component<HealerComponent>();
  healer->healing_range = 15.0F;
  healer->healing_amount = 8;
  healer->healing_cooldown = 4.0F;
  healer->time_since_last_heal = 2.0F;

  QJsonObject json = Serialization::serialize_entity(original_entity);

  auto *new_entity = world->create_entity();
  Serialization::deserialize_entity(new_entity, json);

  auto *deserialized = new_entity->get_component<HealerComponent>();
  ASSERT_NE(deserialized, nullptr);
  EXPECT_FLOAT_EQ(deserialized->healing_range, 15.0F);
  EXPECT_EQ(deserialized->healing_amount, 8);
  EXPECT_FLOAT_EQ(deserialized->healing_cooldown, 4.0F);
  EXPECT_FLOAT_EQ(deserialized->time_since_last_heal, 2.0F);
}

TEST_F(SerializationTest, CatapultLoadingComponentSerialization) {
  auto *entity = world->create_entity();
  auto *catapult = entity->add_component<CatapultLoadingComponent>();

  catapult->state = CatapultLoadingComponent::LoadingState::Loading;
  catapult->loading_time = 1.0F;
  catapult->loading_duration = 3.0F;
  catapult->firing_time = 0.0F;
  catapult->firing_duration = 1.0F;
  catapult->target_id = 42;
  catapult->target_locked_x = 100.0F;
  catapult->target_locked_y = 50.0F;
  catapult->target_locked_z = 200.0F;
  catapult->target_position_locked = true;

  QJsonObject json = Serialization::serialize_entity(entity);

  ASSERT_TRUE(json.contains("catapult_loading"));
  QJsonObject catapult_obj = json["catapult_loading"].toObject();

  EXPECT_EQ(catapult_obj["state"].toInt(),
            static_cast<int>(CatapultLoadingComponent::LoadingState::Loading));
  EXPECT_FLOAT_EQ(catapult_obj["loading_time"].toDouble(), 1.0);
  EXPECT_FLOAT_EQ(catapult_obj["loading_duration"].toDouble(), 3.0);
  EXPECT_FLOAT_EQ(catapult_obj["firing_time"].toDouble(), 0.0);
  EXPECT_FLOAT_EQ(catapult_obj["firing_duration"].toDouble(), 1.0);
  EXPECT_EQ(catapult_obj["target_id"].toVariant().toULongLong(), 42ULL);
  EXPECT_FLOAT_EQ(catapult_obj["target_locked_x"].toDouble(), 100.0);
  EXPECT_FLOAT_EQ(catapult_obj["target_locked_y"].toDouble(), 50.0);
  EXPECT_FLOAT_EQ(catapult_obj["target_locked_z"].toDouble(), 200.0);
  EXPECT_TRUE(catapult_obj["target_position_locked"].toBool());
}

TEST_F(SerializationTest, CatapultLoadingComponentRoundTrip) {
  auto *original_entity = world->create_entity();
  auto *catapult = original_entity->add_component<CatapultLoadingComponent>();
  catapult->state = CatapultLoadingComponent::LoadingState::ReadyToFire;
  catapult->loading_time = 2.0F;
  catapult->loading_duration = 4.0F;
  catapult->firing_time = 0.25F;
  catapult->firing_duration = 0.75F;
  catapult->target_id = 99;
  catapult->target_locked_x = 150.0F;
  catapult->target_locked_y = 75.0F;
  catapult->target_locked_z = 250.0F;
  catapult->target_position_locked = false;

  QJsonObject json = Serialization::serialize_entity(original_entity);

  auto *new_entity = world->create_entity();
  Serialization::deserialize_entity(new_entity, json);

  auto *deserialized = new_entity->get_component<CatapultLoadingComponent>();
  ASSERT_NE(deserialized, nullptr);
  EXPECT_EQ(deserialized->state,
            CatapultLoadingComponent::LoadingState::ReadyToFire);
  EXPECT_FLOAT_EQ(deserialized->loading_time, 2.0F);
  EXPECT_FLOAT_EQ(deserialized->loading_duration, 4.0F);
  EXPECT_FLOAT_EQ(deserialized->firing_time, 0.25F);
  EXPECT_FLOAT_EQ(deserialized->firing_duration, 0.75F);
  EXPECT_EQ(deserialized->target_id, 99U);
  EXPECT_FLOAT_EQ(deserialized->target_locked_x, 150.0F);
  EXPECT_FLOAT_EQ(deserialized->target_locked_y, 75.0F);
  EXPECT_FLOAT_EQ(deserialized->target_locked_z, 250.0F);
  EXPECT_FALSE(deserialized->target_position_locked);
}

// ============================================================================
// Integration Tests: Multi-Unit Battlefield State Preservation
// ============================================================================

TEST_F(SerializationTest, MultipleUnitsPositionsAndHealthPreserved) {
  // Create a battlefield with multiple units at different positions
  struct UnitData {
    float x, y, z;
    int health;
    int max_health;
    int owner_id;
    Game::Units::SpawnType spawn_type;
  };

  std::vector<UnitData> original_units = {
      {10.0F, 0.0F, 20.0F, 80, 100, 1, Game::Units::SpawnType::Archer},
      {15.5F, 1.0F, 25.5F, 45, 100, 1, Game::Units::SpawnType::Spearman},
      {30.0F, 0.0F, 40.0F, 100, 100, 2, Game::Units::SpawnType::Knight},
      {35.5F, 2.0F, 45.5F, 60, 150, 2, Game::Units::SpawnType::HorseArcher},
      {50.0F, 0.5F, 60.0F, 25, 80, 1, Game::Units::SpawnType::Catapult},
  };

  std::vector<EntityID> entity_ids;
  for (const auto &unit_data : original_units) {
    auto *entity = world->create_entity();
    entity_ids.push_back(entity->get_id());

    auto *transform = entity->add_component<TransformComponent>();
    transform->position.x = unit_data.x;
    transform->position.y = unit_data.y;
    transform->position.z = unit_data.z;

    auto *unit = entity->add_component<UnitComponent>();
    unit->health = unit_data.health;
    unit->max_health = unit_data.max_health;
    unit->owner_id = unit_data.owner_id;
    unit->spawn_type = unit_data.spawn_type;
  }

  // Serialize and deserialize the world
  QJsonDocument doc = Serialization::serialize_world(world.get());
  auto restored_world = std::make_unique<World>();
  Serialization::deserialize_world(restored_world.get(), doc);

  // Verify all units are restored with exact positions and health
  const auto &entities = restored_world->get_entities();
  EXPECT_EQ(entities.size(), original_units.size());

  for (size_t i = 0; i < entity_ids.size(); ++i) {
    auto *entity = restored_world->get_entity(entity_ids[i]);
    ASSERT_NE(entity, nullptr) << "Entity " << i << " not found";

    auto *transform = entity->get_component<TransformComponent>();
    ASSERT_NE(transform, nullptr);
    EXPECT_FLOAT_EQ(transform->position.x, original_units[i].x)
        << "Unit " << i << " X position mismatch";
    EXPECT_FLOAT_EQ(transform->position.y, original_units[i].y)
        << "Unit " << i << " Y position mismatch";
    EXPECT_FLOAT_EQ(transform->position.z, original_units[i].z)
        << "Unit " << i << " Z position mismatch";

    auto *unit = entity->get_component<UnitComponent>();
    ASSERT_NE(unit, nullptr);
    EXPECT_EQ(unit->health, original_units[i].health)
        << "Unit " << i << " health mismatch";
    EXPECT_EQ(unit->max_health, original_units[i].max_health)
        << "Unit " << i << " max_health mismatch";
    EXPECT_EQ(unit->owner_id, original_units[i].owner_id)
        << "Unit " << i << " owner_id mismatch";
    EXPECT_EQ(unit->spawn_type, original_units[i].spawn_type)
        << "Unit " << i << " spawn_type mismatch";
  }
}

TEST_F(SerializationTest, OwnerRegistryTeamsAndColorsPreserved) {
  // Setup owner registry with teams and custom colors
  auto &registry = Game::Systems::OwnerRegistry::instance();
  registry.clear();

  // Register players with specific teams and colors
  int player1 = registry.register_owner(Game::Systems::OwnerType::Player, "Blue Kingdom");
  int player2 = registry.register_owner(Game::Systems::OwnerType::AI, "Red Empire");
  int player3 = registry.register_owner(Game::Systems::OwnerType::Player, "Green Alliance");

  // Set teams (player1 and player3 are allies)
  registry.set_owner_team(player1, 1);
  registry.set_owner_team(player2, 2);
  registry.set_owner_team(player3, 1);

  // Set custom colors
  registry.set_owner_color(player1, 0.1F, 0.2F, 0.9F);
  registry.set_owner_color(player2, 0.9F, 0.1F, 0.1F);
  registry.set_owner_color(player3, 0.1F, 0.9F, 0.2F);

  registry.set_local_player_id(player1);

  // Create some entities owned by these players
  for (int i = 0; i < 3; ++i) {
    auto *entity = world->create_entity();
    auto *unit = entity->add_component<UnitComponent>();
    unit->owner_id = player1;
  }
  for (int i = 0; i < 2; ++i) {
    auto *entity = world->create_entity();
    auto *unit = entity->add_component<UnitComponent>();
    unit->owner_id = player2;
  }

  // Serialize world (includes owner_registry)
  QJsonDocument doc = Serialization::serialize_world(world.get());

  // Clear registry and restore
  registry.clear();
  auto restored_world = std::make_unique<World>();
  Serialization::deserialize_world(restored_world.get(), doc);

  // Verify owner registry state is preserved
  EXPECT_EQ(registry.get_local_player_id(), player1);

  // Verify teams are preserved
  EXPECT_EQ(registry.get_owner_team(player1), 1);
  EXPECT_EQ(registry.get_owner_team(player2), 2);
  EXPECT_EQ(registry.get_owner_team(player3), 1);

  // Verify alliances are preserved
  EXPECT_TRUE(registry.are_allies(player1, player3));
  EXPECT_TRUE(registry.are_enemies(player1, player2));
  EXPECT_TRUE(registry.are_enemies(player2, player3));

  // Verify colors are preserved
  auto color1 = registry.get_owner_color(player1);
  EXPECT_FLOAT_EQ(color1[0], 0.1F);
  EXPECT_FLOAT_EQ(color1[1], 0.2F);
  EXPECT_FLOAT_EQ(color1[2], 0.9F);

  auto color2 = registry.get_owner_color(player2);
  EXPECT_FLOAT_EQ(color2[0], 0.9F);
  EXPECT_FLOAT_EQ(color2[1], 0.1F);
  EXPECT_FLOAT_EQ(color2[2], 0.1F);

  auto color3 = registry.get_owner_color(player3);
  EXPECT_FLOAT_EQ(color3[0], 0.1F);
  EXPECT_FLOAT_EQ(color3[1], 0.9F);
  EXPECT_FLOAT_EQ(color3[2], 0.2F);

  // Verify owner names are preserved
  EXPECT_EQ(registry.get_owner_name(player1), "Blue Kingdom");
  EXPECT_EQ(registry.get_owner_name(player2), "Red Empire");
  EXPECT_EQ(registry.get_owner_name(player3), "Green Alliance");

  // Verify owner types are preserved
  EXPECT_TRUE(registry.is_player(player1));
  EXPECT_TRUE(registry.is_ai(player2));
  EXPECT_TRUE(registry.is_player(player3));

  // Clean up
  registry.clear();
}

TEST_F(SerializationTest, BuildingOwnershipAndCaptureStatePreserved) {
  // Create buildings (barracks/villages) with different ownership and capture states
  struct BuildingData {
    float x, z;
    int owner_id;
    int capturing_player_id;
    float capture_progress;
    bool is_being_captured;
  };

  std::vector<BuildingData> buildings = {
      {100.0F, 100.0F, 1, -1, 0.0F, false},        // Owned by player 1, not being captured
      {200.0F, 200.0F, 2, 1, 7.5F, true},          // Owned by player 2, being captured by player 1
      {300.0F, 300.0F, 1, 2, 15.0F, true},         // Owned by player 1, being captured by player 2
      {400.0F, 400.0F, -1, -1, 0.0F, false},       // Neutral building
  };

  std::vector<EntityID> building_ids;
  for (const auto &bldg : buildings) {
    auto *entity = world->create_entity();
    building_ids.push_back(entity->get_id());

    auto *transform = entity->add_component<TransformComponent>();
    transform->position.x = bldg.x;
    transform->position.z = bldg.z;

    entity->add_component<BuildingComponent>();

    auto *unit = entity->add_component<UnitComponent>();
    unit->owner_id = bldg.owner_id;

    auto *capture = entity->add_component<CaptureComponent>();
    capture->capturing_player_id = bldg.capturing_player_id;
    capture->capture_progress = bldg.capture_progress;
    capture->is_being_captured = bldg.is_being_captured;
  }

  // Serialize and restore
  QJsonDocument doc = Serialization::serialize_world(world.get());
  auto restored_world = std::make_unique<World>();
  Serialization::deserialize_world(restored_world.get(), doc);

  // Verify all buildings are restored with correct ownership and capture state
  for (size_t i = 0; i < building_ids.size(); ++i) {
    auto *entity = restored_world->get_entity(building_ids[i]);
    ASSERT_NE(entity, nullptr) << "Building " << i << " not found";

    auto *transform = entity->get_component<TransformComponent>();
    ASSERT_NE(transform, nullptr);
    EXPECT_FLOAT_EQ(transform->position.x, buildings[i].x)
        << "Building " << i << " X position mismatch";
    EXPECT_FLOAT_EQ(transform->position.z, buildings[i].z)
        << "Building " << i << " Z position mismatch";

    EXPECT_NE(entity->get_component<BuildingComponent>(), nullptr);

    auto *unit = entity->get_component<UnitComponent>();
    ASSERT_NE(unit, nullptr);
    EXPECT_EQ(unit->owner_id, buildings[i].owner_id)
        << "Building " << i << " owner mismatch";

    auto *capture = entity->get_component<CaptureComponent>();
    ASSERT_NE(capture, nullptr);
    EXPECT_EQ(capture->capturing_player_id, buildings[i].capturing_player_id)
        << "Building " << i << " capturing_player_id mismatch";
    EXPECT_FLOAT_EQ(capture->capture_progress, buildings[i].capture_progress)
        << "Building " << i << " capture_progress mismatch";
    EXPECT_EQ(capture->is_being_captured, buildings[i].is_being_captured)
        << "Building " << i << " is_being_captured mismatch";
  }
}

TEST_F(SerializationTest, UnitMovementStatePreserved) {
  // Create units with active movement paths
  auto *entity = world->create_entity();
  auto entity_id = entity->get_id();

  auto *transform = entity->add_component<TransformComponent>();
  transform->position.x = 10.0F;
  transform->position.y = 0.0F;
  transform->position.z = 20.0F;

  auto *unit = entity->add_component<UnitComponent>();
  unit->owner_id = 1;
  unit->health = 85;

  auto *movement = entity->add_component<MovementComponent>();
  movement->has_target = true;
  movement->target_x = 50.0F;
  movement->target_y = 60.0F;
  movement->goal_x = 55.0F;
  movement->goal_y = 65.0F;
  movement->vx = 2.5F;
  movement->vz = 3.0F;
  // Add path waypoints
  movement->path.emplace_back(20.0F, 30.0F);
  movement->path.emplace_back(35.0F, 45.0F);
  movement->path.emplace_back(50.0F, 60.0F);
  const size_t expected_path_size = movement->path.size();

  // Serialize and restore
  QJsonDocument doc = Serialization::serialize_world(world.get());
  auto restored_world = std::make_unique<World>();
  Serialization::deserialize_world(restored_world.get(), doc);

  // Verify movement state is preserved
  auto *restored_entity = restored_world->get_entity(entity_id);
  ASSERT_NE(restored_entity, nullptr);

  auto *restored_movement = restored_entity->get_component<MovementComponent>();
  ASSERT_NE(restored_movement, nullptr);

  EXPECT_TRUE(restored_movement->has_target);
  EXPECT_FLOAT_EQ(restored_movement->target_x, 50.0F);
  EXPECT_FLOAT_EQ(restored_movement->target_y, 60.0F);
  EXPECT_FLOAT_EQ(restored_movement->goal_x, 55.0F);
  EXPECT_FLOAT_EQ(restored_movement->goal_y, 65.0F);
  EXPECT_FLOAT_EQ(restored_movement->vx, 2.5F);
  EXPECT_FLOAT_EQ(restored_movement->vz, 3.0F);

  // Verify path is preserved
  ASSERT_EQ(restored_movement->path.size(), expected_path_size);
  EXPECT_FLOAT_EQ(restored_movement->path[0].first, 20.0F);
  EXPECT_FLOAT_EQ(restored_movement->path[0].second, 30.0F);
  EXPECT_FLOAT_EQ(restored_movement->path[1].first, 35.0F);
  EXPECT_FLOAT_EQ(restored_movement->path[1].second, 45.0F);
  EXPECT_FLOAT_EQ(restored_movement->path[2].first, 50.0F);
  EXPECT_FLOAT_EQ(restored_movement->path[2].second, 60.0F);
}

TEST_F(SerializationTest, CombatStatePreserved) {
  // Create units engaged in combat
  auto *attacker = world->create_entity();
  auto *defender = world->create_entity();
  auto attacker_id = attacker->get_id();
  auto defender_id = defender->get_id();

  // Setup attacker
  auto *attacker_transform = attacker->add_component<TransformComponent>();
  attacker_transform->position.x = 10.0F;
  attacker_transform->position.z = 10.0F;

  auto *attacker_unit = attacker->add_component<UnitComponent>();
  attacker_unit->owner_id = 1;
  attacker_unit->health = 90;

  auto *attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->damage = 25;
  attacker_attack->range = 15.0F;
  attacker_attack->current_mode = AttackComponent::CombatMode::Melee;
  attacker_attack->in_melee_lock = true;
  attacker_attack->melee_lock_target_id = defender_id;

  auto *attacker_target = attacker->add_component<AttackTargetComponent>();
  attacker_target->target_id = defender_id;
  attacker_target->should_chase = true;

  // Setup defender
  auto *defender_transform = defender->add_component<TransformComponent>();
  defender_transform->position.x = 12.0F;
  defender_transform->position.z = 12.0F;

  auto *defender_unit = defender->add_component<UnitComponent>();
  defender_unit->owner_id = 2;
  defender_unit->health = 60;

  auto *defender_attack = defender->add_component<AttackComponent>();
  defender_attack->damage = 20;
  defender_attack->in_melee_lock = true;
  defender_attack->melee_lock_target_id = attacker_id;

  // Serialize and restore
  QJsonDocument doc = Serialization::serialize_world(world.get());
  auto restored_world = std::make_unique<World>();
  Serialization::deserialize_world(restored_world.get(), doc);

  // Verify combat state is preserved
  auto *restored_attacker = restored_world->get_entity(attacker_id);
  auto *restored_defender = restored_world->get_entity(defender_id);
  ASSERT_NE(restored_attacker, nullptr);
  ASSERT_NE(restored_defender, nullptr);

  auto *restored_attacker_attack =
      restored_attacker->get_component<AttackComponent>();
  ASSERT_NE(restored_attacker_attack, nullptr);
  EXPECT_TRUE(restored_attacker_attack->in_melee_lock);
  EXPECT_EQ(restored_attacker_attack->melee_lock_target_id, defender_id);
  EXPECT_EQ(restored_attacker_attack->current_mode,
            AttackComponent::CombatMode::Melee);

  auto *restored_attacker_target =
      restored_attacker->get_component<AttackTargetComponent>();
  ASSERT_NE(restored_attacker_target, nullptr);
  EXPECT_EQ(restored_attacker_target->target_id, defender_id);
  EXPECT_TRUE(restored_attacker_target->should_chase);

  auto *restored_defender_attack =
      restored_defender->get_component<AttackComponent>();
  ASSERT_NE(restored_defender_attack, nullptr);
  EXPECT_TRUE(restored_defender_attack->in_melee_lock);
  EXPECT_EQ(restored_defender_attack->melee_lock_target_id, attacker_id);

  // Verify health is preserved
  auto *restored_attacker_unit =
      restored_attacker->get_component<UnitComponent>();
  auto *restored_defender_unit =
      restored_defender->get_component<UnitComponent>();
  EXPECT_EQ(restored_attacker_unit->health, 90);
  EXPECT_EQ(restored_defender_unit->health, 60);
}

TEST_F(SerializationTest, NationIdentityPreserved) {
  // Create units from different nations
  auto *roman_unit = world->create_entity();
  auto *carthage_unit = world->create_entity();
  auto roman_id = roman_unit->get_id();
  auto carthage_id = carthage_unit->get_id();

  auto *roman_unit_comp = roman_unit->add_component<UnitComponent>();
  roman_unit_comp->nation_id = Game::Systems::NationID::RomanRepublic;
  roman_unit_comp->spawn_type = Game::Units::SpawnType::Spearman;

  auto *carthage_unit_comp = carthage_unit->add_component<UnitComponent>();
  carthage_unit_comp->nation_id = Game::Systems::NationID::Carthage;
  carthage_unit_comp->spawn_type = Game::Units::SpawnType::Archer;

  // Serialize and restore
  QJsonDocument doc = Serialization::serialize_world(world.get());
  auto restored_world = std::make_unique<World>();
  Serialization::deserialize_world(restored_world.get(), doc);

  // Verify nation IDs are preserved
  auto *restored_roman = restored_world->get_entity(roman_id);
  auto *restored_carthage = restored_world->get_entity(carthage_id);

  auto *restored_roman_comp = restored_roman->get_component<UnitComponent>();
  EXPECT_EQ(restored_roman_comp->nation_id,
            Game::Systems::NationID::RomanRepublic);
  EXPECT_EQ(restored_roman_comp->spawn_type, Game::Units::SpawnType::Spearman);

  auto *restored_carthage_comp =
      restored_carthage->get_component<UnitComponent>();
  EXPECT_EQ(restored_carthage_comp->nation_id, Game::Systems::NationID::Carthage);
  EXPECT_EQ(restored_carthage_comp->spawn_type, Game::Units::SpawnType::Archer);
}
