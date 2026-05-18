#pragma once

namespace Render::GL {

enum class BallistaAnimState {
  Idle,
  Loading,
  Firing,
  Resetting
};

struct BallistaAnimContext {
  BallistaAnimState state{BallistaAnimState::Idle};
  float loading_progress{0.0F};
  float firing_progress{0.0F};
  bool show_bolt{false};
};

enum class CatapultAnimState {
  Idle,
  Loading,
  Firing,
  Resetting
};

struct CatapultAnimContext {
  CatapultAnimState state{CatapultAnimState::Idle};
  float loading_progress{0.0F};
  float firing_progress{0.0F};
  bool show_stone{false};
};

} // namespace Render::GL
