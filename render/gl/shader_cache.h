#pragma once

#include <memory>
#include <QString>
#include <unordered_map>
#include "shader.h"

namespace Render::GL {

class ShaderCache {
public:
    // Load and cache a shader under a friendly name
    Shader* load(const QString& name, const QString& vertPath, const QString& fragPath) {
        auto it = m_named.find(name);
        if (it != m_named.end()) return it->second.get();
        auto sh = std::make_unique<Shader>();
        if (!sh->loadFromFiles(vertPath, fragPath)) return nullptr;
        Shader* raw = sh.get();
        m_named.emplace(name, std::move(sh));
        return raw;
    }

    // Get a shader by name (nullptr if not loaded)
    Shader* get(const QString& name) const {
        auto it = m_named.find(name);
        return (it != m_named.end()) ? it->second.get() : nullptr;
    }

    // Convenience for loading by paths without a name (kept for compatibility)
    Shader* getOrLoad(const QString& vertPath, const QString& fragPath) {
        auto key = vertPath + "|" + fragPath;
        auto it = m_byPath.find(key);
        if (it != m_byPath.end()) return it->second.get();
        auto sh = std::make_unique<Shader>();
        if (!sh->loadFromFiles(vertPath, fragPath)) return nullptr;
        Shader* raw = sh.get();
        m_byPath.emplace(std::move(key), std::move(sh));
        return raw;
    }

    // Load the default built-in shaders and register them under friendly names
    void initializeDefaults() {
        static const QString kShaderBase = QStringLiteral("assets/shaders/");
        const QString basicVert = kShaderBase + QStringLiteral("basic.vert");
        const QString basicFrag = kShaderBase + QStringLiteral("basic.frag");
        const QString gridFrag  = kShaderBase + QStringLiteral("grid.frag");
    const QString smokeVert = kShaderBase + QStringLiteral("smoke.vert");
    const QString smokeFrag = kShaderBase + QStringLiteral("smoke.frag");
        load(QStringLiteral("basic"), basicVert, basicFrag);
        load(QStringLiteral("grid"),  basicVert, gridFrag);
    load(QStringLiteral("smoke"), smokeVert, smokeFrag);
    }

    void clear() { m_cache.clear(); }

private:
    // Legacy path-keyed cache (optional)
    std::unordered_map<QString, std::unique_ptr<Shader>> m_byPath;
    // Named shaders (preferred)
    std::unordered_map<QString, std::unique_ptr<Shader>> m_named;
    // Backwards compatibility alias
    std::unordered_map<QString, std::unique_ptr<Shader>> m_cache; // deprecated
};

} // namespace Render::GL
