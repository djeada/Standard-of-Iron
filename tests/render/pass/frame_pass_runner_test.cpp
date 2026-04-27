// Stage 10 — FramePassRunner tests.
//
// Verifies ordering, non-owning add, short-circuit guards. We don't need
// a GL context for this because the runner is pure logic over an
// IFramePass interface.

#include "render/pass/frame_context.h"
#include "render/pass/frame_pass_runner.h"
#include "render/pass/i_pass.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace {

class RecordingPass final : public Render::Pass::IFramePass {
public:
  RecordingPass(const char *name, std::vector<std::string> *log)
      : m_name(name), m_log(log) {}
  [[nodiscard]] auto name() const noexcept -> std::string_view override {
    return m_name;
  }
  void execute(Render::Pass::FrameContext &ctx) override {
    m_log->push_back(m_name);
    ctx.frame_counter++;
  }

private:
  const char *m_name;
  std::vector<std::string> *m_log;
};

} // namespace

TEST(FramePassRunnerTest, ExecutesPassesInSubmissionOrder) {
  std::vector<std::string> log;
  Render::Pass::FramePassRunner runner;
  runner.add(std::make_unique<RecordingPass>("a", &log));
  runner.add(std::make_unique<RecordingPass>("b", &log));
  runner.add(std::make_unique<RecordingPass>("c", &log));

  Render::Pass::FrameContext ctx;
  runner.execute(ctx);

  ASSERT_EQ(log.size(), 3U);
  EXPECT_EQ(log[0], "a");
  EXPECT_EQ(log[1], "b");
  EXPECT_EQ(log[2], "c");
  EXPECT_EQ(ctx.frame_counter, 3U);
}

TEST(FramePassRunnerTest, ReportsSizeAndNames) {
  Render::Pass::FramePassRunner runner;
  std::vector<std::string> log;
  runner.add(std::make_unique<RecordingPass>("first", &log));
  runner.add(std::make_unique<RecordingPass>("second", &log));

  ASSERT_EQ(runner.size(), 2U);
  EXPECT_EQ(runner.pass_name(0), "first");
  EXPECT_EQ(runner.pass_name(1), "second");
  EXPECT_EQ(runner.pass_name(2), std::string_view{});
}

TEST(FramePassRunnerTest, NonOwningPassShareIdentity) {
  std::vector<std::string> log;
  RecordingPass pass("stack_owned", &log);

  Render::Pass::FramePassRunner runner;
  runner.add_non_owning(pass);

  Render::Pass::FrameContext ctx;
  runner.execute(ctx);
  runner.execute(ctx);

  ASSERT_EQ(log.size(), 2U);
  EXPECT_EQ(ctx.frame_counter, 2U);
}

TEST(FramePassRunnerTest, NullOwnedPassIsIgnored) {
  Render::Pass::FramePassRunner runner;
  runner.add(nullptr);
  EXPECT_EQ(runner.size(), 0U);

  Render::Pass::FrameContext ctx;
  runner.execute(ctx); // no crash
}

TEST(FramePassRunnerTest, ClearResetsSize) {
  std::vector<std::string> log;
  Render::Pass::FramePassRunner runner;
  runner.add(std::make_unique<RecordingPass>("a", &log));
  runner.add(std::make_unique<RecordingPass>("b", &log));
  EXPECT_EQ(runner.size(), 2U);
  runner.clear();
  EXPECT_EQ(runner.size(), 0U);

  Render::Pass::FrameContext ctx;
  runner.execute(ctx);
  EXPECT_TRUE(log.empty());
}
