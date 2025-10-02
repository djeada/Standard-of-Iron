#include "backend.h"
#include "shader.h"
#include "../draw_queue.h"
#include "mesh.h"
#include "texture.h"
#include <QDebug>

namespace Render::GL {
Backend::~Backend() = default;

namespace {
static const QString kShaderBase = QStringLiteral("assets/shaders/");
static const QString kBasicVert = kShaderBase + QStringLiteral("basic.vert");
static const QString kBasicFrag = kShaderBase + QStringLiteral("basic.frag");
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
	m_basicShader->use();
	m_basicShader->setUniform("u_view", cam.getViewMatrix());
	m_basicShader->setUniform("u_projection", cam.getProjectionMatrix());
	for (const auto& cmd : queue.items()) {
		if (std::holds_alternative<MeshCmd>(cmd)) {
			const auto& it = std::get<MeshCmd>(cmd);
			if (!it.mesh) continue;
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
			// TODO: implement grid overlay draw pass (separate shader)
			// Currently no-op to keep behavior unchanged
		} else if (std::holds_alternative<SelectionRingCmd>(cmd)) {
			// TODO: implement ring-specific rendering (feathered alpha, polygon offset)
			// For now, approximate with a simple mesh draw if a ring mesh is available via resources/geom.
			const auto& sc = std::get<SelectionRingCmd>(cmd);
			// No direct mesh handle here; leave as placeholder.
			(void)sc; // suppress unused warning
		}
	}
	m_basicShader->release();
}

} // namespace Render::GL
#include "backend.h"

namespace Render::GL {
// Backend is thin; all inline for now.
}
