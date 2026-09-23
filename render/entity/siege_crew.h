#pragma once

#include <array>
#include <cstdint>

namespace Render::GL {
struct DrawContext;
class ISubmitter;
struct SiegeMotion;

enum class SiegeCrewMode : std::uint8_t {
  Rest,
  Push,
  Load,
};

struct SiegeCrewMember {
  float x{0.0F};
  float z{0.0F};
  float yaw{0.0F};
  float walked{0.0F};
  std::uint16_t clip{0};
  float phase{0.0F};
  std::uint16_t previous_clip{0xFFFFU};
  float previous_phase{0.0F};
  float blend{0.0F};
  bool placed{false};
};

inline constexpr std::size_t k_max_siege_crew = 4;

struct SiegeCrewState {
  std::array<SiegeCrewMember, k_max_siege_crew> members{};
  SiegeCrewMode mode{SiegeCrewMode::Rest};
  float time{0.0F};
  float last_load_time{-100.0F};
  bool initialized{false};
};

struct SiegeCrewFrame {
  bool ballista{false};
  float engine_scale{1.0F};
  float travelled{0.0F};
  float movement{0.0F};
  bool loading{false};
  bool firing{false};
  float loading_time{0.0F};
  float loading_progress{0.0F};
};

[[nodiscard]] auto siege_crew_size(bool ballista) noexcept -> std::size_t;

void advance_siege_crew(SiegeCrewState& state, const SiegeCrewFrame& frame, float time);

void submit_siege_crew(const DrawContext& ctx,
                       ISubmitter& out,
                       const SiegeCrewState& state,
                       const SiegeCrewFrame& frame,
                       bool carthage);

} // namespace Render::GL
