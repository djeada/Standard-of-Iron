#pragma once

#include <QVector2D>
#include <QVector3D>

#include <array>
#include <cstddef>
#include <vector>

#include "mesh_buffers.h"
#include "pipeline_interface.h"
#include "render/gl/shader_cache.h"

namespace Render::GL::BackendPipelines {

class VegetationPipeline : public IPipeline {
public:
  explicit VegetationPipeline(GL::ShaderCache* shader_cache);
  ~VegetationPipeline() override;

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override { return m_initialized; }

  [[nodiscard]] auto stone_shader() const -> GL::Shader* { return m_stone_shader; }
  [[nodiscard]] auto plant_shader() const -> GL::Shader* { return m_plant_shader; }
  [[nodiscard]] auto pine_shader() const -> GL::Shader* { return m_pine_shader; }
  [[nodiscard]] auto olive_shader() const -> GL::Shader* { return m_olive_shader; }
  [[nodiscard]] auto firecamp_shader() const -> GL::Shader* {
    return m_firecamp_shader;
  }

  struct StoneUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_direction{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle camera_pos{GL::Shader::InvalidUniform};
  } m_stone_uniforms;

  struct FoliageUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle wind_strength{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle wind_speed{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_direction{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle camera_pos{GL::Shader::InvalidUniform};
  };

  FoliageUniforms m_plant_uniforms;
  FoliageUniforms m_pine_uniforms;
  FoliageUniforms m_olive_uniforms;

  struct FireCampUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle flicker_speed{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle flicker_amount{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle glow_strength{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle fire_texture{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle camera_right{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle camera_forward{GL::Shader::InvalidUniform};
  } m_firecamp_uniforms;

  StaticMeshBuffers m_stone_mesh;
  StaticMeshBuffers m_plant_mesh;
  StaticMeshBuffers m_pine_mesh;
  StaticMeshBuffers m_olive_mesh;
  StaticMeshBuffers m_cypress_mesh;
  StaticMeshBuffers m_palm_mesh;
  StaticMeshBuffers m_firecamp_mesh;

  [[nodiscard]] auto tent_shader() const -> GL::Shader* { return m_tent_shader; }
  [[nodiscard]] auto supply_cart_shader() const -> GL::Shader* {
    return m_supply_cart_shader;
  }
  [[nodiscard]] auto weapon_rack_shader() const -> GL::Shader* {
    return m_weapon_rack_shader;
  }
  [[nodiscard]] auto ruins_shader() const -> GL::Shader* { return m_ruins_shader; }
  [[nodiscard]] auto dead_tree_shader() const -> GL::Shader* {
    return m_dead_tree_shader;
  }
  [[nodiscard]] auto iron_ore_shader() const -> GL::Shader* {
    return m_iron_ore_shader;
  }
  [[nodiscard]] auto cursed_gold_vein_shader() const -> GL::Shader* {
    return m_cursed_gold_vein_shader;
  }
  [[nodiscard]] auto magic_shrine_shader() const -> GL::Shader* {
    return m_magic_shrine_shader;
  }
  [[nodiscard]] auto statue_shader() const -> GL::Shader* { return m_statue_shader; }

  struct PropUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_direction{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle camera_pos{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle magic_strength{GL::Shader::InvalidUniform};
  };

  PropUniforms m_tent_uniforms;
  PropUniforms m_supply_cart_uniforms;
  PropUniforms m_weapon_rack_uniforms;
  PropUniforms m_ruins_uniforms;
  PropUniforms m_dead_tree_uniforms;
  PropUniforms m_iron_ore_uniforms;
  PropUniforms m_magic_shrine_uniforms;
  PropUniforms m_cursed_gold_vein_uniforms;
  PropUniforms m_statue_uniforms;

  StaticMeshBuffers m_tent_mesh;
  StaticMeshBuffers m_supply_cart_mesh;
  StaticMeshBuffers m_weapon_rack_mesh;
  StaticMeshBuffers m_ruins_mesh;
  StaticMeshBuffers m_dead_tree_mesh;
  StaticMeshBuffers m_iron_ore_mesh;
  StaticMeshBuffers m_magic_shrine_mesh;
  StaticMeshBuffers m_cursed_gold_vein_mesh;
  StaticMeshBuffers m_abandoned_home_mesh;
  StaticMeshBuffers m_statue_mesh;

private:
  static constexpr std::size_t k_mesh_count = 17;

  auto all_meshes() -> std::array<StaticMeshBuffers*, k_mesh_count>;

  void initialize_stone_pipeline();
  void initialize_plant_pipeline();
  void initialize_pine_pipeline();
  void initialize_olive_pipeline();
  void initialize_cypress_pipeline();
  void initialize_palm_pipeline();
  void initialize_fire_camp_pipeline();
  void initialize_tent_pipeline();
  void initialize_supply_cart_pipeline();
  void initialize_weapon_rack_pipeline();
  void initialize_ruins_pipeline();
  void initialize_dead_tree_pipeline();
  void initialize_iron_ore_pipeline();
  void initialize_magic_shrine_pipeline();
  void initialize_cursed_gold_vein_pipeline();
  void initialize_abandoned_home_pipeline();
  void initialize_statue_pipeline();

  void upload_prop_mesh_impl(const std::vector<std::pair<QVector3D, QVector3D>>& verts,
                             const std::vector<uint16_t>& idx,
                             StaticMeshBuffers& mesh);

  GL::ShaderCache* m_shader_cache;
  bool m_initialized{false};

  GL::Shader* m_stone_shader{nullptr};
  GL::Shader* m_plant_shader{nullptr};
  GL::Shader* m_pine_shader{nullptr};
  GL::Shader* m_olive_shader{nullptr};
  GL::Shader* m_firecamp_shader{nullptr};
  GL::Shader* m_tent_shader{nullptr};
  GL::Shader* m_supply_cart_shader{nullptr};
  GL::Shader* m_weapon_rack_shader{nullptr};
  GL::Shader* m_ruins_shader{nullptr};
  GL::Shader* m_dead_tree_shader{nullptr};
  GL::Shader* m_iron_ore_shader{nullptr};
  GL::Shader* m_magic_shrine_shader{nullptr};
  GL::Shader* m_cursed_gold_vein_shader{nullptr};
  GL::Shader* m_statue_shader{nullptr};
};

} // namespace Render::GL::BackendPipelines
