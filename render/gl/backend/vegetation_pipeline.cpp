#include "vegetation_pipeline.h"
#include "../render_constants.h"
#include "gl/shader_cache.h"
#include <GL/gl.h>
#include <QDebug>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <qglobal.h>
#include <qopenglext.h>
#include <qstringliteral.h>
#include <qvectornd.h>
#include <vector>

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;
using namespace Render::GL::Geometry;

VegetationPipeline::VegetationPipeline(ShaderCache *shaderCache)
    : m_shaderCache(shaderCache) {}

VegetationPipeline::~VegetationPipeline() { shutdown(); }

auto VegetationPipeline::initialize() -> bool {
  initializeOpenGLFunctions();

  if (m_shaderCache == nullptr) {
    return false;
  }

  m_stoneShader = m_shaderCache->get(QStringLiteral("stone_instanced"));
  m_plantShader = m_shaderCache->get(QStringLiteral("plant_instanced"));
  m_pineShader = m_shaderCache->get(QStringLiteral("pine_instanced"));
  m_firecampShader = m_shaderCache->get(QStringLiteral("firecamp"));

  if (m_stoneShader == nullptr) {
    qWarning() << "VegetationPipeline: stone shader missing";
  }
  if (m_plantShader == nullptr) {
    qWarning() << "VegetationPipeline: plant shader missing";
  }
  if (m_pineShader == nullptr) {
    qWarning() << "VegetationPipeline: pine shader missing";
  }
  if (m_firecampShader == nullptr) {
    qWarning() << "VegetationPipeline: firecamp shader missing";
  }

  initializeStonePipeline();
  initializePlantPipeline();
  initializePinePipeline();
  initializeFireCampPipeline();
  cacheUniforms();

  m_initialized = true;
  return true;
}

void VegetationPipeline::shutdown() {
  shutdownStonePipeline();
  shutdownPlantPipeline();
  shutdownPinePipeline();
  shutdownFireCampPipeline();
  m_initialized = false;
}

void VegetationPipeline::cacheUniforms() {
  if (m_stoneShader != nullptr) {
    m_stoneUniforms.view_proj = m_stoneShader->uniformHandle("uViewProj");
    m_stoneUniforms.light_direction =
        m_stoneShader->uniformHandle("uLightDirection");
  }

  if (m_plantShader != nullptr) {
    m_plantUniforms.view_proj = m_plantShader->uniformHandle("uViewProj");
    m_plantUniforms.time = m_plantShader->uniformHandle("uTime");
    m_plantUniforms.windStrength =
        m_plantShader->uniformHandle("uWindStrength");
    m_plantUniforms.windSpeed = m_plantShader->uniformHandle("uWindSpeed");
    m_plantUniforms.light_direction =
        m_plantShader->uniformHandle("uLightDirection");
  }

  if (m_pineShader != nullptr) {
    m_pineUniforms.view_proj = m_pineShader->uniformHandle("uViewProj");
    m_pineUniforms.time = m_pineShader->uniformHandle("uTime");
    m_pineUniforms.windStrength = m_pineShader->uniformHandle("uWindStrength");
    m_pineUniforms.windSpeed = m_pineShader->uniformHandle("uWindSpeed");
    m_pineUniforms.light_direction =
        m_pineShader->uniformHandle("uLightDirection");
  }

  if (m_firecampShader != nullptr) {
    m_firecampUniforms.view_proj =
        m_firecampShader->uniformHandle("u_viewProj");
    m_firecampUniforms.time = m_firecampShader->uniformHandle("u_time");
    m_firecampUniforms.flickerSpeed =
        m_firecampShader->uniformHandle("u_flickerSpeed");
    m_firecampUniforms.flickerAmount =
        m_firecampShader->uniformHandle("u_flickerAmount");
    m_firecampUniforms.glowStrength =
        m_firecampShader->uniformHandle("u_glowStrength");
    m_firecampUniforms.fireTexture =
        m_firecampShader->uniformHandle("fireTexture");
    m_firecampUniforms.camera_right =
        m_firecampShader->uniformHandle("u_cameraRight");
    m_firecampUniforms.camera_forward =
        m_firecampShader->uniformHandle("u_cameraForward");
  }
}

void VegetationPipeline::initializeStonePipeline() {
  initializeOpenGLFunctions();
  shutdownStonePipeline();

  struct StoneVertex {
    QVector3D position;
    QVector3D normal;
  };

  const StoneVertex stone_vertices[] = {
      {{-0.5F, -0.5F, 0.5F}, {0.0F, 0.0F, 1.0F}},
      {{0.5F, -0.5F, 0.5F}, {0.0F, 0.0F, 1.0F}},
      {{0.5F, 0.5F, 0.5F}, {0.0F, 0.0F, 1.0F}},
      {{-0.5F, 0.5F, 0.5F}, {0.0F, 0.0F, 1.0F}},
      {{-0.5F, -0.5F, -0.5F}, {0.0F, 0.0F, -1.0F}},
      {{-0.5F, 0.5F, -0.5F}, {0.0F, 0.0F, -1.0F}},
      {{0.5F, 0.5F, -0.5F}, {0.0F, 0.0F, -1.0F}},
      {{0.5F, -0.5F, -0.5F}, {0.0F, 0.0F, -1.0F}},
      {{-0.5F, 0.5F, -0.5F}, {0.0F, 1.0F, 0.0F}},
      {{-0.5F, 0.5F, 0.5F}, {0.0F, 1.0F, 0.0F}},
      {{0.5F, 0.5F, 0.5F}, {0.0F, 1.0F, 0.0F}},
      {{0.5F, 0.5F, -0.5F}, {0.0F, 1.0F, 0.0F}},
      {{-0.5F, -0.5F, -0.5F}, {0.0F, -1.0F, 0.0F}},
      {{0.5F, -0.5F, -0.5F}, {0.0F, -1.0F, 0.0F}},
      {{0.5F, -0.5F, 0.5F}, {0.0F, -1.0F, 0.0F}},
      {{-0.5F, -0.5F, 0.5F}, {0.0F, -1.0F, 0.0F}},
      {{0.5F, -0.5F, -0.5F}, {1.0F, 0.0F, 0.0F}},
      {{0.5F, 0.5F, -0.5F}, {1.0F, 0.0F, 0.0F}},
      {{0.5F, 0.5F, 0.5F}, {1.0F, 0.0F, 0.0F}},
      {{0.5F, -0.5F, 0.5F}, {1.0F, 0.0F, 0.0F}},
      {{-0.5F, -0.5F, -0.5F}, {-1.0F, 0.0F, 0.0F}},
      {{-0.5F, -0.5F, 0.5F}, {-1.0F, 0.0F, 0.0F}},
      {{-0.5F, 0.5F, 0.5F}, {-1.0F, 0.0F, 0.0F}},
      {{-0.5F, 0.5F, -0.5F}, {-1.0F, 0.0F, 0.0F}},
  };

  const uint16_t stone_indices[] = {
      0,  1,  2,  2,  3,  0,  4,  5,  6,  6,  7,  4,  8,  9,  10, 10, 11, 8,
      12, 13, 14, 14, 15, 12, 16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20};

  glGenVertexArrays(1, &m_stoneVao);
  glBindVertexArray(m_stoneVao);

  glGenBuffers(1, &m_stoneVertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_stoneVertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(stone_vertices), stone_vertices,
               GL_STATIC_DRAW);
  m_stoneVertexCount = CubeVertexCount;

  glGenBuffers(1, &m_stoneIndexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_stoneIndexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(stone_indices), stone_indices,
               GL_STATIC_DRAW);
  m_stoneIndexCount = 36;

  glEnableVertexAttribArray(Position);
  glVertexAttribPointer(
      Position, Vec3, GL_FLOAT, GL_FALSE, sizeof(StoneVertex),
      reinterpret_cast<void *>(offsetof(StoneVertex, position)));
  glEnableVertexAttribArray(Normal);
  glVertexAttribPointer(
      Normal, Vec3, GL_FLOAT, GL_FALSE, sizeof(StoneVertex),
      reinterpret_cast<void *>(offsetof(StoneVertex, normal)));

  glEnableVertexAttribArray(TexCoord);
  glVertexAttribDivisor(TexCoord, 1);
  glEnableVertexAttribArray(InstancePosition);
  glVertexAttribDivisor(InstancePosition, 1);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void VegetationPipeline::shutdownStonePipeline() {
  initializeOpenGLFunctions();
  if (m_stoneIndexBuffer != 0u) {
    glDeleteBuffers(1, &m_stoneIndexBuffer);
    m_stoneIndexBuffer = 0;
  }
  if (m_stoneVertexBuffer != 0u) {
    glDeleteBuffers(1, &m_stoneVertexBuffer);
    m_stoneVertexBuffer = 0;
  }
  if (m_stoneVao != 0u) {
    glDeleteVertexArrays(1, &m_stoneVao);
    m_stoneVao = 0;
  }
  m_stoneVertexCount = 0;
  m_stoneIndexCount = 0;
}

void VegetationPipeline::initializePlantPipeline() {
  initializeOpenGLFunctions();
  shutdownPlantPipeline();

  struct PlantVertex {
    QVector3D position;
    QVector2D tex_coord;
    QVector3D normal;
  };

  const PlantVertex plant_vertices[] = {
      {{-0.5F, 0.0F, 0.0F}, {0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
      {{0.5F, 0.0F, 0.0F}, {1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
      {{0.5F, 1.0F, 0.0F}, {1.0F, 1.0F}, {0.0F, 0.0F, 1.0F}},
      {{-0.5F, 1.0F, 0.0F}, {0.0F, 1.0F}, {0.0F, 0.0F, 1.0F}},
      {{0.5F, 0.0F, 0.0F}, {0.0F, 0.0F}, {0.0F, 0.0F, -1.0F}},
      {{-0.5F, 0.0F, 0.0F}, {1.0F, 0.0F}, {0.0F, 0.0F, -1.0F}},
      {{-0.5F, 1.0F, 0.0F}, {1.0F, 1.0F}, {0.0F, 0.0F, -1.0F}},
      {{0.5F, 1.0F, 0.0F}, {0.0F, 1.0F}, {0.0F, 0.0F, -1.0F}},
      {{0.0F, 0.0F, -0.5F}, {0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}},
      {{0.0F, 0.0F, 0.5F}, {1.0F, 0.0F}, {1.0F, 0.0F, 0.0F}},
      {{0.0F, 1.0F, 0.5F}, {1.0F, 1.0F}, {1.0F, 0.0F, 0.0F}},
      {{0.0F, 1.0F, -0.5F}, {0.0F, 1.0F}, {1.0F, 0.0F, 0.0F}},
      {{0.0F, 0.0F, 0.5F}, {0.0F, 0.0F}, {-1.0F, 0.0F, 0.0F}},
      {{0.0F, 0.0F, -0.5F}, {1.0F, 0.0F}, {-1.0F, 0.0F, 0.0F}},
      {{0.0F, 1.0F, -0.5F}, {1.0F, 1.0F}, {-1.0F, 0.0F, 0.0F}},
      {{0.0F, 1.0F, 0.5F}, {0.0F, 1.0F}, {-1.0F, 0.0F, 0.0F}},
  };

  const unsigned short plant_indices[] = {
      0, 1, 2,  0, 2,  3,  4,  5,  6,  4,  6,  7,
      8, 9, 10, 8, 10, 11, 12, 13, 14, 12, 14, 15,
  };

  glGenVertexArrays(1, &m_plantVao);
  glBindVertexArray(m_plantVao);

  glGenBuffers(1, &m_plantVertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_plantVertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(plant_vertices), plant_vertices,
               GL_STATIC_DRAW);
  m_plantVertexCount = PlantCrossQuadVertexCount;

  glEnableVertexAttribArray(Position);
  glVertexAttribPointer(
      Position, Vec3, GL_FLOAT, GL_FALSE, sizeof(PlantVertex),
      reinterpret_cast<void *>(offsetof(PlantVertex, position)));

  glEnableVertexAttribArray(Normal);
  glVertexAttribPointer(
      Normal, Vec2, GL_FLOAT, GL_FALSE, sizeof(PlantVertex),
      reinterpret_cast<void *>(offsetof(PlantVertex, tex_coord)));

  glEnableVertexAttribArray(TexCoord);
  glVertexAttribPointer(
      TexCoord, Vec3, GL_FLOAT, GL_FALSE, sizeof(PlantVertex),
      reinterpret_cast<void *>(offsetof(PlantVertex, normal)));

  glGenBuffers(1, &m_plantIndexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_plantIndexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(plant_indices), plant_indices,
               GL_STATIC_DRAW);
  m_plantIndexCount = PlantCrossQuadIndexCount;

  glEnableVertexAttribArray(InstancePosition);
  glVertexAttribDivisor(InstancePosition, 1);
  glEnableVertexAttribArray(InstanceScale);
  glVertexAttribDivisor(InstanceScale, 1);
  glEnableVertexAttribArray(InstanceColor);
  glVertexAttribDivisor(InstanceColor, 1);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void VegetationPipeline::shutdownPlantPipeline() {
  initializeOpenGLFunctions();
  if (m_plantIndexBuffer != 0u) {
    glDeleteBuffers(1, &m_plantIndexBuffer);
    m_plantIndexBuffer = 0;
  }
  if (m_plantVertexBuffer != 0u) {
    glDeleteBuffers(1, &m_plantVertexBuffer);
    m_plantVertexBuffer = 0;
  }
  if (m_plantVao != 0u) {
    glDeleteVertexArrays(1, &m_plantVao);
    m_plantVao = 0;
  }
  m_plantVertexCount = 0;
  m_plantIndexCount = 0;
}

void VegetationPipeline::initializePinePipeline() {
  initializeOpenGLFunctions();
  shutdownPinePipeline();

  struct PineVertex {
    QVector3D position;
    QVector2D tex_coord;
    QVector3D normal;
  };

  constexpr int k_segments = PineTreeSegments;
  constexpr float k_two_pi = 6.28318530718F;

  std::vector<PineVertex> vertices;
  vertices.reserve(k_segments * 5 + 1);

  std::vector<unsigned short> indices;
  indices.reserve(k_segments * 6 * 4 + k_segments * 3);

  auto add_ring = [&](float radius, float y, float normalUp,
                      float v_coord) -> int {
    const int start = static_cast<int>(vertices.size());
    for (int i = 0; i < k_segments; ++i) {
      const float t = static_cast<float>(i) / static_cast<float>(k_segments);
      const float angle = t * k_two_pi;
      const float nx = std::cos(angle);
      const float nz = std::sin(angle);
      QVector3D normal(nx, normalUp, nz);
      normal.normalize();
      QVector3D const position(radius * nx, y, radius * nz);
      QVector2D const tex_coord(t, v_coord);
      vertices.push_back({position, tex_coord, normal});
    }
    return start;
  };

  auto connect_rings = [&](int lowerStart, int upperStart) {
    for (int i = 0; i < k_segments; ++i) {
      const int next = (i + 1) % k_segments;
      const auto lower0 = static_cast<unsigned short>(lowerStart + i);
      const auto lower1 = static_cast<unsigned short>(lowerStart + next);
      const auto upper0 = static_cast<unsigned short>(upperStart + i);
      const auto upper1 = static_cast<unsigned short>(upperStart + next);

      indices.push_back(lower0);
      indices.push_back(lower1);
      indices.push_back(upper1);
      indices.push_back(lower0);
      indices.push_back(upper1);
      indices.push_back(upper0);
    }
  };

  const int trunk_bottom = add_ring(0.12F, 0.0F, 0.0F, 0.0F);
  const int trunk_mid = add_ring(0.11F, 0.35F, 0.0F, 0.12F);
  const int trunk_top = add_ring(0.10F, 0.58F, 0.05F, 0.30F);
  const int branch_base = add_ring(0.60F, 0.64F, 0.35F, 0.46F);
  const int branch_mid = add_ring(0.42F, 0.82F, 0.6F, 0.68F);
  const int branch_upper = add_ring(0.24F, 1.00F, 0.7F, 0.88F);
  const int branch_tip = add_ring(0.12F, 1.10F, 0.85F, 0.96F);

  connect_rings(trunk_bottom, trunk_mid);
  connect_rings(trunk_mid, trunk_top);
  connect_rings(trunk_top, branch_base);
  connect_rings(branch_base, branch_mid);
  connect_rings(branch_mid, branch_upper);
  connect_rings(branch_upper, branch_tip);

  const auto trunk_cap_index = static_cast<unsigned short>(vertices.size());
  vertices.push_back({QVector3D(0.0F, 0.0F, 0.0F), QVector2D(0.5F, 0.0F),
                      QVector3D(0.0F, -1.0F, 0.0F)});
  for (int i = 0; i < k_segments; ++i) {
    const int next = (i + 1) % k_segments;
    indices.push_back(static_cast<unsigned short>(trunk_bottom + next));
    indices.push_back(static_cast<unsigned short>(trunk_bottom + i));
    indices.push_back(trunk_cap_index);
  }

  const auto apex_index = static_cast<unsigned short>(vertices.size());
  vertices.push_back({QVector3D(0.0F, 1.18F, 0.0F), QVector2D(0.5F, 1.0F),
                      QVector3D(0.0F, 1.0F, 0.0F)});
  for (int i = 0; i < k_segments; ++i) {
    const int next = (i + 1) % k_segments;
    indices.push_back(static_cast<unsigned short>(branch_tip + i));
    indices.push_back(static_cast<unsigned short>(branch_tip + next));
    indices.push_back(apex_index);
  }

  glGenVertexArrays(1, &m_pineVao);
  glBindVertexArray(m_pineVao);

  glGenBuffers(1, &m_pineVertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_pineVertexBuffer);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(vertices.size() * sizeof(PineVertex)),
               vertices.data(), GL_STATIC_DRAW);
  m_pineVertexCount = static_cast<GLsizei>(vertices.size());

  glEnableVertexAttribArray(Position);
  glVertexAttribPointer(
      Position, Vec3, GL_FLOAT, GL_FALSE, sizeof(PineVertex),
      reinterpret_cast<void *>(offsetof(PineVertex, position)));

  glEnableVertexAttribArray(Normal);
  glVertexAttribPointer(
      Normal, Vec2, GL_FLOAT, GL_FALSE, sizeof(PineVertex),
      reinterpret_cast<void *>(offsetof(PineVertex, tex_coord)));

  glEnableVertexAttribArray(TexCoord);
  glVertexAttribPointer(TexCoord, Vec3, GL_FLOAT, GL_FALSE, sizeof(PineVertex),
                        reinterpret_cast<void *>(offsetof(PineVertex, normal)));

  glGenBuffers(1, &m_pineIndexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_pineIndexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned short)),
               indices.data(), GL_STATIC_DRAW);
  m_pineIndexCount = static_cast<GLsizei>(indices.size());

  glEnableVertexAttribArray(InstancePosition);
  glVertexAttribDivisor(InstancePosition, 1);
  glEnableVertexAttribArray(InstanceScale);
  glVertexAttribDivisor(InstanceScale, 1);
  glEnableVertexAttribArray(InstanceColor);
  glVertexAttribDivisor(InstanceColor, 1);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void VegetationPipeline::shutdownPinePipeline() {
  initializeOpenGLFunctions();
  if (m_pineIndexBuffer != 0u) {
    glDeleteBuffers(1, &m_pineIndexBuffer);
    m_pineIndexBuffer = 0;
  }
  if (m_pineVertexBuffer != 0u) {
    glDeleteBuffers(1, &m_pineVertexBuffer);
    m_pineVertexBuffer = 0;
  }
  if (m_pineVao != 0u) {
    glDeleteVertexArrays(1, &m_pineVao);
    m_pineVao = 0;
  }
  m_pineVertexCount = 0;
  m_pineIndexCount = 0;
}

void VegetationPipeline::initializeFireCampPipeline() {
  initializeOpenGLFunctions();
  shutdownFireCampPipeline();

  struct FireCampVertex {
    QVector3D position;
    QVector2D tex_coord;
  };

  std::vector<FireCampVertex> vertices;
  vertices.reserve(12);
  std::vector<unsigned short> indices;
  indices.reserve(18);

  auto append_plane = [&](float planeIndex) {
    auto const base = static_cast<unsigned short>(vertices.size());
    vertices.push_back(
        {QVector3D(-1.0F, 0.0F, planeIndex), QVector2D(0.0F, 0.0F)});
    vertices.push_back(
        {QVector3D(1.0F, 0.0F, planeIndex), QVector2D(1.0F, 0.0F)});
    vertices.push_back(
        {QVector3D(1.0F, 2.0F, planeIndex), QVector2D(1.0F, 1.0F)});
    vertices.push_back(
        {QVector3D(-1.0F, 2.0F, planeIndex), QVector2D(0.0F, 1.0F)});

    indices.push_back(base + 0);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base + 0);
    indices.push_back(base + 2);
    indices.push_back(base + 3);
  };

  append_plane(0.0F);
  append_plane(1.0F);
  append_plane(2.0F);

  glGenVertexArrays(1, &m_firecampVao);
  glBindVertexArray(m_firecampVao);

  glGenBuffers(1, &m_firecampVertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_firecampVertexBuffer);
  glBufferData(
      GL_ARRAY_BUFFER,
      static_cast<GLsizeiptr>(vertices.size() * sizeof(FireCampVertex)),
      vertices.data(), GL_STATIC_DRAW);
  m_firecampVertexCount = static_cast<GLsizei>(vertices.size());

  glEnableVertexAttribArray(Position);
  glVertexAttribPointer(Position, Vec3, GL_FLOAT, GL_FALSE,
                        sizeof(FireCampVertex), reinterpret_cast<void *>(0));

  glEnableVertexAttribArray(Normal);
  glVertexAttribPointer(
      Normal, Vec2, GL_FLOAT, GL_FALSE, sizeof(FireCampVertex),
      reinterpret_cast<void *>(offsetof(FireCampVertex, tex_coord)));

  glGenBuffers(1, &m_firecampIndexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_firecampIndexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned short)),
               indices.data(), GL_STATIC_DRAW);
  m_firecampIndexCount = static_cast<GLsizei>(indices.size());

  glEnableVertexAttribArray(InstancePosition);
  glVertexAttribDivisor(InstancePosition, 1);

  glEnableVertexAttribArray(InstanceScale);
  glVertexAttribDivisor(InstanceScale, 1);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void VegetationPipeline::shutdownFireCampPipeline() {
  initializeOpenGLFunctions();
  if (m_firecampIndexBuffer != 0u) {
    glDeleteBuffers(1, &m_firecampIndexBuffer);
    m_firecampIndexBuffer = 0;
  }
  if (m_firecampVertexBuffer != 0u) {
    glDeleteBuffers(1, &m_firecampVertexBuffer);
    m_firecampVertexBuffer = 0;
  }
  if (m_firecampVao != 0u) {
    glDeleteVertexArrays(1, &m_firecampVao);
    m_firecampVao = 0;
  }
  m_firecampVertexCount = 0;
  m_firecampIndexCount = 0;
}

} // namespace Render::GL::BackendPipelines
