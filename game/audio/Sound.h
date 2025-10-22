#pragma once

#include <QObject>
#include <atomic>
#include <memory>
#include <string>

class MiniaudioBackend;

class Sound : public QObject {
  Q_OBJECT
public:
  explicit Sound(const std::string &filePath,
                 MiniaudioBackend *backend = nullptr);
  ~Sound();

  bool isLoaded() const;
  void play(float volume = 1.0f, bool loop = false);
  void stop();
  void setVolume(float volume);

  void setBackend(MiniaudioBackend *backend);

private:
  std::string m_filepath;
  QString m_trackId;
  MiniaudioBackend *m_backend;
  std::atomic<bool> m_loaded;
  std::atomic<float> m_volume;
};
