#pragma once

#include "render/horse/horse_spec.h"

namespace Render::Horse {

struct MountedSocketSet {
  HorseBone seat{HorseBone::SourceBack};
  HorseBone saddle{HorseBone::SourceBack};
  HorseBone stirrup_left{HorseBone::SourceBack};
  HorseBone stirrup_right{HorseBone::SourceBack};
  HorseBone rein_left{HorseBone::SourceHead};
  HorseBone rein_right{HorseBone::SourceHead};
  HorseBone bridle{HorseBone::SourceHead};
};

[[nodiscard]] constexpr auto mounted_socket_set() noexcept -> MountedSocketSet {
  return {};
}

} // namespace Render::Horse
