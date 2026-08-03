#include "instance_draw_guard.h"

#include <QDebug>

namespace Render::GL::BackendPipelines {

InstanceDrawGuard::InstanceDrawGuard(const char* tag)
    : m_tag(tag != nullptr ? tag : "unnamed") {
}

auto InstanceDrawGuard::clamp(std::size_t requested,
                              std::size_t resident) -> std::size_t {
  if (requested <= resident) {
    return requested;
  }

  ++m_overflows;
  if (!m_reported) {
    m_reported = true;
    qWarning() << "InstanceDrawGuard:" << m_tag << "asked to draw" << requested
               << "instances but only" << resident
               << "are resident in its instance buffer; clamping. Further "
                  "overflows on this buffer stay silent.";
  }
  return resident;
}

} // namespace Render::GL::BackendPipelines
