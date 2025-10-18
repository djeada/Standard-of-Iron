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

    const QString stoneVert =
        kShaderBase + QStringLiteral("stone_instanced.vert");
    const QString stoneFrag =
        kShaderBase + QStringLiteral("stone_instanced.frag");
    load(QStringLiteral("stone_instanced"), stoneVert, stoneFrag);

    const QString plantVert =
        kShaderBase + QStringLiteral("plant_instanced.vert");
    const QString plantFrag =
        kShaderBase + QStringLiteral("plant_instanced.frag");
    load(QStringLiteral("plant_instanced"), plantVert, plantFrag);

    const QString pineVert =
        kShaderBase + QStringLiteral("pine_instanced.vert");
    const QString pineFrag =
        kShaderBase + QStringLiteral("pine_instanced.frag");
    load(QStringLiteral("pine_instanced"), pineVert, pineFrag);

    const QString groundVert =
        kShaderBase + QStringLiteral("ground_plane.vert");
    const QString groundFrag =
        kShaderBase + QStringLiteral("ground_plane.frag");
    load(QStringLiteral("ground_plane"), groundVert, groundFrag);

    const QString terrainVert =
        kShaderBase + QStringLiteral("terrain_chunk.vert");
    const QString terrainFrag =
        kShaderBase + QStringLiteral("terrain_chunk.frag");
    load(QStringLiteral("terrain_chunk"), terrainVert, terrainFrag);

    const QString riverVert = kShaderBase + QStringLiteral("river.vert");
    const QString riverFrag = kShaderBase + QStringLiteral("river.frag");
    load(QStringLiteral("river"), riverVert, riverFrag);

    const QString archerVert = kShaderBase + QStringLiteral("archer.vert");
    const QString archerFrag = kShaderBase + QStringLiteral("archer.frag");
    load(QStringLiteral("archer"), archerVert, archerFrag);

    const QString knightVert = kShaderBase + QStringLiteral("knight.vert");
    const QString knightFrag = kShaderBase + QStringLiteral("knight.frag");
    load(QStringLiteral("knight"), knightVert, knightFrag);
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
