#pragma once

#include <QPoint>

namespace App::Core {

class PointerDevice {
public:
  PointerDevice() = default;
  PointerDevice(const PointerDevice&) = default;
  PointerDevice(PointerDevice&&) = default;
  auto operator=(const PointerDevice&) -> PointerDevice& = default;
  auto operator=(PointerDevice&&) -> PointerDevice& = default;
  virtual ~PointerDevice() = default;

  [[nodiscard]] virtual auto position() const -> QPoint = 0;
  virtual void warp(const QPoint& global_position) = 0;
};

class CommanderPointerCapture {
public:
  void capture() { m_captured = true; }
  void release();
  void suspend() { m_anchor_valid = false; }

  [[nodiscard]] auto captured() const -> bool { return m_captured; }
  [[nodiscard]] auto warp_supported() const -> bool {
    return m_refused_warps < k_refusals_before_unsupported;
  }

  auto poll(PointerDevice& device, const QPoint& centre) -> QPoint;

private:
  static constexpr int k_refusals_before_unsupported = 3;
  static constexpr int k_unsupported_retry_polls = 30;

  auto
  recentre(PointerDevice& device, const QPoint& here, const QPoint& centre) -> QPoint;

  QPoint m_anchor;
  QPoint m_rest;
  bool m_anchor_valid = false;
  bool m_rest_valid = false;
  bool m_captured = false;
  int m_refused_warps = 0;
  int m_polls_since_refusal = 0;
};

} // namespace App::Core
