#include "presentation_coverage.h"

#include <QString>

#include <array>

namespace Engine::Core {

namespace {

constexpr std::array<std::string_view, PresentationCoverage::k_count> k_names{
    "formation_move",
    "projectile_volley",
    "melee_contact",
    "killing_blow",
    "structure_destroyed",
    "weather_active",
    "fog_reveal",
    "selection_change",
    "production_order",
    "build_panel",
    "resource_flow"};

} // namespace

auto coverage_event_name(CoverageEvent event) noexcept -> std::string_view {
  const auto index = static_cast<std::size_t>(event);
  if (index >= k_names.size()) {
    return "unknown";
  }
  return k_names[index];
}

auto coverage_event_from_name(std::string_view name) noexcept -> CoverageEvent {
  for (std::size_t index = 0; index < k_names.size(); ++index) {
    if (k_names[index] == name) {
      return static_cast<CoverageEvent>(index);
    }
  }
  return CoverageEvent::_Count;
}

void PresentationCoverage::reset() noexcept {
  for (auto& value : m_counts) {
    value.store(0, std::memory_order_relaxed);
  }
}

auto PresentationCoverage::report() const -> QJsonObject {
  QJsonObject object;
  for (std::size_t index = 0; index < k_count; ++index) {
    const auto name = k_names[index];
    object.insert(QString::fromLatin1(name.data(), static_cast<qsizetype>(name.size())),
                  static_cast<qint64>(m_counts[index].load(std::memory_order_relaxed)));
  }
  return object;
}

auto presentation_coverage() noexcept -> PresentationCoverage& {
  static PresentationCoverage coverage;
  return coverage;
}

} // namespace Engine::Core
