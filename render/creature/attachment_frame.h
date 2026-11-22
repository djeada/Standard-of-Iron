#pragma once

#include <QVector3D>

namespace Render::GL {

// An oriented frame on a body (head, hand, saddle) that equipment attaches to.
struct AttachmentFrame {
  QVector3D origin{0.0F, 0.0F, 0.0F};
  QVector3D right{1.0F, 0.0F, 0.0F};
  QVector3D up{0.0F, 1.0F, 0.0F};
  QVector3D forward{0.0F, 0.0F, 1.0F};
  float radius{0.0F};
  float depth{0.0F};
};

using HeadFrame = AttachmentFrame;

} // namespace Render::GL
