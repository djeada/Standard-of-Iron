#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <limits>
#include <string_view>

#include <gtest/gtest.h>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/systems/nation_id.h"
#include "game/units/spawn_type.h"
#include "render/entity/registry.h"
#include "render/rigged_mesh.h"

using namespace Render::GL;

namespace {

struct RenderBounds {
  QVector3D min{std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max()};
  QVector3D max{std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest()};

  [[nodiscard]] auto width() const -> float { return max.x() - min.x(); }
  [[nodiscard]] auto height() const -> float { return max.y() - min.y(); }
  [[nodiscard]] auto depth() const -> float { return max.z() - min.z(); }
};

struct ProportionTolerance {
  float min_height_ratio{0.90F};
  float max_height_ratio{1.12F};
  float min_silhouette_ratio{0.85F};
  float max_silhouette_ratio{1.22F};
};

constexpr ProportionTolerance k_infantry_tolerance{};
// Support robes create a looser rendered depth silhouette than infantry armor.
constexpr ProportionTolerance k_support_tolerance{
    .min_height_ratio = 0.90F,
    .max_height_ratio = 1.12F,
    .min_silhouette_ratio = 0.82F,
    .max_silhouette_ratio = 1.22F};

class BoundsSubmitter : public ISubmitter {
public:
  RenderBounds bounds;
  bool saw_geometry = false;

  void mesh(Mesh* mesh,
            const QMatrix4x4& model,
            const QVector3D&,
            Texture* = nullptr,
            float = 1.0F,
            int = 0) override {
    if (mesh == nullptr) {
      return;
    }

    for (const auto& vertex : mesh->get_vertices()) {
      QVector3D const local(vertex.position[0], vertex.position[1], vertex.position[2]);
      QVector3D const world = model.map(local);
      saw_geometry = true;
      bounds.min.setX(std::min(bounds.min.x(), world.x()));
      bounds.min.setY(std::min(bounds.min.y(), world.y()));
      bounds.min.setZ(std::min(bounds.min.z(), world.z()));
      bounds.max.setX(std::max(bounds.max.x(), world.x()));
      bounds.max.setY(std::max(bounds.max.y(), world.y()));
      bounds.max.setZ(std::max(bounds.max.z(), world.z()));
    }
  }

  void rigged(const RiggedCreatureCmd& cmd) override {
    if (cmd.mesh == nullptr || cmd.bone_palette == nullptr) {
      return;
    }

    for (const auto& vertex : cmd.mesh->get_vertices()) {
      QVector4D const bone_local(vertex.position_bone_local[0],
                                 vertex.position_bone_local[1],
                                 vertex.position_bone_local[2],
                                 1.0F);
      QVector4D skinned_local(0.0F, 0.0F, 0.0F, 0.0F);
      for (int i = 0; i < 4; ++i) {
        float const weight = vertex.bone_weights[i];
        if (weight <= 0.0F) {
          continue;
        }
        auto const bone_index = static_cast<std::uint32_t>(vertex.bone_indices[i]);
        if (bone_index >= cmd.bone_count) {
          continue;
        }
        skinned_local += (cmd.bone_palette[bone_index] * bone_local) * weight;
      }

      QVector3D const world = cmd.world.map(skinned_local.toVector3D());
      saw_geometry = true;
      bounds.min.setX(std::min(bounds.min.x(), world.x()));
      bounds.min.setY(std::min(bounds.min.y(), world.y()));
      bounds.min.setZ(std::min(bounds.min.z(), world.z()));
      bounds.max.setX(std::max(bounds.max.x(), world.x()));
      bounds.max.setY(std::max(bounds.max.y(), world.y()));
      bounds.max.setZ(std::max(bounds.max.z(), world.z()));
    }
  }

  void cylinder(
      const QVector3D&, const QVector3D&, float, const QVector3D&, float) override {}
  void selection_ring(const QMatrix4x4&, float, float, const QVector3D&) override {}
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

auto render_single_soldier_bounds(std::string_view renderer_key,
                                  Game::Units::SpawnType spawn_type,
                                  Game::Systems::NationID nation_id,
                                  uint32_t seed) -> RenderBounds {
  EntityRendererRegistry registry;
  register_built_in_entity_renderers(registry);
  auto const renderer = registry.get(std::string(renderer_key));
  EXPECT_TRUE(static_cast<bool>(renderer));

  DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;
  ctx.has_seed_override = true;
  ctx.seed_override = seed;

  Engine::Core::Entity entity(1);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  EXPECT_NE(unit, nullptr);
  unit->spawn_type = spawn_type;
  unit->nation_id = nation_id;
  ctx.entity = &entity;

  BoundsSubmitter submitter;
  renderer(ctx, submitter);
  EXPECT_TRUE(submitter.saw_geometry);
  return submitter.bounds;
}

void expect_close_overall_proportions(const RenderBounds& roman,
                                      const RenderBounds& carthage,
                                      const ProportionTolerance& tolerance =
                                          k_infantry_tolerance) {
  float const roman_height = roman.height();
  float const carthage_height = carthage.height();
  ASSERT_GT(roman_height, 0.0F);
  ASSERT_GT(carthage_height, 0.0F);
  ASSERT_GT(roman.depth(), 0.0F);
  ASSERT_GT(carthage.depth(), 0.0F);

  float const height_ratio = roman_height / carthage_height;
  EXPECT_GE(height_ratio, tolerance.min_height_ratio);
  EXPECT_LE(height_ratio, tolerance.max_height_ratio);

  float const roman_height_depth_ratio = roman_height / roman.depth();
  float const carthage_height_depth_ratio = carthage_height / carthage.depth();
  float const silhouette_ratio = roman_height_depth_ratio / carthage_height_depth_ratio;
  EXPECT_GE(silhouette_ratio, tolerance.min_silhouette_ratio);
  EXPECT_LE(silhouette_ratio, tolerance.max_silhouette_ratio);
}

} // namespace

TEST(InfantryProportionsTest, RomanAndCarthageArchersStayComparable) {
  auto const roman = render_single_soldier_bounds("troops/roman/archer",
                                                  Game::Units::SpawnType::Archer,
                                                  Game::Systems::NationID::RomanRepublic,
                                                  1337U);
  auto const carthage = render_single_soldier_bounds("troops/carthage/archer",
                                                     Game::Units::SpawnType::Archer,
                                                     Game::Systems::NationID::Carthage,
                                                     1337U);

  expect_close_overall_proportions(roman, carthage);
}

TEST(InfantryProportionsTest, RomanAndCarthageSpearmenStayComparable) {
  auto const roman = render_single_soldier_bounds("troops/roman/spearman",
                                                  Game::Units::SpawnType::Spearman,
                                                  Game::Systems::NationID::RomanRepublic,
                                                  4242U);
  auto const carthage = render_single_soldier_bounds("troops/carthage/spearman",
                                                     Game::Units::SpawnType::Spearman,
                                                     Game::Systems::NationID::Carthage,
                                                     4242U);

  expect_close_overall_proportions(roman, carthage);
}

TEST(InfantryProportionsTest, RomanAndCarthageSwordsmenStayComparable) {
  auto const roman = render_single_soldier_bounds("troops/roman/swordsman",
                                                  Game::Units::SpawnType::Knight,
                                                  Game::Systems::NationID::RomanRepublic,
                                                  9001U);
  auto const carthage = render_single_soldier_bounds("troops/carthage/swordsman",
                                                     Game::Units::SpawnType::Knight,
                                                     Game::Systems::NationID::Carthage,
                                                     9001U);

  expect_close_overall_proportions(roman, carthage);
}

TEST(InfantryProportionsTest, RomanAndCarthageHealersStayComparable) {
  auto const roman = render_single_soldier_bounds("troops/roman/healer",
                                                  Game::Units::SpawnType::Healer,
                                                  Game::Systems::NationID::RomanRepublic,
                                                  777U);
  auto const carthage = render_single_soldier_bounds("troops/carthage/healer",
                                                     Game::Units::SpawnType::Healer,
                                                     Game::Systems::NationID::Carthage,
                                                     777U);

  expect_close_overall_proportions(roman, carthage, k_support_tolerance);
}
