#include "render/creature/pipeline/species_contract.h"

#include <array>
#include <cstddef>
#include <mutex>
#include <span>

namespace Render::Creature::Pipeline {

namespace {

constexpr std::size_t k_kind_slots = 8U;

auto contracts() noexcept -> std::array<const SpeciesContract*, k_kind_slots>& {
  static std::array<const SpeciesContract*, k_kind_slots> table{};
  return table;
}

auto slot(CreatureKind kind) noexcept -> std::size_t {
  return static_cast<std::size_t>(kind);
}

} // namespace

void register_species(const SpeciesContract& contract) {
  if (slot(contract.kind) < k_kind_slots) {
    contracts()[slot(contract.kind)] = &contract;
  }
}

auto registered_species() noexcept -> std::span<const SpeciesContract* const> {
  static std::once_flag registered;
  std::call_once(registered, [] { register_builtin_species(); });
  return contracts();
}

auto species_contract(CreatureKind kind) noexcept -> const SpeciesContract* {
  auto const table = registered_species();
  return slot(kind) < table.size() ? table[slot(kind)] : nullptr;
}

} // namespace Render::Creature::Pipeline
