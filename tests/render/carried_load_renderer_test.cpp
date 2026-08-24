#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/systems/harvest_yields.h"
#include "game/systems/resource_types.h"
#include "render/entity/carried_load_renderer.h"
#include "render/submitter.h"

namespace {

class ColourRecordingSubmitter final : public Render::GL::ISubmitter {
public:
  std::vector<QVector3D> colours;

  void mesh(Render::GL::Mesh*,
            const QMatrix4x4&,
            const QVector3D& color,
            Render::GL::Texture*,
            float,
            int) override {
    colours.push_back(color);
  }

  void cylinder(
      const QVector3D&, const QVector3D&, float, const QVector3D&, float) override {}
  void ground_marker(const Render::GL::GroundMarkerCmd&) override {}
  void grid(const QMatrix4x4&, const QVector3D&, float, float, float) override {}
  void selection_smoke(const QMatrix4x4&, const QVector3D&, float) override {}
  void healing_beam(const QVector3D&,
                    const QVector3D&,
                    const QVector3D&,
                    float,
                    float,
                    float,
                    float) override {}
  void healer_aura(const QVector3D&, const QVector3D&, float, float, float) override {}
  void combat_dust(const QVector3D&, const QVector3D&, float, float, float) override {}
  void stone_impact(const QVector3D&, const QVector3D&, float, float, float) override {}
  void mode_indicator(const QMatrix4x4&, int, const QVector3D&, float) override {}
};

auto carry_food(Engine::Core::World& world,
                Engine::Core::CarriedFoodForm form) -> std::vector<QVector3D> {
  auto* hauler = world.create_entity();
  hauler->add_component<Engine::Core::TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* unit = hauler->add_component<Engine::Core::UnitComponent>();
  unit->health = 100;
  unit->max_health = 100;
  auto* carry = hauler->add_component<Engine::Core::ResourceCarryComponent>();
  carry->amounts.add(Game::Systems::ResourceType::Food,
                     Game::Systems::k_slaughter_sheep_food_reward);
  carry->food_form = form;

  ColourRecordingSubmitter out;
  const auto stats = Render::GL::submit_carried_loads(&world, out, nullptr, nullptr);
  EXPECT_EQ(stats.haulers, 1U);
  EXPECT_EQ(stats.loads, 1U);
  return out.colours;
}

auto contains_near(const std::vector<QVector3D>& colours,
                   const QVector3D& wanted) -> bool {
  return std::any_of(colours.begin(), colours.end(), [&](const QVector3D& colour) {
    return (colour - wanted).length() < 0.16F;
  });
}

TEST(CarriedLoadRendererTest, AButcheredSheepIsCarriedAsACarcassNotASheaf) {
  Engine::Core::World grain_world;
  const auto grain = carry_food(grain_world, Engine::Core::CarriedFoodForm::Grain);

  Engine::Core::World meat_world;
  const auto meat = carry_food(meat_world, Engine::Core::CarriedFoodForm::Meat);

  ASSERT_FALSE(grain.empty());
  ASSERT_FALSE(meat.empty());
  EXPECT_NE(grain, meat) << "meat and grain must not share the same carried prop";

  EXPECT_TRUE(contains_near(grain, QVector3D(0.84F, 0.68F, 0.32F)))
      << "a reaped field rides home as straw";
  EXPECT_TRUE(contains_near(meat, QVector3D(0.86F, 0.83F, 0.76F)))
      << "a butchered sheep rides home as a fleeced carcass";
  EXPECT_FALSE(contains_near(meat, QVector3D(0.84F, 0.68F, 0.32F)));
}

} // namespace
