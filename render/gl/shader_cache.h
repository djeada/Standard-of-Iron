#pragma once

#include "shader.h"
#include <QString>
#include <memory>
#include <unordered_map>

namespace Render::GL {

class ShaderCache {
public:
  Shader *load(const QString &name, const QString &vertPath,
               const QString &fragPath) {
    auto it = m_named.find(name);
    if (it != m_named.end())
      return it->second.get();
    auto sh = std::make_unique<Shader>();
    if (!sh->loadFromFiles(vertPath, fragPath))
      return nullptr;
    Shader *raw = sh.get();
    m_named.emplace(name, std::move(sh));
    return raw;
  }

  Shader *get(const QString &name) const {
    auto it = m_named.find(name);
    return (it != m_named.end()) ? it->second.get() : nullptr;
  }

  Shader *getOrLoad(const QString &vertPath, const QString &fragPath) {
    auto key = vertPath + "|" + fragPath;
    auto it = m_byPath.find(key);
    if (it != m_byPath.end())
      return it->second.get();
    auto sh = std::make_unique<Shader>();
    if (!sh->loadFromFiles(vertPath, fragPath))
      return nullptr;
    Shader *raw = sh.get();
    m_byPath.emplace(std::move(key), std::move(sh));
    return raw;
  }

  void initializeDefaults() {
    static const QString kShaderBase = QStringLiteral("assets/shaders/");
    const QString basicVert = kShaderBase + QStringLiteral("basic.vert");
    const QString basicFrag = kShaderBase + QStringLiteral("basic.frag");
    const QString gridFrag = kShaderBase + QStringLiteral("grid.frag");
    load(QStringLiteral("basic"), basicVert, basicFrag);
    load(QStringLiteral("grid"), basicVert, gridFrag);
  }

  void clear() {
    m_named.clear();
    m_byPath.clear();
    m_cache.clear();
  }

private:
  std::unordered_map<QString, std::unique_ptr<Shader>> m_byPath;

  std::unordered_map<QString, std::unique_ptr<Shader>> m_named;

  std::unordered_map<QString, std::unique_ptr<Shader>> m_cache;
};

} // namespace Render::GL
