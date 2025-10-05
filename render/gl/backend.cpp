#include "backend.h"
#include "../draw_queue.h"
#include "../geom/selection_disc.h"
#include "../geom/selection_ring.h"
#include "mesh.h"
#include "shader.h"
#include "state_scopes.h"
#include "texture.h"
#include <QDebug>

namespace Render::GL {
Backend::~Backend() = default;

void Backend::initialize() {
  initializeOpenGLFunctions();
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  m_resources = std::make_unique<ResourceManager>();
  if (!m_resources->initialize()) {
    qWarning() << "Backend: failed to initialize ResourceManager";
  }
  m_shaderCache = std::make_unique<ShaderCache>();
  m_shaderCache->initializeDefaults();
  m_basicShader = m_shaderCache->get(QStringLiteral("basic"));
  m_gridShader = m_shaderCache->get(QStringLiteral("grid"));
  if (!m_basicShader)
    qWarning() << "Backend: basic shader missing";
  if (!m_gridShader)
    qWarning() << "Backend: grid shader missing";
}

void Backend::beginFrame() {
  if (m_viewportWidth > 0 && m_viewportHeight > 0) {
    glViewport(0, 0, m_viewportWidth, m_viewportHeight);
  }
  glClearColor(m_clearColor[0], m_clearColor[1], m_clearColor[2],
               m_clearColor[3]);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Backend::setViewport(int w, int h) {
  m_viewportWidth = w;
  m_viewportHeight = h;
}

void Backend::setClearColor(float r, float g, float b, float a) {
  m_clearColor[0] = r;
  m_clearColor[1] = g;
  m_clearColor[2] = b;
  m_clearColor[3] = a;
}

void Backend::execute(const DrawQueue &queue, const Camera &cam) {
  if (!m_basicShader)
    return;

  m_basicShader->use();
  m_basicShader->setUniform("u_view", cam.getViewMatrix());
  m_basicShader->setUniform("u_projection", cam.getProjectionMatrix());
  for (const auto &cmd : queue.items()) {
    if (std::holds_alternative<MeshCmd>(cmd)) {
      const auto &it = std::get<MeshCmd>(cmd);
      if (!it.mesh)
        continue;

      glDepthMask(GL_TRUE);
      if (glIsEnabled(GL_POLYGON_OFFSET_FILL))
        glDisable(GL_POLYGON_OFFSET_FILL);

      m_basicShader->use();
      m_basicShader->setUniform("u_view", cam.getViewMatrix());
      m_basicShader->setUniform("u_projection", cam.getProjectionMatrix());
      m_basicShader->setUniform("u_model", it.model);
      if (it.texture) {
        it.texture->bind(0);
        m_basicShader->setUniform("u_texture", 0);
        m_basicShader->setUniform("u_useTexture", true);
      } else {
        if (m_resources && m_resources->white()) {
          m_resources->white()->bind(0);
          m_basicShader->setUniform("u_texture", 0);
        }
        m_basicShader->setUniform("u_useTexture", false);
      }
      m_basicShader->setUniform("u_color", it.color);
      m_basicShader->setUniform("u_alpha", it.alpha);
      it.mesh->draw();
    } else if (std::holds_alternative<GridCmd>(cmd)) {
      if (!m_gridShader)
        continue;
      const auto &gc = std::get<GridCmd>(cmd);
      m_gridShader->use();
      m_gridShader->setUniform("u_view", cam.getViewMatrix());
      m_gridShader->setUniform("u_projection", cam.getProjectionMatrix());

      QMatrix4x4 model = gc.model;

      m_gridShader->setUniform("u_model", model);
      m_gridShader->setUniform("u_gridColor", gc.color);
      m_gridShader->setUniform("u_lineColor", QVector3D(0.22f, 0.25f, 0.22f));
      m_gridShader->setUniform("u_cellSize", gc.cellSize);
      m_gridShader->setUniform("u_thickness", gc.thickness);

      if (m_resources) {
        if (auto *plane = m_resources->ground())
          plane->draw();
      }

    } else if (std::holds_alternative<SelectionRingCmd>(cmd)) {
      const auto &sc = std::get<SelectionRingCmd>(cmd);
      Mesh *ring = Render::Geom::SelectionRing::get();
      if (!ring)
        continue;

      m_basicShader->use();
      m_basicShader->setUniform("u_view", cam.getViewMatrix());
      m_basicShader->setUniform("u_projection", cam.getProjectionMatrix());

      m_basicShader->setUniform("u_useTexture", false);
      m_basicShader->setUniform("u_color", sc.color);

      DepthMaskScope depthMask(false);
      PolygonOffsetScope poly(-1.0f, -1.0f);
      BlendScope blend(true);

      {
        QMatrix4x4 m = sc.model;
        m.scale(1.08f, 1.0f, 1.08f);
        m_basicShader->setUniform("u_model", m);
        m_basicShader->setUniform("u_alpha", sc.alphaOuter);
        ring->draw();
      }

      {
        m_basicShader->setUniform("u_model", sc.model);
        m_basicShader->setUniform("u_alpha", sc.alphaInner);
        ring->draw();
      }
    } else if (std::holds_alternative<SelectionSmokeCmd>(cmd)) {
      const auto &sm = std::get<SelectionSmokeCmd>(cmd);
      Mesh *disc = Render::Geom::SelectionDisc::get();
      if (!disc)
        continue;
      m_basicShader->use();
      m_basicShader->setUniform("u_view", cam.getViewMatrix());
      m_basicShader->setUniform("u_projection", cam.getProjectionMatrix());
      m_basicShader->setUniform("u_useTexture", false);
      m_basicShader->setUniform("u_color", sm.color);
      DepthMaskScope depthMask(false);
      DepthTestScope depthTest(false);

      PolygonOffsetScope poly(-0.1f, -0.1f);
      BlendScope blend(true);
      for (int i = 0; i < 7; ++i) {
        float scale = 1.35f + 0.12f * i;
        float a = sm.baseAlpha * (1.0f - 0.09f * i);
        QMatrix4x4 m = sm.model;
        m.translate(0.0f, 0.02f, 0.0f);
        m.scale(scale, 1.0f, scale);
        m_basicShader->setUniform("u_model", m);
        m_basicShader->setUniform("u_alpha", a);
        disc->draw();
      }
    }
  }
  m_basicShader->release();
}

} // namespace Render::GL
