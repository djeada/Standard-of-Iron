#pragma once

#include <QString>

enum class AudioLoadPolicy {
  Startup,
  Mission,
  Screen,
  Lazy,
  Stream,
  All
};

class AudioResourceLoader {
public:
  static void
  load_audio_resources(AudioLoadPolicy load_policy = AudioLoadPolicy::Startup);
};
