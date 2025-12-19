#include "vegetation_pipeline.h"
#include "../backend.h"
#include "../camera.h"
#include "../render_constants.h"
#include "../state_scopes.h"
#include "../../draw_queue.h"
#include "../../ground/firecamp_gpu.h"
#include "../../ground/olive_gpu.h"
#include "../../ground/pine_gpu.h"
#include "../../ground/plant_gpu.h"
#include "../../ground/stone_gpu.h"
#include "gl/shader_cache.h"
#include <GL/gl.h>
#include <QDebug>
#include <QOpenGLContext>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <qglobal.h>
#include <qopenglcontext.h>
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
  m_oliveShader = m_shaderCache->get(QStringLiteral("olive_instanced"));
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
  if (m_oliveShader == nullptr) {
    qWarning() << "VegetationPipeline: olive shader missing";
  }
  if (m_firecampShader == nullptr) {
    qWarning() << "VegetationPipeline: firecamp shader missing";
  }

  initializeStonePipeline();
  initializePlantPipeline();
  initializePinePipeline();
  initializeOlivePipeline();
  initializeFireCampPipeline();
  cache_uniforms();

  m_initialized = true;
  return true;
}

void VegetationPipeline::shutdown() {
  shutdownStonePipeline();
  shutdownPlantPipeline();
  shutdownPinePipeline();
  shutdownOlivePipeline();
  shutdownFireCampPipeline();
  m_initialized = false;
}

void VegetationPipeline::cache_uniforms() {
  if (m_stoneShader != nullptr) {
    m_stoneUniforms.view_proj = m_stoneShader->uniform_handle("uViewProj");
    m_stoneUniforms.light_direction =
        m_stoneShader->uniform_handle("uLightDirection");
  }

  if (m_plantShader != nullptr) {
    m_plantUniforms.view_proj = m_plantShader->uniform_handle("uViewProj");
    m_plantUniforms.time = m_plantShader->uniform_handle("uTime");
    m_plantUniforms.wind_strength =
        m_plantShader->uniform_handle("uWindStrength");
    m_plantUniforms.wind_speed = m_plantShader->uniform_handle("uWindSpeed");
    m_plantUniforms.light_direction =
        m_plantShader->uniform_handle("uLightDirection");
  }

  if (m_pineShader != nullptr) {
    m_pineUniforms.view_proj = m_pineShader->uniform_handle("uViewProj");
    m_pineUniforms.time = m_pineShader->uniform_handle("uTime");
    m_pineUniforms.wind_strength =
        m_pineShader->uniform_handle("uWindStrength");
    m_pineUniforms.wind_speed = m_pineShader->uniform_handle("uWindSpeed");
    m_pineUniforms.light_direction =
        m_pineShader->uniform_handle("uLightDirection");
  }

  if (m_oliveShader != nullptr) {
    m_oliveUniforms.view_proj = m_oliveShader->uniform_handle("uViewProj");
    m_oliveUniforms.time = m_oliveShader->uniform_handle("uTime");
    m_oliveUniforms.wind_strength =
        m_oliveShader->uniform_handle("uWindStrength");
    m_oliveUniforms.wind_speed = m_oliveShader->uniform_handle("uWindSpeed");
    m_oliveUniforms.light_direction =
        m_oliveShader->uniform_handle("uLightDirection");
  }

  if (m_firecampShader != nullptr) {
    m_firecampUniforms.view_proj =
        m_firecampShader->uniform_handle("u_viewProj");
    m_firecampUniforms.time = m_firecampShader->uniform_handle("u_time");
    m_firecampUniforms.flickerSpeed =
        m_firecampShader->uniform_handle("u_flickerSpeed");
    m_firecampUniforms.flickerAmount =
        m_firecampShader->uniform_handle("u_flickerAmount");
    m_firecampUniforms.glowStrength =
        m_firecampShader->uniform_handle("u_glowStrength");
    m_firecampUniforms.fireTexture =
        m_firecampShader->uniform_handle("fireTexture");
    m_firecampUniforms.camera_right =
        m_firecampShader->uniform_handle("u_cameraRight");
    m_firecampUniforms.camera_forward =
        m_firecampShader->uniform_handle("u_cameraForward");
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
  constexpr int k_stone_index_count = 36;
  m_stoneIndexCount = k_stone_index_count;

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

  if (QOpenGLContext::currentContext() == nullptr) {
    m_stoneVao = 0;
    m_stoneVertexBuffer = 0;
    m_stoneIndexBuffer = 0;
    m_stoneVertexCount = 0;
    m_stoneIndexCount = 0;
    return;
  }

  initializeOpenGLFunctions();
  if (m_stoneIndexBuffer != 0U) {
    glDeleteBuffers(1, &m_stoneIndexBuffer);
    m_stoneIndexBuffer = 0;
  }
  if (m_stoneVertexBuffer != 0U) {
    glDeleteBuffers(1, &m_stoneVertexBuffer);
    m_stoneVertexBuffer = 0;
  }
  if (m_stoneVao != 0U) {
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

  if (QOpenGLContext::currentContext() == nullptr) {
    m_plantVao = 0;
    m_plantVertexBuffer = 0;
    m_plantIndexBuffer = 0;
    m_plantVertexCount = 0;
    m_plantIndexCount = 0;
    return;
  }

  initializeOpenGLFunctions();
  if (m_plantIndexBuffer != 0U) {
    glDeleteBuffers(1, &m_plantIndexBuffer);
    m_plantIndexBuffer = 0;
  }
  if (m_plantVertexBuffer != 0U) {
    glDeleteBuffers(1, &m_plantVertexBuffer);
    m_plantVertexBuffer = 0;
  }
  if (m_plantVao != 0U) {
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

  auto add_ring = [&](float radius, float y, float normalUp, float v_coord,
                      const QVector2D &center_offset =
                          QVector2D(0.0F, 0.0F)) -> int {
    const int start = static_cast<int>(vertices.size());
    for (int i = 0; i < k_segments; ++i) {
      const float t = static_cast<float>(i) / static_cast<float>(k_segments);
      const float angle = t * k_two_pi;
      const float nx = std::cos(angle);
      const float nz = std::sin(angle);
      QVector3D normal(nx, normalUp, nz);
      normal.normalize();
      QVector3D const position(radius * nx + center_offset.x(), y,
                               radius * nz + center_offset.y());
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

  if (QOpenGLContext::currentContext() == nullptr) {
    m_pineVao = 0;
    m_pineVertexBuffer = 0;
    m_pineIndexBuffer = 0;
    m_pineVertexCount = 0;
    m_pineIndexCount = 0;
    return;
  }

  initializeOpenGLFunctions();
  if (m_pineIndexBuffer != 0U) {
    glDeleteBuffers(1, &m_pineIndexBuffer);
    m_pineIndexBuffer = 0;
  }
  if (m_pineVertexBuffer != 0U) {
    glDeleteBuffers(1, &m_pineVertexBuffer);
    m_pineVertexBuffer = 0;
  }
  if (m_pineVao != 0U) {
    glDeleteVertexArrays(1, &m_pineVao);
    m_pineVao = 0;
  }
  m_pineVertexCount = 0;
  m_pineIndexCount = 0;
}

void VegetationPipeline::initializeOlivePipeline() {
  initializeOpenGLFunctions();
  shutdownOlivePipeline();

  struct OliveVertex {
    QVector3D position;
    QVector2D tex_coord;
    QVector3D normal;
  };

  constexpr int k_segments = OliveTreeSegments;
  constexpr float k_two_pi = 6.28318530718F;

  std::vector<OliveVertex> vertices;
  vertices.reserve(k_segments * 40);
  std::vector<unsigned short> indices;
  indices.reserve(k_segments * 6 * 40);

  auto add_ring = [&](float radius, float y, float normalUp, float v_coord,
                      const QVector2D &offset = QVector2D(0.0F, 0.0F)) -> int {
    const int start = static_cast<int>(vertices.size());
    for (int i = 0; i < k_segments; ++i) {
      const float t = static_cast<float>(i) / static_cast<float>(k_segments);
      const float angle = t * k_two_pi;
      const float nx = std::cos(angle);
      const float nz = std::sin(angle);
      QVector3D normal(nx, normalUp, nz);
      normal.normalize();
      QVector3D const position(radius * nx + offset.x(), y,
                               radius * nz + offset.y());
      vertices.push_back({position, QVector2D(t, v_coord), normal});
    }
    return start;
  };

  auto connect_rings = [&](int lower, int upper) {
    for (int i = 0; i < k_segments; ++i) {
      const int next = (i + 1) % k_segments;
      indices.push_back(static_cast<unsigned short>(lower + i));
      indices.push_back(static_cast<unsigned short>(lower + next));
      indices.push_back(static_cast<unsigned short>(upper + next));
      indices.push_back(static_cast<unsigned short>(lower + i));
      indices.push_back(static_cast<unsigned short>(upper + next));
      indices.push_back(static_cast<unsigned short>(upper + i));
    }
  };

  auto add_cap = [&](int ring, float capY, const QVector2D &offset, float v) {
    const int topIdx = static_cast<int>(vertices.size());
    vertices.push_back({QVector3D(offset.x(), capY, offset.y()),
                        QVector2D(0.5F, v), QVector3D(0.0F, 1.0F, 0.0F)});
    for (int i = 0; i < k_segments; ++i) {
      const int next = (i + 1) % k_segments;
      indices.push_back(static_cast<unsigned short>(ring + i));
      indices.push_back(static_cast<unsigned short>(ring + next));
      indices.push_back(static_cast<unsigned short>(topIdx));
    }
  };

  int t0 = add_ring(0.14F, 0.00F, -0.2F, 0.00F);
  int t1 = add_ring(0.12F, 0.08F, 0.0F, 0.06F);
  int t2 = add_ring(0.09F, 0.15F, 0.1F, 0.12F);
  connect_rings(t0, t1);
  connect_rings(t1, t2);

  auto add_branch = [&](float dir_x, float dir_z, float base_y, float length,
                        float branch_r, float leaf_r, float v_start) {
    float len = std::sqrt(dir_x * dir_x + dir_z * dir_z);
    dir_x /= len;
    dir_z /= len;

    float rise = 0.5F;
    float dx = dir_x * std::cos(0.5F);
    float dz = dir_z * std::cos(0.5F);
    float dy = std::sin(0.4F);

    int b0 = add_ring(branch_r, base_y, 0.0F, v_start);

    float mid_dist = length * 0.5F;
    QVector2D mid_offset(dx * mid_dist, dz * mid_dist);
    int b1 = add_ring(branch_r * 0.6F, base_y + dy * mid_dist, 0.3F,
                      v_start + 0.1F, mid_offset);

    float tip_dist = length;
    QVector2D tip_offset(dx * tip_dist, dz * tip_dist);
    float tip_y = base_y + dy * tip_dist;
    int b2 = add_ring(branch_r * 0.3F, tip_y, 0.5F, v_start + 0.2F, tip_offset);

    connect_rings(b0, b1);
    connect_rings(b1, b2);

    float ly = tip_y - leaf_r * 0.2F;
    int l0 = add_ring(leaf_r * 0.6F, ly, -0.4F, 0.50F, tip_offset);
    int l1 =
        add_ring(leaf_r * 0.9F, ly + leaf_r * 0.35F, 0.0F, 0.65F, tip_offset);
    int l2 =
        add_ring(leaf_r * 0.85F, ly + leaf_r * 0.65F, 0.2F, 0.80F, tip_offset);
    int l3 =
        add_ring(leaf_r * 0.5F, ly + leaf_r * 0.90F, 0.6F, 0.92F, tip_offset);

    connect_rings(b2, l0);
    connect_rings(l0, l1);
    connect_rings(l1, l2);
    connect_rings(l2, l3);
    add_cap(l3, ly + leaf_r * 1.0F, tip_offset, 1.0F);
  };

  add_branch(0.8F, 0.3F, 0.14F, 0.30F, 0.025F, 0.18F, 0.18F);
  add_branch(-0.7F, 0.5F, 0.15F, 0.32F, 0.022F, 0.20F, 0.20F);
  add_branch(0.4F, -0.9F, 0.16F, 0.28F, 0.020F, 0.16F, 0.22F);
  add_branch(-0.5F, -0.7F, 0.14F, 0.34F, 0.024F, 0.19F, 0.19F);

  m_oliveVertexCount = static_cast<GLsizei>(vertices.size());
  m_oliveIndexCount = static_cast<GLsizei>(indices.size());

  glGenVertexArrays(1, &m_oliveVao);
  glBindVertexArray(m_oliveVao);

  glGenBuffers(1, &m_oliveVertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_oliveVertexBuffer);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(vertices.size() * sizeof(OliveVertex)),
               vertices.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &m_oliveIndexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_oliveIndexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned short)),
               indices.data(), GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(
      0, 3, GL_FLOAT, GL_FALSE, sizeof(OliveVertex),
      reinterpret_cast<void *>(offsetof(OliveVertex, position)));

  glEnableVertexAttribArray(1);
  glVertexAttribPointer(
      1, 2, GL_FLOAT, GL_FALSE, sizeof(OliveVertex),
      reinterpret_cast<void *>(offsetof(OliveVertex, tex_coord)));

  glEnableVertexAttribArray(2);
  glVertexAttribPointer(
      2, 3, GL_FLOAT, GL_FALSE, sizeof(OliveVertex),
      reinterpret_cast<void *>(offsetof(OliveVertex, normal)));

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

void VegetationPipeline::shutdownOlivePipeline() {

  if (QOpenGLContext::currentContext() == nullptr) {
    m_oliveVao = 0;
    m_oliveVertexBuffer = 0;
    m_oliveIndexBuffer = 0;
    m_oliveVertexCount = 0;
    m_oliveIndexCount = 0;
    return;
  }

  initializeOpenGLFunctions();
  if (m_oliveIndexBuffer != 0U) {
    glDeleteBuffers(1, &m_oliveIndexBuffer);
    m_oliveIndexBuffer = 0;
  }
  if (m_oliveVertexBuffer != 0U) {
    glDeleteBuffers(1, &m_oliveVertexBuffer);
    m_oliveVertexBuffer = 0;
  }
  if (m_oliveVao != 0U) {
    glDeleteVertexArrays(1, &m_oliveVao);
    m_oliveVao = 0;
  }
  m_oliveVertexCount = 0;
  m_oliveIndexCount = 0;
}

void VegetationPipeline::initializeFireCampPipeline() {
  initializeOpenGLFunctions();
  shutdownFireCampPipeline();

  struct FireCampVertex {
    QVector3D position;
    QVector2D tex_coord;
  };

  constexpr std::size_t k_firecamp_vertex_reserve = 12;
  constexpr std::size_t k_firecamp_index_reserve = 18;
  std::vector<FireCampVertex> vertices;
  vertices.reserve(k_firecamp_vertex_reserve);
  std::vector<unsigned short> indices;
  indices.reserve(k_firecamp_index_reserve);

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

  if (QOpenGLContext::currentContext() == nullptr) {
    m_firecampVao = 0;
    m_firecampVertexBuffer = 0;
    m_firecampIndexBuffer = 0;
    m_firecampVertexCount = 0;
    m_firecampIndexCount = 0;
    return;
  }

  initializeOpenGLFunctions();
  if (m_firecampIndexBuffer != 0U) {
    glDeleteBuffers(1, &m_firecampIndexBuffer);
    m_firecampIndexBuffer = 0;
  }
  if (m_firecampVertexBuffer != 0U) {
    glDeleteBuffers(1, &m_firecampVertexBuffer);
    m_firecampVertexBuffer = 0;
  }
  if (m_firecampVao != 0U) {
    glDeleteVertexArrays(1, &m_firecampVao);
    m_firecampVao = 0;
  }
  m_firecampVertexCount = 0;
  m_firecampIndexCount = 0;
}

void VegetationPipeline::render_stones(const GL::DrawQueue &queue, std::size_t &i,
                                        const QMatrix4x4 &view_proj,
                                        GL::Backend *backend) {
  const auto &stone = std::get<StoneBatchCmdIndex>(queue.get_sorted(i));
  if (!stone.instance_buffer || stone.instance_count == 0 || !m_stoneShader ||
      m_stoneVao == 0 || m_stoneIndexCount == 0) {
    return;
  }

  DepthMaskScope const depth_mask(true);
  BlendScope const blend(false);

  backend->bind_shader(m_stoneShader);
  backend->set_view_proj_uniform(m_stoneShader, m_stoneUniforms.view_proj,
                                  view_proj);

  if (m_stoneUniforms.light_direction != GL::Shader::InvalidUniform) {
    QVector3D light_dir = stone.params.light_direction;
    if (!light_dir.isNull()) {
      light_dir.normalize();
    }
    m_stoneShader->set_uniform(m_stoneUniforms.light_direction, light_dir);
  }

  glBindVertexArray(m_stoneVao);
  stone.instance_buffer->bind();
  const auto stride = static_cast<GLsizei>(sizeof(StoneInstanceGpu));
  glVertexAttribPointer(
      TexCoord, Vec4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(StoneInstanceGpu, pos_scale)));
  glVertexAttribPointer(
      InstancePosition, Vec4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(StoneInstanceGpu, color_rot)));
  stone.instance_buffer->unbind();

  glDrawElementsInstanced(GL_TRIANGLES, m_stoneIndexCount, GL_UNSIGNED_SHORT,
                          nullptr, static_cast<GLsizei>(stone.instance_count));
  glBindVertexArray(0);
}

void VegetationPipeline::render_plants(const GL::DrawQueue &queue, std::size_t &i,
                                        const QMatrix4x4 &view_proj,
                                        GL::Backend *backend) {
  const auto &plant = std::get<PlantBatchCmdIndex>(queue.get_sorted(i));
  if (!plant.instance_buffer || plant.instance_count == 0 || !m_plantShader ||
      m_plantVao == 0 || m_plantIndexCount == 0) {
    return;
  }

  DepthMaskScope const depth_mask(true);
  glEnable(GL_DEPTH_TEST);
  BlendScope const blend(true);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  GLboolean const prev_cull = glIsEnabled(GL_CULL_FACE);
  if (prev_cull != 0U) {
    glDisable(GL_CULL_FACE);
  }

  backend->bind_shader(m_plantShader);
  backend->set_view_proj_uniform(m_plantShader, m_plantUniforms.view_proj,
                                  view_proj);

  if (m_plantUniforms.time != GL::Shader::InvalidUniform) {
    m_plantShader->set_uniform(m_plantUniforms.time, plant.params.time);
  }
  if (m_plantUniforms.wind_strength != GL::Shader::InvalidUniform) {
    m_plantShader->set_uniform(m_plantUniforms.wind_strength,
                               plant.params.wind_strength);
  }
  if (m_plantUniforms.wind_speed != GL::Shader::InvalidUniform) {
    m_plantShader->set_uniform(m_plantUniforms.wind_speed,
                               plant.params.wind_speed);
  }
  if (m_plantUniforms.light_direction != GL::Shader::InvalidUniform) {
    QVector3D light_dir = plant.params.light_direction;
    if (!light_dir.isNull()) {
      light_dir.normalize();
    }
    m_plantShader->set_uniform(m_plantUniforms.light_direction, light_dir);
  }

  glBindVertexArray(m_plantVao);
  plant.instance_buffer->bind();
  const auto stride = static_cast<GLsizei>(sizeof(PlantInstanceGpu));
  glVertexAttribPointer(
      InstancePosition, Vec4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(PlantInstanceGpu, pos_scale)));
  glVertexAttribPointer(
      InstanceScale, Vec4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(PlantInstanceGpu, color_sway)));
  glVertexAttribPointer(
      InstanceColor, Vec4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(PlantInstanceGpu, type_params)));
  plant.instance_buffer->unbind();

  glDrawElementsInstanced(GL_TRIANGLES, m_plantIndexCount, GL_UNSIGNED_SHORT,
                          nullptr, static_cast<GLsizei>(plant.instance_count));
  glBindVertexArray(0);

  if (prev_cull != 0U) {
    glEnable(GL_CULL_FACE);
  }
}

void VegetationPipeline::render_pines(const GL::DrawQueue &queue, std::size_t &i,
                                       const QMatrix4x4 &view_proj,
                                       GL::Backend *backend) {
  const auto &pine = std::get<PineBatchCmdIndex>(queue.get_sorted(i));
  if (!pine.instance_buffer || pine.instance_count == 0 || !m_pineShader ||
      m_pineVao == 0 || m_pineIndexCount == 0) {
    return;
  }

  DepthMaskScope const depth_mask(true);
  glEnable(GL_DEPTH_TEST);
  BlendScope const blend(true);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  GLboolean const prev_cull = glIsEnabled(GL_CULL_FACE);
  if (prev_cull != 0U) {
    glDisable(GL_CULL_FACE);
  }

  backend->bind_shader(m_pineShader);
  backend->set_view_proj_uniform(m_pineShader, m_pineUniforms.view_proj,
                                  view_proj);

  if (m_pineUniforms.time != GL::Shader::InvalidUniform) {
    m_pineShader->set_uniform(m_pineUniforms.time, pine.params.time);
  }
  if (m_pineUniforms.wind_strength != GL::Shader::InvalidUniform) {
    m_pineShader->set_uniform(m_pineUniforms.wind_strength,
                              pine.params.wind_strength);
  }
  if (m_pineUniforms.wind_speed != GL::Shader::InvalidUniform) {
    m_pineShader->set_uniform(m_pineUniforms.wind_speed,
                              pine.params.wind_speed);
  }
  if (m_pineUniforms.light_direction != GL::Shader::InvalidUniform) {
    QVector3D light_dir = pine.params.light_direction;
    if (!light_dir.isNull()) {
      light_dir.normalize();
    }
    m_pineShader->set_uniform(m_pineUniforms.light_direction, light_dir);
  }

  glBindVertexArray(m_pineVao);
  pine.instance_buffer->bind();
  const auto stride = static_cast<GLsizei>(sizeof(PineInstanceGpu));
  glVertexAttribPointer(
      InstancePosition, Vec4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(PineInstanceGpu, pos_scale)));
  glVertexAttribPointer(
      InstanceScale, Vec4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(PineInstanceGpu, color_sway)));
  glVertexAttribPointer(
      InstanceColor, Vec4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(PineInstanceGpu, rotation)));
  pine.instance_buffer->unbind();

  glDrawElementsInstanced(GL_TRIANGLES, m_pineIndexCount, GL_UNSIGNED_SHORT,
                          nullptr, static_cast<GLsizei>(pine.instance_count));
  glBindVertexArray(0);

  if (prev_cull != 0U) {
    glEnable(GL_CULL_FACE);
  }
}

void VegetationPipeline::render_olives(const GL::DrawQueue &queue, std::size_t &i,
                                        const QMatrix4x4 &view_proj,
                                        GL::Backend *backend) {
  const auto &olive = std::get<OliveBatchCmdIndex>(queue.get_sorted(i));
  if (!olive.instance_buffer || olive.instance_count == 0 || !m_oliveShader ||
      m_oliveVao == 0 || m_oliveIndexCount == 0) {
    return;
  }

  DepthMaskScope const depth_mask(true);
  glEnable(GL_DEPTH_TEST);
  BlendScope const blend(true);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  GLboolean const prev_cull = glIsEnabled(GL_CULL_FACE);
  if (prev_cull != 0U) {
    glDisable(GL_CULL_FACE);
  }

  backend->bind_shader(m_oliveShader);
  backend->set_view_proj_uniform(m_oliveShader, m_oliveUniforms.view_proj,
                                  view_proj);

  if (m_oliveUniforms.time != GL::Shader::InvalidUniform) {
    m_oliveShader->set_uniform(m_oliveUniforms.time, olive.params.time);
  }
  if (m_oliveUniforms.wind_strength != GL::Shader::InvalidUniform) {
    m_oliveShader->set_uniform(m_oliveUniforms.wind_strength,
                               olive.params.wind_strength);
  }
  if (m_oliveUniforms.wind_speed != GL::Shader::InvalidUniform) {
    m_oliveShader->set_uniform(m_oliveUniforms.wind_speed,
                               olive.params.wind_speed);
  }
  if (m_oliveUniforms.light_direction != GL::Shader::InvalidUniform) {
    QVector3D light_dir = olive.params.light_direction;
    if (!light_dir.isNull()) {
      light_dir.normalize();
    }
    m_oliveShader->set_uniform(m_oliveUniforms.light_direction, light_dir);
  }

  glBindVertexArray(m_oliveVao);
  olive.instance_buffer->bind();
  const auto stride = static_cast<GLsizei>(sizeof(OliveInstanceGpu));
  glVertexAttribPointer(
      InstancePosition, Vec4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(OliveInstanceGpu, pos_scale)));
  glVertexAttribPointer(
      InstanceScale, Vec4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(OliveInstanceGpu, color_sway)));
  glVertexAttribPointer(
      InstanceColor, Vec4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(OliveInstanceGpu, rotation)));
  olive.instance_buffer->unbind();

  glDrawElementsInstanced(GL_TRIANGLES, m_oliveIndexCount, GL_UNSIGNED_SHORT,
                          nullptr, static_cast<GLsizei>(olive.instance_count));
  glBindVertexArray(0);

  if (prev_cull != 0U) {
    glEnable(GL_CULL_FACE);
  }
}

void VegetationPipeline::render_firecamps(const GL::DrawQueue &queue,
                                           std::size_t &i, const GL::Camera &cam,
                                           const QMatrix4x4 &view_proj,
                                           GL::Backend *backend) {
  const auto &firecamp = std::get<FireCampBatchCmdIndex>(queue.get_sorted(i));
  if (!firecamp.instance_buffer || firecamp.instance_count == 0 ||
      !m_firecampShader || m_firecampVao == 0 || m_firecampIndexCount == 0) {
    return;
  }

  DepthMaskScope const depth_mask(true);
  glEnable(GL_DEPTH_TEST);
  BlendScope const blend(true);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  GLboolean const prev_cull = glIsEnabled(GL_CULL_FACE);
  if (prev_cull != 0U) {
    glDisable(GL_CULL_FACE);
  }

  backend->bind_shader(m_firecampShader);
  backend->set_view_proj_uniform(m_firecampShader, m_firecampUniforms.view_proj,
                                  view_proj);

  if (m_firecampUniforms.time != GL::Shader::InvalidUniform) {
    m_firecampShader->set_uniform(m_firecampUniforms.time,
                                  firecamp.params.time);
  }
  if (m_firecampUniforms.flickerSpeed != GL::Shader::InvalidUniform) {
    m_firecampShader->set_uniform(m_firecampUniforms.flickerSpeed,
                                  firecamp.params.flicker_speed);
  }
  if (m_firecampUniforms.flickerAmount != GL::Shader::InvalidUniform) {
    m_firecampShader->set_uniform(m_firecampUniforms.flickerAmount,
                                  firecamp.params.flicker_amount);
  }
  if (m_firecampUniforms.glowStrength != GL::Shader::InvalidUniform) {
    m_firecampShader->set_uniform(m_firecampUniforms.glowStrength,
                                  firecamp.params.glow_strength);
  }
  if (m_firecampUniforms.camera_right != GL::Shader::InvalidUniform) {
    QVector3D camera_right = cam.get_right_vector();
    if (camera_right.lengthSquared() < 1e-6F) {
      camera_right = QVector3D(1.0F, 0.0F, 0.0F);
    } else {
      camera_right.normalize();
    }
    m_firecampShader->set_uniform(m_firecampUniforms.camera_right,
                                  camera_right);
  }
  if (m_firecampUniforms.camera_forward != GL::Shader::InvalidUniform) {
    QVector3D camera_forward = cam.get_forward_vector();
    if (camera_forward.lengthSquared() < 1e-6F) {
      camera_forward = QVector3D(0.0F, 0.0F, -1.0F);
    } else {
      camera_forward.normalize();
    }
    m_firecampShader->set_uniform(m_firecampUniforms.camera_forward,
                                  camera_forward);
  }

  if (m_firecampUniforms.fireTexture != GL::Shader::InvalidUniform) {
    auto *resources = backend->resources();
    if (resources && resources->white()) {
      resources->white()->bind(0);
      m_firecampShader->set_uniform(m_firecampUniforms.fireTexture, 0);
    }
  }

  glBindVertexArray(m_firecampVao);
  firecamp.instance_buffer->bind();
  const auto stride = static_cast<GLsizei>(sizeof(FireCampInstanceGpu));
  glVertexAttribPointer(
      InstancePosition, Vec4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(FireCampInstanceGpu, pos_intensity)));
  glVertexAttribPointer(
      InstanceScale, Vec4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(FireCampInstanceGpu, radius_phase)));
  firecamp.instance_buffer->unbind();

  glDrawElementsInstanced(GL_TRIANGLES, m_firecampIndexCount,
                          GL_UNSIGNED_SHORT, nullptr,
                          static_cast<GLsizei>(firecamp.instance_count));
  glBindVertexArray(0);

  if (prev_cull != 0U) {
    glEnable(GL_CULL_FACE);
  }
}

} // namespace Render::GL::BackendPipelines
