#pragma once

#include "shader.h"

namespace Render::GL {

template <typename Value>
inline void
set_uniform_if_valid(Shader& shader, Shader::UniformHandle handle, const Value& value) {
  if (handle != Shader::InvalidUniform) {
    shader.set_uniform(handle, value);
  }
}

} // namespace Render::GL
