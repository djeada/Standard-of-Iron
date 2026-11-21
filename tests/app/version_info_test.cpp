#include <QString>

#include <array>
#include <gtest/gtest.h>

#include "app/bootstrap/version_info.h"

namespace {

using App::Bootstrap::format_version_text;
using App::Bootstrap::version_requested_from_argv;
using App::Bootstrap::VersionDetails;

TEST(VersionInfo, FullDetailsPrintOneFieldPerLine) {
  VersionDetails details;
  details.version = QStringLiteral("0.3.0");
  details.commit = QStringLiteral("abc1234def56");
  details.build_type = QStringLiteral("Release");
  details.qt_version = QStringLiteral("6.8.2");

  EXPECT_EQ(format_version_text(details),
            QStringLiteral("Standard of Iron 0.3.0\n"
                           "Commit: abc1234def56\n"
                           "Build type: Release\n"
                           "Qt: 6.8.2\n"));
}

TEST(VersionInfo, MissingDetailsAreLeftOut) {
  VersionDetails details;
  details.version = QStringLiteral("0.3.0");
  details.commit = QStringLiteral("unknown");

  EXPECT_EQ(format_version_text(details), QStringLiteral("Standard of Iron 0.3.0\n"));
}

TEST(VersionInfo, RecognisesBothVersionSpellings) {
  std::array<char*, 2> long_form{const_cast<char*>("standard_of_iron"),
                                 const_cast<char*>("--version")};
  std::array<char*, 3> short_form{const_cast<char*>("standard_of_iron"),
                                  const_cast<char*>("--force-software"),
                                  const_cast<char*>("-v")};
  EXPECT_TRUE(version_requested_from_argv(static_cast<int>(long_form.size()),
                                          long_form.data()));
  EXPECT_TRUE(version_requested_from_argv(static_cast<int>(short_form.size()),
                                          short_form.data()));
}

TEST(VersionInfo, OtherArgumentsDoNotRequestTheVersion) {
  std::array<char*, 3> plain{const_cast<char*>("standard_of_iron"),
                             const_cast<char*>("--renderer-self-test"),
                             const_cast<char*>("--versions")};
  std::array<char*, 3> after_separator{const_cast<char*>("standard_of_iron"),
                                       const_cast<char*>("--"),
                                       const_cast<char*>("--version")};
  EXPECT_FALSE(version_requested_from_argv(1, plain.data()));
  EXPECT_FALSE(
      version_requested_from_argv(static_cast<int>(plain.size()), plain.data()));
  EXPECT_FALSE(version_requested_from_argv(static_cast<int>(after_separator.size()),
                                           after_separator.data()));
}

} // namespace
