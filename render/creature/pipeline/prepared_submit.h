#pragma once

#include "../../entity/registry.h"
#include "creature_pipeline.h"
#include "creature_render_state.h"

#include <cstddef>
#include <deque>
#include <span>
#include <vector>

namespace Render::GL {
class ISubmitter;
}

namespace Render::Creature::Pipeline {

class PreparedCreatureSubmitBatch {
public:
  void clear() noexcept;
  void reserve(std::size_t count);

  void add(PreparedCreatureRenderRow row);
  void add_with_legacy_context(PreparedCreatureRenderRow row,
                               const Render::GL::DrawContext &legacy_ctx);

  [[nodiscard]] auto empty() const noexcept -> bool;
  [[nodiscard]] auto size() const noexcept -> std::size_t;
  [[nodiscard]] auto
  rows() const noexcept -> std::span<const PreparedCreatureRenderRow>;

  [[nodiscard]] auto
  submit(Render::GL::ISubmitter &out) const noexcept -> SubmitStats;

private:
  std::vector<PreparedCreatureRenderRow> rows_{};
  std::deque<Render::GL::DrawContext> legacy_contexts_{};
};

[[nodiscard]] auto
submit_prepared_creatures(std::span<const PreparedCreatureRenderRow> rows,
                          Render::GL::ISubmitter &out) noexcept -> SubmitStats;

template <typename Preparation>
inline void submit_preparation(Preparation &prep, Render::GL::ISubmitter &out) {
  PreparedCreatureSubmitBatch prepared_bodies;
  prepared_bodies.reserve(prep.rows.size());
  bool has_main_pass_rows = false;
  for (std::size_t i = 0; i < prep.rows.size(); ++i) {
    if (prep.rows[i].pass != RenderPassIntent::Shadow) {
      has_main_pass_rows = true;
    }
    if constexpr (requires { prep.per_instance_ctx; }) {
      if (i < prep.per_instance_ctx.size()) {
        prepared_bodies.add_with_legacy_context(prep.rows[i],
                                                prep.per_instance_ctx[i]);
        continue;
      }
    }
    prepared_bodies.add(prep.rows[i]);
  }
  if (!prepared_bodies.empty()) {
    (void)prepared_bodies.submit(out);
  }
  if (!prep.rows.empty() && !has_main_pass_rows) {
    return;
  }
  for (auto &draw : prep.post_body_draws) {
    draw(out);
  }
}

} // namespace Render::Creature::Pipeline
