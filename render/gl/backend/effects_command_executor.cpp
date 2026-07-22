#include "command_executor_common.h"

namespace Render::GL {

using namespace Render::GL::ColorIndex;
using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

namespace {
const QVector3D k_grid_line_color(0.22F, 0.25F, 0.22F);
}

void Backend::execute_effects_commands(const PreparedBatch& prepared,
                                       CommandExecutionContext& context) {
  const auto& queue = context.queue;
  const Camera& cam = context.cam;
  const QMatrix4x4& view = context.view;
  const QMatrix4x4& projection = context.projection;
  const QMatrix4x4& view_proj = context.view_proj;
  const float banner_wind_strength = context.banner_wind_strength;
  bool const& polygon_offset_enabled = context.polygon_offset_enabled;
  const bool rigged_instancing_enabled = context.rigged_instancing_enabled;
  (void)cam;
  (void)view;
  (void)projection;
  (void)view_proj;
  (void)banner_wind_strength;
  (void)polygon_offset_enabled;
  (void)rigged_instancing_enabled;

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
  case SelectionRingCmdIndex: {
    Mesh* ring = Render::Geom::SelectionRing::get();
    if (ring == nullptr) {
      break;
    }

    DepthMaskScope const depth_mask(false);
    DepthTestScope const depth_test(true);
    PolygonOffsetScope const poly(-1.0F, -1.0F);
    BlendScope const blend(true);
    CullFaceScope const cull(false);

    const bool can_instance_rings =
        prepared.kind == PreparedBatchKind::SelectionRingInstanced &&
        m_mesh_instancing_pipeline && m_mesh_instancing_pipeline->is_initialized() &&
        m_effects_pipeline->m_basic_instanced_shader != nullptr;

    if (can_instance_rings) {
      GL::Shader* ring_shader = m_effects_pipeline->m_basic_instanced_shader;
      ring_shader->use();
      m_last_bound_shader = ring_shader;
      const auto vp_handle = m_effects_pipeline->m_basic_instanced_uniforms.view_proj;
      if (vp_handle != Shader::InvalidUniform) {
        ring_shader->set_uniform(vp_handle, view_proj);
      }
      const auto ut_handle = m_effects_pipeline->m_basic_instanced_uniforms.use_texture;
      if (ut_handle != Shader::InvalidUniform) {
        ring_shader->set_uniform(ut_handle, false);
      }

      m_mesh_instancing_pipeline->begin_batch(ring, ring_shader, nullptr);
      for (std::size_t j = i; j < batch_end; ++j) {
        const auto& sc = std::get<SelectionRingCmdIndex>(queue.get_sorted(j));
        QMatrix4x4 m = sc.model;
        m.scale(1.08F, 1.0F, 1.08F);
        m_mesh_instancing_pipeline->accumulate(m, sc.color, sc.alpha_outer);
      }
      m_mesh_instancing_pipeline->flush();

      m_mesh_instancing_pipeline->begin_batch(ring, ring_shader, nullptr);
      for (std::size_t j = i; j < batch_end; ++j) {
        const auto& sc = std::get<SelectionRingCmdIndex>(queue.get_sorted(j));
        m_mesh_instancing_pipeline->accumulate(sc.model, sc.color, sc.alpha_inner);
      }
      m_mesh_instancing_pipeline->flush();
    } else {
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

      for (std::size_t j = i; j < batch_end; ++j) {
        const auto& sc = std::get<SelectionRingCmdIndex>(queue.get_sorted(j));
        m_effects_pipeline->m_basic_shader->set_uniform(
            m_effects_pipeline->m_basic_uniforms.color, sc.color);

        {
          QMatrix4x4 m = sc.model;
          m.scale(1.08F, 1.0F, 1.08F);
          const QMatrix4x4 mvp = view_proj * m;
          m_effects_pipeline->m_basic_shader->set_uniform(
              m_effects_pipeline->m_basic_uniforms.mvp, mvp);
          m_effects_pipeline->m_basic_shader->set_uniform(
              m_effects_pipeline->m_basic_uniforms.model, m);
          m_effects_pipeline->m_basic_shader->set_uniform(
              m_effects_pipeline->m_basic_uniforms.alpha, sc.alpha_outer);
          ring->draw();
        }

        {
          const QMatrix4x4 mvp = view_proj * sc.model;
          m_effects_pipeline->m_basic_shader->set_uniform(
              m_effects_pipeline->m_basic_uniforms.mvp, mvp);
          m_effects_pipeline->m_basic_shader->set_uniform(
              m_effects_pipeline->m_basic_uniforms.model, sc.model);
          m_effects_pipeline->m_basic_shader->set_uniform(
              m_effects_pipeline->m_basic_uniforms.alpha, sc.alpha_inner);
          ring->draw();
        }
      }
    }
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
    if (prepared.kind == PreparedBatchKind::EffectInstanced) {
      const auto& first_eff =
          std::get<EffectBatchCmdIndex>(queue.get_sorted(prepared.start));
      switch (first_eff.kind) {
      case EffectBatchCmd::Kind::HealerAura: {
        if (m_healer_aura_pipeline == nullptr ||
            !m_healer_aura_pipeline->is_initialized()) {
          break;
        }
        std::vector<BackendPipelines::HealerAuraPipeline::AuraInstanceData>
            aura_instances;
        aura_instances.reserve(prepared.count);
        for (std::size_t idx = prepared.start; idx < prepared.start + prepared.count;
             ++idx) {
          const auto& eff = std::get<EffectBatchCmdIndex>(queue.get_sorted(idx));
          aura_instances.push_back(
              {eff.position, eff.color, eff.radius, eff.intensity, eff.time});
        }
        m_healer_aura_pipeline->render_aura_batch(
            aura_instances.data(), aura_instances.size(), view_proj);
        m_last_bound_shader = nullptr;
        break;
      }
      case EffectBatchCmd::Kind::CombatDust:
      case EffectBatchCmd::Kind::BuildingFlame:
      case EffectBatchCmd::Kind::BurningFlame:
      case EffectBatchCmd::Kind::Fireball:
      case EffectBatchCmd::Kind::BloodPool:
      case EffectBatchCmd::Kind::StoneImpact:
      case EffectBatchCmd::Kind::MetalSpark: {
        if (m_combat_dust_pipeline == nullptr ||
            !m_combat_dust_pipeline->is_initialized()) {
          break;
        }
        if (first_eff.kind == EffectBatchCmd::Kind::BloodPool) {
          std::vector<BackendPipelines::CombatDustPipeline::BloodPoolInstanceData>
              blood_instances;
          blood_instances.reserve(prepared.count);
          for (std::size_t idx = prepared.start; idx < prepared.start + prepared.count;
               ++idx) {
            const auto& eff = std::get<EffectBatchCmdIndex>(queue.get_sorted(idx));
            blood_instances.push_back({eff.position,
                                       eff.radius,
                                       eff.alpha_scale,
                                       eff.rotation,
                                       eff.aspect_ratio,
                                       eff.seed});
          }
          m_combat_dust_pipeline->render_blood_pool_batch(
              blood_instances.data(), blood_instances.size(), view_proj);
          m_last_bound_shader = nullptr;
          break;
        }
        std::vector<BackendPipelines::CombatDustPipeline::DustInstanceData>
            dust_instances;
        dust_instances.reserve(prepared.count);
        for (std::size_t idx = prepared.start; idx < prepared.start + prepared.count;
             ++idx) {
          const auto& eff = std::get<EffectBatchCmdIndex>(queue.get_sorted(idx));
          BackendPipelines::EffectType const etype =
              (eff.kind == EffectBatchCmd::Kind::BuildingFlame)
                  ? BackendPipelines::EffectType::Flame
              : (eff.kind == EffectBatchCmd::Kind::BurningFlame)
                  ? BackendPipelines::EffectType::BurningFlame
              : (eff.kind == EffectBatchCmd::Kind::Fireball)
                  ? BackendPipelines::EffectType::Fireball
              : (eff.kind == EffectBatchCmd::Kind::StoneImpact)
                  ? BackendPipelines::EffectType::StoneImpact
              : (eff.kind == EffectBatchCmd::Kind::MetalSpark)
                  ? BackendPipelines::EffectType::MetalSpark
                  : BackendPipelines::EffectType::Dust;
          dust_instances.push_back({eff.position,
                                    eff.color,
                                    eff.radius,
                                    eff.intensity,
                                    eff.time,
                                    etype,
                                    eff.kind == EffectBatchCmd::Kind::BurningFlame});
        }
        m_combat_dust_pipeline->render_dust_batch(
            dust_instances.data(), dust_instances.size(), view_proj);
        m_last_bound_shader = nullptr;
        break;
      }
      default:
        break;
      }
      break;
    }

    const auto& eff_cmd_ = std::get<EffectBatchCmdIndex>(cmd);
    switch (eff_cmd_.kind) {
    case EffectBatchCmd::Kind::HealingBeam: {
      struct HealingBeamView {
        const QVector3D& start_pos;
        const QVector3D& end_pos;
        const QVector3D& color;
        float progress;
        float beam_width;
        float intensity;
        float time;
      };
      const HealingBeamView beam{eff_cmd_.position,
                                 eff_cmd_.end_pos,
                                 eff_cmd_.color,
                                 eff_cmd_.progress,
                                 eff_cmd_.beam_width,
                                 eff_cmd_.intensity,
                                 eff_cmd_.time};
      if (m_healing_beam_pipeline == nullptr ||
          !m_healing_beam_pipeline->is_initialized()) {
        break;
      }
      m_healing_beam_pipeline->render_single_beam(beam.start_pos,
                                                  beam.end_pos,
                                                  beam.color,
                                                  beam.progress,
                                                  beam.beam_width,
                                                  beam.intensity,
                                                  beam.time,
                                                  view_proj);
      m_last_bound_shader = nullptr;
      break;
    }
    case EffectBatchCmd::Kind::HealerAura: {
      struct HealerAuraView {
        const QVector3D& position;
        const QVector3D& color;
        float radius;
        float intensity;
        float time;
      };
      const HealerAuraView aura{eff_cmd_.position,
                                eff_cmd_.color,
                                eff_cmd_.radius,
                                eff_cmd_.intensity,
                                eff_cmd_.time};
      if (m_healer_aura_pipeline == nullptr ||
          !m_healer_aura_pipeline->is_initialized()) {
        break;
      }
      m_healer_aura_pipeline->render_single_aura(
          aura.position, aura.color, aura.radius, aura.intensity, aura.time, view_proj);
      m_last_bound_shader = nullptr;
      break;
    }
    case EffectBatchCmd::Kind::CombatDust: {
      struct CombatDustView {
        const QVector3D& position;
        const QVector3D& color;
        float radius;
        float intensity;
        float time;
      };
      const CombatDustView dust{eff_cmd_.position,
                                eff_cmd_.color,
                                eff_cmd_.radius,
                                eff_cmd_.intensity,
                                eff_cmd_.time};
      if (m_combat_dust_pipeline == nullptr ||
          !m_combat_dust_pipeline->is_initialized()) {
        break;
      }
      m_combat_dust_pipeline->render_single_dust(
          dust.position, dust.color, dust.radius, dust.intensity, dust.time, view_proj);
      m_last_bound_shader = nullptr;
      break;
    }
    case EffectBatchCmd::Kind::BuildingFlame: {
      struct BuildingFlameView {
        const QVector3D& position;
        const QVector3D& color;
        float radius;
        float intensity;
        float time;
      };
      const BuildingFlameView flame{eff_cmd_.position,
                                    eff_cmd_.color,
                                    eff_cmd_.radius,
                                    eff_cmd_.intensity,
                                    eff_cmd_.time};
      if (m_combat_dust_pipeline == nullptr ||
          !m_combat_dust_pipeline->is_initialized()) {
        break;
      }
      m_combat_dust_pipeline->render_single_flame(flame.position,
                                                  flame.color,
                                                  flame.radius,
                                                  flame.intensity,
                                                  flame.time,
                                                  view_proj);
      m_last_bound_shader = nullptr;
      break;
    }
    case EffectBatchCmd::Kind::BurningFlame: {
      struct BurningFlameView {
        const QVector3D& position;
        const QVector3D& color;
        float radius;
        float intensity;
        float time;
      };
      const BurningFlameView flame{eff_cmd_.position,
                                   eff_cmd_.color,
                                   eff_cmd_.radius,
                                   eff_cmd_.intensity,
                                   eff_cmd_.time};
      if (m_combat_dust_pipeline == nullptr ||
          !m_combat_dust_pipeline->is_initialized()) {
        break;
      }
      BackendPipelines::CombatDustPipeline::DustInstanceData const instance{
          .position = flame.position,
          .color = flame.color,
          .radius = flame.radius,
          .intensity = flame.intensity,
          .time = flame.time,
          .effect_type = BackendPipelines::EffectType::BurningFlame,
          .overlay = true};
      m_combat_dust_pipeline->render_dust_batch(&instance, 1U, view_proj);
      m_last_bound_shader = nullptr;
      break;
    }
    case EffectBatchCmd::Kind::Fireball: {
      struct FireballView {
        const QVector3D& position;
        const QVector3D& color;
        float radius;
        float intensity;
        float time;
      };
      const FireballView fireball{eff_cmd_.position,
                                  eff_cmd_.color,
                                  eff_cmd_.radius,
                                  eff_cmd_.intensity,
                                  eff_cmd_.time};
      if (m_combat_dust_pipeline == nullptr ||
          !m_combat_dust_pipeline->is_initialized()) {
        break;
      }
      m_combat_dust_pipeline->render_single_fireball(fireball.position,
                                                     fireball.color,
                                                     fireball.radius,
                                                     fireball.intensity,
                                                     fireball.time,
                                                     view_proj);
      m_last_bound_shader = nullptr;
      break;
    }
    case EffectBatchCmd::Kind::BloodPool: {
      struct BloodPoolView {
        const QVector3D& position;
        float radius;
        float alpha_scale;
      };
      const BloodPoolView blood{
          eff_cmd_.position, eff_cmd_.radius, eff_cmd_.alpha_scale};
      if (m_combat_dust_pipeline == nullptr ||
          !m_combat_dust_pipeline->is_initialized()) {
        break;
      }
      m_combat_dust_pipeline->render_single_blood_pool(blood.position,
                                                       blood.radius,
                                                       blood.alpha_scale,
                                                       eff_cmd_.rotation,
                                                       eff_cmd_.aspect_ratio,
                                                       eff_cmd_.seed,
                                                       view_proj);
      m_last_bound_shader = nullptr;
      break;
    }
    case EffectBatchCmd::Kind::StoneImpact: {
      struct StoneImpactView {
        const QVector3D& position;
        const QVector3D& color;
        float radius;
        float intensity;
        float time;
      };
      const StoneImpactView impact{eff_cmd_.position,
                                   eff_cmd_.color,
                                   eff_cmd_.radius,
                                   eff_cmd_.intensity,
                                   eff_cmd_.time};
      if (m_combat_dust_pipeline == nullptr ||
          !m_combat_dust_pipeline->is_initialized()) {
        break;
      }
      m_combat_dust_pipeline->render_single_stone_impact(impact.position,
                                                         impact.color,
                                                         impact.radius,
                                                         impact.intensity,
                                                         impact.time,
                                                         view_proj);
      m_last_bound_shader = nullptr;
      break;
    }
    case EffectBatchCmd::Kind::MetalSpark: {
      if (m_combat_dust_pipeline == nullptr ||
          !m_combat_dust_pipeline->is_initialized()) {
        break;
      }
      BackendPipelines::CombatDustPipeline::DustInstanceData const spark_inst{
          eff_cmd_.position,
          eff_cmd_.color,
          eff_cmd_.radius,
          eff_cmd_.intensity,
          eff_cmd_.time,
          BackendPipelines::EffectType::MetalSpark};
      m_combat_dust_pipeline->render_dust_batch(&spark_inst, 1U, view_proj);
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

    if (prepared.kind == PreparedBatchKind::ModeIndicatorInstanced &&
        m_mesh_instancing_pipeline != nullptr &&
        m_mesh_instancing_pipeline->is_initialized() &&
        m_mode_indicator_pipeline->m_instanced_shader != nullptr) {
      const auto& first_mc =
          std::get<ModeIndicatorCmdIndex>(queue.get_sorted(prepared.start));

      Mesh* indicator_mesh = nullptr;
      if (first_mc.mode_type == Render::Geom::k_mode_type_attack) {
        indicator_mesh = Render::Geom::ModeIndicator::get_attack_mode_mesh();
      } else if (first_mc.mode_type == Render::Geom::k_mode_type_guard) {
        indicator_mesh = Render::Geom::ModeIndicator::get_guard_mode_mesh();
      } else if (first_mc.mode_type == Render::Geom::k_mode_type_hold) {
        indicator_mesh = Render::Geom::ModeIndicator::get_hold_mode_mesh();
      } else if (first_mc.mode_type == Render::Geom::k_mode_type_patrol) {
        indicator_mesh = Render::Geom::ModeIndicator::get_patrol_mode_mesh();
      }

      if (indicator_mesh != nullptr) {
        GL::Shader* inst_shader = m_mode_indicator_pipeline->m_instanced_shader;
        inst_shader->use();
        if (m_mode_indicator_pipeline->m_instanced_uniforms.time !=
            Shader::InvalidUniform) {
          inst_shader->set_uniform(m_mode_indicator_pipeline->m_instanced_uniforms.time,
                                   m_animation_time);
        }

        m_mesh_instancing_pipeline->begin_batch(indicator_mesh, inst_shader, nullptr);
        for (std::size_t idx = prepared.start; idx < prepared.start + prepared.count;
             ++idx) {
          const auto& mc = std::get<ModeIndicatorCmdIndex>(queue.get_sorted(idx));
          m_mesh_instancing_pipeline->accumulate(mc.model, mc.color, mc.alpha);
        }

        DepthMaskScope const depth_mask(false);
        BlendScope const blend(true);
        glEnable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);

        m_mesh_instancing_pipeline->flush();
        m_last_bound_shader = nullptr;
      }
      break;
    }

    const auto& mc = std::get<ModeIndicatorCmdIndex>(cmd);

    Mesh* indicator_mesh = nullptr;
    if (mc.mode_type == Render::Geom::k_mode_type_attack) {
      indicator_mesh = Render::Geom::ModeIndicator::get_attack_mode_mesh();
    } else if (mc.mode_type == Render::Geom::k_mode_type_guard) {
      indicator_mesh = Render::Geom::ModeIndicator::get_guard_mode_mesh();
    } else if (mc.mode_type == Render::Geom::k_mode_type_hold) {
      indicator_mesh = Render::Geom::ModeIndicator::get_hold_mode_mesh();
    } else if (mc.mode_type == Render::Geom::k_mode_type_patrol) {
      indicator_mesh = Render::Geom::ModeIndicator::get_patrol_mode_mesh();
    }

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
