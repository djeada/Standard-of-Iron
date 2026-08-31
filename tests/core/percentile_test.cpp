#include <gtest/gtest.h>
#include <vector>

#include "utils/percentile.h"

namespace {

using Utils::Stats::Distribution;
using Utils::Stats::SampleWindow;

TEST(PercentileTest, EmptyInputIsZeroEverywhere) {
  const Distribution spread = Utils::Stats::distribution_of({});
  EXPECT_EQ(spread.count, 0U);
  EXPECT_DOUBLE_EQ(spread.average, 0.0);
  EXPECT_DOUBLE_EQ(spread.p50, 0.0);
  EXPECT_DOUBLE_EQ(spread.p95, 0.0);
  EXPECT_DOUBLE_EQ(spread.p99, 0.0);
  EXPECT_DOUBLE_EQ(spread.maximum, 0.0);
}

TEST(PercentileTest, SingleSampleIsEveryPercentile) {
  const Distribution spread = Utils::Stats::distribution_of({7.5});
  EXPECT_EQ(spread.count, 1U);
  EXPECT_DOUBLE_EQ(spread.p50, 7.5);
  EXPECT_DOUBLE_EQ(spread.p95, 7.5);
  EXPECT_DOUBLE_EQ(spread.p99, 7.5);
  EXPECT_DOUBLE_EQ(spread.maximum, 7.5);
}

TEST(PercentileTest, IndexNeverRunsPastTheLastSample) {
  for (std::size_t count = 1; count <= 200U; ++count) {
    for (unsigned percent : {0U, 1U, 50U, 95U, 99U, 100U}) {
      EXPECT_LT(Utils::Stats::percentile_index(count, percent), count)
          << "count " << count << " percent " << percent;
    }
  }
  EXPECT_EQ(Utils::Stats::percentile_index(0U, 95U), 0U);
}

TEST(PercentileTest, PercentilesRankOverAHundredSamples) {
  std::vector<double> samples;
  samples.reserve(100);
  for (int i = 1; i <= 100; ++i) {
    samples.push_back(static_cast<double>(i));
  }
  const Distribution spread = Utils::Stats::distribution_of(samples);
  EXPECT_EQ(spread.count, 100U);
  EXPECT_DOUBLE_EQ(spread.average, 50.5);
  EXPECT_DOUBLE_EQ(spread.p50, 50.0);
  EXPECT_DOUBLE_EQ(spread.p95, 95.0);
  EXPECT_DOUBLE_EQ(spread.p99, 99.0);
  EXPECT_DOUBLE_EQ(spread.maximum, 100.0);
}

TEST(PercentileTest, PercentilesAreMonotonic) {
  const std::vector<double> samples{3.0, 1.0, 9.0, 4.0, 2.0, 8.0, 5.0};
  const Distribution spread = Utils::Stats::distribution_of(samples);
  EXPECT_LE(spread.p50, spread.p95);
  EXPECT_LE(spread.p95, spread.p99);
  EXPECT_LE(spread.p99, spread.maximum);
}

TEST(SampleWindowTest, EmptyWindowReportsNothing) {
  SampleWindow<8> window;
  EXPECT_EQ(window.count(), 0U);
  const Distribution spread = window.distribution();
  EXPECT_EQ(spread.count, 0U);
  EXPECT_DOUBLE_EQ(spread.p95, 0.0);
}

TEST(SampleWindowTest, PartiallyFilledWindowOnlyRanksRealSamples) {
  SampleWindow<8> window;
  window.push(4.0);
  window.push(2.0);
  window.push(6.0);
  const Distribution spread = window.distribution();
  EXPECT_EQ(spread.count, 3U);
  EXPECT_DOUBLE_EQ(spread.p50, 4.0);
  EXPECT_DOUBLE_EQ(spread.maximum, 6.0);
  EXPECT_DOUBLE_EQ(spread.average, 4.0);
}

TEST(SampleWindowTest, RingKeepsTheMostRecentSamples) {
  SampleWindow<4> window;
  for (double value : {1.0, 2.0, 3.0, 4.0, 5.0, 6.0}) {
    window.push(value);
  }
  const Distribution spread = window.distribution();
  EXPECT_EQ(spread.count, 4U);
  EXPECT_DOUBLE_EQ(spread.p50, 4.0);
  EXPECT_DOUBLE_EQ(spread.p99, 6.0);
  EXPECT_EQ(window.pushes(), 6U);
}

TEST(SampleWindowTest, DistributionDescribesTheWindow) {
  SampleWindow<2> window;
  window.push(99.0);
  window.push(1.0);
  window.push(3.0);

  const Distribution spread = window.distribution();
  EXPECT_EQ(spread.count, 2U);
  EXPECT_DOUBLE_EQ(spread.average, 2.0);
  EXPECT_DOUBLE_EQ(spread.maximum, 3.0);
  EXPECT_DOUBLE_EQ(spread.p50, 1.0);
}

TEST(SampleWindowTest, LifetimeAccessorsStillSeeEvictedSamples) {
  SampleWindow<2> window;
  window.push(99.0);
  window.push(1.0);
  window.push(3.0);
  EXPECT_DOUBLE_EQ(window.lifetime_maximum(), 99.0);
  EXPECT_NEAR(window.lifetime_average(), (99.0 + 1.0 + 3.0) / 3.0, 1e-9);
  EXPECT_EQ(window.pushes(), 3U);
}

TEST(SampleWindowTest, ClearResetsEverything) {
  SampleWindow<4> window;
  window.push(5.0);
  window.push(9.0);
  window.clear();
  EXPECT_EQ(window.count(), 0U);
  EXPECT_EQ(window.pushes(), 0U);
  EXPECT_DOUBLE_EQ(window.lifetime_maximum(), 0.0);
  EXPECT_DOUBLE_EQ(window.lifetime_average(), 0.0);
}

} // namespace
