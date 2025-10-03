#include "backend.h"
#include "shader.h"
#include "../draw_queue.h"
#include "mesh.h"
#include "texture.h"
#include "../geom/selection_ring.h"
#include <QDebug>

namespace Render::GL {
Backend::~Backend() = default;

void Backend::initialize() {
	initializeOpenGLFunctions();
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// Initialize shader cache and get commonly used shaders
	m_shaderCache = std::make_unique<ShaderCache>();
	m_basicShader = m_shaderCache->get(QStringLiteral("basic"));
	if (!m_basicShader) {
		qWarning() << "Backend: failed to load 'basic' shader";
	}
	m_gridShader = m_shaderCache->get(QStringLiteral("grid"));
	if (!m_gridShader) {
		qWarning() << "Backend: failed to load 'grid' shader";
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

void Backend::execute(const DrawQueue& queue, const Camera& cam) {
	if (!m_basicShader) return;
	Shader* current = nullptr;
	int meshCount = 0;
	int gridCount = 0;
	int ringCount = 0;
	auto bindBasic = [&]() {
		if (current != m_basicShader) {
			m_basicShader->use();
			m_basicShader->setUniform("u_view", cam.getViewMatrix());
			m_basicShader->setUniform("u_projection", cam.getProjectionMatrix());
			current = m_basicShader;
		}
	};
	auto bindGrid = [&]() {
		if (!m_gridShader) return;
		if (current != m_gridShader) {
			m_gridShader->use();
			m_gridShader->setUniform("u_view", cam.getViewMatrix());
			m_gridShader->setUniform("u_projection", cam.getProjectionMatrix());
			current = m_gridShader;
		}
	};
	for (const auto& cmd : queue.items()) {
		if (std::holds_alternative<MeshCmd>(cmd)) {
			const auto& it = std::get<MeshCmd>(cmd);
			if (!it.mesh) continue;
			++meshCount;
			bindBasic();
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
			++gridCount;
			bindGrid();
			m_gridShader->setUniform("u_model", gc.model);
			m_gridShader->setUniform("u_gridColor", gc.color);
			m_gridShader->setUniform("u_lineColor", QVector3D(0.22f, 0.25f, 0.22f));
			m_gridShader->setUniform("u_cellSize", gc.cellSize);
			m_gridShader->setUniform("u_thickness", gc.thickness);
			// Draw a full plane using the default ground mesh if available
			if (m_resources) { if (auto* plane = m_resources->ground()) plane->draw(); }
		} else if (std::holds_alternative<SelectionRingCmd>(cmd)) {
			const auto& sc = std::get<SelectionRingCmd>(cmd);
			++ringCount;
			Mesh* ring = Render::Geom::SelectionRing::get();
			if (!ring) continue;
			bindBasic();
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
	if (current) current->release();
	qDebug() << "Backend frame: meshes" << meshCount << ", rings" << ringCount << ", grids" << gridCount;
}

} // namespace Render::GL
#include "backend.h"

namespace Render::GL {
// Backend is thin; all inline for now.
}
