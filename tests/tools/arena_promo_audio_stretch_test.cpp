#include <gtest/gtest.h>

#include "tools/arena/promo_audio_stretch.h"

namespace {

using Arena::Promo::audio_stretch_filter;

TEST(ArenaPromoAudioStretchTest, RealTimeShotsKeepTheirAudio) {
  EXPECT_TRUE(audio_stretch_filter(4.0F, 4.0F).isEmpty());
  EXPECT_TRUE(audio_stretch_filter(4.0F, 4.02F).isEmpty())
      << "a frame of rounding is not a slow-motion shot";
  EXPECT_TRUE(audio_stretch_filter(6.0F, 4.0F).isEmpty())
      << "a time-lapse or report card already records clip-length audio";
}

TEST(ArenaPromoAudioStretchTest, SlowMotionAudioIsSlowedToTheClip) {

  EXPECT_EQ(audio_stretch_filter(2.6F, 5.2F), QStringLiteral("atempo=0.50000"));
  EXPECT_EQ(audio_stretch_filter(3.0F, 4.5F), QStringLiteral("atempo=0.66667"));
}

TEST(ArenaPromoAudioStretchTest, StrongSlowMotionChainsAtempoStages) {

  EXPECT_EQ(audio_stretch_filter(1.0F, 4.0F),
            QStringLiteral("atempo=0.5,atempo=0.50000"));
  EXPECT_EQ(audio_stretch_filter(1.0F, 3.0F),
            QStringLiteral("atempo=0.5,atempo=0.66667"));
}

} // namespace
