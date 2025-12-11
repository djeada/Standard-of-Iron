#include "core/component.h"
#include "core/entity.h"
#include "core/serialization.h"
#include "core/world.h"
#include "systems/nation_id.h"
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

  QJsonObject json = Serialization::serializeEntity(entity);

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

  QJsonObject json = Serialization::serializeEntity(entity);

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

  QJsonObject json = Serialization::serializeEntity(entity);

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

  QJsonObject json = Serialization::serializeEntity(entity);

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

  QJsonObject json = Serialization::serializeEntity(entity);

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

  QJsonObject json = Serialization::serializeEntity(original_entity);

  auto *new_entity = world->create_entity();
  Serialization::deserializeEntity(new_entity, json);

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
  Serialization::deserializeEntity(entity, json);

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

  EXPECT_NO_THROW({ Serialization::deserializeEntity(entity, json); });

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

  QJsonDocument doc = Serialization::serializeWorld(world.get());

  ASSERT_TRUE(doc.isObject());
  QJsonObject world_obj = doc.object();
  EXPECT_TRUE(world_obj.contains("entities"));
  EXPECT_TRUE(world_obj.contains("nextEntityId"));
  EXPECT_TRUE(world_obj.contains("schemaVersion"));

  auto new_world = std::make_unique<World>();
  Serialization::deserializeWorld(new_world.get(), doc);

  const auto &entities = new_world->get_entities();
  EXPECT_EQ(entities.size(), 2UL);
}

TEST_F(SerializationTest, SaveAndLoadFromFile) {
  auto *entity = world->create_entity();
  auto *transform = entity->add_component<TransformComponent>();
  transform->position.x = 42.0F;
  transform->position.y = 43.0F;
  transform->position.z = 44.0F;

  QJsonDocument doc = Serialization::serializeWorld(world.get());

  QTemporaryFile temp_file;
  ASSERT_TRUE(temp_file.open());
  QString filename = temp_file.fileName();
  temp_file.close();

  EXPECT_TRUE(Serialization::saveToFile(filename, doc));

  QJsonDocument loaded_doc = Serialization::loadFromFile(filename);
  EXPECT_FALSE(loaded_doc.isNull());

  auto new_world = std::make_unique<World>();
  Serialization::deserializeWorld(new_world.get(), loaded_doc);

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

  QJsonObject json = Serialization::serializeEntity(entity);

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

  QJsonObject json = Serialization::serializeEntity(entity);

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

  QJsonObject json = Serialization::serializeEntity(entity);

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

  QJsonObject json = Serialization::serializeEntity(original_entity);

  auto *new_entity = world->create_entity();
  Serialization::deserializeEntity(new_entity, json);

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

  QJsonObject json = Serialization::serializeEntity(entity);

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

  QJsonObject json = Serialization::serializeEntity(original_entity);

  auto *new_entity = world->create_entity();
  Serialization::deserializeEntity(new_entity, json);

  auto *deserialized = new_entity->get_component<AttackTargetComponent>();
  ASSERT_NE(deserialized, nullptr);
  EXPECT_EQ(deserialized->target_id, 123U);
  EXPECT_FALSE(deserialized->should_chase);
}

TEST_F(SerializationTest, BuildingComponentSerialization) {
  auto *entity = world->create_entity();
  entity->add_component<BuildingComponent>();

  QJsonObject json = Serialization::serializeEntity(entity);

  ASSERT_TRUE(json.contains("building"));
  EXPECT_TRUE(json["building"].toBool());
}

TEST_F(SerializationTest, BuildingComponentRoundTrip) {
  auto *original_entity = world->create_entity();
  original_entity->add_component<BuildingComponent>();

  QJsonObject json = Serialization::serializeEntity(original_entity);

  auto *new_entity = world->create_entity();
  Serialization::deserializeEntity(new_entity, json);

  auto *deserialized = new_entity->get_component<BuildingComponent>();
  ASSERT_NE(deserialized, nullptr);
}

TEST_F(SerializationTest, AIControlledComponentSerialization) {
  auto *entity = world->create_entity();
  entity->add_component<AIControlledComponent>();

  QJsonObject json = Serialization::serializeEntity(entity);

  ASSERT_TRUE(json.contains("aiControlled"));
  EXPECT_TRUE(json["aiControlled"].toBool());
}

TEST_F(SerializationTest, AIControlledComponentRoundTrip) {
  auto *original_entity = world->create_entity();
  original_entity->add_component<AIControlledComponent>();

  QJsonObject json = Serialization::serializeEntity(original_entity);

  auto *new_entity = world->create_entity();
  Serialization::deserializeEntity(new_entity, json);

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

  QJsonObject json = Serialization::serializeEntity(entity);

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

  QJsonObject json = Serialization::serializeEntity(original_entity);

  auto *new_entity = world->create_entity();
  Serialization::deserializeEntity(new_entity, json);

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

  QJsonObject json = Serialization::serializeEntity(entity);

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

  auto *new_entity = world->create_entity();
  Serialization::deserializeEntity(new_entity, json);

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
}

TEST_F(SerializationTest, EmptyWorldSerialization) {
  QJsonDocument doc = Serialization::serializeWorld(world.get());

  ASSERT_TRUE(doc.isObject());
  QJsonObject world_obj = doc.object();

  EXPECT_TRUE(world_obj.contains("entities"));
  QJsonArray entities = world_obj["entities"].toArray();
  EXPECT_EQ(entities.size(), 0);
}
