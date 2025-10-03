#include "backend.h"
#include "shader.h"
#include "../draw_queue.h"
#include "mesh.h"
#include "texture.h"
#include "../geom/selection_ring.h"
#include <QDebug>

namespace Render::GL {
Backend::~Backend() = default;

namespace {
static const QString kShaderBase = QStringLiteral("assets/shaders/");
static const QString kBasicVert = kShaderBase + QStringLiteral("basic.vert");
static const QString kBasicFrag = kShaderBase + QStringLiteral("basic.frag");
static const QString kGridFrag  = kShaderBase + QStringLiteral("grid.frag");
}

void Backend::initialize() {
	initializeOpenGLFunctions();
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// Load basic shader
	m_basicShader = std::make_unique<Shader>();
	if (!m_basicShader->loadFromFiles(kBasicVert, kBasicFrag)) {
		qWarning() << "Backend: failed to load basic shader" << kBasicVert << kBasicFrag;
		m_basicShader.reset();
	}

	// Grid shader shares the same vertex stage and uses grid.frag
	m_gridShader = std::make_unique<Shader>();
	if (!m_gridShader->loadFromFiles(kBasicVert, kGridFrag)) {
		qWarning() << "Backend: failed to load grid shader" << kBasicVert << kGridFrag;
		m_gridShader.reset();
	}
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

void Backend::execute(const DrawQueue& queue, const Camera& cam, const ResourceManager& res) {
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
				if (res.white()) {
					res.white()->bind(0);
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
			if (auto* plane = res.ground()) plane->draw();
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
			// Slight polygon offset to avoid z-fighting with ground
			glEnable(GL_POLYGON_OFFSET_FILL);
			glPolygonOffset(-1.0f, -1.0f);
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
			glDisable(GL_POLYGON_OFFSET_FILL);
		}
	}
	m_basicShader->release();
}

} // namespace Render::GL
#include "backend.h"

namespace Render::GL {
// Backend is thin; all inline for now.
}
