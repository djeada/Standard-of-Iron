#pragma once

#include "shader.h"
#include "utils/resource_utils.h"
#include <QDebug>
#include <QString>
#include <memory>
#include <unordered_map>

namespace Render::GL {

class ShaderCache {
public:
  auto load(const QString &name, const QString &vertPath,
            const QString &fragPath) -> Shader * {
    auto it = m_named.find(name);
    if (it != m_named.end()) {
      return it->second.get();
    }
    const QString resolved_vert =
        Utils::Resources::resolveResourcePath(vertPath);
    const QString resolved_frag =
        Utils::Resources::resolveResourcePath(fragPath);
    auto sh = std::make_unique<Shader>();
    if (!sh->loadFromFiles(resolved_vert, resolved_frag)) {
      qWarning() << "ShaderCache: Failed to load shader" << name;
      return nullptr;
    }
    Shader *raw = sh.get();
    m_named.emplace(name, std::move(sh));
    return raw;
  }

  auto get(const QString &name) const -> Shader * {
    auto it = m_named.find(name);
    return (it != m_named.end()) ? it->second.get() : nullptr;
  }

  auto getOrLoad(const QString &vertPath, const QString &fragPath) -> Shader * {
    const QString resolved_vert =
        Utils::Resources::resolveResourcePath(vertPath);
    const QString resolved_frag =
        Utils::Resources::resolveResourcePath(fragPath);
    auto key = resolved_vert + "|" + resolved_frag;
    auto it = m_byPath.find(key);
    if (it != m_byPath.end()) {
      return it->second.get();
    }
    auto sh = std::make_unique<Shader>();
    if (!sh->loadFromFiles(resolved_vert, resolved_frag)) {
      qWarning() << "ShaderCache: Failed to load shader from paths:"
                 << resolved_vert << "," << resolved_frag;
      return nullptr;
    }
    Shader *raw = sh.get();
    m_byPath.emplace(std::move(key), std::move(sh));
    return raw;
  }

  void initializeDefaults() {
    static const QString kShaderBase = QStringLiteral(":/assets/shaders/");
    auto resolve = [](const QString &path) {
      return Utils::Resources::resolveResourcePath(path);
    };

    const QString basicVert =
        resolve(kShaderBase + QStringLiteral("basic.vert"));
    const QString basicFrag =
        resolve(kShaderBase + QStringLiteral("basic.frag"));
    const QString gridFrag = resolve(kShaderBase + QStringLiteral("grid.frag"));
    load(QStringLiteral("basic"), basicVert, basicFrag);
    load(QStringLiteral("grid"), basicVert, gridFrag);
    const QString cylVert =
        resolve(kShaderBase + QStringLiteral("cylinder_instanced.vert"));
    const QString cylFrag =
        resolve(kShaderBase + QStringLiteral("cylinder_instanced.frag"));
    load(QStringLiteral("cylinder_instanced"), cylVert, cylFrag);
    const QString fogVert =
        resolve(kShaderBase + QStringLiteral("fog_instanced.vert"));
    const QString fogFrag =
        resolve(kShaderBase + QStringLiteral("fog_instanced.frag"));
    load(QStringLiteral("fog_instanced"), fogVert, fogFrag);
    const QString grassVert =
        resolve(kShaderBase + QStringLiteral("grass_instanced.vert"));
    const QString grassFrag =
        resolve(kShaderBase + QStringLiteral("grass_instanced.frag"));
    load(QStringLiteral("grass_instanced"), grassVert, grassFrag);

    const QString stoneVert =
        resolve(kShaderBase + QStringLiteral("stone_instanced.vert"));
    const QString stoneFrag =
        resolve(kShaderBase + QStringLiteral("stone_instanced.frag"));
    load(QStringLiteral("stone_instanced"), stoneVert, stoneFrag);

    const QString plantVert =
        resolve(kShaderBase + QStringLiteral("plant_instanced.vert"));
    const QString plantFrag =
        resolve(kShaderBase + QStringLiteral("plant_instanced.frag"));
    load(QStringLiteral("plant_instanced"), plantVert, plantFrag);

    const QString pineVert =
        resolve(kShaderBase + QStringLiteral("pine_instanced.vert"));
    const QString pineFrag =
        resolve(kShaderBase + QStringLiteral("pine_instanced.frag"));
    load(QStringLiteral("pine_instanced"), pineVert, pineFrag);

    const QString firecampVert =
        resolve(kShaderBase + QStringLiteral("firecamp.vert"));
    const QString firecampFrag =
        resolve(kShaderBase + QStringLiteral("firecamp.frag"));
    load(QStringLiteral("firecamp"), firecampVert, firecampFrag);

    const QString groundVert =
        resolve(kShaderBase + QStringLiteral("ground_plane.vert"));
    const QString groundFrag =
        resolve(kShaderBase + QStringLiteral("ground_plane.frag"));
    load(QStringLiteral("ground_plane"), groundVert, groundFrag);

    const QString terrainVert =
        resolve(kShaderBase + QStringLiteral("terrain_chunk.vert"));
    const QString terrainFrag =
        resolve(kShaderBase + QStringLiteral("terrain_chunk.frag"));
    load(QStringLiteral("terrain_chunk"), terrainVert, terrainFrag);

    const QString riverVert =
        resolve(kShaderBase + QStringLiteral("river.vert"));
    const QString riverFrag =
        resolve(kShaderBase + QStringLiteral("river.frag"));
    load(QStringLiteral("river"), riverVert, riverFrag);

    const QString riverbankVert =
        resolve(kShaderBase + QStringLiteral("riverbank.vert"));
    const QString riverbankFrag =
        resolve(kShaderBase + QStringLiteral("riverbank.frag"));
    load(QStringLiteral("riverbank"), riverbankVert, riverbankFrag);

    const QString bridgeVert =
        resolve(kShaderBase + QStringLiteral("bridge.vert"));
    const QString bridgeFrag =
        resolve(kShaderBase + QStringLiteral("bridge.frag"));
    load(QStringLiteral("bridge"), bridgeVert, bridgeFrag);

    const QString archerVert =
        resolve(kShaderBase + QStringLiteral("archer.vert"));
    const QString archerFrag =
        resolve(kShaderBase + QStringLiteral("archer.frag"));
    load(QStringLiteral("archer"), archerVert, archerFrag);

    const QString knightVert =
        resolve(kShaderBase + QStringLiteral("knight.vert"));
    const QString knightFrag =
        resolve(kShaderBase + QStringLiteral("knight.frag"));
    load(QStringLiteral("knight"), knightVert, knightFrag);

    const QString mounted_knightVert =
        resolve(kShaderBase + QStringLiteral("mounted_knight.vert"));
    const QString mounted_knightFrag =
        resolve(kShaderBase + QStringLiteral("mounted_knight.frag"));
    load(QStringLiteral("mounted_knight"), mounted_knightVert,
         mounted_knightFrag);

    const QString spearmanVert =
        resolve(kShaderBase + QStringLiteral("spearman.vert"));
    const QString spearmanFrag =
        resolve(kShaderBase + QStringLiteral("spearman.frag"));
    load(QStringLiteral("spearman"), spearmanVert, spearmanFrag);
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
