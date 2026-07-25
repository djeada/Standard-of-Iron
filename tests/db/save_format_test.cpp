#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <gtest/gtest.h>

#include "systems/save_format.h"

using namespace Game::Systems::Save;

namespace {

auto sample_world() -> QByteArray {
  QByteArray world;
  for (int i = 0; i < 2000; ++i) {
    world.append("{\"id\":");
    world.append(QByteArray::number(i));
    world.append(R"(,"type":"archer"},)");
  }
  return world;
}

auto sample_record() -> Record {
  Record record;
  record.slot_name = QStringLiteral("shared_save");
  record.title = QStringLiteral("Shared Save");
  record.map_name = QStringLiteral("River Crossing");
  record.map_path = QStringLiteral("assets/maps/river.json");
  record.mode = QStringLiteral("campaign");
  record.campaign_id = QStringLiteral("second_punic_war");
  record.mission_id = QStringLiteral("trebia");
  record.difficulty = QStringLiteral("hard");
  record.kind = SlotKind::Manual;
  record.play_time_seconds = 1234.5;
  record.created_at = QStringLiteral("2026-07-25T10:00:00.000");
  record.updated_at = QStringLiteral("2026-07-25T10:30:00.000");
  record.metadata = QJsonObject{{"map_name", "River Crossing"}, {"local_owner_id", 1}};
  record.screenshot = QByteArray("preview-bytes");
  record.world = pack(sample_world());
  return record;
}

} // namespace

TEST(SaveFormatTest, PackShrinksRepetitiveDataAndRoundTrips) {
  const QByteArray world = sample_world();
  const Payload payload = pack(world);

  EXPECT_EQ(payload.compression, Compression::Zlib);
  EXPECT_EQ(payload.raw_size, world.size());
  EXPECT_LT(payload.blob.size(), world.size());

  QString error;
  QByteArray restored;
  ASSERT_TRUE(unpack(payload, restored, &error)) << error.toStdString();
  EXPECT_EQ(restored, world);
}

TEST(SaveFormatTest, UncompressedPayloadRoundTrips) {
  const QByteArray world("raw world state");
  const Payload payload = pack(world, Compression::None);

  EXPECT_EQ(payload.blob, world);

  QString error;
  QByteArray restored;
  ASSERT_TRUE(unpack(payload, restored, &error)) << error.toStdString();
  EXPECT_EQ(restored, world);
}

TEST(SaveFormatTest, TruncatedBlobIsRejectedBeforeDecompression) {
  Payload payload = pack(sample_world());
  payload.blob.truncate(payload.blob.size() / 2);

  QString error;
  EXPECT_FALSE(verify_blob(payload, &error));
  EXPECT_TRUE(error.contains("corrupted")) << error.toStdString();

  QByteArray restored;
  EXPECT_FALSE(unpack(payload, restored, &error));
  EXPECT_TRUE(restored.isEmpty());
}

TEST(SaveFormatTest, FlippedByteIsRejected) {
  Payload payload = pack(sample_world());
  payload.blob[payload.blob.size() / 2] =
      static_cast<char>(payload.blob.at(payload.blob.size() / 2) ^ 0xFF);

  QString error;
  QByteArray restored;
  EXPECT_FALSE(unpack(payload, restored, &error));
  EXPECT_TRUE(restored.isEmpty());
}

TEST(SaveFormatTest, ContentChecksumMismatchIsRejected) {
  Payload payload = pack(QByteArray("world"));
  payload.raw_checksum = checksum_of(QByteArray("a different world"));

  QString error;
  QByteArray restored;
  EXPECT_FALSE(unpack(payload, restored, &error));
  EXPECT_TRUE(error.contains("checksum")) << error.toStdString();
}

TEST(SaveFormatTest, PackageRoundTripsEveryField) {
  const Record record = sample_record();
  const QByteArray package = encode_package(record);

  Record decoded;
  QString error;
  ASSERT_TRUE(decode_package(package, decoded, &error)) << error.toStdString();

  EXPECT_EQ(decoded.slot_name, record.slot_name);
  EXPECT_EQ(decoded.title, record.title);
  EXPECT_EQ(decoded.map_name, record.map_name);
  EXPECT_EQ(decoded.map_path, record.map_path);
  EXPECT_EQ(decoded.mode, record.mode);
  EXPECT_EQ(decoded.campaign_id, record.campaign_id);
  EXPECT_EQ(decoded.mission_id, record.mission_id);
  EXPECT_EQ(decoded.difficulty, record.difficulty);
  EXPECT_EQ(decoded.kind, record.kind);
  EXPECT_DOUBLE_EQ(decoded.play_time_seconds, record.play_time_seconds);
  EXPECT_EQ(decoded.created_at, record.created_at);
  EXPECT_EQ(decoded.updated_at, record.updated_at);
  EXPECT_EQ(decoded.metadata, record.metadata);
  EXPECT_EQ(decoded.screenshot, record.screenshot);

  QByteArray world;
  ASSERT_TRUE(unpack(decoded.world, world, &error)) << error.toStdString();
  EXPECT_EQ(world, sample_world());
}

TEST(SaveFormatTest, ForeignFileIsRejected) {
  Record decoded;
  QString error;
  EXPECT_FALSE(decode_package(QByteArray("this is not a save file"), decoded, &error));
  EXPECT_FALSE(error.isEmpty());
}

TEST(SaveFormatTest, TruncatedPackageIsRejected) {
  QByteArray package = encode_package(sample_record());
  package.truncate(package.size() - 64);

  Record decoded;
  QString error;
  EXPECT_FALSE(decode_package(package, decoded, &error));
  EXPECT_FALSE(error.isEmpty());
}

TEST(SaveFormatTest, TamperedPackageBodyIsRejected) {
  QByteArray package = encode_package(sample_record());
  const int index = package.size() - 32;
  package[index] = static_cast<char>(package.at(index) ^ 0xFF);

  Record decoded;
  QString error;
  EXPECT_FALSE(decode_package(package, decoded, &error));
  EXPECT_FALSE(error.isEmpty());
}

TEST(SaveFormatTest, SlotKindNamesRoundTrip) {
  for (const SlotKind kind :
       {SlotKind::Manual, SlotKind::Quicksave, SlotKind::Autosave}) {
    SlotKind parsed{};
    ASSERT_TRUE(slot_kind_from_string(slot_kind_to_string(kind), parsed));
    EXPECT_EQ(parsed, kind);
  }

  SlotKind parsed{};
  EXPECT_FALSE(slot_kind_from_string(QStringLiteral("nonsense"), parsed));
}

TEST(SaveFormatTest, FileStemSanitizationStripsPathSeparators) {
  EXPECT_EQ(sanitize_file_stem(QStringLiteral("../../etc/passwd")),
            QString("______etc_passwd"));
  EXPECT_EQ(sanitize_file_stem(QStringLiteral("Save 2026-07-25")),
            QString("Save_2026-07-25"));
  EXPECT_EQ(sanitize_file_stem(QStringLiteral("keep_me-1")), QString("keep_me-1"));
  EXPECT_TRUE(sanitize_file_stem(QString()).isEmpty());
}
