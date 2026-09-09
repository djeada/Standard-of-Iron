#pragma once

#include <QJsonObject>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Engine::Core {

enum class CoverageEvent : std::uint8_t {
  FormationMove = 0,
  ProjectileVolley,
  MeleeContact,
  KillingBlow,
  StructureDestroyed,
  WeatherActive,
  FogReveal,
  SelectionChange,
  ProductionOrder,
  BuildPanel,
  ResourceFlow,
  _Count
};

[[nodiscard]] auto
coverage_event_name(CoverageEvent event) noexcept -> std::string_view;

[[nodiscard]] auto
coverage_event_from_name(std::string_view name) noexcept -> CoverageEvent;

class PresentationCoverage {
public:
  static constexpr std::size_t k_count =
      static_cast<std::size_t>(CoverageEvent::_Count);

  void note(CoverageEvent event, std::uint64_t amount = 1) noexcept {
    m_counts[static_cast<std::size_t>(event)].fetch_add(amount,
                                                        std::memory_order_relaxed);
  }

  [[nodiscard]] auto count(CoverageEvent event) const noexcept -> std::uint64_t {
    return m_counts[static_cast<std::size_t>(event)].load(std::memory_order_relaxed);
  }

  void reset() noexcept;

  [[nodiscard]] auto report() const -> QJsonObject;

private:
  std::array<std::atomic<std::uint64_t>, k_count> m_counts{};
};

[[nodiscard]] auto presentation_coverage() noexcept -> PresentationCoverage&;

inline void note_coverage(CoverageEvent event, std::uint64_t amount = 1) noexcept {
  presentation_coverage().note(event, amount);
}

} // namespace Engine::Core
