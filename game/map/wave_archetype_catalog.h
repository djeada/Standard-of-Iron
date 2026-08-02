#pragma once

#include <QString>

#include <vector>

#include "mission_definition.h"

namespace Game::Mission {

struct WaveArchetype {
  QString id;
  QString label;
  std::vector<WaveComposition> composition;
};

class WaveArchetypeCatalog {
public:
  static auto instance() -> WaveArchetypeCatalog&;

  void reload();

  [[nodiscard]] auto find(const QString& id) const -> const WaveArchetype*;
  [[nodiscard]] auto ids() const -> std::vector<QString>;

  [[nodiscard]] auto expand(const QString& id,
                            float strength) const -> std::vector<WaveComposition>;

private:
  WaveArchetypeCatalog();

  void load_built_ins();
  void apply_overlay();

  std::vector<WaveArchetype> m_archetypes;
};

[[nodiscard]] auto difficulty_strength_multiplier(const QString& difficulty) -> float;

[[nodiscard]] auto
scale_wave_composition(const std::vector<WaveComposition>& source,
                       float multiplier) -> std::vector<WaveComposition>;

} // namespace Game::Mission
