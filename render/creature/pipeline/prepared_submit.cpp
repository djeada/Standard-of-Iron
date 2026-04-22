#include "prepared_submit.h"

#include "../../submitter.h"

#include <span>
#include <vector>

namespace Render::Creature::Pipeline {

void PreparedCreatureSubmitBatch::clear() noexcept {
  rows_.clear();
  legacy_contexts_.clear();
}

void PreparedCreatureSubmitBatch::reserve(std::size_t count) {
  rows_.reserve(count);
}

void PreparedCreatureSubmitBatch::add(PreparedCreatureRenderRow row) {
  rows_.push_back(row);
}

void PreparedCreatureSubmitBatch::add_with_legacy_context(
    PreparedCreatureRenderRow row, const Render::GL::DrawContext &legacy_ctx) {
  legacy_contexts_.push_back(legacy_ctx);
  row.legacy_ctx = &legacy_contexts_.back();
  rows_.push_back(row);
}

auto PreparedCreatureSubmitBatch::empty() const noexcept -> bool {
  return rows_.empty();
}

auto PreparedCreatureSubmitBatch::size() const noexcept -> std::size_t {
  return rows_.size();
}

auto PreparedCreatureSubmitBatch::rows() const noexcept
    -> std::span<const PreparedCreatureRenderRow> {
  return rows_;
}

auto PreparedCreatureSubmitBatch::submit(
    Render::GL::ISubmitter &out) const noexcept -> SubmitStats {
  return submit_prepared_creatures(rows_, out);
}

auto submit_prepared_creatures(std::span<const PreparedCreatureRenderRow> rows,
                               Render::GL::ISubmitter &out) noexcept
    -> SubmitStats {
  thread_local CreaturePipeline pipeline;
  thread_local CreatureFrame frame;
  thread_local std::vector<UnitVisualSpec> specs;

  frame.clear();
  specs.clear();
  frame.reserve(rows.size());
  specs.reserve(rows.size());

  for (const PreparedCreatureRenderRow &row : rows) {
    
    
    if (row.pass == RenderPassIntent::Shadow) {
      continue;
    }
    const SpecId spec_id = static_cast<SpecId>(specs.size());
    specs.push_back(row.spec);
    append_prepared_row(frame, row, spec_id);
  }

  FrameContext fctx{};
  return pipeline.submit(
      fctx, std::span<const UnitVisualSpec>{specs.data(), specs.size()}, frame,
      out);
}

} // namespace Render::Creature::Pipeline
