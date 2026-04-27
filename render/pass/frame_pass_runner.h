

#pragma once

#include "i_pass.h"

#include <memory>
#include <string_view>
#include <vector>

namespace Render::Pass {

struct FrameContext;

class FramePassRunner {
public:
  FramePassRunner() = default;
  ~FramePassRunner() = default;

  FramePassRunner(const FramePassRunner &) = delete;
  auto operator=(const FramePassRunner &) -> FramePassRunner & = delete;
  FramePassRunner(FramePassRunner &&) noexcept = default;
  auto operator=(FramePassRunner &&) noexcept -> FramePassRunner & = default;

  void add(std::unique_ptr<IFramePass> pass);

  void add_non_owning(IFramePass &pass);

  void execute(FrameContext &ctx);

  [[nodiscard]] auto size() const noexcept -> std::size_t;
  [[nodiscard]] auto
  pass_name(std::size_t i) const noexcept -> std::string_view;

  void clear() noexcept;

private:
  struct Slot {
    std::unique_ptr<IFramePass> owned;
    IFramePass *ref{nullptr};
  };
  std::vector<Slot> m_slots;
};

} // namespace Render::Pass
