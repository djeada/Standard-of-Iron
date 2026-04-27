#include "horse_mount_archetype.h"

#include "../../../creature/pipeline/unit_visual_spec.h"
#include "../../../horse/dimensions.h"
#include "../../../horse/horse_renderer_base.h"
#include "../../../horse/horse_spec.h"
#include "../horse_attachment_archetype.h"
#include "../tack/reins_renderer.h"

#include <array>
#include <span>

namespace Render::GL {

namespace {

struct Slot {
  std::uint32_t (*saddle_fn)(const HorseVariant &, QVector3D *, std::size_t);
  std::uint32_t (*reins_fn)(const HorseVariant &, QVector3D *, std::size_t);
};
constexpr std::size_t kMaxSlots = 16;
std::array<Slot, kMaxSlots> g_slots{};
std::size_t g_slot_count = 0;

template <std::size_t IDX>
auto trampoline(const void *variant_void, QVector3D *out,
                std::uint32_t base_count,
                std::size_t max_count) noexcept -> std::uint32_t {
  if (variant_void == nullptr || max_count <= base_count) {
    return base_count;
  }
  auto const slot = g_slots[IDX];
  if (slot.saddle_fn == nullptr || slot.reins_fn == nullptr) {
    return base_count;
  }
  const auto &v = *static_cast<const HorseVariant *>(variant_void);
  auto count = base_count;
  count += slot.saddle_fn(v, out + count, max_count - count);
  if (count >= max_count) {
    return count;
  }
  count += slot.reins_fn(v, out + count, max_count - count);
  return count;
}

using ExtraFn = Render::Creature::ArchetypeDescriptor::ExtraRoleColorsFn;

constexpr std::array<ExtraFn, kMaxSlots> g_trampolines = {
    &trampoline<0>,  &trampoline<1>,  &trampoline<2>,  &trampoline<3>,
    &trampoline<4>,  &trampoline<5>,  &trampoline<6>,  &trampoline<7>,
    &trampoline<8>,  &trampoline<9>,  &trampoline<10>, &trampoline<11>,
    &trampoline<12>, &trampoline<13>, &trampoline<14>, &trampoline<15>,
};

} // namespace

auto register_mount_saddle_archetype(
    std::string_view debug_name,
    Render::Creature::StaticAttachmentSpec (*make_static_attachment)(
        std::uint16_t, std::uint8_t, const HorseAttachmentFrame &,
        const QMatrix4x4 &),
    std::uint32_t (*fill_role_colors)(const HorseVariant &, QVector3D *,
                                      std::size_t))
    -> Render::Creature::ArchetypeId {
  if (g_slot_count >= kMaxSlots) {
    return Render::Creature::kInvalidArchetype;
  }

  constexpr std::uint16_t k_root_bone =
      static_cast<std::uint16_t>(Render::Horse::HorseBone::Root);

  constexpr std::uint8_t k_saddle_base_role_byte = 9U;
  constexpr std::uint8_t k_reins_base_role_byte =
      static_cast<std::uint8_t>(k_saddle_base_role_byte + 1U);

  auto const root_bind_matrix =
      Render::Horse::horse_bind_palette()[static_cast<std::size_t>(
          Render::Horse::HorseBone::Root)];
  auto const back_center_bind_frame = horse_baseline_back_center_frame();

  auto const saddle_spec =
      make_static_attachment(k_root_bone, k_saddle_base_role_byte,
                             back_center_bind_frame, root_bind_matrix);
  auto const reins_spec =
      reins_make_static_attachment(k_root_bone, k_reins_base_role_byte,
                                   back_center_bind_frame, root_bind_matrix);

  std::array<Render::Creature::StaticAttachmentSpec, 2> const attachments{
      saddle_spec, reins_spec};

  std::size_t const slot_index = g_slot_count++;
  g_slots[slot_index].saddle_fn = fill_role_colors;
  g_slots[slot_index].reins_fn = &reins_fill_role_colors;

  return Render::Creature::ArchetypeRegistry::instance()
      .register_unit_archetype(
          debug_name, Render::Creature::Pipeline::CreatureKind::Horse,
          std::span<const Render::Creature::StaticAttachmentSpec>(
              attachments.data(), attachments.size()),
          g_trampolines[slot_index]);
}

} // namespace Render::GL
