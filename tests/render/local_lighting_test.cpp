#include <gtest/gtest.h>
#include <vector>

#include "render/draw_queue.h"
#include "render/local_lighting.h"

TEST(LocalLightingTest, SelectsTheMostRelevantLightsDeterministically) {
  std::vector<Render::LocalLight> lights;
  for (int i = 0; i < 12; ++i) {
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

  // Instanced emitters resubmit every frame, so a stale light must never
  // survive into the next one.
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

TEST(LocalLightingTest, EqualScoresKeepInputOrderSoLightsDoNotSwap) {
  // Ranking is recomputed every frame; if equally-scored lights could displace
  // each other the selection would flicker between frames.
  std::vector<Render::LocalLight> lights;
  for (int i = 0; i < 12; ++i) {
    Render::LocalLight light;
    light.position = QVector3D(0.0F, static_cast<float>(i), 0.0F);
    light.radius = 4.0F;
    light.intensity = 1.0F;
    light.color = QVector3D(static_cast<float>(i), 0.0F, 0.0F);
    lights.push_back(light);
  }
  // All at the same distance from this camera, so every score is identical.
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
