#pragma once

#include "../render_archetype.h"
#include "../submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <span>
#include <vector>

namespace Render::GL {

struct DrawContext;
struct BodyFrames;
struct HorseBodyFrames;
struct HumanoidPalette;
struct HumanoidAnimationContext;
struct HorseVariant;
struct HorseAnimationContext;
class IEquipmentRenderer;
class IHorseEquipmentRenderer;

struct EquipmentMeshPrim {
  Mesh *mesh{nullptr};
  Material *material{nullptr};
  QMatrix4x4 model{};
  QVector3D color{};
  Texture *texture{nullptr};
  float alpha{1.0F};
  int material_id{0};
};

struct EquipmentCylinderPrim {
  QVector3D start{};
  QVector3D end{};
  float radius{0.0F};
  QVector3D color{};
  float alpha{1.0F};
};

inline constexpr std::size_t kEquipmentArchetypePaletteCapacity = 8;

struct EquipmentArchetypePrim {
  const RenderArchetype *archetype{nullptr};
  QMatrix4x4 world{};
  std::array<QVector3D, kEquipmentArchetypePaletteCapacity> palette{};
  std::uint8_t palette_count{0U};
  Texture *default_texture{nullptr};
  float alpha_multiplier{1.0F};
  RenderArchetypeLod lod{RenderArchetypeLod::Full};
};

struct EquipmentBatch {
  std::vector<EquipmentMeshPrim> meshes;
  std::vector<EquipmentCylinderPrim> cylinders;
  std::vector<EquipmentArchetypePrim> archetypes;

  void clear() {
    meshes.clear();
    cylinders.clear();
    archetypes.clear();
  }

  void reserve(std::size_t mesh_count, std::size_t cyl_count = 0,
               std::size_t archetype_count = 0) {
    meshes.reserve(mesh_count);
    if (cyl_count > 0) {
      cylinders.reserve(cyl_count);
    }
    if (archetype_count > 0) {
      archetypes.reserve(archetype_count);
    }
  }
};

void append_equipment_archetype(
    EquipmentBatch &batch, const RenderArchetype &archetype,
    const QMatrix4x4 &world, std::span<const QVector3D> palette = {},
    Texture *default_texture = nullptr, float alpha_multiplier = 1.0F,
    RenderArchetypeLod lod = RenderArchetypeLod::Full);

void submit_equipment_batch(const EquipmentBatch &batch,
                            ISubmitter &out) noexcept;

class BatchSubmitterAdapter : public ISubmitter {
public:
  explicit BatchSubmitterAdapter(EquipmentBatch &batch) : m_batch(batch) {}

  void mesh(Mesh *mesh, const QMatrix4x4 &model, const QVector3D &color,
            Texture *tex = nullptr, float alpha = 1.0F,
            int material_id = 0) override {
    m_batch.meshes.push_back(
        {mesh, nullptr, model, color, tex, alpha, material_id});
  }

  void part(Mesh *mesh, Material *material, const QMatrix4x4 &model,
            const QVector3D &color, Texture *tex = nullptr, float alpha = 1.0F,
            int material_id = 0) override {
    m_batch.meshes.push_back(
        {mesh, material, model, color, tex, alpha, material_id});
  }

  void cylinder(const QVector3D &start, const QVector3D &end, float radius,
                const QVector3D &color, float alpha = 1.0F) override {
    m_batch.cylinders.push_back({start, end, radius, color, alpha});
  }

  void selection_ring(const QMatrix4x4 &, float, float,
                      const QVector3D &) override {}
  void grid(const QMatrix4x4 &, const QVector3D &, float, float,
            float) override {}
  void selection_smoke(const QMatrix4x4 &, const QVector3D &, float) override {}
  void healing_beam(const QVector3D &, const QVector3D &, const QVector3D &,
                    float, float, float, float) override {}
  void healer_aura(const QVector3D &, const QVector3D &, float, float,
                   float) override {}
  void combat_dust(const QVector3D &, const QVector3D &, float, float,
                   float) override {}
  void stone_impact(const QVector3D &, const QVector3D &, float, float,
                    float) override {}
  void mode_indicator(const QMatrix4x4 &, int, const QVector3D &,
                      float) override {}

private:
  EquipmentBatch &m_batch;
};

void render_equipment(IEquipmentRenderer &renderer, const DrawContext &ctx,
                      const BodyFrames &frames, const HumanoidPalette &palette,
                      const HumanoidAnimationContext &anim,
                      ISubmitter &out) noexcept;

void render_horse_equipment(const IHorseEquipmentRenderer &renderer,
                            const DrawContext &ctx,
                            const HorseBodyFrames &frames,
                            const HorseVariant &variant,
                            const HorseAnimationContext &anim,
                            ISubmitter &out) noexcept;

} // namespace Render::GL
