#include "command_executor_common.h"

namespace Render::GL {

using namespace Render::GL::ColorIndex;
using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

namespace {
const QVector3D k_grid_line_color(0.22F, 0.25F, 0.22F);
constexpr float k_ground_marker_offset = 0.06F;

auto billboard_effect_type(EffectBatchCmd::Kind kind) -> BackendPipelines::EffectType {
  switch (kind) {
  case EffectBatchCmd::Kind::BuildingFlame:
    return BackendPipelines::EffectType::Flame;
  case EffectBatchCmd::Kind::BurningFlame:
    return BackendPipelines::EffectType::BurningFlame;
  case EffectBatchCmd::Kind::Fireball:
    return BackendPipelines::EffectType::Fireball;
  case EffectBatchCmd::Kind::StoneImpact:
    return BackendPipelines::EffectType::StoneImpact;
  case EffectBatchCmd::Kind::MetalSpark:
    return BackendPipelines::EffectType::MetalSpark;
  default:
    return BackendPipelines::EffectType::Dust;
  }
}

auto make_dust_instance(const EffectBatchCmd& eff)
    -> BackendPipelines::CombatDustPipeline::DustInstanceData {
  return {.position = eff.position,
          .color = eff.color,
          .radius = eff.radius,
          .intensity = eff.intensity,
          .time = eff.time,
          .effect_type = billboard_effect_type(eff.kind),
          .overlay = false,
          .direction = eff.direction};
}
} // namespace

void Backend::execute_effects_commands(const PreparedBatch& prepared,
                                       CommandExecutionContext& context) {
  const auto& queue = context.queue;
  const Camera& cam = context.cam;
  const QMatrix4x4& view = context.view;
  const QMatrix4x4& projection = context.projection;
  const QMatrix4x4& view_proj = context.view_proj;
  const float banner_wind_strength = context.banner_wind_strength;
  bool const& polygon_offset_enabled = context.polygon_offset_enabled;
  (void)cam;
  (void)view;
  (void)projection;
  (void)view_proj;
  (void)banner_wind_strength;
  (void)polygon_offset_enabled;

  if (m_combat_dust_pipeline != nullptr) {
    m_combat_dust_pipeline->set_view_position(cam.get_position());
  }

  const std::size_t i = prepared.start;
  const std::size_t batch_end = prepared.end();
  const auto& cmd = queue.get_sorted(i);
  switch (cmd.index()) {
  case RainBatchCmdIndex: {
    const auto& rain = std::get<RainBatchCmdIndex>(cmd);
    if (m_rain_pipeline == nullptr || !m_rain_pipeline->is_initialized()) {
      break;
    }
    m_rain_pipeline->render(cam, rain.params);
    break;
  }
  case GridCmdIndex: {
    if (m_effects_pipeline->m_grid_shader == nullptr) {
      break;
    }
    const auto& gc = std::get<GridCmdIndex>(cmd);

    if (m_last_bound_shader != m_effects_pipeline->m_grid_shader) {
      m_effects_pipeline->m_grid_shader->use();
      m_last_bound_shader = m_effects_pipeline->m_grid_shader;
    }

    m_effects_pipeline->m_grid_shader->set_uniform(
        m_effects_pipeline->m_grid_uniforms.mvp, gc.mvp);
    m_effects_pipeline->m_grid_shader->set_uniform(
        m_effects_pipeline->m_grid_uniforms.model, gc.model);
    m_effects_pipeline->m_grid_shader->set_uniform(
        m_effects_pipeline->m_grid_uniforms.grid_color, gc.color);
    m_effects_pipeline->m_grid_shader->set_uniform(
        m_effects_pipeline->m_grid_uniforms.line_color, k_grid_line_color);
    m_effects_pipeline->m_grid_shader->set_uniform(
        m_effects_pipeline->m_grid_uniforms.cell_size, gc.cell_size);
    m_effects_pipeline->m_grid_shader->set_uniform(
        m_effects_pipeline->m_grid_uniforms.thickness, gc.thickness);

    if (m_resources) {
      if (auto* plane = m_resources->ground()) {
        plane->draw();
      }
    }
    break;
  }
  case GroundMarkerCmdIndex: {
    if (!m_ground_marker_pipeline || !m_ground_marker_pipeline->is_initialized()) {
      break;
    }

    DepthMaskScope const depth_mask(false);
    DepthTestScope const depth_test(true);
    PolygonOffsetScope const poly(-1.0F, -1.0F);
    BlendScope const blend(true);
    CullFaceScope const cull(false);

    Shader* marker_shader = m_ground_marker_pipeline->shader();
    marker_shader->use();
    m_last_bound_shader = marker_shader;
    m_last_bound_texture = nullptr;

    const auto& uniforms = m_ground_marker_pipeline->uniforms();
    m_ground_marker_pipeline->upload_pattern_table();
    set_uniform_if_valid(*marker_shader, uniforms.time, m_animation_time);
    set_uniform_if_valid(
        *marker_shader, uniforms.ground_offset, k_ground_marker_offset);

    const auto& height = std::get<GroundMarkerCmdIndex>(queue.get_sorted(i)).height;
    const bool has_height = height.enabled && height.texture != nullptr;
    set_uniform_if_valid(*marker_shader, uniforms.has_height_tex, has_height ? 1 : 0);
    if (has_height) {
      height.texture->bind(TextureUnit::terrain_height);
      m_last_bound_texture = height.texture;
      set_uniform_if_valid(
          *marker_shader, uniforms.height_tex, TextureUnit::terrain_height);
      set_uniform_if_valid(*marker_shader, uniforms.height_uv_scale, height.uv_scale);
      set_uniform_if_valid(*marker_shader, uniforms.height_uv_offset, height.uv_offset);
      set_uniform_if_valid(*marker_shader, uniforms.height_to_world, height.to_world);
    }

    m_ground_marker_pipeline->m_scratch.clear();
    for (std::size_t j = i; j < batch_end; ++j) {
      const auto& marker = std::get<GroundMarkerCmdIndex>(queue.get_sorted(j));
      BackendPipelines::GroundMarkerPipeline::InstanceGpu gpu{};
      gpu.center_radius = QVector4D(marker.center, marker.outer_radius);
      gpu.color_alpha = QVector4D(marker.color, marker.alpha);
      gpu.shape = QVector4D(marker.thickness,
                            static_cast<float>(marker.pattern),
                            marker.focused ? 1.0F : 0.0F,
                            marker.phase);
      m_ground_marker_pipeline->m_scratch.emplace_back(gpu);
    }

    const std::size_t marker_count = m_ground_marker_pipeline->m_scratch.size();
    m_ground_marker_pipeline->upload_instances(marker_count);
    m_ground_marker_pipeline->draw(marker_count);
    break;
  }
  case SelectionSmokeCmdIndex: {
    const auto& sm = std::get<SelectionSmokeCmdIndex>(cmd);
    Mesh* disc = Render::Geom::SelectionDisc::get();
    if (disc == nullptr) {
      break;
    }

    if (m_last_bound_shader != m_effects_pipeline->m_basic_shader) {
      m_effects_pipeline->m_basic_shader->use();
      m_last_bound_shader = m_effects_pipeline->m_basic_shader;
    }
    m_effects_pipeline->m_basic_shader->set_uniform(
        m_effects_pipeline->m_basic_uniforms.use_texture, false);
    m_effects_pipeline->m_basic_shader->set_uniform(
        m_effects_pipeline->m_basic_uniforms.instanced, false);
    m_effects_pipeline->m_basic_shader->set_uniform(
        m_effects_pipeline->m_basic_uniforms.view_proj, view_proj);
    m_effects_pipeline->m_basic_shader->set_uniform(
        m_effects_pipeline->m_basic_uniforms.color, sm.color);
    DepthMaskScope const depth_mask(false);
    DepthTestScope const depth_test(true);

    PolygonOffsetScope const poly(-1.0F, -1.0F);
    BlendScope const blend(true);
    for (int i = 0; i < 7; ++i) {
      float const scale = 1.35F + 0.12F * i;
      float const a = sm.base_alpha * (1.0F - 0.09F * i);
      QMatrix4x4 m = sm.model;
      m.translate(0.0F, 0.02F, 0.0F);
      m.scale(scale, 1.0F, scale);
      const QMatrix4x4 mvp = view_proj * m;
      m_effects_pipeline->m_basic_shader->set_uniform(
          m_effects_pipeline->m_basic_uniforms.mvp, mvp);
      m_effects_pipeline->m_basic_shader->set_uniform(
          m_effects_pipeline->m_basic_uniforms.model, m);
      m_effects_pipeline->m_basic_shader->set_uniform(
          m_effects_pipeline->m_basic_uniforms.alpha, a);
      disc->draw();
    }
    break;
  }
  case PrimitiveBatchCmdIndex: {
    const auto& batch = std::get<PrimitiveBatchCmdIndex>(cmd);
    if (batch.instance_count() == 0 || m_primitive_batch_pipeline == nullptr ||
        !m_primitive_batch_pipeline->is_initialized()) {
      break;
    }

    const auto* data = batch.instance_data();

    switch (batch.type) {
    case PrimitiveType::Sphere:
      m_primitive_batch_pipeline->upload_sphere_instances(data, batch.instance_count());
      m_primitive_batch_pipeline->draw_spheres(batch.instance_count(),
                                               view_proj,
                                               batch.params.light_direction,
                                               batch.params.ambient_strength);
      break;
    case PrimitiveType::Cylinder:
      m_primitive_batch_pipeline->upload_cylinder_instances(data,
                                                            batch.instance_count());
      m_primitive_batch_pipeline->draw_cylinders(batch.instance_count(),
                                                 view_proj,
                                                 batch.params.light_direction,
                                                 batch.params.ambient_strength);
      break;
    case PrimitiveType::Cone:
      m_primitive_batch_pipeline->upload_cone_instances(data, batch.instance_count());
      m_primitive_batch_pipeline->draw_cones(batch.instance_count(),
                                             view_proj,
                                             batch.params.light_direction,
                                             batch.params.ambient_strength);
      break;
    }

    m_last_bound_shader = m_primitive_batch_pipeline->shader();
    break;
  }
  case EffectBatchCmdIndex: {
    const auto& first_eff = std::get<EffectBatchCmdIndex>(cmd);

    auto for_each_effect = [&](auto&& visit) {
      for (std::size_t idx = i; idx < batch_end; ++idx) {
        visit(std::get<EffectBatchCmdIndex>(queue.get_sorted(idx)));
      }
    };

    switch (first_eff.kind) {
    case EffectBatchCmd::Kind::HealingBeam: {
      if (m_healing_beam_pipeline == nullptr ||
          !m_healing_beam_pipeline->is_initialized()) {
        break;
      }
      for_each_effect([&](const EffectBatchCmd& eff) {
        m_healing_beam_pipeline->render_single_beam(eff.position,
                                                    eff.end_pos,
                                                    eff.color,
                                                    eff.progress,
                                                    eff.beam_width,
                                                    eff.intensity,
                                                    eff.time,
                                                    view_proj);
      });
      m_last_bound_shader = nullptr;
      break;
    }
    case EffectBatchCmd::Kind::HealerAura: {
      if (m_healer_aura_pipeline == nullptr ||
          !m_healer_aura_pipeline->is_initialized()) {
        break;
      }
      std::vector<BackendPipelines::HealerAuraPipeline::AuraInstanceData> instances;
      instances.reserve(batch_end - i);
      for_each_effect([&](const EffectBatchCmd& eff) {
        instances.push_back(
            {eff.position, eff.color, eff.radius, eff.intensity, eff.time});
      });
      m_healer_aura_pipeline->render_aura_batch(
          instances.data(), instances.size(), view_proj);
      m_last_bound_shader = nullptr;
      break;
    }
    case EffectBatchCmd::Kind::BloodPool: {
      if (m_combat_dust_pipeline == nullptr ||
          !m_combat_dust_pipeline->is_initialized()) {
        break;
      }
      std::vector<BackendPipelines::CombatDustPipeline::BloodPoolInstanceData>
          instances;
      instances.reserve(batch_end - i);
      for_each_effect([&](const EffectBatchCmd& eff) {
        instances.push_back({.position = eff.position,
                             .radius = eff.radius,
                             .alpha_scale = eff.alpha_scale,
                             .rotation = eff.rotation,
                             .aspect_ratio = eff.aspect_ratio,
                             .seed = eff.seed});
      });
      m_combat_dust_pipeline->render_blood_pool_batch(
          instances.data(), instances.size(), view_proj);
      m_last_bound_shader = nullptr;
      break;
    }
    case EffectBatchCmd::Kind::CombatDust:
    case EffectBatchCmd::Kind::BuildingFlame:
    case EffectBatchCmd::Kind::BurningFlame:
    case EffectBatchCmd::Kind::Fireball:
    case EffectBatchCmd::Kind::StoneImpact:
    case EffectBatchCmd::Kind::MetalSpark: {
      if (m_combat_dust_pipeline == nullptr ||
          !m_combat_dust_pipeline->is_initialized()) {
        break;
      }
      std::vector<BackendPipelines::CombatDustPipeline::DustInstanceData> instances;
      instances.reserve(batch_end - i);
      for_each_effect([&](const EffectBatchCmd& eff) {
        instances.push_back(make_dust_instance(eff));
      });
      m_combat_dust_pipeline->render_dust_batch(
          instances.data(), instances.size(), view_proj);
      m_last_bound_shader = nullptr;
      break;
    }
    }
    break;
  }
  case ModeIndicatorCmdIndex: {
    if (m_mode_indicator_pipeline == nullptr ||
        !m_mode_indicator_pipeline->is_initialized()) {
      break;
    }

    auto resolve_glyph = [](int mode_type) -> Mesh* {
      auto const index = static_cast<std::size_t>(mode_type);
      if (index >= Render::Geom::k_indicator_kind_count) {
        return nullptr;
      }
      return Render::Geom::ModeIndicator::mesh_for(
          static_cast<Render::Geom::IndicatorKind>(index));
    };

    if (prepared.kind == PreparedBatchKind::ModeIndicatorInstanced &&
        m_mesh_instancing_pipeline != nullptr &&
        m_mesh_instancing_pipeline->is_initialized() &&
        m_mode_indicator_pipeline->m_instanced_shader != nullptr) {
      const auto& first_mc =
          std::get<ModeIndicatorCmdIndex>(queue.get_sorted(prepared.start));

      Mesh* const batch_mesh = resolve_glyph(first_mc.mode_type);
      if (batch_mesh != nullptr) {
        GL::Shader* inst_shader = m_mode_indicator_pipeline->m_instanced_shader;
        inst_shader->use();
        if (m_mode_indicator_pipeline->m_instanced_uniforms.time !=
            Shader::InvalidUniform) {
          inst_shader->set_uniform(m_mode_indicator_pipeline->m_instanced_uniforms.time,
                                   m_animation_time);
        }

        m_mesh_instancing_pipeline->begin_batch(batch_mesh, inst_shader, nullptr);
        for (std::size_t idx = prepared.start; idx < prepared.start + prepared.count;
             ++idx) {
          const auto& mc = std::get<ModeIndicatorCmdIndex>(queue.get_sorted(idx));
          m_mesh_instancing_pipeline->accumulate(mc.model, mc.color, mc.alpha);
        }

        DepthMaskScope const depth_mask(false);
        BlendScope const blend(true);
        DepthTestScope const depth_test(false);
        CullFaceScope const cull(false);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        m_mesh_instancing_pipeline->flush();
        m_last_bound_shader = nullptr;
      }
      break;
    }

    const auto& mc = std::get<ModeIndicatorCmdIndex>(cmd);
    Mesh* const indicator_mesh = resolve_glyph(mc.mode_type);
    if (indicator_mesh == nullptr) {
      break;
    }

    m_mode_indicator_pipeline->render_indicator(
        indicator_mesh, mc.model, view_proj, mc.color, mc.alpha, m_animation_time);

    m_last_bound_shader = nullptr;
    break;
  }
  default:
    break;
  }
}

} // namespace Render::GL
