#pragma once

#include <QObject>
#include <atomic>
#include <memory>
#include <string>

class MiniaudioBackend;

class Sound : public QObject {
  Q_OBJECT
public:
  static constexpr float DEFAULT_VOLUME = 1.0F;

  explicit Sound(const std::string &file_path,
                 MiniaudioBackend *backend = nullptr);
  ~Sound() override;

  [[nodiscard]] auto is_loaded() const -> bool;
  void play(float volume = DEFAULT_VOLUME, bool loop = false);
  void stop();
  void set_volume(float volume);

  void set_backend(MiniaudioBackend *backend);

private:
  std::string m_file_path;
  QString m_track_id;
  MiniaudioBackend *m_backend;
  std::atomic<bool> m_loaded;
  std::atomic<float> m_volume;
};
