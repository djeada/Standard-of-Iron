#pragma once

#include "../../creature/movement_state.h"
#include "../../horse/horse_renderer_base.h"
#include "../i_equipment_renderer.h"

namespace Render::GL {

struct DrawContext;
struct EquipmentBatch;

struct HorseAnimationContext {
  float time{0.0F};
  float phase{0.0F};
  float bob{0.0F};
  Render::Creature::MovementAnimationState movement_state{
      Render::Creature::MovementAnimationState::Idle};
  float rider_intensity{0.0F};
};

class IHorseEquipmentRenderer : public IEquipmentRenderer {
public:
  virtual ~IHorseEquipmentRenderer() = default;

  virtual void render(const DrawContext& ctx,
                      const HorseBodyFrames& frames,
                      const HorseVariant& variant,
                      const HorseAnimationContext& anim,
                      EquipmentBatch& batch) const = 0;

private:
  void render(const DrawContext&,
              const BodyFrames&,
              const HumanoidPalette&,
              const HumanoidAnimationContext&,
              EquipmentBatch&) final {}
};

} // namespace Render::GL
