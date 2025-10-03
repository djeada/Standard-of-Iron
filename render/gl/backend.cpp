#include "backend.h"
#include "shader.h"
#include "../draw_queue.h"
#include "mesh.h"
#include "texture.h"
#include "../geom/selection_ring.h"
#include "state_scopes.h"
#include <QDebug>

namespace Render::GL {
Backend::~Backend() = default;

void Backend::initialize() {
	initializeOpenGLFunctions();
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// Create resource and shader managers
	m_resources = std::make_unique<ResourceManager>();
	if (!m_resources->initialize()) {
		qWarning() << "Backend: failed to initialize ResourceManager";
	}
	m_shaderCache = std::make_unique<ShaderCache>();
	m_shaderCache->initializeDefaults();
	m_basicShader = m_shaderCache->get(QStringLiteral("basic"));
	m_gridShader  = m_shaderCache->get(QStringLiteral("grid"));
	if (!m_basicShader) qWarning() << "Backend: basic shader missing";
	if (!m_gridShader)  qWarning() << "Backend: grid shader missing";
}

void Backend::beginFrame() {
	if (m_viewportWidth > 0 && m_viewportHeight > 0) {
		glViewport(0, 0, m_viewportWidth, m_viewportHeight);
	}
	glClearColor(m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Backend::setViewport(int w, int h) {
	m_viewportWidth = w;
	m_viewportHeight = h;
}

void Backend::setClearColor(float r, float g, float b, float a) {
	m_clearColor[0]=r; m_clearColor[1]=g; m_clearColor[2]=b; m_clearColor[3]=a;
}

void Backend::execute(const DrawQueue& queue, const Camera& cam) {
	if (!m_basicShader) return;
	// Bind once up front; we'll defensively rebind before draws that use it
	m_basicShader->use();
	m_basicShader->setUniform("u_view", cam.getViewMatrix());
	m_basicShader->setUniform("u_projection", cam.getProjectionMatrix());
	for (const auto& cmd : queue.items()) {
		if (std::holds_alternative<MeshCmd>(cmd)) {
			const auto& it = std::get<MeshCmd>(cmd);
			if (!it.mesh) continue;
			// Ensure the correct program is bound in case a prior GridCmd changed it
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
			if (!m_gridShader) continue;
			const auto& gc = std::get<GridCmd>(cmd);
			m_gridShader->use();
			m_gridShader->setUniform("u_view", cam.getViewMatrix());
			m_gridShader->setUniform("u_projection", cam.getProjectionMatrix());
			m_gridShader->setUniform("u_model", gc.model);
			m_gridShader->setUniform("u_gridColor", gc.color);
			m_gridShader->setUniform("u_lineColor", QVector3D(0.22f, 0.25f, 0.22f));
			m_gridShader->setUniform("u_cellSize", gc.cellSize);
			m_gridShader->setUniform("u_thickness", gc.thickness);
			// Draw a full plane using the default ground mesh if available
			if (m_resources) {
				if (auto* plane = m_resources->ground()) plane->draw();
			}
			// Do not release to program 0 here; subsequent draws will rebind their shader as needed
		} else if (std::holds_alternative<SelectionRingCmd>(cmd)) {
			const auto& sc = std::get<SelectionRingCmd>(cmd);
			Mesh* ring = Render::Geom::SelectionRing::get();
			if (!ring) continue;
			// Ensure basic program is bound
			m_basicShader->use();
			m_basicShader->setUniform("u_view", cam.getViewMatrix());
			m_basicShader->setUniform("u_projection", cam.getProjectionMatrix());
			// Use white texture path for consistent shading
			m_basicShader->setUniform("u_useTexture", false);
			m_basicShader->setUniform("u_color", sc.color);
			// Slight polygon offset and disable depth writes while blending
			DepthMaskScope depthMask(false);
			PolygonOffsetScope poly(-1.0f, -1.0f);
			BlendScope blend(true);
			// Outer feather
			{
				QMatrix4x4 m = sc.model; m.scale(1.08f, 1.0f, 1.08f);
				m_basicShader->setUniform("u_model", m);
				m_basicShader->setUniform("u_alpha", sc.alphaOuter);
				ring->draw();
			}
			// Inner ring
			{
				m_basicShader->setUniform("u_model", sc.model);
				m_basicShader->setUniform("u_alpha", sc.alphaInner);
				ring->draw();
			}
		}
	}
	m_basicShader->release();
}

} // namespace Render::GL
#include "backend.h"

namespace Render::GL {
// Backend is thin; all inline for now.
}
