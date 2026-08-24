#pragma once

#include <memory>
#include <mutex>
#include <utility>

namespace App::Core {

template <typename T>
class Published {
public:
  using Snapshot = std::shared_ptr<const T>;

  void publish(T value) {
    auto next = std::make_shared<const T>(std::move(value));
    const std::lock_guard<std::mutex> guard(m_mutex);
    m_value.swap(next);
  }

  [[nodiscard]] auto read() const -> Snapshot {
    const std::lock_guard<std::mutex> guard(m_mutex);
    return m_value;
  }

  void clear() {
    const std::lock_guard<std::mutex> guard(m_mutex);
    m_value.reset();
  }

private:
  mutable std::mutex m_mutex;
  Snapshot m_value;
};

} // namespace App::Core
