#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <vector>

#include "game/core/component_core.h"
#include "game/core/entity.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "game/units/building_body.h"
#include "game/units/building_spawn_setup.h"
#include "render/entity/building_archetype_catalog.h"
#include "render/entity/home_renderer_common.h"
#include "render/entity/registry.h"
#include "render/gl/mesh.h"
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
  bool skip_yard{false};
  static constexpr float k_knee_height = 0.32F;
  static constexpr float k_plinth_reach = 1.19F;

  static auto outside_structure(const std::vector<QVector3D>& points) -> bool {
    return std::any_of(points.begin(), points.end(), [](const QVector3D& p) {
      return std::abs(p.x()) > k_plinth_reach || std::abs(p.z()) > k_plinth_reach;
    });
  }

  void mesh(Render::GL::Mesh* mesh,
            const QMatrix4x4& model,
            const QVector3D&,
            Render::GL::Texture*,
            float,
            int) override {
    if (mesh == nullptr) {
      return;
    }
    std::vector<QVector3D> points;
    points.reserve(mesh->get_vertices().size());
    float top = std::numeric_limits<float>::lowest();
    for (const auto& vertex : mesh->get_vertices()) {
      points.push_back(model.map(
          QVector3D(vertex.position[0], vertex.position[1], vertex.position[2])));
      top = std::max(top, points.back().y());
    }

    if (skip_yard && top < k_knee_height && outside_structure(points)) {
      return;
    }
    for (const auto& point : points) {
      bounds.include(point);
    }
  }

  void cylinder(const QVector3D& from,
                const QVector3D& to,
                float,
                const QVector3D&,
                float) override {
    if (skip_yard && std::max(from.y(), to.y()) < k_knee_height &&
        outside_structure({from, to})) {
      return;
    }
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
                             const std::string& renderer_key,
                             std::uint32_t entity_id = 1) -> PlanarBounds {
    Render::GL::RenderFunc const func =
        registry.get("troops/" + nation + "/" + renderer_key);
    if (!func) {
      return {};
    }

    Engine::Core::StandaloneEntity entity_scratch(entity_id);
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
    submitter.skip_yard = renderer_key == "home";
    func(ctx, submitter);
    return to_world_metres(submitter.bounds, renderer_key);
  }

  [[nodiscard]] static auto
  to_world_metres(PlanarBounds bounds,
                  const std::string& building_type) -> PlanarBounds {
    if (!bounds.valid()) {
      return bounds;
    }
    const QVector3D scale = Game::Units::building_transform_scale(building_type);
    bounds.min_x *= scale.x();
    bounds.max_x *= scale.x();
    bounds.min_z *= scale.z();
    bounds.max_z *= scale.z();
    return bounds;
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
      const float padding_limit =
          0.35F * Game::Units::building_transform_scale(entry.collision_type).x();

      EXPECT_GE(body.width + 0.02F, drawn.span_x())
          << nation << "/" << entry.collision_type << ": the commander's collider is "
          << body.width << " m wide but the building is drawn " << drawn.span_x()
          << " m wide, so he can walk into the wall";
      EXPECT_GE(body.depth + 0.02F, drawn.span_z())
          << nation << "/" << entry.collision_type << ": the commander's collider is "
          << body.depth << " m deep but the building is drawn " << drawn.span_z()
          << " m deep";

      EXPECT_LE(body.width, drawn.span_x() + padding_limit)
          << nation << "/" << entry.collision_type
          << ": the collider is padded well past the drawn wall; that padding "
             "belongs to the navigation footprint, not the body";
      EXPECT_LE(body.depth, drawn.span_z() + padding_limit)
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

TEST_F(BuildingBodyFootprintTest, HouseVariationKeepsTheExistingCollisionFootprint) {
  std::array<bool, Render::GL::k_home_visual_variant_count> seen{};
  const auto body = Game::Systems::BuildingCollisionRegistry::get_building_body("home");
  for (std::uint32_t id = 1; id <= 24; ++id) {
    const auto variant = Render::GL::home_visual_variant(id);
    ASSERT_GE(variant, 0);
    ASSERT_LT(variant, static_cast<int>(seen.size()));
    if (seen[variant]) {
      continue;
    }
    seen[variant] = true;
    for (const char* nation : {"roman", "carthage"}) {
      const auto drawn = measure(nation, "home", id);
      ASSERT_TRUE(drawn.valid());
      EXPECT_LE(drawn.span_x(), body.width + 0.02F) << nation << " variant " << variant;
      EXPECT_LE(drawn.span_z(), body.depth + 0.02F) << nation << " variant " << variant;
      EXPECT_NEAR(drawn.center_x(), body.offset_x, 0.20F);
      EXPECT_NEAR(drawn.center_z(), body.offset_z, 0.20F);
    }
  }
  EXPECT_TRUE(std::all_of(seen.begin(), seen.end(), [](bool value) { return value; }));
}

auto standing_archetype_bounds(const std::string& catalog_name,
                               float head_height) -> PlanarBounds {
  PlanarBounds bounds;
  for (const auto& entry : Render::GL::building_archetype_catalog()) {
    if (entry.name != catalog_name) {
      continue;
    }
    const auto archetype = Render::GL::build_building_archetype(
        entry.build(Render::GL::BuildingState::Normal),
        Render::GL::BuildingState::Normal);
    for (const auto& draw :
         archetype.lods[static_cast<std::size_t>(Render::GL::RenderArchetypeLod::Full)]
             .draws) {
      if (draw.mesh == nullptr) {
        continue;
      }
      for (const auto& vertex : draw.mesh->get_vertices()) {
        const QVector3D point = draw.local_model.map(
            QVector3D(vertex.position[0], vertex.position[1], vertex.position[2]));
        if (point.y() <= head_height) {
          bounds.include(point);
        }
      }
    }
  }
  return bounds;
}

TEST_F(BuildingBodyFootprintTest, BarracksBodyIsTheKeepNotTheStockpileYard) {
  constexpr float k_head_height_m = 1.8F;
  const auto body = Game::Units::find_building_body_extent("barracks");
  ASSERT_TRUE(body.has_value());
  const float vertical = Game::Units::building_transform_scale("barracks").y();
  for (const char* nation : {"roman", "carthage"}) {
    const auto drawn = standing_archetype_bounds(std::string(nation) + "_barracks",
                                                 k_head_height_m / vertical);
    ASSERT_TRUE(drawn.valid()) << nation;
    EXPECT_GE(body->width + 0.02F, drawn.span_x()) << nation;
    EXPECT_GE(body->depth + 0.02F, drawn.span_z()) << nation;
    EXPECT_LE(body->width, drawn.span_x() + 0.20F)
        << nation << ": the walk-through stockpile yard is not part of the body";
    EXPECT_LE(body->depth, drawn.span_z() + 0.20F) << nation;
    EXPECT_NEAR(body->offset_x, drawn.center_x(), 0.10F) << nation;
    EXPECT_NEAR(body->offset_z, drawn.center_z(), 0.10F) << nation;
  }
}

TEST_F(BuildingBodyFootprintTest, BarracksFoundationStillLevelsTheStockpileYard) {
  const auto foundation = Game::Units::find_building_foundation_extent("barracks");
  ASSERT_TRUE(foundation.has_value());
  for (const char* nation : {"roman", "carthage"}) {
    const auto drawn =
        standing_archetype_bounds(std::string(nation) + "_barracks", 1.0e6F);
    ASSERT_TRUE(drawn.valid()) << nation;
    EXPECT_GE(foundation->width + 0.30F, drawn.span_x()) << nation;
    EXPECT_GE(foundation->depth + 0.30F, drawn.span_z()) << nation;
  }
}

TEST_F(BuildingBodyFootprintTest, RegisteredBodyIsMeasuredInWorldMetres) {
  for (const auto& extent : Game::Units::k_building_body_extents) {
    const std::string type(extent.type);
    const QVector3D scale = Game::Units::building_transform_scale(type);
    const auto world =
        Game::Systems::BuildingCollisionRegistry::get_building_body(type);
    EXPECT_NEAR(world.width, extent.width * scale.x(), 1.0e-4F)
        << type
        << ": the building is drawn at its spawn scale, so its body must be too";
    EXPECT_NEAR(world.depth, extent.depth * scale.z(), 1.0e-4F) << type;
    EXPECT_NEAR(world.offset_x, extent.offset_x * scale.x(), 1.0e-4F) << type;
    EXPECT_NEAR(world.offset_z, extent.offset_z * scale.z(), 1.0e-4F) << type;
  }
}

} // namespace
