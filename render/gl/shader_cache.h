#pragma once

#include "shader.h"
#include "utils/resource_utils.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <memory>
#include <unordered_map>
#include <utility>

namespace Render::GL {

class ShaderCache {
public:
  auto load(const QString &name, const QString &vert_path,
            const QString &frag_path) -> Shader * {
    auto it = m_named.find(name);
    if (it != m_named.end()) {
      return it->second.get();
    }
    const QString resolved_vert =
        Utils::Resources::resolveResourcePath(vert_path);
    const QString resolved_frag =
        Utils::Resources::resolveResourcePath(frag_path);
    auto sh = std::make_unique<Shader>();
    if (!sh->load_from_files(resolved_vert, resolved_frag)) {
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

  auto get_or_load(const QString &vert_path,
                   const QString &frag_path) -> Shader * {
    const QString resolved_vert =
        Utils::Resources::resolveResourcePath(vert_path);
    const QString resolved_frag =
        Utils::Resources::resolveResourcePath(frag_path);
    auto key = resolved_vert + "|" + resolved_frag;
    auto it = m_by_path.find(key);
    if (it != m_by_path.end()) {
      return it->second.get();
    }
    auto sh = std::make_unique<Shader>();
    if (!sh->load_from_files(resolved_vert, resolved_frag)) {
      qWarning() << "ShaderCache: Failed to load shader from paths:"
                 << resolved_vert << "," << resolved_frag;
      return nullptr;
    }
    Shader *raw = sh.get();
    m_by_path.emplace(std::move(key), std::move(sh));
    return raw;
  }

  void initialize_defaults() {
    static const QString shader_base = QStringLiteral(":/assets/shaders/");
    auto resolve = [](const QString &path) {
      return Utils::Resources::resolveResourcePath(path);
    };

    const QString basic_vert =
        resolve(shader_base + QStringLiteral("basic.vert"));
    const QString basic_frag =
        resolve(shader_base + QStringLiteral("basic.frag"));
    const QString grid_frag =
        resolve(shader_base + QStringLiteral("grid.frag"));
    load(QStringLiteral("basic"), basic_vert, basic_frag);
    load(QStringLiteral("grid"), basic_vert, grid_frag);
    const QString cyl_vert =
        resolve(shader_base + QStringLiteral("cylinder_instanced.vert"));
    const QString cyl_frag =
        resolve(shader_base + QStringLiteral("cylinder_instanced.frag"));
    load(QStringLiteral("cylinder_instanced"), cyl_vert, cyl_frag);

    const QString prim_vert =
        resolve(shader_base + QStringLiteral("primitive_instanced.vert"));
    const QString prim_frag =
        resolve(shader_base + QStringLiteral("primitive_instanced.frag"));
    load(QStringLiteral("primitive_instanced"), prim_vert, prim_frag);

    const QString fog_vert =
        resolve(shader_base + QStringLiteral("fog_instanced.vert"));
    const QString fog_frag =
        resolve(shader_base + QStringLiteral("fog_instanced.frag"));
    load(QStringLiteral("fog_instanced"), fog_vert, fog_frag);
    const QString grass_vert =
        resolve(shader_base + QStringLiteral("grass_instanced.vert"));
    const QString grass_frag =
        resolve(shader_base + QStringLiteral("grass_instanced.frag"));
    load(QStringLiteral("grass_instanced"), grass_vert, grass_frag);

    const QString stone_vert =
        resolve(shader_base + QStringLiteral("stone_instanced.vert"));
    const QString stone_frag =
        resolve(shader_base + QStringLiteral("stone_instanced.frag"));
    load(QStringLiteral("stone_instanced"), stone_vert, stone_frag);

    const QString plant_vert =
        resolve(shader_base + QStringLiteral("plant_instanced.vert"));
    const QString plant_frag =
        resolve(shader_base + QStringLiteral("plant_instanced.frag"));
    load(QStringLiteral("plant_instanced"), plant_vert, plant_frag);

    const QString pine_vert =
        resolve(shader_base + QStringLiteral("pine_instanced.vert"));
    const QString pine_frag =
        resolve(shader_base + QStringLiteral("pine_instanced.frag"));
    load(QStringLiteral("pine_instanced"), pine_vert, pine_frag);
    const QString olive_vert =
        resolve(shader_base + QStringLiteral("olive_instanced.vert"));
    const QString olive_frag =
        resolve(shader_base + QStringLiteral("olive_instanced.frag"));
    load(QStringLiteral("olive_instanced"), olive_vert, olive_frag);

    const QString firecamp_vert =
        resolve(shader_base + QStringLiteral("firecamp.vert"));
    const QString firecamp_frag =
        resolve(shader_base + QStringLiteral("firecamp.frag"));
    load(QStringLiteral("firecamp"), firecamp_vert, firecamp_frag);

    const QString ground_vert =
        resolve(shader_base + QStringLiteral("ground_plane.vert"));
    const QString ground_frag =
        resolve(shader_base + QStringLiteral("ground_plane.frag"));
    load(QStringLiteral("ground_plane"), ground_vert, ground_frag);

    const QString terrain_vert =
        resolve(shader_base + QStringLiteral("terrain_chunk.vert"));
    const QString terrain_frag =
        resolve(shader_base + QStringLiteral("terrain_chunk.frag"));
    load(QStringLiteral("terrain_chunk"), terrain_vert, terrain_frag);

    const QString river_vert =
        resolve(shader_base + QStringLiteral("river.vert"));
    const QString river_frag =
        resolve(shader_base + QStringLiteral("river.frag"));
    load(QStringLiteral("river"), river_vert, river_frag);

    const QString riverbank_vert =
        resolve(shader_base + QStringLiteral("riverbank.vert"));
    const QString riverbank_frag =
        resolve(shader_base + QStringLiteral("riverbank.frag"));
    load(QStringLiteral("riverbank"), riverbank_vert, riverbank_frag);

    const QString road_vert =
        resolve(shader_base + QStringLiteral("road.vert"));
    const QString road_frag =
        resolve(shader_base + QStringLiteral("road.frag"));
    load(QStringLiteral("road"), road_vert, road_frag);

    const QString bridge_vert =
        resolve(shader_base + QStringLiteral("bridge.vert"));
    const QString bridge_frag =
        resolve(shader_base + QStringLiteral("bridge.frag"));
    load(QStringLiteral("bridge"), bridge_vert, bridge_frag);

    const QString troop_shadow_vert =
        resolve(shader_base + QStringLiteral("troop_shadow.vert"));
    const QString troop_shadow_frag =
        resolve(shader_base + QStringLiteral("troop_shadow.frag"));
    load(QStringLiteral("troop_shadow"), troop_shadow_vert, troop_shadow_frag);

    const QString banner_vert =
        resolve(shader_base + QStringLiteral("banner.vert"));
    const QString banner_frag =
        resolve(shader_base + QStringLiteral("banner.frag"));
    load(QStringLiteral("banner"), banner_vert, banner_frag);

    const QString healing_beam_vert =
        resolve(shader_base + QStringLiteral("healing_beam.vert"));
    const QString healing_beam_frag =
        resolve(shader_base + QStringLiteral("healing_beam.frag"));
    load(QStringLiteral("healing_beam"), healing_beam_vert, healing_beam_frag);

    const QString healing_aura_vert =
        resolve(shader_base + QStringLiteral("healing_aura.vert"));
    const QString healing_aura_frag =
        resolve(shader_base + QStringLiteral("healing_aura.frag"));
    load(QStringLiteral("healing_aura"), healing_aura_vert, healing_aura_frag);

    load(QStringLiteral("combat_dust"),
         resolve(shader_base + QStringLiteral("combat_dust.vert")),
         resolve(shader_base + QStringLiteral("combat_dust.frag")));

    load(QStringLiteral("mode_indicator"),
         resolve(shader_base + QStringLiteral("mode_indicator.vert")),
         resolve(shader_base + QStringLiteral("mode_indicator.frag")));

    const auto load_base_shader = [&](const QString &name) {
      const QString vert =
          resolve(shader_base + name + QStringLiteral(".vert"));
      const QString frag =
          resolve(shader_base + name + QStringLiteral(".frag"));
      load(name, vert, frag);
      return std::pair<QString, QString>{vert, frag};
    };

    const auto [archer_vert, archer_frag] =
        load_base_shader(QStringLiteral("archer"));
    const auto [swordsman_vert, swordsman_frag] =
        load_base_shader(QStringLiteral("swordsman"));
    const auto [horse_knight_vert, horse_knight_frag] =
        load_base_shader(QStringLiteral("horse_swordsman"));
    const auto [spearman_vert, spearman_frag] =
        load_base_shader(QStringLiteral("spearman"));
    const auto [healer_vert, healer_frag] =
        load_base_shader(QStringLiteral("healer"));

    const QStringList nation_variants = {QStringLiteral("roman_republic"),
                                         QStringLiteral("carthage")};

    auto resource_exists = [](const QString &path) -> bool {
      QFileInfo const info(path);
      return info.exists();
    };

    auto load_variant = [&](const QString &base_key,
                            const QString &base_vert_path,
                            const QString &base_frag_path) {
      for (const QString &nation : nation_variants) {
        const QString shader_name = base_key + QStringLiteral("_") + nation;
        const QString variant_vert_res = shader_base + base_key +
                                         QStringLiteral("_") + nation +
                                         QStringLiteral(".vert");
        const QString variant_frag_res = shader_base + base_key +
                                         QStringLiteral("_") + nation +
                                         QStringLiteral(".frag");

        QString resolved_vert = resolve(variant_vert_res);
        if (!resource_exists(resolved_vert)) {
          resolved_vert = base_vert_path;
        }

        QString resolved_frag = resolve(variant_frag_res);
        if (!resource_exists(resolved_frag)) {
          resolved_frag = base_frag_path;
        }

        load(shader_name, resolved_vert, resolved_frag);
      }
    };

    load_variant(QStringLiteral("archer"), archer_vert, archer_frag);
    load_variant(QStringLiteral("spearman"), spearman_vert, spearman_frag);
    load_variant(QStringLiteral("swordsman"), swordsman_vert, swordsman_frag);
    load_variant(QStringLiteral("horse_swordsman"), horse_knight_vert,
                 horse_knight_frag);
    load_variant(QStringLiteral("healer"), healer_vert, healer_frag);
    load_variant(QStringLiteral("horse_archer"), horse_knight_vert,
                 horse_knight_frag);
    load_variant(QStringLiteral("horse_spearman"), horse_knight_vert,
                 horse_knight_frag);
  }

  void clear() {
    m_named.clear();
    m_by_path.clear();
    m_cache.clear();
  }

private:
  std::unordered_map<QString, std::unique_ptr<Shader>> m_by_path;

  std::unordered_map<QString, std::unique_ptr<Shader>> m_named;

  std::unordered_map<QString, std::unique_ptr<Shader>> m_cache;
};

} // namespace Render::GL
