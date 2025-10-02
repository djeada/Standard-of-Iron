#include "shader_cache.h"

namespace Render::GL {

namespace {
static const QString kShaderBase = QStringLiteral("assets/shaders/");
static const QString kBasicVert = kShaderBase + QStringLiteral("basic.vert");
static const QString kBasicFrag = kShaderBase + QStringLiteral("basic.frag");
static const QString kGridFrag  = kShaderBase + QStringLiteral("grid.frag");
}

Shader* ShaderCache::get(const QString& name) {
	if (name == QStringLiteral("basic")) {
		return getOrLoad(kBasicVert, kBasicFrag);
	}
	if (name == QStringLiteral("grid")) {
		return getOrLoad(kBasicVert, kGridFrag);
	}
	return nullptr;
}

} // namespace Render::GL
