#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <vector>

#include "render/draw_queue.h"
#include "render/ground/firecamp_renderer.h"
#include "render/local_lighting.h"

TEST(LocalLightingTest, SelectsTheMostRelevantLightsDeterministically) {
  std::vector<Render::LocalLight> lights;
  const int candidate_count = static_cast<int>(Render::k_max_local_lights) + 4;
  for (int i = 0; i < candidate_count; ++i) {
    Render::LocalLight light;
    light.position = QVector3D(static_cast<float>(i + 1), 0.0F, 0.0F);
    light.radius = 5.0F;
    light.intensity = 1.0F;
    lights.push_back(light);
  }

  const auto first = Render::select_local_lights(lights, QVector3D());
  const auto second = Render::select_local_lights(lights, QVector3D());
  ASSERT_EQ(first.size(), Render::k_max_local_lights);
  for (std::size_t i = 0; i < first.size(); ++i) {
    EXPECT_EQ(first[i].position, second[i].position);
    EXPECT_FLOAT_EQ(first[i].position.x(), static_cast<float>(i + 1));
  }
}

TEST(LocalLightingTest, DrawQueueCarriesSubmitterLightsAndClearsThemPerFrame) {
  Render::GL::DrawQueue queue;
  EXPECT_TRUE(queue.local_lights().empty());

  Render::LocalLight firelight;
  firelight.position = QVector3D(3.0F, 0.5F, -2.0F);
  firelight.radius = 8.0F;
  firelight.intensity = 1.25F;
  queue.submit_local_light(firelight);

  Render::LocalLight votive;
  votive.position = QVector3D(-6.0F, 1.0F, 4.0F);
  queue.submit_local_light(votive);

  ASSERT_EQ(queue.local_lights().size(), 2U);
  EXPECT_EQ(queue.local_lights().front().position, firelight.position);
  EXPECT_FLOAT_EQ(queue.local_lights().front().intensity, 1.25F);

  queue.clear();
  EXPECT_TRUE(queue.local_lights().empty());
}

TEST(LocalLightingTest, SanitizesNegativeRadiusAndIntensity) {
  Render::LocalLight invalid;
  invalid.radius = -4.0F;
  invalid.intensity = -2.0F;
  const auto selected =
      Render::select_local_lights({invalid}, QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_FLOAT_EQ(selected.front().radius, 0.01F);
  EXPECT_FLOAT_EQ(selected.front().intensity, 0.0F);
}

TEST(LocalLightingTest, FaderRampsNewLightsInInsteadOfPoppingThem) {
  Render::LocalLightFader fader;
  Render::LocalLight camp;
  camp.position = QVector3D(0.0F, 1.0F, 0.0F);
  camp.radius = 8.0F;
  camp.intensity = 1.0F;

  const auto first_frame = fader.update({}, QVector3D(), 0.0F);
  EXPECT_FLOAT_EQ(first_frame.front().intensity, 0.0F);

  const auto appearing = fader.update({camp}, QVector3D(), 0.05F);
  EXPECT_GT(appearing.front().intensity, 0.0F);
  EXPECT_LT(appearing.front().intensity, camp.intensity)
      << "a light entering the budget must ramp up, not pop";

  auto settled = appearing;
  for (int frame = 2; frame < 30; ++frame) {
    settled = fader.update({camp}, QVector3D(), 0.05F * static_cast<float>(frame));
  }
  EXPECT_FLOAT_EQ(settled.front().intensity, camp.intensity);
}

TEST(LocalLightingTest, FaderRampsDroppedLightsOutInsteadOfPoppingThem) {
  Render::LocalLightFader fader;
  Render::LocalLight camp;
  camp.position = QVector3D(0.0F, 1.0F, 0.0F);
  camp.radius = 8.0F;
  camp.intensity = 1.0F;

  float time = 0.0F;
  auto lit = fader.update({camp}, QVector3D(), time);
  ASSERT_FLOAT_EQ(lit.front().intensity, camp.intensity);

  time += 0.05F;
  const auto fading = fader.update({}, QVector3D(), time);
  EXPECT_GT(fading.front().intensity, 0.0F)
      << "a light leaving the budget must ramp down, not pop";
  EXPECT_LT(fading.front().intensity, camp.intensity);

  auto dark = fading;
  for (int frame = 2; frame < 30; ++frame) {
    time += 0.05F;
    dark = fader.update({}, QVector3D(), time);
  }
  EXPECT_FLOAT_EQ(dark.front().intensity, 0.0F);
  EXPECT_FLOAT_EQ(dark.front().radius, 0.0F);
}

TEST(LocalLightingTest, FaderResetDropsLightsFromThePreviousMap) {
  Render::LocalLightFader fader;
  Render::LocalLight camp;
  camp.position = QVector3D(12.0F, 1.0F, -4.0F);
  camp.radius = 8.0F;
  camp.intensity = 1.0F;

  ASSERT_FLOAT_EQ(fader.update({camp}, QVector3D(), 0.0F).front().intensity,
                  camp.intensity);

  fader.reset();
  const auto after_reset = fader.update({}, QVector3D(), 0.05F);
  EXPECT_FLOAT_EQ(after_reset.front().intensity, 0.0F);
  EXPECT_FLOAT_EQ(after_reset.front().radius, 0.0F);
}

TEST(LocalLightingTest, FaderPacksActiveLightsContiguously) {
  Render::LocalLightFader fader;
  std::vector<Render::LocalLight> lights;
  for (int i = 0; i < 3; ++i) {
    Render::LocalLight light;
    light.position = QVector3D(static_cast<float>(i) * 10.0F, 1.0F, 0.0F);
    light.radius = 6.0F;
    light.intensity = 1.0F;
    lights.push_back(light);
  }

  const auto selected = fader.update(lights, QVector3D(), 0.0F);
  for (std::size_t i = 0; i < selected.size(); ++i) {
    if (i < lights.size()) {
      EXPECT_GT(selected[i].intensity, 0.0F) << "slot " << i;
    } else {
      EXPECT_FLOAT_EQ(selected[i].intensity, 0.0F) << "slot " << i;
      EXPECT_FLOAT_EQ(selected[i].radius, 0.0F) << "slot " << i;
    }
  }
}

TEST(LocalLightingTest, EqualScoresKeepInputOrderSoLightsDoNotSwap) {

  std::vector<Render::LocalLight> lights;
  const int candidate_count = static_cast<int>(Render::k_max_local_lights) + 4;
  for (int i = 0; i < candidate_count; ++i) {
    Render::LocalLight light;
    light.position = QVector3D(0.0F, static_cast<float>(i), 0.0F);
    light.radius = 4.0F;
    light.intensity = 1.0F;
    light.color = QVector3D(static_cast<float>(i), 0.0F, 0.0F);
    lights.push_back(light);
  }

  const QVector3D camera(100.0F, 0.0F, 0.0F);
  for (auto& light : lights) {
    light.position = QVector3D(0.0F, 0.0F, 0.0F);
  }

  const auto selected = Render::select_local_lights(lights, camera);
  ASSERT_EQ(selected.size(), Render::k_max_local_lights);
  for (std::size_t i = 0; i < selected.size(); ++i) {
    EXPECT_FLOAT_EQ(selected[i].color.x(), static_cast<float>(i))
        << "tie at index " << i << " did not keep input order";
  }
}

TEST(LocalLightingTest, Std140PackingMatchesTheUniformBlockSlots) {
  std::array<Render::LocalLight, Render::k_max_local_lights> lights{};
  for (auto& light : lights) {
    light.radius = 0.0F;
    light.intensity = 0.0F;
  }
  lights[0].position = QVector3D(1.0F, 2.0F, 3.0F);
  lights[0].color = QVector3D(0.25F, 0.5F, 0.75F);
  lights[0].radius = 7.0F;
  lights[0].intensity = 1.5F;
  lights[1].position = QVector3D(-4.0F, 0.0F, 8.0F);
  lights[1].color = QVector3D(1.0F, 0.0F, 0.0F);
  lights[1].radius = 3.0F;
  lights[1].intensity = 0.5F;

  const auto packed = Render::pack_local_lights_std140(lights);
  using Block = Render::LocalLightingBlock;

  EXPECT_FLOAT_EQ(packed[Block::k_position_radius_offset + 0], 1.0F);
  EXPECT_FLOAT_EQ(packed[Block::k_position_radius_offset + 1], 2.0F);
  EXPECT_FLOAT_EQ(packed[Block::k_position_radius_offset + 2], 3.0F);
  EXPECT_FLOAT_EQ(packed[Block::k_position_radius_offset + 3], 7.0F);
  EXPECT_FLOAT_EQ(packed[Block::k_color_intensity_offset + 0], 0.25F);
  EXPECT_FLOAT_EQ(packed[Block::k_color_intensity_offset + 3], 1.5F);
  EXPECT_FLOAT_EQ(packed[Block::k_position_radius_offset + 4], -4.0F);
  EXPECT_FLOAT_EQ(packed[Block::k_color_intensity_offset + 7], 0.5F);
  EXPECT_FLOAT_EQ(packed[Block::k_meta_offset], 2.0F);
}

TEST(LocalLightingTest, Std140PackingSkipsDarkLightsSoSlotsStayContiguous) {
  std::array<Render::LocalLight, Render::k_max_local_lights> lights{};
  for (auto& light : lights) {
    light.radius = 0.0F;
    light.intensity = 0.0F;
  }
  lights[0].radius = 0.0F;
  lights[0].intensity = 1.0F;
  lights[1].position = QVector3D(5.0F, 0.0F, 0.0F);
  lights[1].radius = 4.0F;
  lights[1].intensity = 2.0F;

  const auto packed = Render::pack_local_lights_std140(lights);
  using Block = Render::LocalLightingBlock;

  EXPECT_FLOAT_EQ(packed[Block::k_position_radius_offset + 0], 5.0F)
      << "the surviving light must land in the first slot";
  EXPECT_FLOAT_EQ(packed[Block::k_meta_offset], 1.0F);
}

// A campfire lit a circle nearly thirty metres across. The pool had a visible
// edge on the ground and it recoloured a barracks twenty metres away orange -
// the brightest thing on a Second Punic War hillside was a cooking fire. The
// authored `radius` is the camp's own extent, the stone ring and the logs, not
// the reach of its light; multiplying it by 4.2 and lifting the light to the
// height of that same radius spread the pool out over the ground instead of
// pooling it where the fire is.
namespace {

// What every shipped map authors for a camp.
constexpr float k_authored_camp_radius = 3.4F;

// The falloff in assets/shaders/lib/local_lighting.glsl, so this test measures
// the light the player actually sees rather than the number in the struct.
auto shader_falloff(float distance, float radius) -> float {
  const float normalized = std::clamp(distance / radius, 0.0F, 1.0F);
  float window = 1.0F - normalized * normalized;
  window *= window;
  return window / (1.0F + 4.0F * normalized * normalized);
}

} // namespace

TEST(FireCampLightTest, ACampfireDoesNotLightHalfTheHillside) {
  const auto shape =
      Render::GL::FireCampRenderer::fire_light_shape(k_authored_camp_radius);

  EXPECT_LE(shape.reach, 7.0F) << "the firelight carries " << shape.reach
                               << " m, a pool " << (shape.reach * 2.0F) << " m across";
  EXPECT_GE(shape.reach, 3.0F) << "a campfire that lights nothing is not a campfire";

  // A barracks pitched a sensible distance from the camp must not be tinted by
  // it. Twenty metres was where the orange wash was seen.
  EXPECT_FLOAT_EQ(shader_falloff(20.0F, shape.reach), 0.0F)
      << "the fire is still recolouring a building twenty metres away";

  // And a soldier standing at the fire is lit by it.
  EXPECT_GT(shader_falloff(1.5F, shape.reach), 0.25F);
}

TEST(FireCampLightTest, TheFlameSitsOverTheLogsRatherThanOverTheCamp) {
  const auto shape =
      Render::GL::FireCampRenderer::fire_light_shape(k_authored_camp_radius);

  EXPECT_LE(shape.height_above_ground, 0.6F)
      << "the light hangs " << shape.height_above_ground
      << " m up, so its pool spreads instead of gathering at the fire";
  EXPECT_GT(shape.height_above_ground, 0.0F)
      << "a light buried in the ground lights nothing";
}

TEST(FireCampLightTest, ASmallCampStillGetsAUsableLight) {
  const auto tiny = Render::GL::FireCampRenderer::fire_light_shape(0.2F);
  EXPECT_GE(tiny.reach, 3.0F);
  EXPECT_GT(tiny.height_above_ground, 0.0F);
}
