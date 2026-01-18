#include "formation_arrow.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include <QMatrix4x4>
#include <cmath>
#include <numbers>
#include <qvectornd.h>

namespace Render::GL {

void render_formation_arrow(Renderer *renderer, ResourceManager *resources,
                          const FormationPlacementInfo &placement) {
  if ((renderer == nullptr) || (resources == nullptr) || !placement.active) {
    return;
  }

  float const visual_angle_degrees = placement.angle_degrees + 180.0F;

  QVector3D const arrow_main(0.1F, 0.7F, 0.9F);
  QVector3D const arrow_accent(0.0F, 0.9F, 1.0F);
  QVector3D const arrow_edge(0.05F, 0.4F, 0.6F);

  float const arrow_length = 1.5F;
  float const arrow_width = 0.15F;
  float const arrow_head_size = 0.6F;
  float const arrow_height = 0.125F;

  float const base_y = placement.position.y() + 0.12F;

  {
    QMatrix4x4 shaft_model;
    shaft_model.translate(placement.position.x(), base_y,
                          placement.position.z());
    shaft_model.rotate(visual_angle_degrees, 0.0F, 1.0F, 0.0F);
    shaft_model.translate(0.0F, 0.0F, -arrow_length * 0.25F);
    shaft_model.scale(arrow_width * 0.3F, arrow_height * 0.8F,
                      arrow_length * 0.5F);

    renderer->mesh(resources->unit(), shaft_model, arrow_main,
                   resources->white(), 0.85F);
  }

  float const head_tip_z = -arrow_length * 0.55F;
  float const stick_len = arrow_head_size * 0.7F;
  float const stick_thickness = 0.05F;
  float const stick_height = arrow_height * 0.8F;
  float const head_angle_deg = 35.0F;

  {
    QMatrix4x4 head_left;
    head_left.translate(placement.position.x(), base_y, placement.position.z());
    head_left.rotate(visual_angle_degrees, 0.0F, 1.0F, 0.0F);
    head_left.translate(-arrow_head_size * 0.22F, 0.0F, head_tip_z);
    head_left.rotate(-head_angle_deg, 0.0F, 1.0F, 0.0F);

    head_left.translate(0.0F, 0.0F, stick_len * 0.5F);
    head_left.scale(stick_thickness, stick_height, stick_len);

    renderer->mesh(resources->unit(), head_left, arrow_accent,
                   resources->white(), 0.95F);
  }

  {
    QMatrix4x4 head_right;
    head_right.translate(placement.position.x(), base_y,
                         placement.position.z());
    head_right.rotate(visual_angle_degrees, 0.0F, 1.0F, 0.0F);
    head_right.translate(arrow_head_size * 0.22F, 0.0F, head_tip_z);
    head_right.rotate(head_angle_deg, 0.0F, 1.0F, 0.0F);
    head_right.translate(0.0F, 0.0F, stick_len * 0.5F);
    head_right.scale(stick_thickness, stick_height, stick_len);

    renderer->mesh(resources->unit(), head_right, arrow_accent,
                   resources->white(), 0.95F);
  }

  {
    QMatrix4x4 edge_model;
    edge_model.translate(placement.position.x(), base_y + arrow_height * 0.3F,
                         placement.position.z());
    edge_model.rotate(visual_angle_degrees, 0.0F, 1.0F, 0.0F);
    edge_model.translate(arrow_width * 0.1F, 0.0F, -arrow_length * 0.2F);
    edge_model.scale(0.05F, 0.04F, arrow_length * 0.45F);

    renderer->mesh(resources->unit(), edge_model, arrow_accent,
                   resources->white(), 0.6F);
  }
}

} // namespace Render::GL
