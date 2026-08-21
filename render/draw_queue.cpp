#include "render/draw_queue.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <variant>

namespace Render::GL {

void DrawQueue::clear() {
  m_items_high_water = std::max(m_items_high_water, m_items.size());
  m_prepared_high_water = std::max(m_prepared_high_water, m_prepared_batches.size());
  m_submission_bucket_high_water =
      std::max(m_submission_bucket_high_water, m_submission_bucket_spans.size());
  m_local_light_high_water = std::max(m_local_light_high_water, m_local_lights.size());
  m_items.clear();
  m_sort_indices.clear();
  m_sort_keys.clear();
  m_prepared_batches.clear();
  m_submission_bucket_spans.clear();
  m_submission_bucket_ordered = true;
  m_local_lights.clear();
}

void DrawQueue::submit_local_light(const LocalLight& light) {
  if (m_local_lights.capacity() < m_local_light_high_water) {
    m_local_lights.reserve(m_local_light_high_water);
  }
  m_local_lights.push_back(light);
}

void DrawQueue::reserve_for_frame(std::size_t items_hint) {
  const std::size_t target = std::max(items_hint, m_items_high_water);
  if (target > m_items.capacity()) {
    m_items.reserve(target);
    m_sort_indices.reserve(target);
    m_sort_keys.reserve(target);
  }
  if (m_prepared_high_water > m_prepared_batches.capacity()) {
    m_prepared_batches.reserve(m_prepared_high_water);
  }
  if (m_submission_bucket_high_water > m_submission_bucket_spans.capacity()) {
    m_submission_bucket_spans.reserve(m_submission_bucket_high_water);
  }
}

void DrawQueue::sort_for_batching() {
  const std::size_t count = m_items.size();

  m_sort_keys.resize(count);
  m_sort_indices.resize(count);
  m_prepared_batches.clear();

  for (std::size_t i = 0; i < count; ++i) {
    m_sort_indices[i] = static_cast<uint32_t>(i);
    m_sort_keys[i] = compute_sort_key(m_items[i]);
  }

  if (count >= 2) {
    if (!m_submission_bucket_ordered || !sort_bucketed_ranges(count)) {
      sort_full_keys(0, count);
    }
  }
  build_prepared_batches();
}

auto DrawQueue::can_batch_mesh(std::size_t sorted_idx_a,
                               std::size_t sorted_idx_b) const -> bool {
  if (sorted_idx_a >= m_items.size() || sorted_idx_b >= m_items.size()) {
    return false;
  }
  const auto& a = m_items[m_sort_indices[sorted_idx_a]];
  const auto& b = m_items[m_sort_indices[sorted_idx_b]];
  if (a.index() != MeshCmdIndex || b.index() != MeshCmdIndex) {
    return false;
  }
  const auto& mesh_a = std::get<MeshCmdIndex>(a);
  const auto& mesh_b = std::get<MeshCmdIndex>(b);
  if (mesh_a.alpha < k_opaque_threshold || mesh_b.alpha < k_opaque_threshold) {
    return false;
  }
  return mesh_a.mesh == mesh_b.mesh && mesh_a.shader == mesh_b.shader &&
         mesh_a.texture == mesh_b.texture && mesh_a.material_id == mesh_b.material_id;
}

void DrawQueue::sort_full_keys(std::size_t start, std::size_t end) {
  std::stable_sort(m_sort_indices.begin() + static_cast<std::ptrdiff_t>(start),
                   m_sort_indices.begin() + static_cast<std::ptrdiff_t>(end),
                   [&](std::uint32_t lhs, std::uint32_t rhs) {
                     if (m_sort_keys[lhs] == m_sort_keys[rhs]) {
                       const auto lhs_full = full_resource_identity(m_items[lhs]);
                       const auto rhs_full = full_resource_identity(m_items[rhs]);
                       if (lhs_full != rhs_full) {
                         return lhs_full < rhs_full;
                       }
                       return lhs < rhs;
                     }
                     return m_sort_keys[lhs] < m_sort_keys[rhs];
                   });
}

auto DrawQueue::sort_bucketed_ranges(std::size_t count) -> bool {
  if (m_submission_bucket_spans.empty()) {
    return false;
  }

  std::size_t covered = 0;
  for (const SubmissionBucketSpan& span : m_submission_bucket_spans) {

    if (span.start != covered || span.end() > count) {
      return false;
    }
    if (span.count >= 2U && !span.preserves_append_order) {
      sort_full_keys(span.start, span.end());
    }
    covered = span.end();
  }

  return covered == count;
}

void DrawQueue::populate_sort_identity_prefix(const DrawCmd& cmd,
                                              SortIdentity& identity) const {
  const std::size_t type_index = cmd.index();
  identity.pass = type_index < k_render_pass_by_cmd_type.size()
                      ? k_render_pass_by_cmd_type[type_index]
                      : static_cast<std::uint8_t>(type_index);

  switch (draw_cmd_type(cmd)) {
  case DrawCmdType::Mesh: {
    const auto& mesh = std::get<MeshCmdIndex>(cmd);
    identity.pipeline = static_cast<std::uint8_t>(SortPipeline::Mesh);
    identity.transparency_bucket = transparency_bucket(mesh.alpha);
    break;
  }
  case DrawCmdType::TerrainScatter: {
    const auto& deco = std::get<TerrainScatterCmdIndex>(cmd);
    identity.pipeline = static_cast<std::uint8_t>(SortPipeline::TerrainScatterGrass) +
                        static_cast<std::uint8_t>(deco.species);
    if (scatter_species_is_blended(deco.species)) {
      identity.pass = static_cast<std::uint8_t>(RenderPassOrder::Mesh);
      identity.transparency_bucket = 1U;
    }
    break;
  }
  case DrawCmdType::TerrainSurface:
    identity.pipeline = static_cast<std::uint8_t>(SortPipeline::TerrainSurface);
    break;
  case DrawCmdType::TerrainFeature: {
    const auto& feature = std::get<TerrainFeatureCmdIndex>(cmd);
    identity.pipeline = static_cast<std::uint8_t>(SortPipeline::TerrainFeatureWater) +
                        static_cast<std::uint8_t>(feature.kind);
    identity.transparency_bucket = transparency_bucket(feature.alpha);
    break;
  }
  case DrawCmdType::PrimitiveBatch: {
    const auto& prim = std::get<PrimitiveBatchCmdIndex>(cmd);
    identity.pipeline = static_cast<std::uint8_t>(SortPipeline::PrimitiveSphere) +
                        static_cast<std::uint8_t>(prim.type);
    break;
  }
  case DrawCmdType::DrawPart: {
    const auto& part = std::get<DrawPartCmdIndex>(cmd);
    identity.pipeline = static_cast<std::uint8_t>(SortPipeline::DrawPart);
    identity.transparency_bucket = transparency_bucket(part.alpha);
    break;
  }
  case DrawCmdType::RiggedCreature: {
    const auto& rig = std::get<RiggedCreatureCmdIndex>(cmd);
    identity.pipeline = static_cast<std::uint8_t>(SortPipeline::RiggedCreature);
    identity.transparency_bucket = transparency_bucket(rig.alpha);
    break;
  }
  case DrawCmdType::Cylinder: {
    const auto& cylinder = std::get<CylinderCmdIndex>(cmd);
    identity.pipeline = static_cast<std::uint8_t>(SortPipeline::Cylinder);
    identity.transparency_bucket = transparency_bucket(cylinder.alpha);
    break;
  }
  case DrawCmdType::FogBatch:
    identity.pipeline = static_cast<std::uint8_t>(SortPipeline::Fog);
    break;
  case DrawCmdType::RainBatch:
    identity.pipeline = static_cast<std::uint8_t>(SortPipeline::Rain);
    break;
  case DrawCmdType::EffectBatch:
    identity.pipeline = static_cast<std::uint8_t>(SortPipeline::Effect);
    break;
  case DrawCmdType::SelectionSmoke:
    identity.pipeline = static_cast<std::uint8_t>(SortPipeline::SelectionSmoke);
    identity.transparency_bucket = 1U;
    break;
  case DrawCmdType::Grid:
    identity.pipeline = static_cast<std::uint8_t>(SortPipeline::Grid);
    break;
  case DrawCmdType::GroundMarker:
    identity.pipeline = static_cast<std::uint8_t>(SortPipeline::GroundMarker);
    identity.transparency_bucket = 1U;
    break;
  case DrawCmdType::ModeIndicator:
    identity.pipeline = static_cast<std::uint8_t>(SortPipeline::ModeIndicator);
    break;
  }
}

auto DrawQueue::compute_submission_bucket(const DrawCmd& cmd) const -> std::uint32_t {
  SortIdentity identity;
  populate_sort_identity_prefix(cmd, identity);
  return (static_cast<std::uint32_t>(identity.pass) << 16) |
         (static_cast<std::uint32_t>(identity.pipeline) << 8) |
         static_cast<std::uint32_t>(identity.transparency_bucket & 0x0FU);
}

auto DrawQueue::preserves_append_order(const DrawCmd& cmd) const -> bool {
  switch (draw_cmd_type(cmd)) {
  case DrawCmdType::TerrainScatter:
  case DrawCmdType::RainBatch:
  case DrawCmdType::TerrainSurface:
  case DrawCmdType::PrimitiveBatch:
  case DrawCmdType::FogBatch:
  case DrawCmdType::SelectionSmoke:
  case DrawCmdType::GroundMarker:
  case DrawCmdType::Grid:
  case DrawCmdType::EffectBatch:
    return true;
  case DrawCmdType::TerrainFeature: {
    const auto& feature = std::get<TerrainFeatureCmdIndex>(cmd);
    return feature.kind != LinearFeatureKind::Shoreline || !feature.visibility.enabled;
  }
  case DrawCmdType::Mesh:
  case DrawCmdType::Cylinder:
  case DrawCmdType::DrawPart:
  case DrawCmdType::RiggedCreature:
  case DrawCmdType::ModeIndicator:
    return false;
  }
  return false;
}

void DrawQueue::record_submission_bucket(const DrawCmd& cmd) {
  const std::uint32_t bucket = compute_submission_bucket(cmd);
  const bool append_ordered = preserves_append_order(cmd);
  const std::size_t next_index = m_items.size();
  if (!m_submission_bucket_spans.empty()) {
    SubmissionBucketSpan& last = m_submission_bucket_spans.back();
    if (bucket < last.bucket) {
      m_submission_bucket_ordered = false;
    }
    if (bucket == last.bucket) {
      ++last.count;
      last.preserves_append_order = last.preserves_append_order && append_ordered;
      return;
    }
  }

  m_submission_bucket_spans.push_back(
      SubmissionBucketSpan{.bucket = bucket,
                           .start = next_index,
                           .count = 1U,
                           .preserves_append_order = append_ordered});
}

auto DrawQueue::compute_sort_key(const DrawCmd& cmd) -> uint64_t {
  SortIdentity identity;
  populate_sort_identity_prefix(cmd, identity);

  switch (draw_cmd_type(cmd)) {
  case DrawCmdType::Mesh: {
    const auto& mesh = std::get<MeshCmdIndex>(cmd);
    identity.material = pack_12(sort_id(mesh.shader));
    identity.mesh = pack_16(sort_id(mesh.mesh));
    identity.texture = pack_12(sort_id(mesh.texture));
    break;
  }
  case DrawCmdType::TerrainScatter: {
    const auto& deco = std::get<TerrainScatterCmdIndex>(cmd);
    identity.material = pack_12(sort_id(deco.material));
    identity.mesh = pack_16(sort_id(deco.instance_buffer));
    break;
  }
  case DrawCmdType::FogBatch: {
    const auto& fog = std::get<FogBatchCmdIndex>(cmd);
    identity.mesh = pack_16(sort_id(fog.instance_buffer != nullptr
                                        ? static_cast<const void*>(fog.instance_buffer)
                                        : static_cast<const void*>(fog.instances)));
    break;
  }
  case DrawCmdType::TerrainSurface: {
    const auto& chunk = std::get<TerrainSurfaceCmdIndex>(cmd);
    identity.material = pack_12(chunk.sort_key);
    identity.mesh = pack_16(sort_id(chunk.mesh));
    identity.texture = pack_12(sort_id(chunk.material));
    break;
  }
  case DrawCmdType::TerrainFeature: {
    const auto& feature = std::get<TerrainFeatureCmdIndex>(cmd);
    identity.mesh = pack_16(sort_id(feature.mesh));
    identity.texture = pack_12(sort_id(feature.visibility.texture));
    break;
  }
  case DrawCmdType::PrimitiveBatch: {
    const auto& prim = std::get<PrimitiveBatchCmdIndex>(cmd);
    identity.mesh = pack_16(static_cast<std::uint32_t>(std::min<std::size_t>(
        prim.instance_count(), std::numeric_limits<std::uint16_t>::max())));
    break;
  }
  case DrawCmdType::DrawPart: {
    const auto& part = std::get<DrawPartCmdIndex>(cmd);
    identity.material = pack_12(sort_id(part.material));
    identity.mesh = pack_16(sort_id(part.mesh));
    identity.texture = pack_12(sort_id(part.texture));
    identity.skeleton = part.palette.empty() ? 0U : 1U;
    break;
  }
  case DrawCmdType::RiggedCreature: {
    const auto& rig = std::get<RiggedCreatureCmdIndex>(cmd);
    identity.material = pack_12(sort_id(rig.material));
    identity.mesh = pack_16(sort_id(rig.mesh));
    identity.texture = pack_12(sort_id(rig.texture));
    identity.skeleton = pack_4(rig.bone_count);
    break;
  }
  case DrawCmdType::EffectBatch: {
    const auto& effect = std::get<EffectBatchCmdIndex>(cmd);
    identity.material = pack_12(static_cast<std::uint32_t>(effect.kind));
    break;
  }
  case DrawCmdType::ModeIndicator: {
    const auto& mode = std::get<ModeIndicatorCmdIndex>(cmd);
    identity.material = pack_12(static_cast<std::uint32_t>(mode.mode_type));
    break;
  }
  case DrawCmdType::Grid:
  case DrawCmdType::GroundMarker:
  case DrawCmdType::SelectionSmoke:
  case DrawCmdType::Cylinder:
  case DrawCmdType::RainBatch:
    break;
  }

  return identity.pack();
}

void DrawQueue::build_prepared_batches() {
  m_prepared_batches.clear();
  const std::size_t count = m_sort_indices.size();
  std::size_t i = 0;
  while (i < count) {
    const DrawCmd& head = get_sorted(i);
    const DrawCmdType type = draw_cmd_type(head);
    PreparedBatchKind kind = PreparedBatchKind::Single;
    std::size_t end = i + 1;

    switch (type) {
    case DrawCmdType::Cylinder:
      while (end < count && get_sorted(end).index() == CylinderCmdIndex) {
        ++end;
      }
      if (end - i > 1U) {
        kind = PreparedBatchKind::CylinderInstanced;
      }
      break;
    case DrawCmdType::Mesh:
      while (end < count && can_batch_mesh(i, end)) {
        ++end;
      }
      if (end - i > 1U) {
        kind = PreparedBatchKind::MeshInstanced;
      }
      break;
    case DrawCmdType::DrawPart: {
      while (end < count && can_batch_draw_part(i, end)) {
        ++end;
      }
      constexpr std::size_t k_draw_part_min_run = 4;
      if (end - i >= k_draw_part_min_run) {
        kind = PreparedBatchKind::DrawPartInstanced;
      } else {
        end = i + 1;
      }
      break;
    }
    case DrawCmdType::RiggedCreature:
      while (end < count && can_batch_rigged(i, end)) {
        ++end;
      }
      if (end - i > 1U) {
        kind = PreparedBatchKind::RiggedCreatureInstanced;
      }
      break;
    case DrawCmdType::GroundMarker:
      while (end < count && get_sorted(end).index() == GroundMarkerCmdIndex) {
        ++end;
      }
      if (end - i > 1U) {
        kind = PreparedBatchKind::GroundMarkerInstanced;
      }
      break;
    case DrawCmdType::EffectBatch: {
      const auto& head_eff = std::get<EffectBatchCmdIndex>(head);
      while (end < count) {
        const DrawCmd& next_cmd = get_sorted(end);
        if (next_cmd.index() != EffectBatchCmdIndex) {
          break;
        }
        if (std::get<EffectBatchCmdIndex>(next_cmd).kind != head_eff.kind) {
          break;
        }
        ++end;
      }
      if (end - i > 1U) {
        kind = PreparedBatchKind::EffectInstanced;
      }
      break;
    }
    case DrawCmdType::ModeIndicator: {
      const auto& head_mode = std::get<ModeIndicatorCmdIndex>(head);
      while (end < count) {
        const DrawCmd& next_cmd = get_sorted(end);
        if (next_cmd.index() != ModeIndicatorCmdIndex) {
          break;
        }
        if (std::get<ModeIndicatorCmdIndex>(next_cmd).mode_type !=
            head_mode.mode_type) {
          break;
        }
        ++end;
      }
      if (end - i > 1U) {
        kind = PreparedBatchKind::ModeIndicatorInstanced;
      }
      break;
    }
    case DrawCmdType::TerrainSurface:
      while (end < count && can_batch_terrain_surface(i, end)) {
        ++end;
      }
      break;
    case DrawCmdType::TerrainFeature:
      while (end < count && can_batch_terrain_feature(i, end)) {
        ++end;
      }
      break;
    case DrawCmdType::Grid:
    case DrawCmdType::SelectionSmoke:
    case DrawCmdType::FogBatch:
    case DrawCmdType::TerrainScatter:
    case DrawCmdType::RainBatch:
    case DrawCmdType::PrimitiveBatch:
      break;
    }

    m_prepared_batches.push_back(
        PreparedBatch{.start = i,
                      .count = end - i,
                      .type = type,
                      .kind = kind,
                      .sort_key = m_sort_keys[m_sort_indices[i]]});
    i = end;
  }
}

auto DrawQueue::can_batch_draw_part(std::size_t sorted_idx_a,
                                    std::size_t sorted_idx_b) const -> bool {
  if (sorted_idx_a >= m_items.size() || sorted_idx_b >= m_items.size()) {
    return false;
  }
  const auto& a = m_items[m_sort_indices[sorted_idx_a]];
  const auto& b = m_items[m_sort_indices[sorted_idx_b]];
  if (a.index() != DrawPartCmdIndex || b.index() != DrawPartCmdIndex) {
    return false;
  }
  const auto& part_a = std::get<DrawPartCmdIndex>(a);
  const auto& part_b = std::get<DrawPartCmdIndex>(b);
  if (part_a.alpha < k_opaque_threshold || part_b.alpha < k_opaque_threshold) {
    return false;
  }
  return part_a.mesh == part_b.mesh && part_a.material == part_b.material &&
         part_a.texture == part_b.texture && part_a.material_id == part_b.material_id &&
         part_a.priority == part_b.priority && part_a.palette.empty() &&
         part_b.palette.empty();
}

auto DrawQueue::can_batch_rigged(std::size_t sorted_idx_a,
                                 std::size_t sorted_idx_b) const -> bool {
  if (sorted_idx_a >= m_items.size() || sorted_idx_b >= m_items.size()) {
    return false;
  }
  const auto& a = m_items[m_sort_indices[sorted_idx_a]];
  const auto& b = m_items[m_sort_indices[sorted_idx_b]];
  if (a.index() != RiggedCreatureCmdIndex || b.index() != RiggedCreatureCmdIndex) {
    return false;
  }
  const auto& rig_a = std::get<RiggedCreatureCmdIndex>(a);
  const auto& rig_b = std::get<RiggedCreatureCmdIndex>(b);

  return rig_a.mesh != nullptr && rig_a.texture == nullptr &&
         rig_b.texture == nullptr && rig_a.mesh == rig_b.mesh &&
         rig_a.material == rig_b.material;
}

auto DrawQueue::can_batch_terrain_surface(std::size_t sorted_idx_a,
                                          std::size_t sorted_idx_b) const -> bool {
  if (sorted_idx_a >= m_items.size() || sorted_idx_b >= m_items.size()) {
    return false;
  }
  const auto& a = m_items[m_sort_indices[sorted_idx_a]];
  const auto& b = m_items[m_sort_indices[sorted_idx_b]];
  if (a.index() != TerrainSurfaceCmdIndex || b.index() != TerrainSurfaceCmdIndex) {
    return false;
  }
  const auto& surface_a = std::get<TerrainSurfaceCmdIndex>(a);
  const auto& surface_b = std::get<TerrainSurfaceCmdIndex>(b);
  return surface_a.mesh != nullptr && surface_b.mesh != nullptr &&
         surface_a.params.is_ground_plane == surface_b.params.is_ground_plane &&
         surface_a.depth_write == surface_b.depth_write &&
         surface_a.wireframe == surface_b.wireframe &&
         surface_a.depth_bias == surface_b.depth_bias;
}

auto DrawQueue::can_batch_terrain_feature(std::size_t sorted_idx_a,
                                          std::size_t sorted_idx_b) const -> bool {
  if (sorted_idx_a >= m_items.size() || sorted_idx_b >= m_items.size()) {
    return false;
  }
  const auto& a = m_items[m_sort_indices[sorted_idx_a]];
  const auto& b = m_items[m_sort_indices[sorted_idx_b]];
  if (a.index() != TerrainFeatureCmdIndex || b.index() != TerrainFeatureCmdIndex) {
    return false;
  }
  const auto& feature_a = std::get<TerrainFeatureCmdIndex>(a);
  const auto& feature_b = std::get<TerrainFeatureCmdIndex>(b);
  return feature_a.mesh != nullptr && feature_b.mesh != nullptr &&
         feature_a.kind == feature_b.kind &&
         feature_a.road_surface_kind == feature_b.road_surface_kind &&
         transparency_bucket(feature_a.alpha) == transparency_bucket(feature_b.alpha) &&
         feature_a.visibility.texture == feature_b.visibility.texture &&
         feature_a.visibility.size == feature_b.visibility.size &&
         feature_a.visibility.tile_size == feature_b.visibility.tile_size &&
         feature_a.visibility.explored_alpha == feature_b.visibility.explored_alpha &&
         feature_a.visibility.enabled == feature_b.visibility.enabled;
}

auto DrawQueue::sort_id(const void* ptr) noexcept -> std::uint32_t {
  if (ptr == nullptr) {
    return 0;
  }
  const auto value = static_cast<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(ptr));
  return static_cast<std::uint32_t>(value ^ (value >> 16U) ^ (value >> 32U));
}

auto DrawQueue::full_resource_identity(const DrawCmd& cmd) noexcept
    -> std::array<std::uintptr_t, 4> {
  if (cmd.index() == MeshCmdIndex) {
    const auto& mesh = std::get<MeshCmdIndex>(cmd);
    return {ptr_value(mesh.shader), ptr_value(mesh.mesh), ptr_value(mesh.texture), 0U};
  }
  if (cmd.index() == TerrainScatterCmdIndex) {
    const auto& deco = std::get<TerrainScatterCmdIndex>(cmd);
    return {ptr_value(deco.material), ptr_value(deco.instance_buffer), 0U, 0U};
  }
  if (cmd.index() == FogBatchCmdIndex) {
    const auto& fog = std::get<FogBatchCmdIndex>(cmd);
    return {ptr_value(fog.instance_buffer), ptr_value(fog.instances), fog.count, 0U};
  }
  if (cmd.index() == TerrainSurfaceCmdIndex) {
    const auto& chunk = std::get<TerrainSurfaceCmdIndex>(cmd);
    return {static_cast<std::uintptr_t>(chunk.sort_key),
            ptr_value(chunk.mesh),
            ptr_value(chunk.material),
            0U};
  }
  if (cmd.index() == TerrainFeatureCmdIndex) {
    const auto& feature = std::get<TerrainFeatureCmdIndex>(cmd);
    return {ptr_value(feature.mesh), ptr_value(feature.visibility.texture), 0U, 0U};
  }
  if (cmd.index() == PrimitiveBatchCmdIndex) {
    const auto& prim = std::get<PrimitiveBatchCmdIndex>(cmd);
    return {prim.instance_count(), 0U, 0U, 0U};
  }
  if (cmd.index() == DrawPartCmdIndex) {
    const auto& part = std::get<DrawPartCmdIndex>(cmd);
    return {ptr_value(part.material),
            ptr_value(part.mesh),
            ptr_value(part.texture),
            part.palette.empty() ? 0U : 1U};
  }
  if (cmd.index() == RiggedCreatureCmdIndex) {
    const auto& rig = std::get<RiggedCreatureCmdIndex>(cmd);
    return {ptr_value(rig.material),
            ptr_value(rig.mesh),
            ptr_value(rig.texture),
            rig.bone_count};
  }
  if (cmd.index() == EffectBatchCmdIndex) {
    const auto& effect = std::get<EffectBatchCmdIndex>(cmd);
    return {static_cast<std::uintptr_t>(effect.kind), 0U, 0U, 0U};
  }
  if (cmd.index() == ModeIndicatorCmdIndex) {
    const auto& mode = std::get<ModeIndicatorCmdIndex>(cmd);
    return {static_cast<std::uintptr_t>(mode.mode_type), 0U, 0U, 0U};
  }
  return {0U, 0U, 0U, 0U};
}

} // namespace Render::GL
