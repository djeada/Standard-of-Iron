#pragma once

#include <cstdint>

namespace Render::Creature::Pipeline {

enum class RenderPassIntent : std::uint8_t {
  Main = 0,
  Shadow = 1,
};

}
