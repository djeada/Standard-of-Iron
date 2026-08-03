#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace Game::Audio {

struct AudioCommand {
  enum class Type : std::uint8_t {
    None,
    Play,
    Stop,
    Pause,
    Resume,
    SetVolume,
    StopAll,
    SetMasterVolume,
    PlaySound,
    StopSound,
    ReleaseTrack,
  };

  Type type = Type::None;
  bool loop = false;
  std::int16_t channel = -1;
  std::int16_t track = -1;
  float volume = 0.0F;
  std::uint32_t fade_samples = 0;
};

template <std::size_t CAPACITY>
class CommandRing {
public:
  static constexpr std::int64_t REJECTED = -1;

  auto push(const AudioCommand& command) -> std::int64_t {
    std::uint64_t claimed = m_write.load(std::memory_order_relaxed);
    for (;;) {
      if (claimed - m_read.load(std::memory_order_acquire) >= CAPACITY) {
        m_dropped.fetch_add(1, std::memory_order_relaxed);
        return REJECTED;
      }
      if (m_write.compare_exchange_weak(claimed,
                                        claimed + 1,
                                        std::memory_order_acq_rel,
                                        std::memory_order_relaxed)) {
        break;
      }
    }
    Slot& slot = m_slots[claimed % CAPACITY];
    slot.command = command;
    slot.ready.store(true, std::memory_order_release);
    return static_cast<std::int64_t>(claimed);
  }

  template <typename Apply>
  void drain(Apply&& apply) {
    std::uint64_t read = m_read.load(std::memory_order_relaxed);
    const std::uint64_t write = m_write.load(std::memory_order_acquire);
    while (read < write) {
      Slot& slot = m_slots[read % CAPACITY];
      if (!slot.ready.load(std::memory_order_acquire)) {
        break;
      }
      const AudioCommand command = slot.command;
      slot.ready.store(false, std::memory_order_relaxed);
      ++read;
      m_read.store(read, std::memory_order_release);
      apply(command);
    }
  }

  [[nodiscard]] auto processed() const -> std::uint64_t {
    return m_read.load(std::memory_order_acquire);
  }

  [[nodiscard]] auto pending() const -> std::uint64_t {
    return m_write.load(std::memory_order_acquire) -
           m_read.load(std::memory_order_acquire);
  }

  [[nodiscard]] auto dropped() const -> std::uint64_t {
    return m_dropped.load(std::memory_order_relaxed);
  }

private:
  struct Slot {
    AudioCommand command;
    std::atomic<bool> ready{false};
  };

  std::array<Slot, CAPACITY> m_slots{};
  std::atomic<std::uint64_t> m_write{0};
  std::atomic<std::uint64_t> m_read{0};
  std::atomic<std::uint64_t> m_dropped{0};
};

} // namespace Game::Audio
