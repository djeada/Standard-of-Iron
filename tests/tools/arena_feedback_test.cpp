#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QPointF>

#include <gtest/gtest.h>

#include "game/core/component_core.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "tools/arena/arena_feedback.h"

namespace {

class ArenaFeedbackTest : public ::testing::Test {
protected:
  void SetUp() override {
    Engine::Core::EventManager::instance().clear_all_subscriptions();
  }

  void TearDown() override {
    Engine::Core::EventManager::instance().clear_all_subscriptions();
  }

  auto spawn(float x, float z, int health = 100) -> Engine::Core::EntityID {
    auto* entity = m_world.create_entity();
    auto* transform = entity->add_component<Engine::Core::TransformComponent>();
    transform->position.x = x;
    transform->position.z = z;
    auto* unit = entity->add_component<Engine::Core::UnitComponent>(health, health);
    unit->owner_id = 2;
    return entity->get_id();
  }

  Engine::Core::World m_world;
};

TEST_F(ArenaFeedbackTest, ACombatHitBecomesAFloatingNumberOverTheTarget) {
  ArenaFeedback feedback;
  feedback.set_world(&m_world);

  const auto target = spawn(4.0F, -6.0F);

  Engine::Core::EventManager::instance().publish(Engine::Core::CombatHitEvent(
      0, target, 17, Game::Units::SpawnType::Knight, false, 3, 2));

  feedback.advance(0.2F);
  ASSERT_EQ(feedback.live_count(), 1)
      << "a hit in the arena must raise the same floating number the game shows";

  float seen_x = 0.0F;
  float seen_y = 0.0F;
  float seen_z = 0.0F;
  QPixmap canvas(64, 64);
  canvas.fill(Qt::black);
  {
    QPainter painter(&canvas);
    feedback.draw(painter, [&](float x, float y, float z, QPointF& out) {
      seen_x = x;
      seen_y = y;
      seen_z = z;
      out = QPointF(32.0, 32.0);
      return true;
    });
  }
  EXPECT_FLOAT_EQ(seen_x, 4.0F);
  EXPECT_FLOAT_EQ(seen_z, -6.0F);
  EXPECT_GT(seen_y, 1.0F) << "the number floats above the unit, not at its feet";

  feedback.advance(1.2F);
  EXPECT_EQ(feedback.live_count(), 0) << "the number fades instead of piling up";
}

TEST_F(ArenaFeedbackTest, AHarvestDeliveryBecomesAnEconomyNumber) {
  ArenaFeedback feedback;
  feedback.set_world(&m_world);

  const auto depot = spawn(-2.0F, 3.0F);

  Engine::Core::EventManager::instance().publish(
      Engine::Core::WorldFeedbackEvent::make_resource(
          2, depot, Game::Systems::ResourceType::Wood, 40));

  feedback.advance(0.6F);
  EXPECT_EQ(feedback.live_count(), 1)
      << "economy feedback reaches the arena overlay too";
}

TEST_F(ArenaFeedbackTest, TheNumbersScaleWithTheFrameTheyAreRecordedInto) {
  ArenaFeedback feedback;
  feedback.set_world(&m_world);

  const auto target = spawn(0.0F, 0.0F);
  Engine::Core::EventManager::instance().publish(Engine::Core::CombatHitEvent(
      0, target, 12, Game::Units::SpawnType::Knight, false, 3, 2));
  feedback.advance(0.2F);
  ASSERT_EQ(feedback.live_count(), 1);

  const auto painted_pixels = [&feedback](float ui_scale) {
    QImage canvas(320, 320, QImage::Format_ARGB32);
    canvas.fill(Qt::black);
    {
      QPainter painter(&canvas);
      feedback.draw(
          painter,
          [](float, float, float, QPointF& out) {
            out = QPointF(160.0, 260.0);
            return true;
          },
          ui_scale);
    }
    int painted = 0;
    for (int y = 0; y < canvas.height(); ++y) {
      for (int x = 0; x < canvas.width(); ++x) {
        painted += canvas.pixelColor(x, y) != QColor(Qt::black) ? 1 : 0;
      }
    }
    return painted;
  };

  const int small = painted_pixels(1.0F);
  const int large = painted_pixels(2.0F);
  EXPECT_GT(small, 0);
  EXPECT_GT(large, small * 2)
      << "a pill tuned for a 1080p window has to grow with the recorded frame, "
         "or it is unreadable in the clip";
}

TEST_F(ArenaFeedbackTest, AnEventWithoutAWorldIsIgnored) {
  ArenaFeedback feedback;

  Engine::Core::EventManager::instance().publish(Engine::Core::CombatHitEvent(
      0, 1234, 9, Game::Units::SpawnType::Archer, false, 3, 2));

  feedback.advance(0.5F);
  EXPECT_EQ(feedback.live_count(), 0);
}

} // namespace
