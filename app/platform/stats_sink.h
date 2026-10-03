#pragma once

#include <memory>
#include <string>

namespace App::Platform {

// Durable integer statistics held by the platform (Steam User Stats). The API
// names are the stable identifiers configured in Steamworks; never rename one
// after release.
class StatsSink {
public:
  virtual ~StatsSink() = default;
  // False when the platform is unavailable or does not know the stat.
  virtual auto get_int(const std::string& api_name, int& value) -> bool = 0;
  virtual void set_int(const std::string& api_name, int value) = 0;
  // Persist everything set since the last store. Called once per match.
  virtual void store() = 0;
};

class NullStatsSink final : public StatsSink {
public:
  auto get_int(const std::string&, int&) -> bool override { return false; }
  void set_int(const std::string&, int) override {}
  void store() override {}
};

auto make_platform_stats_sink() -> std::unique_ptr<StatsSink>;

} // namespace App::Platform
