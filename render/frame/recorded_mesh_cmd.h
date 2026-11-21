#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <vector>

#include "render/frame/material.h"

namespace Render::GL {

class Mesh;
class Texture;
class Shader;

struct RecordedMeshCmd {
  Mesh* mesh{nullptr};
  Texture* texture{nullptr};
  Shader* shader{nullptr};

  const Material* material{nullptr};

  QMatrix4x4 local_model;
  QVector3D color{1.0F, 1.0F, 1.0F};
  float alpha{1.0F};
  int material_id{0};
};

struct PoseTemplate {
  std::vector<RecordedMeshCmd> commands;
};

} // namespace Render::GL
