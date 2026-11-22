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

// A submission that draws from what the kind's presentation systems decided.
using PresentedRenderFunc =
    std::function<void(const DrawContext&, const EntityPresentation&, ISubmitter&)>;

// Maps a renderer kind ("troops/<nation>/<type>", "wildlife/sheep", ...) to
// three things:
//   descriptions  the building archetype descriptions the kind draws, from the
//                 one catalog the baker reads (descriptions());
//   presentation  the systems that turn the snapshot into this frame's values
//                 for one entity, with no submitter and no GL (presentation());
//   submission    the function that draws from those values.
// get() hands out one function per kind that runs the presentation systems and
// then the submission, so the scene's per-entity path is: look up the kind,
// present, submit.
//
// Presentation kept inside submission, because separating it would reorder
// what is drawn: the per-resident and per-worker choices inside the home and
// farm activity (doorstep plans, gags, worker rosters) mutate frame budgets as
// they draw; the stockpile, ambient people and flag renderers resolve state
// while they emit; units and creatures prepare through their IParallelPreparer.
class EntityRendererRegistry {
public:
  struct TransparentStringHash {
    using is_transparent = void;
    auto operator()(std::string_view value) const noexcept -> std::size_t {
      return std::hash<std::string_view>{}(value);
    }
  };

  // A kind whose submission decides everything itself.
  void register_renderer(const std::string& type,
                         RenderFunc func,
                         std::shared_ptr<const IParallelPreparer> preparer = {});
  // A kind whose presentation systems run before its submission.
  void
  register_presented_renderer(const std::string& type,
                              PresentationSystems presentation,
                              PresentedRenderFunc submit,
                              std::shared_ptr<const IParallelPreparer> preparer = {});
  auto get(std::string_view type) const -> RenderFunc;
  auto get(RendererHandle handle) const -> const RenderFunc*;
  auto get_preparer(RendererHandle handle) const -> const IParallelPreparer*;
  auto get_handle(std::string_view type) const -> RendererHandle;

  // The presentation systems of a kind, in the order they run; empty for a kind
  // registered without any.
  [[nodiscard]] auto
  presentation(std::string_view type) const -> const PresentationSystems&;
  // The catalog descriptions the kind draws (BuildingArchetypeCatalogEntry::kinds).
  [[nodiscard]] auto
  descriptions(std::string_view type) const -> std::vector<std::string>;
  // Every registered kind.
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
