#pragma once

#include <QLatin1String>
#include <QRegularExpression>
#include <QString>

#include "render/frame/graphics_settings.h"

namespace Render::GL {

enum class AdapterClass {
  Software,
  BasicIntegrated,
  CapableIntegrated,
  Discrete,
};

[[nodiscard]] inline auto classify_adapter(const QString& renderer) -> AdapterClass {
  auto has = [&renderer](const char* marker) {
    return renderer.contains(QLatin1String(marker), Qt::CaseInsensitive);
  };

  for (const char* marker : {"llvmpipe",
                             "softpipe",
                             "swrast",
                             "SwiftShader",
                             "Microsoft Basic Render",
                             "GDI Generic",
                             "Apple Software Renderer"}) {
    if (has(marker)) {
      return AdapterClass::Software;
    }
  }

  if (has("Intel")) {

    static const QRegularExpression discrete_arc(
        QStringLiteral("Arc\\S*\\s+[AB]\\d{3}"),
        QRegularExpression::CaseInsensitiveOption);
    if (discrete_arc.match(renderer).hasMatch()) {
      return AdapterClass::Discrete;
    }
    if (has("Iris") || has("Arc") || has("Xe")) {
      return AdapterClass::CapableIntegrated;
    }

    return AdapterClass::BasicIntegrated;
  }

  if (has("Radeon") || has("AMD")) {

    static const QRegularExpression discrete_radeon(
        QStringLiteral(
            "\\b(RX|R[579]|Pro|FirePro|Vega\\s*(56|64)|VII|HD\\s*\\d{4})\\b"),
        QRegularExpression::CaseInsensitiveOption);
    if (discrete_radeon.match(renderer).hasMatch()) {
      return AdapterClass::Discrete;
    }

    return AdapterClass::CapableIntegrated;
  }

  if (has("GeForce MX") || has("Mali") || has("Adreno") || has("PowerVR")) {
    return AdapterClass::CapableIntegrated;
  }

  return AdapterClass::Discrete;
}

[[nodiscard]] inline auto
first_run_quality_cap(AdapterClass adapter_class) -> Render::GraphicsQuality {
  switch (adapter_class) {
  case AdapterClass::Software:
  case AdapterClass::BasicIntegrated:
    return Render::GraphicsQuality::Low;
  case AdapterClass::CapableIntegrated:
    return Render::GraphicsQuality::Medium;
  case AdapterClass::Discrete:
    return Render::GraphicsQuality::Ultra;
  }
  return Render::GraphicsQuality::Ultra;
}

} // namespace Render::GL
