#pragma once

#include <cstdint>
#include <vector>

namespace Game::Wildlife {

struct ThreatSource {
  float x{0.0F};
  float z{0.0F};
  float strength{1.0F};
  bool civilian{false};
};

struct ThreatQuery {
  bool found{false};
  float x{0.0F};
  float z{0.0F};
  float distance{0.0F};
  float strength{0.0F};
  bool civilian{false};
};

class ThreatField {
public:
  void clear();
  void add(const ThreatSource& source);
  void finalize();

  [[nodiscard]] auto empty() const noexcept -> bool { return m_sources.empty(); }
  [[nodiscard]] auto size() const noexcept -> std::size_t { return m_sources.size(); }
  [[nodiscard]] auto sources() const noexcept -> const std::vector<ThreatSource>& {
    return m_sources;
  }

  [[nodiscard]] auto nearest(float world_x,
                             float world_z,
                             float radius,
                             bool civilians_only = false) const -> ThreatQuery;

  [[nodiscard]] auto
  strength_within(float world_x, float world_z, float radius) const -> float;

private:
  struct Cell {
    std::uint32_t first{0U};
    std::uint32_t count{0U};
  };

  [[nodiscard]] auto cell_index(int cell_x, int cell_z) const noexcept -> std::size_t;

  template <typename Visitor>
  void visit(float world_x, float world_z, float radius, Visitor&& visitor) const;

  std::vector<ThreatSource> m_sources;
  std::vector<std::uint32_t> m_order;
  std::vector<Cell> m_cells;
  float m_min_x{0.0F};
  float m_min_z{0.0F};
  int m_width{0};
  int m_height{0};
  bool m_indexed{false};
};

} // namespace Game::Wildlife
