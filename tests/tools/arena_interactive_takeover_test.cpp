#include <QByteArray>
#include <QFile>
#include <QString>

#include <gtest/gtest.h>

namespace {

auto read_source(const char* path) -> QByteArray {
  QFile file(QString::fromLatin1(path));
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return file.readAll();
}

auto contains(const QByteArray& source, const char* needle) -> bool {
  return source.contains(needle);
}

} // namespace

TEST(ArenaInteractiveTakeoverTest, TheViewportKeepsTabInsteadOfLosingItToFocus) {
  const auto header = read_source("tools/arena/arena_viewport.h");
  const auto source = read_source("tools/arena/arena_viewport.cpp");
  ASSERT_FALSE(header.isEmpty()) << "run the suite from the repo root";
  ASSERT_FALSE(source.isEmpty()) << "run the suite from the repo root";

  EXPECT_TRUE(contains(header, "auto focusNextPrevChild(bool next) -> bool override;"))
      << "the viewport must override focus navigation, or Tab never reaches "
         "keyPressEvent";
  EXPECT_TRUE(contains(source, "auto ArenaViewport::focusNextPrevChild(bool next)"));
  EXPECT_TRUE(
      contains(source, "if (event->key() == Qt::Key_Tab && !event->isAutoRepeat()) {"))
      << "Tab is still the takeover key";
  EXPECT_TRUE(contains(source, "enter_rpg_interactive_control()"));
}

TEST(ArenaInteractiveTakeoverTest, InteractiveControlCanBeTracedOnDemand) {
  const auto source = read_source("tools/arena/arena_viewport.cpp");
  ASSERT_FALSE(source.isEmpty()) << "run the suite from the repo root";

  EXPECT_TRUE(contains(source, "SOI_ARENA_RPG_TRACE"));
  EXPECT_TRUE(contains(source, "SOI_RPG_INTERACTIVE pos="));
  EXPECT_TRUE(contains(source, "report_rpg_interactive_state(simulation_dt);"))
      << "the report has to be driven from the commander tick";
}
