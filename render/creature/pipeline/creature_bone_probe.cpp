#include "creature_bone_probe.h"

namespace Render::Creature::Pipeline {
namespace {

thread_local BoneProbe* t_active_probe = nullptr;

} // namespace

void set_active_bone_probe(BoneProbe* probe) noexcept {
  t_active_probe = probe;
}

auto active_bone_probe() noexcept -> BoneProbe* {
  return t_active_probe;
}

} // namespace Render::Creature::Pipeline
