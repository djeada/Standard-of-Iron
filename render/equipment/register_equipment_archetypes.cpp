#include "armor/carthage_shoulder_cover.h"
#include "armor/commander_regalia.h"
#include "armor/roman_greaves.h"
#include "armor/roman_shoulder_cover.h"
#include "armor/tool_belt_renderer.h"
#include "helmets/carthage_heavy_helmet.h"
#include "helmets/carthage_light_helmet.h"
#include "helmets/commander_helmets.h"
#include "helmets/headwrap.h"
#include "helmets/historical_helmets.h"
#include "helmets/roman_heavy_helmet.h"
#include "helmets/roman_light_helmet.h"
#include "horse/armor/champion_renderer.h"
#include "horse/armor/crupper_renderer.h"
#include "horse/armor/leather_barding_renderer.h"
#include "horse/armor/scale_barding_renderer.h"
#include "horse/decorations/saddle_bag_renderer.h"
#include "horse/saddles/carthage_saddle_renderer.h"
#include "horse/saddles/light_cavalry_saddle_renderer.h"
#include "horse/saddles/roman_saddle_renderer.h"
#include "horse/tack/blanket_renderer.h"
#include "horse/tack/bridle_renderer.h"
#include "horse/tack/reins_renderer.h"
#include "register_equipment_internal.h"
#include "render_archetype_registry.h"
#include "weapons/roman_scutum.h"

namespace Render::GL::EquipmentRegistration {

void register_equipment_archetypes() {
  auto& ar = RenderArchetypeRegistry::instance();

  ar.register_archetype("roman_light_helmet",
                        [] { (void)roman_light_helmet_archetype(); });
  ar.register_archetype("roman_heavy_helmet",
                        [] { (void)roman_heavy_helmet_archetype(); });
  ar.register_archetype("roman_montefortino_helmet", [] {
    (void)historical_helmet_archetype(HistoricalHelmet::RomanMontefortino);
  });
  ar.register_archetype("roman_boeotian_cavalry_helmet", [] {
    (void)historical_helmet_archetype(HistoricalHelmet::RomanBoeotianCavalry);
  });
  ar.register_archetype("carthage_punic_conical_helmet", [] {
    (void)historical_helmet_archetype(HistoricalHelmet::CarthagePunicConical);
  });
  ar.register_archetype("carthage_thracian_crested_helmet", [] {
    (void)historical_helmet_archetype(HistoricalHelmet::CarthageThracianCrested);
  });
  ar.register_archetype("headwrap_helmet", [] { (void)headwrap_helmet_archetype(); });
  ar.register_archetype("commander_helmet_fabius", [] {
    (void)commander_helmet_archetype(CommanderHelmetStyle::Fabius);
  });
  ar.register_archetype("commander_helmet_scipio", [] {
    (void)commander_helmet_archetype(CommanderHelmetStyle::Scipio);
  });
  ar.register_archetype("commander_helmet_marcellus", [] {
    (void)commander_helmet_archetype(CommanderHelmetStyle::Marcellus);
  });
  ar.register_archetype("commander_helmet_hanno", [] {
    (void)commander_helmet_archetype(CommanderHelmetStyle::Hanno);
  });
  ar.register_archetype("commander_helmet_hasdrubal", [] {
    (void)commander_helmet_archetype(CommanderHelmetStyle::Hasdrubal);
  });
  ar.register_archetype("commander_helmet_hannibal", [] {
    (void)commander_helmet_archetype(CommanderHelmetStyle::Hannibal);
  });
  ar.register_archetype("carthage_light_helmet_shell",
                        [] { (void)carthage_light_helmet_shell_archetype(); });
  ar.register_archetype("carthage_light_helmet_neck_guard",
                        [] { (void)carthage_light_helmet_neck_guard_archetype(); });
  ar.register_archetype("carthage_light_helmet_cheek_guards",
                        [] { (void)carthage_light_helmet_cheek_guards_archetype(); });
  ar.register_archetype("carthage_light_helmet_nasal_guard",
                        [] { (void)carthage_light_helmet_nasal_guard_archetype(); });
  ar.register_archetype("carthage_light_helmet_crest_low",
                        [] { (void)carthage_light_helmet_crest_archetype(false); });
  ar.register_archetype("carthage_light_helmet_crest_high",
                        [] { (void)carthage_light_helmet_crest_archetype(true); });
  ar.register_archetype("carthage_light_helmet_rivets",
                        [] { (void)carthage_light_helmet_rivets_archetype(); });
  ar.register_archetype("carthage_heavy_helmet_shell",
                        [] { (void)carthage_heavy_helmet_shell_archetype(); });
  ar.register_archetype("carthage_heavy_helmet_neck_guard",
                        [] { (void)carthage_heavy_helmet_neck_guard_archetype(); });
  ar.register_archetype("carthage_heavy_helmet_cheek_guards",
                        [] { (void)carthage_heavy_helmet_cheek_guards_archetype(); });
  ar.register_archetype("carthage_heavy_helmet_face_plate",
                        [] { (void)carthage_heavy_helmet_face_plate_archetype(); });
  ar.register_archetype("carthage_heavy_helmet_crest",
                        [] { (void)carthage_heavy_helmet_crest_archetype(); });
  ar.register_archetype("carthage_heavy_helmet_rivets",
                        [] { (void)carthage_heavy_helmet_rivets_archetype(); });

  ar.register_archetype("roman_shoulder_cover",
                        [] { (void)roman_shoulder_cover_archetype(); });
  ar.register_archetype("carthage_shoulder_cover",
                        [] { (void)carthage_shoulder_cover_archetype(); });
  ar.register_archetype("roman_greaves", [] { (void)roman_greaves_archetype(); });

  ar.register_archetype("roman_scutum", [] { (void)roman_scutum_archetype(); });
  ar.register_archetype("commander_regalia_fabius", [] {
    (void)commander_regalia_archetype(CommanderRegaliaStyle::Fabius);
  });
  ar.register_archetype("commander_regalia_scipio", [] {
    (void)commander_regalia_archetype(CommanderRegaliaStyle::Scipio);
  });
  ar.register_archetype("commander_regalia_marcellus", [] {
    (void)commander_regalia_archetype(CommanderRegaliaStyle::Marcellus);
  });
  ar.register_archetype("commander_regalia_hanno", [] {
    (void)commander_regalia_archetype(CommanderRegaliaStyle::Hanno);
  });
  ar.register_archetype("commander_regalia_hasdrubal", [] {
    (void)commander_regalia_archetype(CommanderRegaliaStyle::Hasdrubal);
  });
  ar.register_archetype("commander_regalia_hannibal", [] {
    (void)commander_regalia_archetype(CommanderRegaliaStyle::Hannibal);
  });

  ar.register_archetype("tool_belt_ring", [] { (void)tool_belt_ring_archetype(); });
  ar.register_archetype("tool_belt_buckle", [] { (void)tool_belt_buckle_archetype(); });
  ar.register_archetype("tool_belt_hammer", [] { (void)tool_belt_hammer_archetype(); });
  ar.register_archetype("tool_belt_chisel", [] { (void)tool_belt_chisel_archetype(); });
  ar.register_archetype("tool_belt_saw", [] { (void)tool_belt_saw_archetype(); });
  ar.register_archetype("tool_belt_pouches",
                        [] { (void)tool_belt_pouches_archetype(); });

  ar.register_archetype("reins", [] { (void)reins_archetype(); });
  ar.register_archetype("blanket", [] { (void)blanket_archetype(); });
  ar.register_archetype("bridle", [] { (void)bridle_archetype(); });

  ar.register_archetype("roman_saddle", [] { (void)roman_saddle_archetype(); });
  ar.register_archetype("carthage_saddle", [] { (void)carthage_saddle_archetype(); });
  ar.register_archetype("light_cavalry_saddle",
                        [] { (void)light_cavalry_saddle_archetype(); });

  ar.register_archetype("champion_barding", [] { (void)champion_archetype(); });
  ar.register_archetype("crupper", [] { (void)crupper_archetype(); });
  ar.register_archetype("leather_barding_chest",
                        [] { (void)leather_barding_chest_archetype(); });
  ar.register_archetype("leather_barding_barrel",
                        [] { (void)leather_barding_barrel_archetype(); });
  ar.register_archetype("scale_barding_chest",
                        [] { (void)scale_barding_chest_archetype(); });
  ar.register_archetype("scale_barding_barrel",
                        [] { (void)scale_barding_barrel_archetype(); });
  ar.register_archetype("scale_barding_neck",
                        [] { (void)scale_barding_neck_archetype(); });

  ar.register_archetype("saddle_bag", [] { (void)saddle_bag_archetype(); });
}

} // namespace Render::GL::EquipmentRegistration
