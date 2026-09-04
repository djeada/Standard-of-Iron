

#include <QCoreApplication>

#include <algorithm>
#include <array>
#include <chrono>
#include <cinttypes>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "game/core/component.h"
#include "game/core/nav_profile.h"
#include "game/core/system_profiler.h"
#include "game/core/world.h"
#include "game/core/world_spatial_index.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/session/world_digest.h"
#include "game/systems/default_content.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "utils/percentile.h"

namespace {

using Engine::Core::AttackComponent;
using Engine::Core::MovementComponent;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Game::Session::ScopedSession;
using Game::Session::SessionContext;

constexpr int k_left_owner = 1;
constexpr int k_right_owner = 2;

constexpr float k_file_spacing = 4.0F;
constexpr float k_rank_spacing = 5.0F;
constexpr int k_default_ticks = 240;

constexpr int k_ranks_per_army = 8;

struct Options {
  std::vector<int> unit_counts{1000, 5000, 10000};
  int ticks{k_default_ticks};
  bool per_system{true};
  std::string json_path;
};

auto files_per_army(int units_per_side) -> int {
  return std::max(1, (units_per_side + k_ranks_per_army - 1) / k_ranks_per_army);
}

auto map_size_for(int units_per_side) -> int {
  const int width = static_cast<int>(
      static_cast<float>(files_per_army(units_per_side)) * k_file_spacing);
  return std::clamp(width + 64, 96, 4096);
}

auto peak_rss_kb() -> std::uint64_t {
#if defined(__linux__)
  std::FILE* status = std::fopen("/proc/self/status", "r");
  if (status == nullptr) {
    return 0;
  }
  char line[256];
  std::uint64_t value = 0;
  while (std::fgets(line, sizeof(line), status) != nullptr) {
    if (std::strncmp(line, "VmHWM:", 6) == 0) {
      value = std::strtoull(line + 6, nullptr, 10);
      break;
    }
  }
  std::fclose(status);
  return value;
#else
  return 0;
#endif
}

void muster(SessionContext& session,
            int owner_id,
            int count,
            float origin_x,
            float origin_z,
            float facing_z) {
  const int files = files_per_army(count);
  for (int index = 0; index < count; ++index) {
    const int rank = index / files;
    const int file = index % files;

    auto* entity = session.world().create_entity();
    auto* transform = entity->add_component<TransformComponent>();
    transform->position.x = origin_x + static_cast<float>(file) * k_file_spacing;
    transform->position.z =
        origin_z + static_cast<float>(rank) * k_rank_spacing * facing_z;

    auto* unit = entity->add_component<UnitComponent>(120, 120, 2.4F, 14.0F);
    unit->owner_id = owner_id;
    unit->spawn_type = (index % 4 == 0) ? Game::Units::SpawnType::Archer
                                        : Game::Units::SpawnType::Spearman;

    entity->add_component<MovementComponent>();
    entity->add_component<AttackComponent>(12.0F, 8.0F, 1.0F);
  }
}

struct Result {
  int units{0};
  int ticks{0};
  double total_ms{0.0};
  double mean_ms{0.0};
  double min_ms{0.0};
  double p50_ms{0.0};
  double p95_ms{0.0};
  double p99_ms{0.0};
  double max_ms{0.0};
  std::uint64_t digest{0};
  std::uint64_t peak_rss_kb{0};
  std::size_t entities{0};
  Engine::Core::SystemProfiler::TickSummary last_tick;
  std::string system_report;
  std::string navigation_report;

  double navigation_average_ms{0.0};
  double navigation_p95_ms{0.0};
  std::uint64_t navigation_ticks{0};
  std::array<std::uint64_t, Engine::Core::NavProfile::k_count> navigation_totals{};
};

auto run_scenario(int units_per_side, int ticks, bool per_system) -> Result {
  const int map_size = map_size_for(units_per_side);

  auto factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
  Game::Units::register_built_in_units(*factory);

  auto session = std::make_unique<SessionContext>();
  const ScopedSession scope(*session);

  Game::Systems::NavGrid::initialize(map_size, map_size);

  auto& owners = session->owners();
  owners.register_owner_with_id(k_left_owner, Game::Systems::OwnerType::Player, "left");
  owners.register_owner_with_id(k_right_owner, Game::Systems::OwnerType::AI, "right");
  owners.set_owner_team(k_left_owner, 1);
  owners.set_owner_team(k_right_owner, 2);
  Game::Systems::initialize_default_content(session->nations());
  Game::Systems::register_runtime_systems(session->world());

  Game::Map::MapDefinition map_definition;
  map_definition.grid.width = map_size;
  map_definition.grid.height = map_size;
  map_definition.grid.tile_size = 1.0F;
  session->terrain().initialize(map_definition);

  const auto centre = static_cast<float>(map_size) * 0.5F;
  const float line_width =
      static_cast<float>(files_per_army(units_per_side)) * k_file_spacing;
  const float line_left = centre - line_width * 0.5F;

  muster(*session, k_left_owner, units_per_side, line_left, centre - 8.0F, -1.0F);
  muster(*session, k_right_owner, units_per_side, line_left, centre + 8.0F, 1.0F);

  auto& world = session->world();
  auto& profiler = world.system_profiler();
  profiler.set_enabled(per_system);
  Engine::Core::nav_profile().clear();
  Engine::Core::nav_profile().set_enabled(per_system);

  Result result;
  result.units = units_per_side * 2;
  result.ticks = ticks;
  result.entities = world.entity_count();

  const auto step = static_cast<float>(session->clock().tick_seconds());

  world.update(step);

  std::vector<double> samples;
  samples.reserve(static_cast<std::size_t>(ticks));
  for (int tick = 0; tick < ticks; ++tick) {
    const auto started = std::chrono::steady_clock::now();
    world.update(step);
    const auto elapsed = std::chrono::steady_clock::now() - started;
    samples.push_back(std::chrono::duration<double, std::milli>(elapsed).count());
  }

  for (const double sample : samples) {
    result.total_ms += sample;
    result.max_ms = std::max(result.max_ms, sample);
  }
  result.mean_ms = result.total_ms / static_cast<double>(samples.size());

  const Utils::Stats::Distribution spread = Utils::Stats::distribution_of(samples);
  result.min_ms = *std::min_element(samples.begin(), samples.end());
  result.p50_ms = spread.p50;
  result.p95_ms = spread.p95;
  result.p99_ms = spread.p99;

  result.digest = Game::Session::session_digest(*session);
  result.peak_rss_kb = peak_rss_kb();
  result.last_tick = profiler.last_tick();
  {
    const Engine::Core::NavProfile& nav = Engine::Core::nav_profile();
    const auto nav_time = nav.tick_time_ms();
    result.navigation_average_ms = nav_time.average;
    result.navigation_p95_ms = nav_time.p95;
    result.navigation_ticks = nav.ticks();
    for (std::size_t i = 0; i < Engine::Core::NavProfile::k_count; ++i) {
      result.navigation_totals[i] = nav.total(static_cast<Engine::Core::NavCounter>(i));
    }
  }
  if (per_system) {
    result.system_report = profiler.format_report();
    result.navigation_report = Engine::Core::nav_profile().format_report();
  }
  Engine::Core::nav_profile().set_enabled(false);
  return result;
}

void print_result(const Result& result) {
  std::printf("\n=== %d units (%zu entities), %d ticks "
              "=======================================\n",
              result.units,
              result.entities,
              result.ticks);
  std::printf("simulation   min %7.3f   mean %7.3f   p50 %7.3f   p95 %7.3f   "
              "p99 %7.3f   max %7.3f ms\n",
              result.min_ms,
              result.mean_ms,
              result.p50_ms,
              result.p95_ms,
              result.p99_ms,
              result.max_ms);
  std::printf("headroom     %6.1f%% of a 16.67 ms frame at the mean, %.1f%% at the "
              "fastest tick\n",
              result.mean_ms / 16.67 * 100.0,
              result.min_ms / 16.67 * 100.0);
  std::printf("memory       peak RSS %" PRIu64 " MB\n", result.peak_rss_kb / 1024U);
  std::printf("digest       %016" PRIx64 "\n", result.digest);

  if (!result.system_report.empty()) {
    std::printf("\n%s", result.system_report.c_str());
  }
  if (!result.navigation_report.empty()) {
    std::printf("\n%s", result.navigation_report.c_str());
  }
}

void write_json(const std::string& path, const std::vector<Result>& results) {
  std::FILE* out = std::fopen(path.c_str(), "w");
  if (out == nullptr) {
    std::fprintf(stderr, "sim_benchmark: cannot write %s\n", path.c_str());
    return;
  }
  std::fprintf(out, "{\n  \"scenarios\": [\n");
  for (std::size_t i = 0; i < results.size(); ++i) {
    const Result& result = results[i];
    std::fprintf(out,
                 "    {\n"
                 "      \"units\": %d,\n"
                 "      \"entities\": %zu,\n"
                 "      \"ticks\": %d,\n"
                 "      \"digest\": \"%016" PRIx64 "\",\n"
                 "      \"peak_rss_kb\": %" PRIu64 ",\n"
                 "      \"tick_ms\": {\"average\": %.6f, \"min\": %.6f, "
                 "\"p50\": %.6f, \"p95\": %.6f, \"p99\": %.6f, \"max\": %.6f},\n"
                 "      \"navigation\": {\n"
                 "        \"ticks\": %" PRIu64 ",\n"
                 "        \"average_ms\": %.6f,\n"
                 "        \"p95_ms\": %.6f,\n"
                 "        \"total\": {\n",
                 result.units,
                 result.entities,
                 result.ticks,
                 result.digest,
                 result.peak_rss_kb,
                 result.mean_ms,
                 result.min_ms,
                 result.p50_ms,
                 result.p95_ms,
                 result.p99_ms,
                 result.max_ms,
                 result.navigation_ticks,
                 result.navigation_average_ms,
                 result.navigation_p95_ms);
    for (std::size_t c = 0; c < Engine::Core::NavProfile::k_count; ++c) {
      const auto counter = static_cast<Engine::Core::NavCounter>(c);
      const std::string_view name = Engine::Core::nav_counter_name(counter);
      std::fprintf(out,
                   "          \"%.*s\": %" PRIu64 "%s\n",
                   static_cast<int>(name.size()),
                   name.data(),
                   result.navigation_totals[c],
                   c + 1U == Engine::Core::NavProfile::k_count ? "" : ",");
    }
    std::fprintf(
        out, "        }\n      }\n    }%s\n", i + 1U == results.size() ? "" : ",");
  }
  std::fprintf(out, "  ]\n}\n");
  std::fclose(out);
}

auto parse_options(int argc, char** argv, Options& options) -> bool {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--units" && i + 1 < argc) {
      options.unit_counts = {std::atoi(argv[++i])};
    } else if (arg == "--ticks" && i + 1 < argc) {
      options.ticks = std::atoi(argv[++i]);
    } else if (arg == "--no-systems") {
      options.per_system = false;
    } else if (arg == "--json" && i + 1 < argc) {
      options.json_path = argv[++i];
    } else if (arg == "--help" || arg == "-h") {
      std::printf("usage: sim_benchmark [--units N] [--ticks N] [--no-systems] "
                  "[--json PATH]\n");
      return false;
    } else {
      std::fprintf(stderr, "sim_benchmark: unknown argument '%s'\n", arg.c_str());
      return false;
    }
  }
  return true;
}

} // namespace

auto main(int argc, char** argv) -> int {
  QCoreApplication app(argc, argv);

  std::setlocale(LC_NUMERIC, "C");

  Options options;
  if (!parse_options(argc, argv, options)) {
    return 1;
  }

  std::printf("Standard of Iron -- simulation benchmark\n");
  std::printf("%d ticks per scenario, fixed 1/60 s step\n", options.ticks);

  std::vector<Result> results;
  results.reserve(options.unit_counts.size());
  for (const int units : options.unit_counts) {
    results.push_back(run_scenario(units / 2, options.ticks, options.per_system));
    print_result(results.back());
  }

  if (!options.json_path.empty()) {
    write_json(options.json_path, results);
  }

  return 0;
}
