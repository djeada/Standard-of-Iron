#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "render/creature/humanoid/humanoid_types.h"
#include "render/creature/part_graph.h"
#include "render/entity/entity_presentation.h"
#include "render/frame/draw_context.h"

namespace Render::GL {

using PresentedRenderFunc =
    std::function<void(const DrawContext&, const EntityPresentation&, ISubmitter&)>;

class EntityRendererRegistry {
public:
  struct TransparentStringHash {
    using is_transparent = void;
    auto operator()(std::string_view value) const noexcept -> std::size_t {
      return std::hash<std::string_view>{}(value);
    }
  };

  void register_renderer(const std::string& type,
                         RenderFunc func,
                         std::shared_ptr<const IParallelPreparer> preparer = {});

  void
  register_presented_renderer(const std::string& type,
                              PresentationSystems presentation,
                              PresentedRenderFunc submit,
                              std::shared_ptr<const IParallelPreparer> preparer = {});
  auto get(std::string_view type) const -> RenderFunc;
  auto get(RendererHandle handle) const -> const RenderFunc*;
  auto get_preparer(RendererHandle handle) const -> const IParallelPreparer*;
  auto get_handle(std::string_view type) const -> RendererHandle;

  [[nodiscard]] auto
  presentation(std::string_view type) const -> const PresentationSystems&;

  [[nodiscard]] auto
  descriptions(std::string_view type) const -> std::vector<std::string>;

  [[nodiscard]] auto kinds() const -> std::vector<std::string>;

private:
  std::
      unordered_map<std::string, RendererHandle, TransparentStringHash, std::equal_to<>>
          m_lookup;
  void store(const std::string& type,
             RenderFunc func,
             PresentationSystems presentation,
             std::shared_ptr<const IParallelPreparer> preparer);

  std::vector<RenderFunc> m_renderers;
  std::vector<PresentationSystems> m_presentation;
  std::vector<std::shared_ptr<const IParallelPreparer>> m_preparers;
};

class HumanoidRendererBase;

void register_built_in_entity_renderers(EntityRendererRegistry& registry);

void register_humanoid_renderer(EntityRendererRegistry& registry,
                                std::string key,
                                std::shared_ptr<const HumanoidRendererBase> renderer);

} // namespace Render::GL
