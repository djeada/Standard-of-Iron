#pragma once

namespace Render::GL {
class Mesh;
}

namespace Render::GL::Headwear {

auto hair_cap(bool tied_back) -> Mesh*;
auto bun_coil() -> Mesh*;
auto bun_tie() -> Mesh*;
auto turban_wraps() -> Mesh*;
auto turban_tail() -> Mesh*;

} // namespace Render::GL::Headwear
