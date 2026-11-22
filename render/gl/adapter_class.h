#pragma once

#include <QLatin1String>
#include <QRegularExpression>
#include <QString>

#include "render/graphics_settings.h"

namespace Render::GL {

// Rough class of a GPU from its GL_RENDERER string, used only to pick a
// sensible preset before the player has chosen one. It errs towards "discrete"
// for anything it does not recognise: the battle-time frame-rate governor
// steps down later if that guess was too generous.
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
    // Discrete Arc cards carry a model number ("Arc(TM) A770", "B580");
    // the integrated Arc in Core Ultra laptops does not.
    static const QRegularExpression discrete_arc(
        QStringLiteral("Arc\\S*\\s+[AB]\\d{3}"),
        QRegularExpression::CaseInsensitiveOption);
    if (discrete_arc.match(renderer).hasMatch()) {
      return AdapterClass::Discrete;
    }
    if (has("Iris") || has("Arc") || has("Xe")) {
      return AdapterClass::CapableIntegrated;
    }
    // HD/UHD Graphics and the generic Mesa "Intel(R) Graphics" names.
    return AdapterClass::BasicIntegrated;
  }

  if (has("Radeon") || has("AMD")) {
    // Discrete Radeons name a series: RX, R9, Pro, HD 7xxx, Vega 56/64...
    static const QRegularExpression discrete_radeon(
        QStringLiteral(
            "\\b(RX|R[579]|Pro|FirePro|Vega\\s*(56|64)|VII|HD\\s*\\d{4})\\b"),
        QRegularExpression::CaseInsensitiveOption);
    if (discrete_radeon.match(renderer).hasMatch()) {
      return AdapterClass::Discrete;
    }
    // "AMD Radeon(TM) Graphics", "Radeon Vega 8", "Radeon 780M": APUs.
    return AdapterClass::CapableIntegrated;
  }

  if (has("GeForce MX") || has("Mali") || has("Adreno") || has("PowerVR")) {
    return AdapterClass::CapableIntegrated;
  }

  return AdapterClass::Discrete;
}

// Highest preset worth starting on for this class of adapter.
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
