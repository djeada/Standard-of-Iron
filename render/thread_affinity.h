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
  static bool pinToCore(QThread *thread, int coreId) {
    if (!thread) {
      qWarning() << "ThreadAffinity: null thread";
      return false;
    }

#ifdef __linux__

    pthread_t nativeThread =
        reinterpret_cast<pthread_t>(thread->currentThreadId());

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(coreId, &cpuset);

    int result =
        pthread_setaffinity_np(nativeThread, sizeof(cpu_set_t), &cpuset);

    if (result == 0) {
      qDebug() << "ThreadAffinity: Pinned thread to core" << coreId;
      return true;
    } else {
      qWarning() << "ThreadAffinity: Failed to pin thread to core" << coreId
                 << "error:" << result;
      return false;
    }
#else
    qDebug() << "ThreadAffinity: Not supported on this platform";
    return false;
#endif
  }

  static bool pinCurrentThreadToCore(int coreId) {
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(coreId, &cpuset);

    int result =
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    if (result == 0) {
      qDebug() << "ThreadAffinity: Pinned current thread to core" << coreId;
      return true;
    } else {
      qWarning() << "ThreadAffinity: Failed to pin current thread, error:"
                 << result;
      return false;
    }
#else
    Q_UNUSED(coreId);
    qDebug() << "ThreadAffinity: Not supported on this platform";
    return false;
#endif
  }

  static bool pinToCores(QThread *thread, const std::vector<int> &coreIds) {
    if (!thread || coreIds.empty()) {
      qWarning() << "ThreadAffinity: invalid parameters";
      return false;
    }

#ifdef __linux__
    pthread_t nativeThread =
        reinterpret_cast<pthread_t>(thread->currentThreadId());

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int coreId : coreIds) {
      CPU_SET(coreId, &cpuset);
    }

    int result =
        pthread_setaffinity_np(nativeThread, sizeof(cpu_set_t), &cpuset);

    if (result == 0) {
      qDebug() << "ThreadAffinity: Pinned thread to cores:" << coreIds.size();
      return true;
    } else {
      qWarning() << "ThreadAffinity: Failed to pin thread, error:" << result;
      return false;
    }
#else
    Q_UNUSED(coreIds);
    qDebug() << "ThreadAffinity: Not supported on this platform";
    return false;
#endif
  }

  static int getCoreCount() {
#ifdef __linux__
    return static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
#else
    return QThread::idealThreadCount();
#endif
  }

  static std::vector<int> getCurrentAffinity() {
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

  static bool resetAffinity(QThread *thread) {
    if (!thread) {
      return false;
    }

#ifdef __linux__
    pthread_t nativeThread =
        reinterpret_cast<pthread_t>(thread->currentThreadId());

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    int coreCount = getCoreCount();
    for (int i = 0; i < coreCount; ++i) {
      CPU_SET(i, &cpuset);
    }

    int result =
        pthread_setaffinity_np(nativeThread, sizeof(cpu_set_t), &cpuset);
    return result == 0;
#else
    Q_UNUSED(thread);
    return false;
#endif
  }

  struct AffinityStrategy {
    int renderCore{-1};
    int mainCore{-1};
    std::vector<int> workerCores;

    static AffinityStrategy autoDetect() {
      AffinityStrategy strategy;
      int coreCount = getCoreCount();

      if (coreCount >= 8) {

        strategy.mainCore = 0;
        strategy.renderCore = 1;

        strategy.workerCores = {2, 3};
      } else if (coreCount >= 4) {

        strategy.mainCore = 0;
        strategy.renderCore = 2;
        strategy.workerCores = {1, 3};
      } else {

        strategy.mainCore = -1;
        strategy.renderCore = -1;
      }

      return strategy;
    }
  };
};

} // namespace Render
