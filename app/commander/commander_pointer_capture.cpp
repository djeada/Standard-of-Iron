#include "app/commander/commander_pointer_capture.h"

namespace App::Core {

void CommanderPointerCapture::release() {
  m_captured = false;
  m_anchor_valid = false;
  m_rest_valid = false;
  m_refused_warps = 0;
  m_polls_since_refusal = 0;
}

auto CommanderPointerCapture::recentre(PointerDevice& device,
                                       const QPoint& here,
                                       const QPoint& centre) -> QPoint {
  if (here == centre || (m_rest_valid && here == m_rest)) {
    return here;
  }
  if (!warp_supported() && ++m_polls_since_refusal < k_unsupported_retry_polls) {
    return here;
  }
  m_polls_since_refusal = 0;

  device.warp(centre);
  const QPoint landed = device.position();
  if (landed == here) {
    if (m_refused_warps < k_refusals_before_unsupported) {
      ++m_refused_warps;
    }
    return landed;
  }
  m_refused_warps = 0;
  m_rest = landed;
  m_rest_valid = true;
  return landed;
}

auto CommanderPointerCapture::poll(PointerDevice& device,
                                   const QPoint& centre) -> QPoint {
  if (!m_captured) {
    m_anchor_valid = false;
    return {};
  }

  const QPoint here = device.position();
  if (!m_anchor_valid) {
    m_anchor = recentre(device, here, centre);
    m_anchor_valid = true;
    return {};
  }

  const QPoint travel = here - m_anchor;
  m_anchor = recentre(device, here, centre);
  return travel;
}

} // namespace App::Core
