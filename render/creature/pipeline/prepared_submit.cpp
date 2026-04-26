#include "prepared_submit.h"

#include "../../submitter.h"

#include <string_view>
#include <vector>

namespace Render::Creature::Pipeline {

void submit_preparation(CreaturePreparationResult &prep,
                        Render::GL::ISubmitter &out) noexcept {
  thread_local CreaturePipeline pipeline;
  thread_local std::vector<Render::Creature::CreatureRenderRequest>
      visible_requests;
  visible_requests.clear();

  auto const rows = prep.bodies.rows();
  auto const requests = prep.bodies.requests();
  visible_requests.reserve(rows.size());

  for (std::size_t i = 0; i < rows.size(); ++i) {
    if (rows[i].pass == RenderPassIntent::Shadow) {
      continue;
    }
    if (i < requests.size()) {
      visible_requests.push_back(requests[i]);
    }
  }

  if (!visible_requests.empty()) {
    (void)pipeline.submit_requests(visible_requests, out);
  }

  for (std::size_t i = 0; i < rows.size(); ++i) {
    if (rows[i].pass == RenderPassIntent::Shadow) {
      continue;
    }
  }

  for (auto &request : prep.post_body_draws) {
    if (request.pass_intent == RenderPassIntent::Shadow || !request.draw) {
      continue;
    }
    request.draw(out);
  }
}

void PreparedCreatureSubmitBatch::clear() noexcept { rows_.clear(); }

void PreparedCreatureSubmitBatch::reserve(std::size_t count) {
  rows_.reserve(count);
}

void PreparedCreatureSubmitBatch::add(PreparedCreatureRenderRow row) {
  rows_.push_back(std::move(row));
}

auto PreparedCreatureSubmitBatch::empty() const noexcept -> bool {
  return rows_.empty();
}

auto PreparedCreatureSubmitBatch::size() const noexcept -> std::size_t {
  return rows_.size();
}

auto PreparedCreatureSubmitBatch::rows() const noexcept
    -> std::span<const PreparedCreatureRenderRow> {
  return {rows_.data(), rows_.size()};
}

auto PreparedCreatureSubmitBatch::submit(
    Render::GL::ISubmitter &out) const noexcept -> SubmitStats {
  SubmitStats stats{};
  for (const auto &row : rows_) {
    submit_row_body(row, out, stats);
  }
  return stats;
}

} // namespace Render::Creature::Pipeline
