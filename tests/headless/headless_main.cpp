#include <QCoreApplication>

#include <gtest/gtest.h>

#include "game/session/session_context.h"

auto main(int argc, char** argv) -> int {
  QCoreApplication app(argc, argv);

  Game::Session::SessionContext session;
  Game::Session::ScopedSession const active_session(session);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
