#pragma once

#include <memory>
#include <string>

namespace App::Platform {

class StatsSink {
public:
  virtual ~StatsSink() = default;

  virtual auto get_int(const std::string& api_name, int& value) -> bool = 0;
  virtual void set_int(const std::string& api_name, int value) = 0;

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
