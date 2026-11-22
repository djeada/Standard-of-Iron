#include "render/backend/command_executor_common.h"

namespace Render::GL {

using namespace Render::GL::ColorIndex;
using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

void Backend::execute_effects_commands(const PreparedBatch& prepared,
                                       PipelineDrawContext& context) {
  const Camera& cam = *context.frame.camera;
  const auto& cmd = context.queue.get_sorted(prepared.start);

  if (m_combat_dust_pipeline != nullptr) {
    m_combat_dust_pipeline->set_view_position(cam.get_position());
  }

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
    Mesh* const ground = (m_resources != nullptr) ? m_resources->ground() : nullptr;
    m_effects_pipeline->draw_grid(std::get<GridCmdIndex>(cmd), ground, context);
    break;
  }
  case GroundMarkerCmdIndex: {
    if (m_ground_marker_pipeline == nullptr) {
      break;
    }
    m_ground_marker_pipeline->draw_markers(prepared, context);
    break;
  }
  case SelectionSmokeCmdIndex: {
    Mesh* disc = Render::Geom::SelectionDisc::get();
    if (disc == nullptr) {
      break;
    }
    m_effects_pipeline->draw_selection_smoke(
        std::get<SelectionSmokeCmdIndex>(cmd), disc, context);
    break;
  }
  case PrimitiveBatchCmdIndex: {
    if (m_primitive_batch_pipeline == nullptr) {
      break;
    }
    m_primitive_batch_pipeline->draw_batch(std::get<PrimitiveBatchCmdIndex>(cmd),
                                           context);
    break;
  }
  case EffectBatchCmdIndex: {
    const auto& first_eff = std::get<EffectBatchCmdIndex>(cmd);

    switch (first_eff.kind) {
    case EffectBatchCmd::Kind::HealingBeam:
      if (m_healing_beam_pipeline != nullptr) {
        m_healing_beam_pipeline->draw_beams(prepared, context);
      }
      break;
    case EffectBatchCmd::Kind::HealerAura:
      if (m_healer_aura_pipeline != nullptr) {
        m_healer_aura_pipeline->draw_auras(prepared, context);
      }
      break;
    case EffectBatchCmd::Kind::BloodPool:
      if (m_combat_dust_pipeline != nullptr) {
        m_combat_dust_pipeline->draw_blood_pools(prepared, context);
      }
      break;
    case EffectBatchCmd::Kind::CombatDust:
    case EffectBatchCmd::Kind::BuildingFlame:
    case EffectBatchCmd::Kind::BurningFlame:
    case EffectBatchCmd::Kind::Fireball:
    case EffectBatchCmd::Kind::StoneImpact:
    case EffectBatchCmd::Kind::MetalSpark:
    case EffectBatchCmd::Kind::HearthSmoke:
    case EffectBatchCmd::Kind::WeaponArc:
      if (m_combat_dust_pipeline != nullptr) {
        m_combat_dust_pipeline->draw_dust_effects(prepared, context);
      }
      break;
    }
    break;
  }
  case ModeIndicatorCmdIndex: {
    if (m_mode_indicator_pipeline == nullptr) {
      break;
    }
    m_mode_indicator_pipeline->draw_indicators(
        prepared, m_mesh_instancing_pipeline.get(), context);
    break;
  }
  default:
    break;
  }
}

} // namespace Render::GL
