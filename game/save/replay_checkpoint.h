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

[[nodiscard]] auto restore_replay_checkpoint(Game::Session::SessionContext& session,
                                             const QByteArray& snapshot) -> bool;

[[nodiscard]] auto
seek_replay(Game::Session::SessionContext& session,
            std::uint64_t target_tick) -> std::optional<std::uint64_t>;

} // namespace Game::Save
