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
    for (const auto tag : tip.value(QStringLiteral("tags")).toArray()) {
      EXPECT_EQ(tag.toString(), QStringLiteral("undead"))
          << "unknown tag on tip "
          << tip.value(QStringLiteral("text")).toString().toStdString();
    }
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

TEST(LoadingTipsTest, ReadsTagsAndKeepsUntaggedTipsValid) {
  LoadingTips tips;
  tips.load_from_json(QByteArrayLiteral(
      R"({"tips":[{"text":"Plain."},{"text":"Grave.","tags":["Undead"," "]}]})"));
  ASSERT_EQ(tips.count(), 2);
  EXPECT_TRUE(tips.tags_of(QStringLiteral("Plain.")).isEmpty());
  EXPECT_EQ(tips.tags_of(QStringLiteral("Grave.")), QStringList{"undead"});
}

TEST(LoadingTipsTest, PreferredTagsAreDealtFirstAndTheDeckStaysWhole) {
  LoadingTips tips;
  tips.load_from_json(QByteArrayLiteral(R"({"tips":[
      {"text":"A"},{"text":"B"},{"text":"C"},{"text":"D"},
      {"text":"U1","tags":["undead"]},{"text":"U2","tags":["undead"]}]})"));
  tips.reseed(3);
  tips.set_preferred_tags({QStringLiteral("undead")});

  const QString first = tips.next();
  const QString second = tips.next();
  EXPECT_TRUE(first.startsWith(QLatin1Char('U'))) << first.toStdString();
  EXPECT_TRUE(second.startsWith(QLatin1Char('U'))) << second.toStdString();
  EXPECT_NE(first, second);

  QSet<QString> seen{first, second};
  for (int i = 2; i < tips.count(); ++i) {
    const QString drawn = tips.next();
    EXPECT_FALSE(seen.contains(drawn)) << drawn.toStdString();
    seen.insert(drawn);
  }
  EXPECT_EQ(seen.size(), tips.count());

  tips.set_preferred_tags({});
  EXPECT_TRUE(tips.preferred_tags().isEmpty());
}

TEST(LoadingTipsTest, UndeadMissionsPreferUndeadTips) {
  EXPECT_EQ(LoadingTips::tags_for_load(
                QStringLiteral("assets/maps/map_iron_sepulcher_watch.json"), {}, false),
            QStringList{"undead"});
  EXPECT_EQ(LoadingTips::tags_for_load(
                QStringLiteral("assets/maps/map_battle_zama.json"), {}, false),
            QStringList{"undead"});
  EXPECT_EQ(LoadingTips::tags_for_load(QStringLiteral("assets/maps/map_forest.json"),
                                       QStringLiteral("hold_the_sallow_ford"),
                                       true),
            QStringList{"undead"});
  EXPECT_TRUE(LoadingTips::tags_for_load(QStringLiteral("assets/maps/map_forest.json"),
                                         QStringLiteral("battle_of_cannae"),
                                         false)
                  .isEmpty());
}

TEST(LoadingTipsTest, ShipsSeveralUndeadTips) {
  LoadingTips tips;
  int undead = 0;
  for (const QString& source : tips.source_texts()) {
    if (tips.tags_of(source).contains(QStringLiteral("undead"))) {
      ++undead;
    }
  }
  EXPECT_GE(undead, 4) << "the Iron Sepulcher needs its own loading tips.";
}
