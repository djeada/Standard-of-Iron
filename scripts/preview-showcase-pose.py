"""Python twin of Animation::resolve_humanoid_showcase_pose.

Mirrors animation/showcase_pose_manifest.cpp + animation/rig/pose_fk.h exactly so
seated poses can be shaped visually before paying for a build+bake+render.
"""

import math

from PIL import Image, ImageDraw

RIG = dict(
    pelvis_y=0.975,
    neck_rise=0.540,
    head_rise=0.140,
    shoulder_rise=0.450,
    shoulder_half_width=0.252,
    hip_half_width=0.100,
    hip_drop=0.020,
    upper_arm_len=0.320,
    fore_arm_len=0.270,
    upper_leg_len=0.500,
    lower_leg_len=0.470,
)


def rad(d):
    return d * math.pi / 180.0


def mat_mul(a, b):
    return [
        sum(a[r * 3 + k] * b[k * 3 + c] for k in range(3))
        for r in range(3)
        for c in range(3)
    ]


def rot_x(t):
    c, s = math.cos(t), math.sin(t)
    return [1, 0, 0, 0, c, -s, 0, s, c]


def rot_y(t):
    c, s = math.cos(t), math.sin(t)
    return [c, 0, s, 0, 1, 0, -s, 0, c]


def rot_z(t):
    c, s = math.cos(t), math.sin(t)
    return [c, -s, 0, s, c, 0, 0, 0, 1]


IDENT = [1, 0, 0, 0, 1, 0, 0, 0, 1]


def xf(m, v):
    return (
        m[0] * v[0] + m[1] * v[1] + m[2] * v[2],
        m[3] * v[0] + m[4] * v[1] + m[5] * v[2],
        m[6] * v[0] + m[7] * v[1] + m[8] * v[2],
    )


def add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def scaled(a, k):
    return (a[0] * k, a[1] * k, a[2] * k)


def limb_segments(aim, side_sign, bend_sign, pre):
    """aim = (pitch, splay, yaw, bend) in degrees."""
    pitch, splay, yaw, bend = aim
    down = (0.0, -1.0, 0.0)
    base = mat_mul(pre, mat_mul(rot_y(rad(yaw)), rot_z(rad(splay) * side_sign)))
    upper = xf(mat_mul(base, rot_x(-rad(pitch))), down)
    lower = xf(mat_mul(base, rot_x(-rad(pitch + bend_sign * bend))), down)
    return upper, lower


def resolve(key, height_scale=1.0, rig=RIG):
    spine = mat_mul(
        rot_x(rad(key["spine_pitch"])),
        mat_mul(rot_z(rad(key["spine_roll"])), rot_y(rad(key["spine_yaw"]))),
    )

    pelvis = (0.0, rig["pelvis_y"], 0.0)
    neck = add(pelvis, xf(spine, (0.0, rig["neck_rise"], 0.0)))
    sc = add(pelvis, xf(spine, (0.0, rig["shoulder_rise"], 0.0)))
    sh_l = add(sc, xf(spine, (-rig["shoulder_half_width"], 0.0, 0.0)))
    sh_r = add(sc, xf(spine, (rig["shoulder_half_width"], 0.0, 0.0)))
    head = add(
        neck,
        xf(mat_mul(spine, rot_x(rad(key["head_pitch"]))), (0.0, rig["head_rise"], 0.0)),
    )

    au_l, al_l = limb_segments(key["arm_l"], -1.0, 1.0, spine)
    au_r, al_r = limb_segments(key["arm_r"], 1.0, 1.0, spine)
    el_l = add(sh_l, scaled(au_l, rig["upper_arm_len"]))
    hd_l = add(el_l, scaled(al_l, rig["fore_arm_len"]))
    el_r = add(sh_r, scaled(au_r, rig["upper_arm_len"]))
    hd_r = add(el_r, scaled(al_r, rig["fore_arm_len"]))

    hip_l = add(pelvis, (-rig["hip_half_width"], -rig["hip_drop"], 0.0))
    hip_r = add(pelvis, (rig["hip_half_width"], -rig["hip_drop"], 0.0))
    lu_l, ll_l = limb_segments(key["leg_l"], -1.0, -1.0, IDENT)
    lu_r, ll_r = limb_segments(key["leg_r"], 1.0, -1.0, IDENT)
    kn_l = add(hip_l, scaled(lu_l, rig["upper_leg_len"]))
    ft_l = add(kn_l, scaled(ll_l, rig["lower_leg_len"]))
    kn_r = add(hip_r, scaled(lu_r, rig["upper_leg_len"]))
    ft_r = add(kn_r, scaled(ll_r, rig["lower_leg_len"]))

    body = mat_mul(
        rot_x(rad(key["body_pitch"])),
        mat_mul(rot_z(rad(key["body_roll"])), rot_y(rad(key["body_yaw"]))),
    )
    root = (key["root_x"], key["root_y"], key["root_z"])
    h = max(0.1, height_scale)

    def place(p):
        return scaled(add(xf(body, sub(p, pelvis)), root), h)

    return {
        "pelvis": place(pelvis),
        "neck": place(neck),
        "head": place(head),
        "shoulder_l": place(sh_l),
        "shoulder_r": place(sh_r),
        "elbow_l": place(el_l),
        "elbow_r": place(el_r),
        "hand_l": place(hd_l),
        "hand_r": place(hd_r),
        "knee_l": place(kn_l),
        "knee_r": place(kn_r),
        "foot_l": place(ft_l),
        "foot_r": place(ft_r),
    }


BONES = [
    ("pelvis", "neck"),
    ("neck", "head"),
    ("neck", "shoulder_l"),
    ("neck", "shoulder_r"),
    ("shoulder_l", "elbow_l"),
    ("elbow_l", "hand_l"),
    ("shoulder_r", "elbow_r"),
    ("elbow_r", "hand_r"),
    ("pelvis", "knee_l"),
    ("knee_l", "foot_l"),
    ("pelvis", "knee_r"),
    ("knee_r", "foot_r"),
]


def draw(pose, view, size=340, scale=150, label=""):
    """view: 'side' (z,y), 'front' (x,y), 'top' (x,z)."""
    img = Image.new("RGB", (size, size), (24, 26, 32))
    d = ImageDraw.Draw(img)
    cx, cy = size // 2, size - 60

    def proj(p):
        if view == "side":
            return (cx + p[2] * scale, cy - p[1] * scale)
        if view == "front":
            return (cx + p[0] * scale, cy - p[1] * scale)
        return (cx + p[0] * scale, cy - p[2] * scale)

    if view != "top":
        d.line([(0, cy), (size, cy)], fill=(70, 78, 90), width=2)
    for a, b in BONES:
        colour = (
            (235, 180, 90) if a.endswith("_l") or b.endswith("_l") else (150, 200, 235)
        )
        d.line([proj(pose[a]), proj(pose[b])], fill=colour, width=3)
    for _name, p in pose.items():
        x, y = proj(p)
        d.ellipse([x - 3, y - 3, x + 3, y + 3], fill=(240, 240, 240))
    d.text((8, 8), f"{label} [{view}]", fill=(200, 200, 200))
    lowest = min(p[1] for p in pose.values())
    d.text(
        (8, 22),
        f"min y = {lowest:+.3f}",
        fill=(230, 140, 140) if lowest < -0.02 else (140, 230, 150),
    )
    return img


def contact(poses, path):
    cols = len(poses)
    views = ["side", "front", "top"]
    sheet = Image.new("RGB", (340 * cols, 340 * len(views)), (16, 17, 20))
    for c, (label, key) in enumerate(poses):
        pose = resolve(key)
        for r, view in enumerate(views):
            sheet.paste(draw(pose, view, label=label), (340 * c, 340 * r))
    sheet.save(path)
    print(f"wrote {path}")


def key(**kw):
    base = dict(
        t=0.0,
        root_x=0.0,
        root_y=0.975,
        root_z=0.0,
        body_pitch=0.0,
        body_roll=0.0,
        body_yaw=0.0,
        spine_pitch=0.0,
        spine_roll=0.0,
        spine_yaw=0.0,
        head_pitch=0.0,
        blade_pitch=0.0,
        blade_yaw=0.0,
        blade_amount=0.0,
        arm_l=(4.0, 7.0, 0.0, 14.0),
        arm_r=(4.0, 7.0, 0.0, 14.0),
        leg_l=(16.0, 5.0, 0.0, 32.0),
        leg_r=(16.0, 5.0, 0.0, 32.0),
    )
    base.update(kw)
    return base
