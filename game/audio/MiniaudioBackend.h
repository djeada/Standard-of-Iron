#pragma once
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QVector>
#include <atomic>

struct ma_device;

class MiniaudioBackend : public QObject {
  Q_OBJECT
public:
  explicit MiniaudioBackend(QObject *parent = nullptr);
  ~MiniaudioBackend() override;

  bool initialize(int deviceRate, int outChannels, int musicChannels);
  void shutdown();

  bool predecode(const QString &id, const QString &path);

  void play(int channel, const QString &id, float volume, bool loop,
            int fadeMs);
  void stop(int channel, int fadeMs);
  void pause(int channel);
  void resume(int channel);
  void setVolume(int channel, float volume, int fadeMs);
  void stopAll(int fadeMs);
  void setMasterVolume(float volume, int fadeMs);

  bool anyChannelPlaying() const;
  bool channelPlaying(int channel) const;

  void playSound(const QString &id, float volume, bool loop = false);

  void onAudio(float *out, unsigned frames);

private:
  struct DecodedTrack {
    QVector<float> pcm;
    unsigned frames = 0;
  };

  struct Channel {
    const DecodedTrack *track = nullptr;
    unsigned framePos = 0;
    float curVol = 0.0f;
    float tgtVol = 1.0f;
    float volStep = 0.0f;
    unsigned fadeSamples = 0;
    bool looping = false;
    bool paused = false;
    bool active = false;
  };

  struct SoundEffect {
    const DecodedTrack *track = nullptr;
    unsigned framePos = 0;
    float volume = 1.0f;
    bool looping = false;
    bool active = false;
  };

private:
  void startDevice();
  void stopDevice();
  int findFreeSoundSlot() const;

private:
  ma_device *m_device{nullptr};
  int m_rate{48000};
  int m_outCh{2};

  mutable QMutex m_mutex;
  QMap<QString, DecodedTrack> m_tracks;
  QVector<Channel> m_channels;
  QVector<SoundEffect> m_soundEffects;
  float m_masterVol{1.0f};
};
