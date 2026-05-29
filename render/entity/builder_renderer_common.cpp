#include "builder_renderer_common.h"

#include <string>
#include <unordered_map>

namespace Render::GL {
namespace {

using StyleRegistry = std::unordered_map<std::string, BuilderStyleConfig>;

auto style_registry() -> StyleRegistry& {
  static StyleRegistry styles;
  return styles;
}

} // namespace

void register_builder_style(std::string_view style_key,
                            const BuilderStyleConfig& style) {
  style_registry()[std::string(style_key)] = style;
}

void register_builder_styles(std::span<const BuilderStyleRegistration> styles) {
  for (const auto& style : styles) {
    register_builder_style(style.key, style.style);
  }
}

} // namespace Render::GL
