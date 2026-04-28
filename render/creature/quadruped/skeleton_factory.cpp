#include "skeleton_factory.h"

namespace Render::Creature::Quadruped {

namespace {

auto push_bone(TopologyStorage &out, std::string_view name,
               BoneIndex parent) -> BoneIndex {
  out.owned_names.emplace_back(name);
  out.bones.push_back(BoneDef{out.owned_names.back(), parent});
  return static_cast<BoneIndex>(out.bones.size() - 1U);
}

} // namespace

auto make_topology(const TopologyOptions &options) -> TopologyStorage {
  TopologyStorage out;
  out.bones.reserve(17U);

  out.root = push_bone(out, "Root", kInvalidBone);
  if (options.include_body) {
    out.body = push_bone(out, "Body", out.root);
  }

  out.shoulder_fl = push_bone(out, "ShoulderFL", out.root);
  out.knee_fl = push_bone(out, "KneeFL", out.shoulder_fl);
  out.foot_fl = push_bone(out, "FootFL", out.knee_fl);

  out.shoulder_fr = push_bone(out, "ShoulderFR", out.root);
  out.knee_fr = push_bone(out, "KneeFR", out.shoulder_fr);
  out.foot_fr = push_bone(out, "FootFR", out.knee_fr);

  out.shoulder_bl = push_bone(out, "ShoulderBL", out.root);
  out.knee_bl = push_bone(out, "KneeBL", out.shoulder_bl);
  out.foot_bl = push_bone(out, "FootBL", out.knee_bl);

  out.shoulder_br = push_bone(out, "ShoulderBR", out.root);
  out.knee_br = push_bone(out, "KneeBR", out.shoulder_br);
  out.foot_br = push_bone(out, "FootBR", out.knee_br);

  BoneIndex head_parent = out.root;
  if (options.include_neck_top) {
    out.neck_top = push_bone(out, "NeckTop", out.root);
    head_parent = out.neck_top;
  }
  if (options.include_head) {
    out.head = push_bone(out, "Head", head_parent);
  }
  if (options.include_appendage_tip) {
    BoneIndex const parent = out.head != kInvalidBone ? out.head : head_parent;
    out.appendage_tip = push_bone(out, options.appendage_tip_name, parent);
  }

  return out;
}

} // namespace Render::Creature::Quadruped
