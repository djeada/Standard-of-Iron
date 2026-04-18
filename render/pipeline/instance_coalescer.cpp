#include "instance_coalescer.h"

#include "../draw_queue.h" // for k_opaque_threshold

namespace Render::Pipeline {

auto parts_are_compatible(const Render::GL::DrawPartCmd &a,
                          const Render::GL::DrawPartCmd &b) noexcept -> bool {
  if (a.mesh != b.mesh) {
    return false;
  }
  if (a.material != b.material) {
    return false;
  }
  if (a.texture != b.texture) {
    return false;
  }
  if (a.material_id != b.material_id) {
    return false;
  }
  if (a.priority != b.priority) {
    return false;
  }
  // Transparency forces depth ordering; never fuse.
  if (a.alpha < Render::GL::k_opaque_threshold ||
      b.alpha < Render::GL::k_opaque_threshold) {
    return false;
  }
  // BonePaletteRef carries per-part skinning palette. Non-empty palettes
  // can't trivially share one instanced draw (each instance would need its
  // own indirect indexing), so any skinned part is routed to the per-part
  // path. Keeps the compatibility predicate symmetric.
  if (!a.palette.empty() || !b.palette.empty()) {
    return false;
  }
  return true;
}

auto coalesce_instances(std::span<const Render::GL::DrawPartCmd> parts,
                        std::size_t min_run)
    -> std::vector<InstancedPartBatch> {
  std::vector<InstancedPartBatch> out;
  if (parts.empty() || min_run < 2) {
    return out;
  }

  std::size_t i = 0;
  while (i < parts.size()) {
    std::size_t j = i + 1;
    while (j < parts.size() && parts_are_compatible(parts[i], parts[j])) {
      ++j;
    }
    const std::size_t run_len = j - i;
    if (run_len >= min_run) {
      InstancedPartBatch batch;
      batch.header = &parts[i];
      batch.start = i;
      batch.count = run_len;
      batch.instances.reserve(run_len);
      for (std::size_t k = i; k < j; ++k) {
        InstanceEntry inst;
        inst.world = parts[k].world;
        inst.color = parts[k].color;
        inst.alpha = parts[k].alpha;
        batch.instances.push_back(inst);
      }
      out.push_back(std::move(batch));
    }
    i = j;
  }
  return out;
}

auto compute_coalesce_stats(std::span<const InstancedPartBatch> batches,
                            std::size_t input_parts) -> CoalesceStats {
  CoalesceStats s;
  s.input_parts = input_parts;
  s.batch_count = batches.size();
  for (const auto &b : batches) {
    s.batched_parts += b.count;
  }
  return s;
}

} // namespace Render::Pipeline
