#pragma once

#include "rig_def.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>

namespace Render::GL {
class ISubmitter;
struct Material;
} // namespace Render::GL

namespace Render::RigDSL {

class AnchorResolver {
public:
  virtual ~AnchorResolver() = default;

  [[nodiscard]] virtual auto resolve(AnchorId id) const -> QVector3D = 0;
};

class PaletteResolver {
public:
  virtual ~PaletteResolver() = default;
  [[nodiscard]] virtual auto resolve(PaletteSlot slot) const -> QVector3D = 0;
};

class ScalarResolver {
public:
  virtual ~ScalarResolver() = default;
  [[nodiscard]] virtual auto resolve(ScalarId id) const -> float = 0;
};

struct InterpretContext {
  QMatrix4x4 model;
  const AnchorResolver *anchors{nullptr};
  const PaletteResolver *palette{nullptr};
  const ScalarResolver *scalars{nullptr};
  Render::GL::Material *material{nullptr};
  std::uint8_t lod{0};
  float global_alpha{1.0F};
};

void render_rig(const RigDef &def, const InterpretContext &ctx,
                Render::GL::ISubmitter &submitter);

void render_part(const PartDef &part, const InterpretContext &ctx,
                 Render::GL::ISubmitter &submitter);

} // namespace Render::RigDSL
