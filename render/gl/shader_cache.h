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
    const QString cylVert =
        kShaderBase + QStringLiteral("cylinder_instanced.vert");
    const QString cylFrag =
        kShaderBase + QStringLiteral("cylinder_instanced.frag");
    load(QStringLiteral("cylinder_instanced"), cylVert, cylFrag);
    const QString fogVert = kShaderBase + QStringLiteral("fog_instanced.vert");
    const QString fogFrag = kShaderBase + QStringLiteral("fog_instanced.frag");
    load(QStringLiteral("fog_instanced"), fogVert, fogFrag);
    const QString grassVert =
        kShaderBase + QStringLiteral("grass_instanced.vert");
    const QString grassFrag =
        kShaderBase + QStringLiteral("grass_instanced.frag");
    load(QStringLiteral("grass_instanced"), grassVert, grassFrag);

    const QString terrainVert =
        kShaderBase + QStringLiteral("terrain_chunk.vert");
    const QString terrainFrag =
        kShaderBase + QStringLiteral("terrain_chunk.frag");
    load(QStringLiteral("terrain_chunk"), terrainVert, terrainFrag);
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
