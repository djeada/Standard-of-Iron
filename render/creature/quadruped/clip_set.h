#pragma once

#include "../../horse/horse_gait.h"

#include <cstdint>

namespace Render::Creature::Quadruped {

inline constexpr std::uint16_t kInvalidClip = 0xFFFFu;

struct ClipSet {
  std::uint16_t idle{kInvalidClip};
  std::uint16_t walk{kInvalidClip};
  std::uint16_t trot{kInvalidClip};
  std::uint16_t canter{kInvalidClip};
  std::uint16_t gallop{kInvalidClip};
  std::uint16_t fight{kInvalidClip};
};

[[nodiscard]] constexpr auto first_valid_clip(std::uint16_t a, std::uint16_t b,
                                              std::uint16_t c, std::uint16_t d,
                                              std::uint16_t e) noexcept
    -> std::uint16_t {
  if (a != kInvalidClip) {
    return a;
  }
  if (b != kInvalidClip) {
    return b;
  }
  if (c != kInvalidClip) {
    return c;
  }
  if (d != kInvalidClip) {
    return d;
  }
  if (e != kInvalidClip) {
    return e;
  }
  return kInvalidClip;
}

[[nodiscard]] constexpr auto clip_for_gait(const ClipSet &clips,
                                           Render::GL::GaitType gait) noexcept
    -> std::uint16_t {
  using Render::GL::GaitType;
  switch (gait) {
  case GaitType::IDLE:
    return first_valid_clip(clips.idle, clips.walk, clips.trot, clips.canter,
                            clips.gallop);
  case GaitType::WALK:
    return first_valid_clip(clips.walk, clips.trot, clips.canter, clips.gallop,
                            clips.idle);
  case GaitType::TROT:
    return first_valid_clip(clips.trot, clips.canter, clips.gallop, clips.walk,
                            clips.idle);
  case GaitType::CANTER:
    return first_valid_clip(clips.canter, clips.gallop, clips.trot, clips.walk,
                            clips.idle);
  case GaitType::GALLOP:
    return first_valid_clip(clips.gallop, clips.canter, clips.trot, clips.walk,
                            clips.idle);
  }
  return clip_for_gait(clips, GaitType::IDLE);
}

[[nodiscard]] constexpr auto clip_for_motion(const ClipSet &clips,
                                             Render::GL::GaitType gait,
                                             bool is_fighting) noexcept
    -> std::uint16_t {
  if (is_fighting && clips.fight != kInvalidClip) {
    return clips.fight;
  }
  return clip_for_gait(clips, gait);
}

[[nodiscard]] constexpr auto clip_for_motion(const ClipSet &clips, bool is_moving,
                                             bool is_running,
                                             bool is_fighting) noexcept
    -> std::uint16_t {
  if (is_fighting && clips.fight != kInvalidClip) {
    return clips.fight;
  }
  if (!is_moving) {
    return clip_for_gait(clips, Render::GL::GaitType::IDLE);
  }
  return clip_for_gait(
      clips, is_running ? Render::GL::GaitType::GALLOP
                        : Render::GL::GaitType::WALK);
}

} // namespace Render::Creature::Quadruped
