#include "asset_compression.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QCryptographicHash>
#include <QFile>
#include <QSaveFile>
#include <QString>

#include <array>
#include <fstream>
#include <iterator>
#include <utility>
#include <zstd.h>

namespace Render::Creature::Bpat {

namespace {

constexpr std::array<std::uint8_t, 4> k_zstd_magic = {0x28, 0xB5, 0x2F, 0xFD};

constexpr unsigned long long k_max_decoded_bytes = 1ULL << 31U;

auto compress_asset(std::string_view bytes,
                    int level,
                    std::string& error) -> QByteArray {
  QByteArray frame(static_cast<qsizetype>(ZSTD_compressBound(bytes.size())),
                   Qt::Uninitialized);
  std::size_t const size = ZSTD_compress(frame.data(),
                                         static_cast<std::size_t>(frame.size()),
                                         bytes.data(),
                                         bytes.size(),
                                         level);
  if (ZSTD_isError(size) != 0U) {
    error = std::string("zstd compression failed: ") + ZSTD_getErrorName(size);
    return {};
  }
  frame.resize(static_cast<qsizetype>(size));
  return frame;
}

auto write_atomically(const QString& path,
                      QByteArrayView bytes,
                      std::string& error) -> bool {
  QSaveFile out(path);
  if (!out.open(QIODevice::WriteOnly) ||
      out.write(bytes.data(), bytes.size()) != bytes.size() || !out.commit()) {
    error =
        "cannot write " + path.toStdString() + ": " + out.errorString().toStdString();
    return false;
  }
  return true;
}

auto sha256_hex(QByteArrayView bytes) -> QByteArray {
  return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
}

auto matches_cached_frame(const QString& path,
                          const QString& sidecar_path,
                          const QByteArray& key) -> bool {
  QFile sidecar(sidecar_path);
  if (!sidecar.open(QIODevice::ReadOnly)) {
    return false;
  }

  if (sidecar.readLine(key.size() + 1) != key) {
    return false;
  }
  QFile frame(path);
  QCryptographicHash hash(QCryptographicHash::Sha256);
  if (!frame.open(QIODevice::ReadOnly) || !hash.addData(&frame)) {
    return false;
  }
  auto const digest = hash.result().toHex();
  return sidecar.read(digest.size() + 1) == digest;
}

} // namespace

auto is_zstd_frame(const std::uint8_t* data, std::size_t size) -> bool {
  if (data == nullptr || size < k_zstd_magic.size()) {
    return false;
  }
  for (std::size_t i = 0; i < k_zstd_magic.size(); ++i) {
    if (data[i] != k_zstd_magic[i]) {
      return false;
    }
  }
  return true;
}

auto decode_asset_bytes(std::vector<std::uint8_t> bytes,
                        std::vector<std::uint8_t>& out,
                        std::string& error) -> bool {
  if (!is_zstd_frame(bytes.data(), bytes.size())) {
    out = std::move(bytes);
    return true;
  }

  unsigned long long const decoded_size =
      ZSTD_getFrameContentSize(bytes.data(), bytes.size());
  if (decoded_size == ZSTD_CONTENTSIZE_ERROR) {
    error = "corrupt zstd frame header";
    return false;
  }
  if (decoded_size == ZSTD_CONTENTSIZE_UNKNOWN) {
    error = "zstd frame without a content size";
    return false;
  }
  if (decoded_size > k_max_decoded_bytes) {
    error = "zstd frame declares an implausible size";
    return false;
  }

  out.resize(static_cast<std::size_t>(decoded_size));
  std::size_t const written =
      ZSTD_decompress(out.data(), out.size(), bytes.data(), bytes.size());
  if (ZSTD_isError(written) != 0U) {
    error = std::string("zstd decompression failed: ") + ZSTD_getErrorName(written);
    out.clear();
    return false;
  }
  if (written != out.size()) {
    error = "zstd frame decoded to fewer bytes than it declared";
    out.clear();
    return false;
  }
  return true;
}

auto read_asset_file(const std::string& path,
                     std::vector<std::uint8_t>& out,
                     std::string& error) -> bool {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    error = "failed to open " + path;
    return false;
  }
  std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
  if (!decode_asset_bytes(std::move(bytes), out, error)) {
    error = path + ": " + error;
    return false;
  }
  return true;
}

auto write_compressed_asset(const std::filesystem::path& path,
                            std::string_view bytes,
                            std::string& error,
                            int level) -> bool {
  auto const frame = compress_asset(bytes, level, error);
  return !frame.isEmpty() &&
         write_atomically(QString::fromStdU16String(path.u16string()), frame, error);
}

auto write_cached_asset(const std::filesystem::path& path,
                        const std::filesystem::path& sidecar,
                        std::string_view bytes,
                        std::string& error,
                        int level) -> bool {
  auto const asset_path = QString::fromStdU16String(path.u16string());
  auto const sidecar_path = QString::fromStdU16String(sidecar.u16string());
  auto const key =
      QByteArray::number(ZSTD_versionNumber()) + ':' + QByteArray::number(level) + ':' +
      sha256_hex(QByteArrayView(bytes.data(), static_cast<qsizetype>(bytes.size()))) +
      '\n';
  if (matches_cached_frame(asset_path, sidecar_path, key)) {
    return true;
  }

  auto const frame = compress_asset(bytes, level, error);
  return !frame.isEmpty() && write_atomically(asset_path, frame, error) &&
         write_atomically(sidecar_path, key + sha256_hex(frame), error);
}

} // namespace Render::Creature::Bpat
