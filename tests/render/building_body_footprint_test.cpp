#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <vector>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "render/entity/registry.h"
#include "render/submitter.h"

namespace {

struct PlanarBounds {
  float min_x{std::numeric_limits<float>::max()};
  float max_x{std::numeric_limits<float>::lowest()};
  float min_z{std::numeric_limits<float>::max()};
  float max_z{std::numeric_limits<float>::lowest()};

  void include(const QVector3D& point) {
    min_x = std::min(min_x, point.x());
    max_x = std::max(max_x, point.x());
    min_z = std::min(min_z, point.z());
    max_z = std::max(max_z, point.z());
  }

  [[nodiscard]] auto valid() const -> bool { return max_x >= min_x; }
  [[nodiscard]] auto span_x() const -> float { return max_x - min_x; }
  [[nodiscard]] auto span_z() const -> float { return max_z - min_z; }
  [[nodiscard]] auto center_x() const -> float { return (min_x + max_x) * 0.5F; }
  [[nodiscard]] auto center_z() const -> float { return (min_z + max_z) * 0.5F; }
};

class BoundsSubmitter final : public Render::GL::ISubmitter {
public:
  PlanarBounds bounds;

  void mesh(Render::GL::Mesh* mesh,
            const QMatrix4x4& model,
            const QVector3D&,
            Render::GL::Texture*,
            float,
            int) override {
    if (mesh == nullptr) {
      return;
    }
    for (const auto& vertex : mesh->get_vertices()) {
      bounds.include(model.map(
          QVector3D(vertex.position[0], vertex.position[1], vertex.position[2])));
    }
  }

  void cylinder(const QVector3D& from,
                const QVector3D& to,
                float,
                const QVector3D&,
                float) override {
    bounds.include(from);
    bounds.include(to);
  }

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

class BuildingBodyFootprintTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    Game::Systems::initialize_default_content(nations);
    Render::GL::register_built_in_entity_renderers(registry);
  }

  [[nodiscard]] auto measure(const std::string& nation,
                             const std::string& renderer_key) -> PlanarBounds {
    Render::GL::RenderFunc const func =
        registry.get("troops/" + nation + "/" + renderer_key);
    if (!func) {
      return {};
    }

    Engine::Core::StandaloneEntity entity_scratch(1);
    Engine::Core::Entity& entity = entity_scratch.entity();
    entity.add_component<Engine::Core::RenderableComponent>();
    entity.add_component<Engine::Core::TransformComponent>();
    auto* unit = entity.add_component<Engine::Core::UnitComponent>();
    if (unit != nullptr) {
      unit->owner_id = 1;
      unit->health = unit->max_health;
      unit->nation_id = (nation == "carthage") ? Game::Systems::NationID::Carthage
                                               : Game::Systems::NationID::RomanRepublic;
    }

    Render::GL::DrawContext ctx;
    ctx.entity = &entity;
    ctx.resources = nullptr;
    ctx.model = QMatrix4x4{};

    BoundsSubmitter submitter;
    func(ctx, submitter);
    return submitter.bounds;
  }

  Render::GL::EntityRendererRegistry registry;
};

struct BodyCase {
  const char* collision_type;
  const char* renderer_key;
};

TEST_F(BuildingBodyFootprintTest, RegisteredBodyCoversTheDrawnBuilding) {
  const std::vector<BodyCase> cases = {
      {"home", "home"},
      {"marketplace", "marketplace"},
      {"temple", "temple"},
      {"defense_tower", "defense_tower"},
  };

  for (const auto& entry : cases) {
    for (const char* nation : {"roman", "carthage"}) {
      const auto drawn = measure(nation, entry.renderer_key);
      if (!drawn.valid()) {
        continue;
      }

      const auto body = Game::Systems::BuildingCollisionRegistry::get_building_body(
          entry.collision_type);

      EXPECT_GE(body.width + 0.02F, drawn.span_x())
          << nation << "/" << entry.collision_type << ": the commander's collider is "
          << body.width << " m wide but the building is drawn " << drawn.span_x()
          << " m wide, so he can walk into the wall";
      EXPECT_GE(body.depth + 0.02F, drawn.span_z())
          << nation << "/" << entry.collision_type << ": the commander's collider is "
          << body.depth << " m deep but the building is drawn " << drawn.span_z()
          << " m deep";

      EXPECT_LE(body.width, drawn.span_x() + 0.35F)
          << nation << "/" << entry.collision_type
          << ": the collider is padded well past the drawn wall; that padding "
             "belongs to the navigation footprint, not the body";
      EXPECT_LE(body.depth, drawn.span_z() + 0.35F)
          << nation << "/" << entry.collision_type
          << ": the collider is padded well past the drawn wall";

      EXPECT_NEAR(body.offset_x, drawn.center_x(), 0.20F)
          << nation << "/" << entry.collision_type
          << ": the collider is not centred on the building that is drawn";
      EXPECT_NEAR(body.offset_z, drawn.center_z(), 0.20F)
          << nation << "/" << entry.collision_type
          << ": the collider is not centred on the building that is drawn";
    }
  }
}

TEST_F(BuildingBodyFootprintTest, TheNavigationFootprintIsNotTheBody) {
  for (const char* type : {"home", "marketplace", "temple"}) {
    const auto nav = Game::Systems::BuildingCollisionRegistry::get_building_size(type);
    const auto body = Game::Systems::BuildingCollisionRegistry::get_building_body(type);

    EXPECT_LT(body.width, nav.width)
        << type
        << ": the navigation footprint keeps whole formations clear of a "
           "structure and is deliberately larger than the building. If these "
           "ever match, one of the two tables has been edited by mistake.";
    EXPECT_LT(body.depth, nav.depth) << type;
  }
}

} // namespace
