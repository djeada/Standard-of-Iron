#include "system_access_recorder.h"

namespace Engine::Core::Detail {

auto active_access_recorder() -> SystemAccessRecorder*& {
  thread_local SystemAccessRecorder* recorder = nullptr;
  return recorder;
}

} // namespace Engine::Core::Detail
