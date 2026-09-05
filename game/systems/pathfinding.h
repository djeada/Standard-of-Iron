#pragma once

#include <QVector3D>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "nav_grid_types.h"

namespace Game::Map {
class TerrainService;
}

namespace Game::Systems {

class BuildingCollisionRegistry;
struct BuildingFootprint;

class Pathfinding {
public:
  enum class CellValue : std::uint8_t {
    Walkable = 0,
    Blocked = 1,
    Tree = 2,
    Boulder = 3,
    IronOre = 4,

    Forest = 5,
  };

  enum class Passability : std::uint8_t {
    Light = 0,
    Heavy = 1,
  };

  class NavigationGrid {
  public:
    NavigationGrid() = default;
    NavigationGrid(int width, int height);

    void fill(CellValue value);
    [[nodiscard]] auto in_bounds(int x, int y) const -> bool;
    [[nodiscard]] auto get(int x, int y) const -> CellValue;
    void set(int x, int y, CellValue value);

    [[nodiscard]] auto at_unchecked(int x, int y) const noexcept -> CellValue {
      return static_cast<CellValue>(
          m_cells[static_cast<std::size_t>((y * m_width) + x)]);
    }

  private:
    int m_width{0};
    int m_height{0};
    std::vector<std::uint8_t> m_cells;
  };

  Pathfinding(int width, int height);
  ~Pathfinding();

  void set_grid_offset(float offset_x, float offset_z);

  auto get_grid_offset_x() const -> float { return m_grid_offset_x; }
  auto get_grid_offset_z() const -> float { return m_grid_offset_z; }

  [[nodiscard]] auto grid_cell_size() const -> float { return m_grid_cell_size; }

  auto world_to_grid(float world_x, float world_z) const -> Point;
  auto grid_to_world(const Point& grid_pos) const -> QVector3D;

  [[nodiscard]] auto cells_covering(float center_x,
                                    float center_z,
                                    float half_x,
                                    float half_z) const -> CellRange;
  [[nodiscard]] auto cell_range_world_bounds(const CellRange& range) const -> WorldRect;

  void set_obstacle(int x, int y, bool is_obstacle);
  auto
  is_walkable(int x, int y, Passability passability = Passability::Light) const -> bool;
  auto is_tree(int x, int y) const -> bool;
  auto is_forest(int x, int y) const -> bool;
  auto is_boulder(int x, int y) const -> bool;
  auto is_iron_ore(int x, int y) const -> bool;
  auto cell_value(int x, int y) const -> CellValue;

  [[nodiscard]] auto is_terrain_walkable(int x, int y) const -> bool;
  auto is_world_position_walkable(const QVector3D& world_position,
                                  Passability passability = Passability::Light,
                                  float clearance_radius = 0.0F) const -> bool;

  static constexpr float k_min_traversal_clearance = 0.1F;
  static constexpr float k_max_traversal_clearance = 0.45F;

  [[nodiscard]] static auto traversal_clearance_for_body(float body_radius) -> float {
    return std::clamp(
        body_radius, k_min_traversal_clearance, k_max_traversal_clearance);
  }
  auto is_world_segment_walkable(const QVector3D& from,
                                 const QVector3D& to,
                                 Passability passability = Passability::Light,
                                 float clearance_radius = 0.0F) const -> bool;
  auto path_waypoint_world_position(const Point& path_cell) const -> QVector3D;

  void update_navigation_grid();

  void mark_navigation_grid_dirty();

  void mark_region_dirty(int min_x, int max_x, int min_z, int max_z);

  void
  mark_building_region_dirty(float center_x, float center_z, float width, float depth);

  void mark_obstruction_released();

  void mark_obstruction_released_at(float center_x, float center_z);

  [[nodiscard]] auto obstruction_revision() const -> std::uint64_t;

  struct ObstructionRelease {
    QVector3D center;
    bool located{false};
  };

  [[nodiscard]] auto last_obstruction_release() const -> ObstructionRelease;

  auto find_path(const Point& start,
                 const Point& end,
                 Passability passability = Passability::Light,
                 float clearance_radius = 0.0F) -> std::vector<Point>;

  static constexpr std::uint32_t k_unreachable_region = 0U;

  [[nodiscard]] auto region_of(const Point& cell,
                               Passability passability) -> std::uint32_t;

  [[nodiscard]] auto can_reach(const Point& start,
                               const Point& end,
                               Passability passability = Passability::Light) -> bool;

  [[nodiscard]] auto navigation_revision() const -> std::uint64_t {
    return m_navigation_revision.load(std::memory_order_acquire);
  }

  static auto
  find_nearest_walkable_point(const Point& point,
                              int max_search_radius,
                              const Pathfinding& pathfinder,
                              Passability passability = Passability::Light) -> Point;

private:
  auto find_path_internal(const Point& start,
                          const Point& end,
                          Passability passability,
                          float clearance_radius) -> std::vector<Point>;
  auto resolve_walkable_endpoint(const Point& requested,
                                 Point& resolved,
                                 Passability passability,
                                 float clearance_radius) const -> bool;
  void apply_forest_cells(int min_x, int max_x, int min_z, int max_z);
  void rebuild_forest_index();
  void force_map_passage_cells_walkable(int min_x, int max_x, int min_z, int max_z);
  void force_navigation_passages_walkable(int min_x, int max_x, int min_z, int max_z);
  void apply_gate_blocker_cells(int min_x, int max_x, int min_z, int max_z);

  static auto calculate_heuristic(const Point& a, const Point& b) -> int;

  [[nodiscard]] auto
  clamp_to_grid(int& min_x, int& max_x, int& min_z, int& max_z) const -> bool;

  [[nodiscard]] auto clearance_penalty(int x, int y) const -> int;
  void rebuild_clearance(int min_x, int max_x, int min_z, int max_z);

  void rebuild_elevation(int min_x, int max_x, int min_z, int max_z);
  [[nodiscard]] auto climb_penalty(int from_index, int to_index) const -> int;

  static constexpr int k_straight_step_cost = 10;
  static constexpr int k_diagonal_step_cost = 14;

  static constexpr int k_edge_step_penalty = 1;

  static constexpr int k_clearance_radius = 3;
  static constexpr float k_max_body_clearance = 1.5F;
  static constexpr int k_clearance_ring_penalty = 4;
  static constexpr int k_clearance_avoid_weight = 6;
  static constexpr int k_turn_penalty = 1;

  static constexpr float k_climb_noise_floor_metres = 0.05F;
  static constexpr int k_climb_cost_per_metre = 40;
  static constexpr int k_max_climb_penalty = 400;

  static constexpr int k_heuristic_weight_numerator = 12;
  static constexpr int k_heuristic_weight_denominator = 10;

  struct SearchBuffers;
  static auto
  search_buffers_by_grid() -> std::unordered_map<const Pathfinding*, SearchBuffers>&;
  static auto search_buffers_for(const Pathfinding* pathfinding) -> SearchBuffers&;
  static void release_search_buffers(const Pathfinding* pathfinding);
  void ensure_working_buffers(SearchBuffers& buffers) const;
  static auto next_generation(SearchBuffers& buffers) -> std::uint32_t;
  static void reset_generations(SearchBuffers& buffers);

  auto to_index(int x, int y) const -> int { return y * m_width + x; }
  auto to_index(const Point& p) const -> int { return to_index(p.x, p.y); }
  auto to_point(int index) const -> Point { return {index % m_width, index / m_width}; }

  [[nodiscard]] auto in_bounds(int x, int y) const noexcept -> bool {
    return x >= 0 && x < m_width && y >= 0 && y < m_height;
  }

  [[nodiscard]] auto cell_is(int x, int y, CellValue value) const noexcept -> bool {
    return in_bounds(x, y) && m_navigation_grid.at_unchecked(x, y) == value;
  }

  static auto
  is_closed(const SearchBuffers& buffers, int index, std::uint32_t generation) -> bool;
  static void set_closed(SearchBuffers& buffers, int index, std::uint32_t generation);

  static auto
  get_g_cost(const SearchBuffers& buffers, int index, std::uint32_t generation) -> int;
  static void
  set_g_cost(SearchBuffers& buffers, int index, std::uint32_t generation, int cost);

  static auto
  has_parent(const SearchBuffers& buffers, int index, std::uint32_t generation) -> bool;
  static auto
  get_parent(const SearchBuffers& buffers, int index, std::uint32_t generation) -> int;
  static void set_parent(SearchBuffers& buffers,
                         int index,
                         std::uint32_t generation,
                         int parent_index);

  auto collect_neighbors(const Point& point,
                         std::array<Point, 8>& buffer,
                         Passability passability) const -> std::size_t;
  void build_path(int start_index,
                  int end_index,
                  std::uint32_t generation,
                  int expected_length,
                  const SearchBuffers& buffers,
                  std::vector<Point>& out_path) const;

  struct QueueNode {
    int index;
    int f_cost;
    int g_cost;
  };

  struct SearchBuffers {
    std::vector<std::uint32_t> closed_generation;
    std::vector<std::uint32_t> g_cost_generation;
    std::vector<int> g_cost_values;
    std::vector<std::uint32_t> parent_generation;
    std::vector<int> parent_values;
    std::vector<QueueNode> open_heap;
    std::uint32_t generation_counter{0};
  };

  struct PathCacheKey {
    int start_x;
    int start_y;
    int end_x;
    int end_y;
    Passability passability;
    int clearance_quarters;

    auto operator==(const PathCacheKey&) const -> bool = default;
  };

  struct PathCacheKeyHash {
    auto operator()(const PathCacheKey& key) const noexcept -> std::size_t;
  };

  static auto heap_less(const QueueNode& lhs, const QueueNode& rhs) -> bool;
  static void push_open_node(SearchBuffers& buffers, const QueueNode& node);
  static auto pop_open_node(SearchBuffers& buffers) -> QueueNode;

  struct RegionMap {
    std::vector<std::uint32_t> labels;
    std::uint64_t revision{0};
    bool built{false};
  };

  struct NavChange {
    std::uint64_t revision{0};
    int min_x{0};
    int max_x{0};
    int min_z{0};
    int max_z{0};
  };

  struct CachedPath {
    std::vector<Point> path;
    std::uint64_t last_used{0};
    int min_x{0};
    int max_x{0};
    int min_z{0};
    int max_z{0};
  };

  static constexpr std::size_t k_max_tracked_nav_changes = 64U;
  static constexpr std::size_t k_max_cached_paths = 256U;

  void note_navigation_change(int min_x, int max_x, int min_z, int max_z);
  void drop_paths_crossing_changes(std::uint64_t from_revision,
                                   std::uint64_t to_revision);
  void evict_cold_paths();

  static constexpr std::size_t k_passability_count = 2U;

  void region_labels(const Point& first,
                     const Point& second,
                     Passability passability,
                     std::uint32_t& first_label,
                     std::uint32_t& second_label);
  void rebuild_region_map(RegionMap& map, Passability passability) const;
  [[nodiscard]] auto label_at(const RegionMap& map,
                              const Point& cell) const -> std::uint32_t;

  auto process_dirty_regions() -> DirtyRegion;

  void update_region(int min_x, int max_x, int min_z, int max_z);
  void apply_building_cells(
      const BuildingFootprint& building, int min_x, int max_x, int min_z, int max_z);
  void apply_resource_prop_cells(int min_x, int max_x, int min_z, int max_z);
  void rebuild_world_prop_index();

  int m_width, m_height;
  NavigationGrid m_navigation_grid;
  Game::Map::TerrainService* m_terrain{nullptr};

  float m_grid_cell_size{1.0F};
  float m_grid_offset_x{0.0F}, m_grid_offset_z{0.0F};
  std::atomic<bool> m_navigation_grid_dirty;
  mutable std::shared_mutex m_navigation_mutex;
  mutable std::mutex m_path_cache_mutex;

  std::mutex m_dirty_mutex;
  std::vector<DirtyRegion> m_dirty_regions;
  bool m_full_update_required{true};
  std::atomic<std::uint64_t> m_applied_world_props_revision{0};
  std::atomic<std::uint64_t> m_obstruction_revision{0};
  std::atomic<float> m_obstruction_center_x{0.0F};
  std::atomic<float> m_obstruction_center_z{0.0F};
  std::atomic<bool> m_obstruction_center_located{false};
  std::atomic<std::uint64_t> m_applied_terrain_topology_revision{0};
  std::unordered_map<int, CellValue> m_world_prop_cells;
  std::vector<bool> m_forest_cells;
  std::vector<std::uint8_t> m_clearance_penalty;
  std::vector<float> m_cell_height;
  std::atomic<std::uint64_t> m_navigation_revision{1};
  std::uint64_t m_path_cache_revision{0};
  std::uint64_t m_path_cache_clock{0};
  std::unordered_map<PathCacheKey, CachedPath, PathCacheKeyHash> m_path_cache;
  std::mutex m_nav_change_mutex;
  std::vector<NavChange> m_nav_changes;
  mutable std::mutex m_region_mutex;
  std::array<RegionMap, k_passability_count> m_region_maps;
};

} // namespace Game::Systems
