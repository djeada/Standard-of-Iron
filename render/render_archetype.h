#pragma once

#include "world_chunk.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace Render::GL {
class Mesh;
class Texture;
struct Material;
class ISubmitter;
} // namespace Render::GL

namespace Render::GL {

inline constexpr std::uint8_t kRenderArchetypeFixedColorSlot =
    std::numeric_limits<std::uint8_t>::max();

enum class RenderArchetypeLod : std::uint8_t { Full = 0, Minimal = 1, Count };

struct RenderArchetypeDraw {
  Mesh *mesh = nullptr;
  Material *material = nullptr;
  QMatrix4x4 local_model;
  QVector3D color{1.0F, 1.0F, 1.0F};
  Texture *texture = nullptr;
  float alpha = 1.0F;
  int material_id = 0;
  std::uint8_t palette_slot = kRenderArchetypeFixedColorSlot;
};

struct RenderArchetypeSlice {
  std::vector<RenderArchetypeDraw> draws;
  BoundingBox local_bounds;
  float max_distance = std::numeric_limits<float>::infinity();
};

struct RenderArchetype {
  std::string debug_name;
  std::array<RenderArchetypeSlice,
             static_cast<std::size_t>(RenderArchetypeLod::Count)>
      lods{};
};

struct RenderInstance {
  const RenderArchetype *archetype = nullptr;
  QMatrix4x4 world;
  std::span<const QVector3D> palette;
  Texture *default_texture = nullptr;
  float alpha_multiplier = 1.0F;
  RenderArchetypeLod lod = RenderArchetypeLod::Full;
};

template <std::size_t PaletteCapacity> struct StoredRenderInstance {
  const RenderArchetype *archetype = nullptr;
  QMatrix4x4 world;
  std::array<QVector3D, PaletteCapacity> palette_storage{};
  std::uint8_t palette_count = 0U;
  Texture *default_texture = nullptr;
  float alpha_multiplier = 1.0F;
  RenderArchetypeLod lod = RenderArchetypeLod::Full;

  void set_palette(std::span<const QVector3D> palette) {
    assert(palette.size() <= PaletteCapacity);
    palette_count = static_cast<std::uint8_t>(palette.size());
    std::copy(palette.begin(), palette.end(), palette_storage.begin());
  }

  [[nodiscard]] auto render_instance() const -> RenderInstance {
    return RenderInstance{
        .archetype = archetype,
        .world = world,
        .palette =
            std::span<const QVector3D>(palette_storage.data(), palette_count),
        .default_texture = default_texture,
        .alpha_multiplier = alpha_multiplier,
        .lod = lod,
    };
  }
};

auto empty_bounding_box() -> BoundingBox;
auto box_local_model(const QVector3D &center,
                     const QVector3D &scale) -> QMatrix4x4;
auto cylinder_local_model(const QVector3D &start, const QVector3D &end,
                          float radius) -> QMatrix4x4;
auto select_render_archetype_lod(const RenderArchetype &archetype,
                                 float distance) -> RenderArchetypeLod;
void submit_render_instance(ISubmitter &out, const RenderInstance &instance);

class RenderArchetypeBuilder {
public:
  explicit RenderArchetypeBuilder(std::string name);

  void use_lod(RenderArchetypeLod lod);
  void set_max_distance(float max_distance);

  void add_mesh(Mesh *mesh, const QMatrix4x4 &local_model,
                const QVector3D &color, Texture *texture = nullptr,
                float alpha = 1.0F, int material_id = 0,
                Material *material = nullptr);
  void add_palette_mesh(Mesh *mesh, const QMatrix4x4 &local_model,
                        std::uint8_t palette_slot, Texture *texture = nullptr,
                        float alpha = 1.0F, int material_id = 0,
                        Material *material = nullptr);

  void add_box(const QVector3D &center, const QVector3D &scale,
               const QVector3D &color, Texture *texture = nullptr,
               float alpha = 1.0F, int material_id = 0,
               Material *material = nullptr);
  void add_palette_box(const QVector3D &center, const QVector3D &scale,
                       std::uint8_t palette_slot, Texture *texture = nullptr,
                       float alpha = 1.0F, int material_id = 0,
                       Material *material = nullptr);

  void add_cylinder(const QVector3D &start, const QVector3D &end, float radius,
                    const QVector3D &color, Texture *texture = nullptr,
                    float alpha = 1.0F, int material_id = 0,
                    Material *material = nullptr);
  void add_palette_cylinder(const QVector3D &start, const QVector3D &end,
                            float radius, std::uint8_t palette_slot,
                            Texture *texture = nullptr, float alpha = 1.0F,
                            int material_id = 0, Material *material = nullptr);

  auto build() && -> RenderArchetype;

private:
  void add_draw(RenderArchetypeDraw draw);

  RenderArchetype m_archetype;
  RenderArchetypeLod m_active_lod{RenderArchetypeLod::Full};
};

} // namespace Render::GL
