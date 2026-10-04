#include <gtest/gtest.h>
#include <vector>

#include "app/core/deferred_presentation_queue.h"

namespace {

using App::Core::DeferredPresentationQueue;

TEST(DeferredPresentationQueueTest, JobsRunInPostOrderOnlyWhenDrained) {
  DeferredPresentationQueue queue;
  std::vector<int> ran;
  queue.post([&] { ran.push_back(1); });
  queue.post([&] { ran.push_back(2); });
  queue.post({});
  EXPECT_TRUE(ran.empty()) << "posting never runs work in the locked section";
  EXPECT_EQ(queue.pending(), 2U);

  EXPECT_EQ(queue.drain(), 2U);
  EXPECT_EQ(ran, (std::vector<int>{1, 2}));
  EXPECT_EQ(queue.pending(), 0U);
  EXPECT_EQ(queue.drain(), 0U);
  EXPECT_EQ(queue.drained_jobs(), 2U);
}

TEST(DeferredPresentationQueueTest, AJobPostedWhileDrainingWaitsForTheNextDrain) {
  DeferredPresentationQueue queue;
  int late = 0;
  queue.post([&] { queue.post([&] { ++late; }); });
  EXPECT_EQ(queue.drain(), 1U);
  EXPECT_EQ(late, 0);
  EXPECT_EQ(queue.drain(), 1U);
  EXPECT_EQ(late, 1);
}

} // namespace
