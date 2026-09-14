#include <QFile>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions_3_3_Core>
#include <QSurfaceFormat>

#include <algorithm>
#include <array>
#include <filesystem>
#include <gtest/gtest.h>
#include <vector>

#include "render/draw_queue.h"
#include "render/gl/shader.h"
#include "render/gl/ubo_bindings.h"
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

namespace {

constexpr float k_authored_camp_radius = 3.4F;

} // namespace

TEST(FireCampLightTest, ACampfireDoesNotLightHalfTheHillside) {
  const auto shape =
      Render::GL::FireCampRenderer::fire_light_shape(k_authored_camp_radius);

  EXPECT_LE(shape.reach, 7.0F) << "the firelight carries " << shape.reach
                               << " m, a pool " << (shape.reach * 2.0F) << " m across";
  EXPECT_GE(shape.reach, 3.0F) << "a campfire that lights nothing is not a campfire";
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

TEST(FireCampLightTest, ShaderLightsNearbyGroundAndObjectsWithinItsRadius) {
  QSurfaceFormat format;
  format.setVersion(3, 3);
  format.setProfile(QSurfaceFormat::CoreProfile);
  QOffscreenSurface surface;
  surface.setFormat(format);
  surface.create();
  QOpenGLContext context;
  context.setFormat(format);
  if (!surface.isValid() || !context.create() || !context.makeCurrent(&surface)) {
    GTEST_SKIP() << "No OpenGL 3.3 context available";
  }
  QOpenGLFunctions_3_3_Core gl;
  ASSERT_TRUE(gl.initializeOpenGLFunctions());

  const auto shader_path = std::filesystem::path(__FILE__).parent_path() /
                           "../../assets/shaders/include/local_lighting.glsl";
  QFile source(QString::fromStdString(shader_path.string()));
  ASSERT_TRUE(source.open(QIODevice::ReadOnly));
  Render::GL::Shader shader;
  ASSERT_TRUE(shader.load_from_source(QStringLiteral(R"(#version 330 core
void main() {
  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)"),
                                      QStringLiteral("#version 330 core\n") +
                                          QString::fromUtf8(source.readAll()) +
                                          QStringLiteral(R"(
uniform vec3 u_sample_position;
uniform vec3 u_sample_normal;
out vec4 frag_color;
void main() {
  frag_color = vec4(local_lighting(u_sample_position, u_sample_normal), 1.0);
}
)")));

  const auto shape =
      Render::GL::FireCampRenderer::fire_light_shape(k_authored_camp_radius);
  Render::LocalLight fire;
  fire.position = QVector3D(0.0F, shape.height_above_ground, 0.0F);
  fire.radius = shape.reach;
  fire.color = QVector3D(1.0F, 0.52F, 0.19F);
  const auto packed = Render::pack_local_lights_std140(
      Render::select_local_lights({fire}, QVector3D()));
  GLuint buffer = 0;
  gl.glGenBuffers(1, &buffer);
  gl.glBindBuffer(GL_UNIFORM_BUFFER, buffer);
  gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(packed), packed.data(), GL_STATIC_DRAW);
  gl.glBindBufferBase(
      GL_UNIFORM_BUFFER, Render::GL::k_local_lighting_binding_point, buffer);
  GLuint vao = 0;
  gl.glGenVertexArrays(1, &vao);
  gl.glBindVertexArray(vao);
  QOpenGLFramebufferObjectFormat target_format;
  target_format.setInternalTextureFormat(GL_RGBA32F);
  QOpenGLFramebufferObject target(1, 1, target_format);
  ASSERT_TRUE(target.bind());
  gl.glViewport(0, 0, 1, 1);
  shader.use();
  const auto sample = [&](const QVector3D& position, const QVector3D& normal) {
    shader.set_uniform("u_sample_position", position);
    shader.set_uniform("u_sample_normal", normal);
    gl.glDrawArrays(GL_TRIANGLES, 0, 3);
    std::array<float, 4> pixel{};
    gl.glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, pixel.data());
    return QVector3D(pixel[0], pixel[1], pixel[2]);
  };

  const QVector3D up(0.0F, 1.0F, 0.0F);
  const auto ground = sample(QVector3D(3.0F, 0.0F, 0.0F), up);
  EXPECT_GT(ground.x(), 0.16F) << "the ground halfway through the light pool is dim";
  EXPECT_GT(ground.y(), 0.08F);
  EXPECT_GT(ground.x(), ground.y());
  EXPECT_GT(ground.y(), ground.z());
  const QVector3D object_position(3.0F, shape.height_above_ground, 0.0F);
  const auto facing = sample(object_position, QVector3D(-1.0F, 0.0F, 0.0F));
  EXPECT_GT(facing.x(), 0.4F) << "nearby objects should visibly catch the firelight";
  EXPECT_LT(facing.x(), 1.0F);
  EXPECT_GT(sample(QVector3D(1.0F, 0.0F, 0.0F), up).x(), ground.x());
  EXPECT_EQ(sample(QVector3D(shape.reach, 0.0F, 0.0F), up), QVector3D());
  EXPECT_EQ(sample(QVector3D(20.0F, 0.0F, 0.0F), up), QVector3D());
  EXPECT_EQ(sample(object_position, QVector3D(1.0F, 0.0F, 0.0F)), QVector3D());

  shader.set_uniform("u_has_local_light_mask", 1);
  shader.set_uniform("u_local_light_mask", 0);
  EXPECT_EQ(sample(QVector3D(3.0F, 0.0F, 0.0F), up), QVector3D());
  shader.set_uniform("u_local_light_mask", 1);
  EXPECT_EQ(sample(QVector3D(3.0F, 0.0F, 0.0F), up), ground);
  EXPECT_EQ(gl.glGetError(), static_cast<GLenum>(GL_NO_ERROR));

  shader.release();
  target.release();
  gl.glDeleteVertexArrays(1, &vao);
  gl.glDeleteBuffers(1, &buffer);
}
