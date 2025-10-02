#pragma once

#include <memory>
#include <QString>
#include <unordered_map>
#include "shader.h"

namespace Render::GL {

class ShaderCache {
public:
    Shader* getOrLoad(const QString& vertPath, const QString& fragPath) {
        auto key = vertPath + "|" + fragPath;
        auto it = m_cache.find(key);
        if (it != m_cache.end()) return it->second.get();
        auto sh = std::make_unique<Shader>();
        if (!sh->loadFromFiles(vertPath, fragPath)) return nullptr;
        Shader* raw = sh.get();
        m_cache.emplace(std::move(key), std::move(sh));
        return raw;
    }

    void clear() { m_cache.clear(); }

private:
    std::unordered_map<QString, std::unique_ptr<Shader>> m_cache;
};

} // namespace Render::GL
