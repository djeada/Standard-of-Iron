#include "command_log.h"

#include <algorithm>
#include <limits>
#include <ranges>
#include <utility>

namespace Game::Command {

namespace {
constexpr std::uint64_t k_retained_ticks = 600;
}

auto CommandLog::submit(Source source, int owner, LogPayload payload) -> bool {
  const std::lock_guard lock(m_mutex);
  if (m_follower || m_ended) {
    ++m_dropped_inputs;
    return false;
  }
  append_locked({m_next_input_tick + m_input_delay,
                 m_next_seq++,
                 owner,
                 source,
                 std::move(payload)});
  return true;
}

void CommandLog::append(std::uint64_t tick,
                        Source source,
                        int owner,
                        LogPayload payload) {
  const std::lock_guard lock(m_mutex);
  if (!m_follower && !m_ended && tick > m_completed_tick) {
    append_locked({tick, m_next_seq++, owner, source, std::move(payload)});
  }
}

auto CommandLog::append_replicated(const LogEntry& entry) -> bool {
  const std::lock_guard lock(m_mutex);
  const bool match_end = std::holds_alternative<MatchEnd>(entry.payload);
  const bool tick_complete = std::holds_alternative<TickComplete>(entry.payload);
  if (!m_follower || m_ended || entry.seq == 0 ||
      entry.seq == std::numeric_limits<std::uint64_t>::max() ||
      (match_end
           ? entry.tick != m_replicated_through
           : entry.tick < m_next_input_tick || entry.tick <= m_replicated_through) ||
      (tick_complete && entry.tick != m_replicated_through + 1)) {
    return false;
  }
  const auto found = m_ticks.find(entry.tick);
  if (found != m_ticks.end() && !found->second.empty() &&
      found->second.back()->seq >= entry.seq) {
    return false;
  }
  m_next_seq = std::max(m_next_seq, entry.seq + 1);
  append_locked(entry);
  if (tick_complete) {
    m_replicated_through = entry.tick;
  }
  m_ended = match_end;
  return true;
}

void CommandLog::append_locked(LogEntry entry) {
  auto record = std::make_shared<const LogEntry>(std::move(entry));
  m_ticks[record->tick].push_back(record);
  if (m_write) {
    m_write(*record);
  }
}

auto CommandLog::entries_at(std::uint64_t tick) const -> Entries {
  const std::lock_guard lock(m_mutex);
  const auto found = m_ticks.find(tick);
  return found != m_ticks.end() ? found->second : Entries{};
}

auto CommandLog::seal_inputs(std::uint64_t tick) -> Entries {
  const std::lock_guard lock(m_mutex);
  if (tick < m_next_input_tick) {
    return {};
  }
  m_next_input_tick = tick + 1;
  flush_locked();
  const auto found = m_ticks.find(tick);
  return found != m_ticks.end() ? found->second : Entries{};
}

void CommandLog::complete_tick(std::uint64_t tick) {
  const std::lock_guard lock(m_mutex);
  if (tick <= m_completed_tick) {
    return;
  }
  m_next_input_tick = std::max(m_next_input_tick, tick + 1);
  m_completed_tick = tick;
  if (!m_follower && !m_ended) {
    append_locked({tick, m_next_seq++, 0, Source::Script, TickComplete{}});
  }
  flush_locked();
  if (tick > k_retained_ticks) {
    m_ticks.erase(m_ticks.begin(), m_ticks.upper_bound(tick - k_retained_ticks));
  }
}

void CommandLog::finish_match() {
  const std::lock_guard lock(m_mutex);
  if (!m_follower && !m_ended) {
    append_locked({m_completed_tick, m_next_seq++, 0, Source::Script, MatchEnd{}});
    m_ended = true;
  }
  flush_locked();
}

void CommandLog::set_input_delay(std::uint32_t ticks) {
  const std::lock_guard lock(m_mutex);
  m_input_delay = ticks;
}

void CommandLog::set_follower(bool follower) {
  const std::lock_guard lock(m_mutex);
  m_follower = follower;
}

auto CommandLog::follower() const -> bool {
  const std::lock_guard lock(m_mutex);
  return m_follower;
}

auto CommandLog::available_tick() const -> std::uint64_t {
  const std::lock_guard lock(m_mutex);
  return m_replicated_through;
}

auto CommandLog::pending_commands() const -> std::size_t {
  const std::lock_guard lock(m_mutex);
  std::size_t count = 0;
  for (auto it = m_ticks.lower_bound(m_next_input_tick); it != m_ticks.end(); ++it) {
    count += std::count_if(it->second.begin(), it->second.end(), [](const auto& entry) {
      return std::holds_alternative<Payload>(entry->payload);
    });
  }
  return count;
}

auto CommandLog::dropped_inputs() const -> std::uint64_t {
  const std::lock_guard lock(m_mutex);
  return m_dropped_inputs;
}

void CommandLog::reset(std::uint64_t completed_tick) {
  const std::lock_guard lock(m_mutex);
  m_ticks.clear();
  m_completed_tick = completed_tick;
  m_replicated_through = completed_tick;
  m_next_input_tick = completed_tick + 1;
  m_next_seq = 1;
  m_dropped_inputs = 0;
  m_ended = false;
}

void CommandLog::attach_writer(Writer write, Flusher flush) {
  const std::lock_guard lock(m_mutex);
  m_write = std::move(write);
  m_flush = std::move(flush);
  Entries existing;
  for (const auto& [tick, records] : m_ticks) {
    existing.insert(existing.end(), records.begin(), records.end());
  }
  std::ranges::sort(existing, {}, [](const auto& entry) { return entry->seq; });
  for (const auto& entry : existing) {
    m_write(*entry);
  }
  flush_locked();
}

void CommandLog::detach_writer() {
  const std::lock_guard lock(m_mutex);
  flush_locked();
  m_write = {};
  m_flush = {};
}

void CommandLog::flush() {
  const std::lock_guard lock(m_mutex);
  flush_locked();
}

void CommandLog::flush_locked() {
  if (m_flush) {
    m_flush();
  }
}

} // namespace Game::Command
