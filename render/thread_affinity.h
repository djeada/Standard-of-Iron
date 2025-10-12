#pragma once

#include <QThread>
#include <QDebug>

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif

namespace Render {

// Thread affinity manager for pinning render thread to specific CPU cores
// Reduces cache thrashing and context switching overhead
class ThreadAffinity {
public:
  // Pin a thread to a specific CPU core
  static bool pinToCore(QThread *thread, int coreId) {
    if (!thread) {
      qWarning() << "ThreadAffinity: null thread";
      return false;
    }

#ifdef __linux__
    // Get native thread handle
    pthread_t nativeThread = reinterpret_cast<pthread_t>(thread->currentThreadId());
    
    // Create CPU set with single core
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(coreId, &cpuset);
    
    // Set affinity
    int result = pthread_setaffinity_np(nativeThread, sizeof(cpu_set_t), &cpuset);
    
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

  // Pin current thread to a specific CPU core
  static bool pinCurrentThreadToCore(int coreId) {
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(coreId, &cpuset);
    
    int result = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    
    if (result == 0) {
      qDebug() << "ThreadAffinity: Pinned current thread to core" << coreId;
      return true;
    } else {
      qWarning() << "ThreadAffinity: Failed to pin current thread, error:" << result;
      return false;
    }
#else
    Q_UNUSED(coreId);
    qDebug() << "ThreadAffinity: Not supported on this platform";
    return false;
#endif
  }

  // Pin thread to a set of cores (allows migration between specified cores)
  static bool pinToCores(QThread *thread, const std::vector<int> &coreIds) {
    if (!thread || coreIds.empty()) {
      qWarning() << "ThreadAffinity: invalid parameters";
      return false;
    }

#ifdef __linux__
    pthread_t nativeThread = reinterpret_cast<pthread_t>(thread->currentThreadId());
    
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int coreId : coreIds) {
      CPU_SET(coreId, &cpuset);
    }
    
    int result = pthread_setaffinity_np(nativeThread, sizeof(cpu_set_t), &cpuset);
    
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

  // Get number of available CPU cores
  static int getCoreCount() {
#ifdef __linux__
    return static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
#else
    return QThread::idealThreadCount();
#endif
  }

  // Get current thread's affinity
  static std::vector<int> getCurrentAffinity() {
    std::vector<int> cores;

#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    
    if (pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0) {
      for (int i = 0; i < CPU_SETSIZE; ++i) {
        if (CPU_ISSET(i, &cpuset)) {
          cores.push_back(i);
        }
      }
    }
#endif
    
    return cores;
  }

  // Reset thread affinity to all cores
  static bool resetAffinity(QThread *thread) {
    if (!thread) {
      return false;
    }

#ifdef __linux__
    pthread_t nativeThread = reinterpret_cast<pthread_t>(thread->currentThreadId());
    
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    
    // Set all available cores
    int coreCount = getCoreCount();
    for (int i = 0; i < coreCount; ++i) {
      CPU_SET(i, &cpuset);
    }
    
    int result = pthread_setaffinity_np(nativeThread, sizeof(cpu_set_t), &cpuset);
    return result == 0;
#else
    Q_UNUSED(thread);
    return false;
#endif
  }

  // Suggested affinity strategy for game rendering
  struct AffinityStrategy {
    int renderCore{-1};     // Core for render thread (-1 = auto)
    int mainCore{-1};       // Core for main thread (-1 = auto)
    std::vector<int> workerCores; // Cores for worker threads
    
    // Auto-detect good strategy based on CPU topology
    static AffinityStrategy autoDetect() {
      AffinityStrategy strategy;
      int coreCount = getCoreCount();
      
      if (coreCount >= 8) {
        // High-end: Dedicate cores
        strategy.mainCore = 0;
        strategy.renderCore = 1;
        // Reserve cores 2-3 for workers, leave rest for OS
        strategy.workerCores = {2, 3};
      } else if (coreCount >= 4) {
        // Mid-range: Share some cores
        strategy.mainCore = 0;
        strategy.renderCore = 2;
        strategy.workerCores = {1, 3};
      } else {
        // Low-end: No pinning (overhead not worth it)
        strategy.mainCore = -1;
        strategy.renderCore = -1;
      }
      
      return strategy;
    }
  };
};

} // namespace Render

// Usage Example:
//
// // At application startup:
// auto strategy = Render::ThreadAffinity::AffinityStrategy::autoDetect();
// 
// // Pin render thread:
// if (strategy.renderCore >= 0) {
//   Render::ThreadAffinity::pinCurrentThreadToCore(strategy.renderCore);
// }
//
// // Or pin a specific QThread:
// QThread *renderThread = getRenderThread();
// if (strategy.renderCore >= 0) {
//   Render::ThreadAffinity::pinToCore(renderThread, strategy.renderCore);
// }
//
// // Check current affinity:
// auto cores = Render::ThreadAffinity::getCurrentAffinity();
// qDebug() << "Thread running on cores:" << cores;
