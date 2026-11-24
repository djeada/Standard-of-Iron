#pragma once

#include <QByteArray>

namespace Game::Session {
class SessionContext;
}

namespace Game::Save {

[[nodiscard]] auto
capture_replay_checkpoint(Game::Session::SessionContext& session) -> QByteArray;

// Use a fresh session with the match systems already configured.
[[nodiscard]] auto restore_replay_checkpoint(Game::Session::SessionContext& session,
                                             const QByteArray& snapshot) -> bool;

} // namespace Game::Save
