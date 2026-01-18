#pragma once

#include <QDebug>
#include <QThread>

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif

namespace Render {

class ThreadAffinity {
public:
  static bool pin_to_core(QThread *thread, int core_id) {
    if (!thread) {
      qWarning() << "ThreadAffinity: null thread";
      return false;
    }

#ifdef __linux__

    pthread_t native_thread =
        reinterpret_cast<pthread_t>(thread->currentThreadId());

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    int result =
        pthread_setaffinity_np(native_thread, sizeof(cpu_set_t), &cpuset);

    if (result == 0) {
      return true;
    } else {
      qWarning() << "ThreadAffinity: Failed to pin thread to core" << core_id
                 << "error:" << result;
      return false;
    }
#else
    return false;
#endif
  }

  static bool pin_current_thread_to_core(int core_id) {
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    int result =
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    if (result == 0) {
      return true;
    } else {
      qWarning() << "ThreadAffinity: Failed to pin current thread, error:"
                 << result;
      return false;
    }
#else
    Q_UNUSED(core_id);
    return false;
#endif
  }

  static bool pin_to_cores(QThread *thread, const std::vector<int> &core_ids) {
    if (!thread || core_ids.empty()) {
      qWarning() << "ThreadAffinity: invalid parameters";
      return false;
    }

#ifdef __linux__
    pthread_t native_thread =
        reinterpret_cast<pthread_t>(thread->currentThreadId());

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int core_id : core_ids) {
      CPU_SET(core_id, &cpuset);
    }

    int result =
        pthread_setaffinity_np(native_thread, sizeof(cpu_set_t), &cpuset);

    if (result == 0) {
      return true;
    } else {
      qWarning() << "ThreadAffinity: Failed to pin thread, error:" << result;
      return false;
    }
#else
    Q_UNUSED(core_ids);
    return false;
#endif
  }

  static int get_core_count() {
#ifdef __linux__
    return static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
#else
    return QThread::idealThreadCount();
#endif
  }

  static std::vector<int> get_current_affinity() {
    std::vector<int> cores;

#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    if (pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) ==
        0) {
      for (int i = 0; i < CPU_SETSIZE; ++i) {
        if (CPU_ISSET(i, &cpuset)) {
          cores.push_back(i);
        }
      }
    }
#endif

    return cores;
  }

  static bool reset_affinity(QThread *thread) {
    if (!thread) {
      return false;
    }

#ifdef __linux__
    pthread_t native_thread =
        reinterpret_cast<pthread_t>(thread->currentThreadId());

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    int core_count = get_core_count();
    for (int i = 0; i < core_count; ++i) {
      CPU_SET(i, &cpuset);
    }

    int result =
        pthread_setaffinity_np(native_thread, sizeof(cpu_set_t), &cpuset);
    return result == 0;
#else
    Q_UNUSED(thread);
    return false;
#endif
  }

  struct AffinityStrategy {
    int render_core{-1};
    int main_core{-1};
    std::vector<int> worker_cores;

    static AffinityStrategy auto_detect() {
      AffinityStrategy strategy;
      int core_count = get_core_count();

      if (core_count >= 8) {

        strategy.main_core = 0;
        strategy.render_core = 1;

        strategy.worker_cores = {2, 3};
      } else if (core_count >= 4) {

        strategy.main_core = 0;
        strategy.render_core = 2;
        strategy.worker_cores = {1, 3};
      } else {

        strategy.main_core = -1;
        strategy.render_core = -1;
      }

      return strategy;
    }
  };
};

} // namespace Render
