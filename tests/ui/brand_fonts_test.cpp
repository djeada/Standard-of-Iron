
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetrics>
#include <QRawFont>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <gtest/gtest.h>

#include "ui/brand_fonts.h"

namespace {

TEST(BrandFontsTest, RegistersTheBundledFaces) {
  const QStringList families = Ui::BrandFonts::register_bundled();
  EXPECT_FALSE(families.isEmpty())
      << "nothing registered from assets/fonts/; the staged copy beside the "
         "test binary is what resolve_resource_path finds first";
  for (const auto* expected : {"Standard Iron Display", "EB Garamond"}) {
    EXPECT_TRUE(families.contains(QString::fromLatin1(expected)))
        << expected << " missing; registered: "
        << qUtf8Printable(families.join(QStringLiteral(", ")));
  }
}

TEST(BrandFontsTest, RegistrationIsIdempotent) {
  const QStringList first = Ui::BrandFonts::register_bundled();
  const QStringList second = Ui::BrandFonts::register_bundled();
  EXPECT_EQ(first, second);
}

TEST(BrandFontsTest, TitleFamilyIsTheBrandFace) {
  EXPECT_EQ(Ui::BrandFonts::title_family(), QStringLiteral("Standard Iron Display"));
}

TEST(BrandFontsTest, TitleFamilyIsBundledRatherThanASystemFallback) {
  const QString family = Ui::BrandFonts::title_family();
  ASSERT_NE(family, QStringLiteral("serif"))
      << "title_family() fell through to the generic request, which means no "
         "bundled face resolved and captures are host-dependent again";
  EXPECT_TRUE(Ui::BrandFonts::register_bundled().contains(family));
}

TEST(BrandFontsTest, TitleFamilyActuallyResolvesToItself) {

  const QString family = Ui::BrandFonts::title_family();
  const QFontInfo info{QFont(family)};
  EXPECT_EQ(info.family(), family);
}

auto missing_from(const QString& family, const QString& sample) -> QString {
  const QRawFont face = QRawFont::fromFont(QFont(family));
  EXPECT_TRUE(face.isValid()) << qUtf8Printable(family) << " did not resolve";
  QString missing;
  for (const QChar character : sample) {
    if (!face.supportsCharacter(character)) {
      missing.append(character);
    }
  }
  return missing;
}

TEST(BrandFontsTest, TheBundledSerifCoversWhatTheDisplayFaceCannot) {

  ASSERT_TRUE(
      Ui::BrandFonts::register_bundled().contains(QStringLiteral("EB Garamond")));
  const QString missing = missing_from(
      QStringLiteral("EB Garamond"),
      QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"));
  EXPECT_EQ(missing, QString())
      << "the backing face is missing glyphs the captions need";
}

TEST(BrandFontsTest, TheDisplayFaceCoversCapsFiguresAndAccents) {

  const QString missing = missing_from(
      Ui::BrandFonts::title_family(),
      QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.\u2026,:;!?-+/%&#()[]"
                     "\u00c1\u00c0\u00c2\u00c3\u00c4\u00c5\u00c7\u00c9\u00c8"
                     "\u00ca\u00cb\u00cd\u00cc\u00ce\u00cf\u00d1\u00d3\u00d2"
                     "\u00d4\u00d5\u00d6\u00da\u00d9\u00db\u00dc\u00dd"
                     "\u00bf\u00a1\u00d7"
                     "\u011e\u0130\u015e"
                     "\u0104\u0106\u0118\u0141\u0143\u015a\u0179\u017b"));
  EXPECT_EQ(missing, QString()) << "characters missing from the display face";
}

TEST(BrandFontsTest, TheDisplayFaceCoversTheRussianCapitals) {

  const QString missing =
      missing_from(Ui::BrandFonts::title_family(),
                   QStringLiteral("\u0410\u0411\u0412\u0413\u0414\u0415\u0401\u0416"
                                  "\u0417\u0418\u0419\u041a\u041b\u041c\u041d\u041e"
                                  "\u041f\u0420\u0421\u0422\u0423\u0424\u0425\u0426"
                                  "\u0427\u0428\u0429\u042a\u042b\u042c\u042d\u042e"
                                  "\u042f"));
  EXPECT_EQ(missing, QString())
      << "a Russian title would be set in whatever face the host offers";
}

TEST(BrandFontsTest, QtAppliesTheDisplayFacesKerning) {

  QFont font(Ui::BrandFonts::title_family());
  font.setPixelSize(100);
  const QFontMetricsF metrics(font);

  const qreal pair = metrics.horizontalAdvance(QStringLiteral("AV"));
  const qreal apart = metrics.horizontalAdvance(QStringLiteral("A")) +
                      metrics.horizontalAdvance(QStringLiteral("V"));
  EXPECT_LT(pair, apart - 1.0) << "AV is not kerned: " << pair << " vs " << apart
                               << " unkerned. Qt is ignoring the kern table.";
}

TEST(BrandFontsTest, TheDisplayFaceHasNoLowercaseToFallBackFrom) {

  EXPECT_EQ(missing_from(Ui::BrandFonts::title_family(), QStringLiteral("aeiou")),
            QStringLiteral("aeiou"));
}

class TitleFamilyUsageTest : public ::testing::Test {
protected:
  static auto qml_sources() -> QStringList {
    QStringList files;
    QDirIterator it(QDir::current().filePath(QStringLiteral("ui/qml")),
                    {QStringLiteral("*.qml")},
                    QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
      files.append(it.next());
    }
    return files;
  }

  static auto read(const QString& path) -> QString {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      return {};
    }
    return QString::fromUtf8(file.readAll());
  }
};

TEST_F(TitleFamilyUsageTest, EveryBindingIsUppercasedOrNumeric) {
  const QStringList files = qml_sources();
  ASSERT_FALSE(files.isEmpty()) << "no QML found; run the suite from the repo root";

  static const QRegularExpression binding(
      QStringLiteral(R"(font\.family:\s*Design\.Typography\.titleFamily)"));

  QStringList offenders;
  for (const QString& path : files) {
    const QStringList lines = read(path).split(QLatin1Char('\n'));
    for (qsizetype index = 0; index < lines.size(); ++index) {
      if (!binding.match(lines.at(index)).hasMatch()) {
        continue;
      }

      const qsizetype from = std::max<qsizetype>(0, index - 12);
      const qsizetype to = std::min<qsizetype>(lines.size() - 1, index + 12);
      const QString block = lines.mid(from, to - from + 1).join(QLatin1Char('\n'));
      const bool uppercased = block.contains(QStringLiteral("Font.AllUppercase"));
      const bool numeric = block.contains(QStringLiteral("root.figure("));
      if (!uppercased && !numeric) {
        offenders.append(
            QStringLiteral("%1:%2").arg(QFileInfo(path).fileName()).arg(index + 1));
      }
    }
  }

  EXPECT_EQ(offenders.join(QStringLiteral(", ")), QString())
      << "titleFamily bound without Font.AllUppercase and without a numeric "
         "source. The display face has no lowercase; see docs/TYPOGRAPHY.md.";
}

TEST_F(TitleFamilyUsageTest, EveryBindingUsesTheDisplayRasterizationSettings) {
  const QStringList files = qml_sources();
  ASSERT_FALSE(files.isEmpty()) << "no QML found; run the suite from the repo root";

  static const QRegularExpression binding(
      QStringLiteral(R"(font\.family:\s*Design\.Typography\.titleFamily)"));

  QStringList offenders;
  for (const QString& path : files) {
    const QStringList lines = read(path).split(QLatin1Char('\n'));
    for (qsizetype index = 0; index < lines.size(); ++index) {
      if (!binding.match(lines.at(index)).hasMatch()) {
        continue;
      }

      const qsizetype from = std::max<qsizetype>(0, index - 12);
      const qsizetype to = std::min<qsizetype>(lines.size() - 1, index + 12);
      const QString block = lines.mid(from, to - from + 1).join(QLatin1Char('\n'));
      const bool vertically_hinted = block.contains(
          QStringLiteral("font.hintingPreference: Design.Typography.titleHinting"));
      const bool kerned = block.contains(QStringLiteral("font.kerning: true"));
      if (!vertically_hinted || !kerned) {
        offenders.append(
            QStringLiteral("%1:%2").arg(QFileInfo(path).fileName()).arg(index + 1));
      }
    }
  }

  EXPECT_EQ(offenders.join(QStringLiteral(", ")), QString())
      << "titleFamily bound without the shared hinting and kerning settings";
}

TEST_F(TitleFamilyUsageTest, TheGuardActuallyFindsTheBindingsItGuards) {

  const QStringList files = qml_sources();
  ASSERT_FALSE(files.isEmpty())
      << "no QML found under " << qUtf8Printable(QDir::current().filePath("ui/qml"))
      << "; this suite reads the source tree, so run it from the repository root "
         "(ctest does: tests/CMakeLists.txt sets WORKING_DIRECTORY)";

  int found = 0;
  static const QRegularExpression binding(
      QStringLiteral(R"(font\.family:\s*Design\.Typography\.titleFamily)"));
  for (const QString& path : files) {
    found += read(path).count(binding);
  }
  EXPECT_GE(found, 7)
      << "expected the main-menu, load-screen, outcome and battle-report bindings";
}

} // namespace
