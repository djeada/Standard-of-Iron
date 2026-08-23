#include <gtest/gtest.h>
#include <vector>

#include "app/orders/context_intent.h"
#include "game/core/component.h"
#include "game/core/world.h"

namespace {

using App::Core::ContextIntent;
using App::Core::ContextIntentRequest;
using App::Core::resolve_context_intent;

class ContextIntentTest : public ::testing::Test {
protected:
  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> selection;

  auto base_request() -> ContextIntentRequest {
    ContextIntentRequest request;
    request.world = &world;
    request.local_owner_id = 1;
    request.selection = &selection;
    request.has_ground = true;
    request.ground = QVector3D(4.0F, 0.0F, 6.0F);
    request.ground_is_walkable = true;
    return request;
  }

  auto add_selected_unit() -> Engine::Core::EntityID {
    auto* entity = world.create_entity();
    entity->add_component<Engine::Core::TransformComponent>(0.0F, 0.0F, 0.0F);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->owner_id = 1;
    unit->health = 100;
    unit->max_health = 100;
    selection.push_back(entity->get_id());
    return entity->get_id();
  }
};

TEST_F(ContextIntentTest, GroundUnderTheCursorWithASelectionReadsAsMove) {
  add_selected_unit();
  const auto resolution = resolve_context_intent(base_request());

  EXPECT_EQ(resolution.intent, ContextIntent::Move);
  EXPECT_TRUE(resolution.valid());
  EXPECT_TRUE(resolution.has_position);
  EXPECT_FLOAT_EQ(resolution.position.x(), 4.0F);
  EXPECT_EQ(resolution.target, 0U);
}

TEST_F(ContextIntentTest, AnEnemyUnderTheCursorReadsAsAttack) {
  add_selected_unit();
  auto request = base_request();
  request.hovered_entity_id = 77;
  request.hovered_is_enemy_unit = true;

  const auto resolution = resolve_context_intent(request);
  EXPECT_EQ(resolution.intent, ContextIntent::Attack);
  EXPECT_EQ(resolution.target, 77U);
}

TEST_F(ContextIntentTest, AFriendlyUnitUnderTheCursorStillReadsAsMove) {
  add_selected_unit();
  auto request = base_request();
  request.hovered_entity_id = 91;
  request.hovered_is_enemy_unit = false;

  EXPECT_EQ(resolve_context_intent(request).intent, ContextIntent::Move);
}

TEST_F(ContextIntentTest, UnwalkableGroundReadsAsInvalidWithAReason) {
  add_selected_unit();
  auto request = base_request();
  request.ground_is_walkable = false;

  const auto resolution = resolve_context_intent(request);
  EXPECT_EQ(resolution.intent, ContextIntent::Invalid);
  EXPECT_FALSE(resolution.valid());
  EXPECT_TRUE(resolution.advises());
  EXPECT_FALSE(resolution.reason.isEmpty());
}

TEST_F(ContextIntentTest, NoGroundUnderTheCursorReadsAsInvalid) {
  add_selected_unit();
  auto request = base_request();
  request.has_ground = false;

  EXPECT_EQ(resolve_context_intent(request).intent, ContextIntent::Invalid);
}

TEST_F(ContextIntentTest, AnEmptySelectionAdvisesNothingRatherThanRefusing) {
  const auto resolution = resolve_context_intent(base_request());
  EXPECT_EQ(resolution.intent, ContextIntent::None);
  EXPECT_FALSE(resolution.advises());
  EXPECT_FALSE(resolution.valid());
  EXPECT_FALSE(resolution.reason.isEmpty());
}

TEST_F(ContextIntentTest, RallyPlacementReadsAsRallyEvenWithNoSelection) {
  auto request = base_request();
  request.cursor_mode = CursorMode::PlaceCommanderRally;

  const auto resolution = resolve_context_intent(request);
  EXPECT_EQ(resolution.intent, ContextIntent::SetRally);
  EXPECT_TRUE(resolution.has_position);
}

TEST_F(ContextIntentTest, BarracksRallyPlacementAlsoReadsAsRally) {
  auto request = base_request();
  request.cursor_mode = CursorMode::PlaceBarracksRally;
  EXPECT_EQ(resolve_context_intent(request).intent, ContextIntent::SetRally);
}

TEST_F(ContextIntentTest, InteractModesNeedATargetUnderTheCursor) {
  add_selected_unit();
  auto request = base_request();
  request.cursor_mode = CursorMode::Repair;
  EXPECT_EQ(resolve_context_intent(request).intent, ContextIntent::Invalid);

  request.hovered_entity_id = 42;
  const auto resolution = resolve_context_intent(request);
  EXPECT_EQ(resolution.intent, ContextIntent::Interact);
  EXPECT_EQ(resolution.target, 42U);
}

TEST_F(ContextIntentTest, AttackCursorModeReadsAsAttackEvenOverBareGround) {
  add_selected_unit();
  auto request = base_request();
  request.cursor_mode = CursorMode::Attack;

  const auto resolution = resolve_context_intent(request);
  EXPECT_EQ(resolution.intent, ContextIntent::Attack);
  EXPECT_EQ(resolution.target, 0U);
  EXPECT_TRUE(resolution.has_position);
}

TEST_F(ContextIntentTest, SpectatorsGetNoIntentAtAll) {
  add_selected_unit();
  auto request = base_request();
  request.spectator_mode = true;
  const auto resolution = resolve_context_intent(request);
  EXPECT_EQ(resolution.intent, ContextIntent::None);
  EXPECT_FALSE(resolution.advises());
}

TEST_F(ContextIntentTest, ConstructionPlacementReadsAsInteract) {
  auto request = base_request();
  request.placing_construction = true;
  EXPECT_EQ(resolve_context_intent(request).intent, ContextIntent::Interact);
}

TEST_F(ContextIntentTest, EveryIntentHasAStableNameForQml) {
  EXPECT_STREQ(App::Core::context_intent_name(ContextIntent::None), "none");
  EXPECT_STREQ(App::Core::context_intent_name(ContextIntent::Invalid), "invalid");
  EXPECT_STREQ(App::Core::context_intent_name(ContextIntent::Move), "move");
  EXPECT_STREQ(App::Core::context_intent_name(ContextIntent::Attack), "attack");
  EXPECT_STREQ(App::Core::context_intent_name(ContextIntent::Interact), "interact");
  EXPECT_STREQ(App::Core::context_intent_name(ContextIntent::SetRally), "rally");
}

TEST_F(ContextIntentTest, TheVariantMapCarriesEverythingQmlNeeds) {
  add_selected_unit();
  auto request = base_request();
  request.hovered_entity_id = 12;
  request.hovered_is_enemy_unit = true;

  const auto map = App::Core::to_variant_map(resolve_context_intent(request));
  EXPECT_EQ(map.value(QStringLiteral("intent")).toString(), QStringLiteral("attack"));
  EXPECT_TRUE(map.value(QStringLiteral("valid")).toBool());
  EXPECT_EQ(map.value(QStringLiteral("targetId")).toULongLong(), 12ULL);
  EXPECT_TRUE(map.contains(QStringLiteral("reason")));
}

} // namespace

namespace {

TEST_F(ContextIntentTest, AGatherableNodeUnderTheCursorReadsAsInteract) {
  add_selected_unit();
  auto request = base_request();
  request.interaction_available = true;
  request.interaction_product_type = QStringLiteral("cut_tree");
  request.hovered_entity_id = 55;

  const auto resolution = resolve_context_intent(request);
  EXPECT_EQ(resolution.intent, ContextIntent::Interact);
  EXPECT_EQ(resolution.target, 55U);
  EXPECT_TRUE(resolution.has_position);
}

TEST_F(ContextIntentTest, AnEnemyStillWinsOverAnAvailableInteraction) {
  add_selected_unit();
  auto request = base_request();
  request.interaction_available = true;
  request.hovered_entity_id = 61;
  request.hovered_is_enemy_unit = true;

  EXPECT_EQ(resolve_context_intent(request).intent, ContextIntent::Attack);
}

} // namespace
