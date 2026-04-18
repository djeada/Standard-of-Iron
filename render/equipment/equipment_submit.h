// Stage 17 — central equipment submission.
//
// Every equipment renderer builds an EquipmentBatch (mesh + cylinder
// prims) directly. The batch is then replayed through a single
// submit_equipment_batch() helper — the ONLY call site that invokes
// ISubmitter::mesh/part/cylinder on behalf of equipment renderers.
// No equipment code touches ISubmitter directly anymore.

#pragma once

#include "../submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
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

struct EquipmentBatch {
  std::vector<EquipmentMeshPrim> meshes;
  std::vector<EquipmentCylinderPrim> cylinders;

  void clear() {
    meshes.clear();
    cylinders.clear();
  }

  void reserve(std::size_t mesh_count, std::size_t cyl_count = 0) {
    meshes.reserve(mesh_count);
    if (cyl_count > 0) {
      cylinders.reserve(cyl_count);
    }
  }
};

// THE single dispatch point for every equipment draw in the engine.
void submit_equipment_batch(const EquipmentBatch &batch,
                            ISubmitter &out) noexcept;

// Lightweight adapter: presents an EquipmentBatch as an ISubmitter for
// legacy code paths (e.g. rig DSL interpreter) that still need one.
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

  // Effects not used by equipment renderers — no-ops.
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

// High-level one-shot wrappers. These build an EquipmentBatch,
// invoke the equipment renderer's render() to populate it, then
// replay the batch through submit_equipment_batch(). Nation
// renderers call these instead of renderer->render() directly.
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
