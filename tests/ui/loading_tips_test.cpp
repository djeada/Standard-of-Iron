#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QString>

#include <gtest/gtest.h>

#include "app/models/loading_tips.h"

namespace {

auto tips_from_disk() -> QJsonArray {
  QFile file(QStringLiteral("assets/data/loading_tips.json"));
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return QJsonDocument::fromJson(file.readAll())
      .object()
      .value(QStringLiteral("tips"))
      .toArray();
}

} // namespace

TEST(LoadingTipsTest, ShipsEveryTipTheAssetDeclares) {
  const QJsonArray authored = tips_from_disk();
  ASSERT_FALSE(authored.isEmpty())
      << "assets/data/loading_tips.json must ship with the game.";

  LoadingTips tips;
  EXPECT_EQ(tips.count(), authored.size())
      << "every authored tip must reach the loading screen.";

  for (const QString& source : tips.source_texts()) {
    EXPECT_FALSE(source.trimmed().isEmpty());
    EXPECT_FALSE(source.contains(QStringLiteral("**")))
        << "tips are plain sentences, not markdown: " << source.toStdString();
  }
}

TEST(LoadingTipsTest, EveryAuthoredTipDeclaresAKnownTone) {
  for (const auto entry : tips_from_disk()) {
    const QJsonObject tip = entry.toObject();
    const QString tone = tip.value(QStringLiteral("tone")).toString();
    EXPECT_TRUE(tone == QStringLiteral("plain") || tone == QStringLiteral("wry"))
        << "unknown tone '" << tone.toStdString() << "' on tip "
        << tip.value(QStringLiteral("text")).toString().toStdString();
  }
}

TEST(LoadingTipsTest, DealsTheWholeDeckBeforeRepeatingATip) {
  LoadingTips tips;
  tips.reseed(1234);
  const int count = tips.count();
  ASSERT_GT(count, 1);

  QSet<QString> seen;
  for (int i = 0; i < count; ++i) {
    const QString drawn = tips.next();
    EXPECT_FALSE(drawn.isEmpty());
    EXPECT_FALSE(seen.contains(drawn))
        << "a player should see every tip once before any of them comes round "
           "again; repeated: "
        << drawn.toStdString();
    seen.insert(drawn);
  }
  EXPECT_EQ(seen.size(), count);
}

TEST(LoadingTipsTest, NeverShowsTheSameTipTwiceInARow) {
  LoadingTips tips;
  tips.reseed(99);
  const int count = tips.count();
  ASSERT_GT(count, 1);

  QString previous = tips.next();
  for (int i = 0; i < count * 3; ++i) {
    const QString drawn = tips.next();
    EXPECT_NE(drawn, previous)
        << "the deck refill must not hand back the tip already on screen.";
    previous = drawn;
  }
}

TEST(LoadingTipsTest, ReseedingMakesTheOrderReproducible) {
  LoadingTips first;
  LoadingTips second;
  first.reseed(7);
  second.reseed(7);
  ASSERT_GT(first.count(), 1);

  for (int i = 0; i < first.count(); ++i) {
    EXPECT_EQ(first.next(), second.next());
  }
}

TEST(LoadingTipsTest, SurvivesAMalformedTipFile) {
  LoadingTips tips;
  tips.load_from_json(QByteArrayLiteral("{ this is not json"));
  EXPECT_EQ(tips.count(), 0);
  EXPECT_TRUE(tips.next().isEmpty())
      << "a broken tip file must not take the loading screen down with it.";

  tips.load_from_json(
      QByteArrayLiteral(R"({"tips":[{"text":"  "},{"text":"Hold the line."}]})"));
  EXPECT_EQ(tips.count(), 1) << "blank tips must be dropped, not shown.";
  EXPECT_EQ(tips.next(), QStringLiteral("Hold the line."));
}
