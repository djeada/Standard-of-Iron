#include "backend.h"
#include "shader.h"
#include "../draw_queue.h"
#include "mesh.h"
#include "texture.h"
#include "../geom/selection_ring.h"
#include "../geom/selection_disc.h"
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
	// Load smoke shader lazily
	if (m_shaderCache) {
		m_smokeShader = m_shaderCache->load(QStringLiteral("smoke"),
			QStringLiteral("assets/shaders/smoke.vert"),
			QStringLiteral("assets/shaders/smoke.frag"));
		if (!m_smokeShader) {
			qWarning() << "Backend: smoke shader failed to load";
		}
	}
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
			// Reset critical state for opaque mesh pass
			glDepthMask(GL_TRUE);
			if (glIsEnabled(GL_POLYGON_OFFSET_FILL)) glDisable(GL_POLYGON_OFFSET_FILL);
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
			// Apply extent by scaling XZ on top of provided model
			QMatrix4x4 model = gc.model;
			model.scale(gc.extent, 1.0f, gc.extent);
			m_gridShader->setUniform("u_model", model);
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
		} else if (std::holds_alternative<SelectionSmokeCmd>(cmd)) {
			const auto& sm = std::get<SelectionSmokeCmd>(cmd);
			Mesh* disc = Render::Geom::SelectionDisc::get();
			if (!disc) continue;
			m_basicShader->use();
			m_basicShader->setUniform("u_view", cam.getViewMatrix());
			m_basicShader->setUniform("u_projection", cam.getProjectionMatrix());
			m_basicShader->setUniform("u_useTexture", false);
			m_basicShader->setUniform("u_color", sm.color);
			DepthMaskScope depthMask(false);
			DepthTestScope depthTest(false); // draw without depth test so smoke sits visible over ground
			// Slight negative offset helps avoid coincident z with ground if depth test toggles elsewhere
			PolygonOffsetScope poly(-0.1f, -0.1f);
			BlendScope blend(true);
			for (int i = 0; i < 7; ++i) {
				float scale = 1.35f + 0.12f * i;
				float a = sm.baseAlpha * (1.0f - 0.09f * i);
				QMatrix4x4 m = sm.model; m.translate(0.0f, 0.02f, 0.0f); m.scale(scale, 1.0f, scale);
				m_basicShader->setUniform("u_model", m);
				m_basicShader->setUniform("u_alpha", a);
				disc->draw();
			}
		} else if (std::holds_alternative<BillboardSmokeCmd>(cmd)) {
			const auto& ps = std::get<BillboardSmokeCmd>(cmd);
			if (!m_resources) continue;
			if (!m_smokeShader || !m_resources->quad()) {
				// Fallback: draw a single disc so the highlight is still visible
				Mesh* disc = Render::Geom::SelectionDisc::get();
				if (!disc) continue;
				m_basicShader->use();
				m_basicShader->setUniform("u_view", cam.getViewMatrix());
				m_basicShader->setUniform("u_projection", cam.getProjectionMatrix());
				m_basicShader->setUniform("u_useTexture", false);
				m_basicShader->setUniform("u_color", ps.color);
				DepthMaskScope dms(false);
				DepthTestScope dts(false);
				BlendScope blend(true);
				QMatrix4x4 m = ps.model; m.translate(0.0f, 0.02f, 0.0f);
				m_basicShader->setUniform("u_model", m);
				m_basicShader->setUniform("u_alpha", ps.baseAlpha);
				disc->draw();
				continue;
			}
			// Compute camera right/up from view matrix
			QMatrix4x4 view = cam.getViewMatrix();
			// Extract right (column 0 of inverse view rotation) and up (column 1)
			QMatrix4x4 invView = view.inverted();
			QVector3D camRight = invView.column(0).toVector3D().normalized();
			QVector3D camUp    = invView.column(1).toVector3D().normalized();
			Mesh* quad = m_resources->quad();
			DepthMaskScope depthMask(false);
			DepthTestScope depthTest(false); // ensure visible over ground/grid
			BlendScope blend(true);
			m_smokeShader->use();
			m_smokeShader->setUniform("u_view", view);
			m_smokeShader->setUniform("u_projection", cam.getProjectionMatrix());
			m_smokeShader->setUniform("u_color", ps.color);
			m_smokeShader->setUniform("u_model", ps.model);
			// Deterministic pseudo-random via LCG
			unsigned int seed = ps.seed;
			auto nextRand = [&seed]() {
				seed = 1664525u * seed + 1013904223u;
				return seed;
			};
			for (int i = 0; i < ps.count; ++i) {
				// Random size, height, offset in a small radius
				float r01 = (nextRand() & 0xFFFFFF) / float(0xFFFFFF);
				float r02 = (nextRand() & 0xFFFFFF) / float(0xFFFFFF);
				float r03 = (nextRand() & 0xFFFFFF) / float(0xFFFFFF);
				auto lerp = [](float a, float b, float t){ return a + (b - a) * t; };
				float size = lerp(0.5f, 0.9f, r01);
				float height = lerp(0.10f, 0.60f, r02);
				float angle = r03 * 6.2831853f;
				float radius = lerp(0.6f, 1.0f, r01); // cluster near outer ring
				QVector3D center(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
				float alpha = ps.baseAlpha * (0.8f + 0.4f * r02);
				m_smokeShader->setUniform("u_camRight", camRight);
				m_smokeShader->setUniform("u_camUp", camUp);
				m_smokeShader->setUniform("u_center", center);
				m_smokeShader->setUniform("u_size", size);
				m_smokeShader->setUniform("u_height", height);
				m_smokeShader->setUniform("u_alpha", alpha);
				quad->draw();
			}
			// Base soft disc to reinforce highlight visibility
			{
				Mesh* disc = Render::Geom::SelectionDisc::get();
				if (disc) {
					m_basicShader->use();
					m_basicShader->setUniform("u_view", cam.getViewMatrix());
					m_basicShader->setUniform("u_projection", cam.getProjectionMatrix());
					m_basicShader->setUniform("u_useTexture", false);
					m_basicShader->setUniform("u_color", ps.color);
					QMatrix4x4 base = ps.model; base.translate(0.0f, 0.02f, 0.0f);
					m_basicShader->setUniform("u_model", base);
					m_basicShader->setUniform("u_alpha", ps.baseAlpha * 0.35f);
					disc->draw();
				}
			}
		}
	}
	m_basicShader->release();
}

} // namespace Render::GL
