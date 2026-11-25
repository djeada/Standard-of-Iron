#pragma once

#include <QByteArray>

#include <cstdint>
#include <optional>

namespace Game::Session {
class SessionContext;
}

namespace Game::Save {

[[nodiscard]] auto
capture_replay_checkpoint(Game::Session::SessionContext& session) -> QByteArray;

// Use a fresh session with the match systems already configured.
[[nodiscard]] auto restore_replay_checkpoint(Game::Session::SessionContext& session,
                                             const QByteArray& snapshot) -> bool;

// Seek in a freshly configured replay session. Returns the checkpoint tick
// used, or zero when playback started at the beginning of the log.
[[nodiscard]] auto
seek_replay(Game::Session::SessionContext& session,
            std::uint64_t target_tick) -> std::optional<std::uint64_t>;

} // namespace Game::Save
