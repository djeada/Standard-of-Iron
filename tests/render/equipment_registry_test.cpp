#include <atomic>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

#include "render/equipment/equipment_registry.h"
#include "render/equipment/horse_equipment_archetype.h"
#include "render/equipment/humanoid_equipment_archetype.h"

using namespace Render::GL;

namespace {

auto no_attachment_builder(std::uint8_t)
    -> std::vector<Render::Creature::StaticAttachmentSpec> {
  return {};
}

} // namespace

class EquipmentRegistryTest : public ::testing::Test {
protected:
  void SetUp() override { registry = &EquipmentRegistry::instance(); }

  EquipmentRegistry* registry = nullptr;
};

TEST_F(EquipmentRegistryTest, SingletonInstance) {
  auto& instance1 = EquipmentRegistry::instance();
  auto& instance2 = EquipmentRegistry::instance();

  EXPECT_EQ(&instance1, &instance2);
}

TEST_F(EquipmentRegistryTest, RegisterAndResolveEquipmentId) {
  registry->register_equipment_id(EquipmentCategory::Helmet, "iron_helmet");

  EXPECT_TRUE(registry->has(EquipmentCategory::Helmet, "iron_helmet"));
  const auto handle =
      registry->resolve_handle(EquipmentCategory::Helmet, "iron_helmet");
  EXPECT_NE(handle, k_invalid_equipment_handle);
  EXPECT_TRUE(registry->has(handle));
}

TEST_F(EquipmentRegistryTest, RegisterMultipleInSameCategory) {
  registry->register_equipment_id(EquipmentCategory::Helmet, "helmet_1");
  registry->register_equipment_id(EquipmentCategory::Helmet, "helmet_2");

  const auto handle1 = registry->resolve_handle(EquipmentCategory::Helmet, "helmet_1");
  const auto handle2 = registry->resolve_handle(EquipmentCategory::Helmet, "helmet_2");

  EXPECT_NE(handle1, k_invalid_equipment_handle);
  EXPECT_NE(handle2, k_invalid_equipment_handle);
  EXPECT_NE(handle1, handle2);
}

TEST_F(EquipmentRegistryTest, RegisterAcrossDifferentCategories) {
  registry->register_equipment_id(EquipmentCategory::Helmet, "item_1");
  registry->register_equipment_id(EquipmentCategory::Armor, "item_2");
  registry->register_equipment_id(EquipmentCategory::Weapon, "item_3");

  EXPECT_TRUE(registry->has(EquipmentCategory::Helmet, "item_1"));
  EXPECT_TRUE(registry->has(EquipmentCategory::Armor, "item_2"));
  EXPECT_TRUE(registry->has(EquipmentCategory::Weapon, "item_3"));

  EXPECT_FALSE(registry->has(EquipmentCategory::Helmet, "item_2"));
  EXPECT_FALSE(registry->has(EquipmentCategory::Armor, "item_1"));
}

TEST_F(EquipmentRegistryTest, ResolveHandleForMissingEquipment) {
  const auto handle =
      registry->resolve_handle(EquipmentCategory::Weapon, "missing_weapon");

  EXPECT_EQ(handle, k_invalid_equipment_handle);
  EXPECT_FALSE(registry->has(handle));
}

TEST_F(EquipmentRegistryTest, DuplicateRegistrationKeepsStableHandle) {
  registry->register_equipment_id(EquipmentCategory::Armor, "stable_armor");
  const auto handle_v1 =
      registry->resolve_handle(EquipmentCategory::Armor, "stable_armor");

  registry->register_equipment_id(EquipmentCategory::Armor, "stable_armor");
  const auto handle_v2 =
      registry->resolve_handle(EquipmentCategory::Armor, "stable_armor");

  EXPECT_EQ(handle_v1, handle_v2);
  EXPECT_NE(handle_v1, k_invalid_equipment_handle);
}

TEST_F(EquipmentRegistryTest, PlaceholderEquipmentResolvesStableHandle) {
  registry->register_placeholder_equipment(EquipmentCategory::Armor,
                                           "placeholder_robe");

  const auto handle =
      registry->resolve_handle(EquipmentCategory::Armor, "placeholder_robe");

  EXPECT_NE(handle, k_invalid_equipment_handle);
  EXPECT_TRUE(registry->has(EquipmentCategory::Armor, "placeholder_robe"));
  EXPECT_TRUE(registry->has(handle));
}

TEST_F(EquipmentRegistryTest, HorseEquipmentIdsShareHandleRegistry) {
  registry->register_equipment_id(EquipmentCategory::HorseTack, "horse_bridle_handle");

  const auto handle =
      registry->resolve_handle(EquipmentCategory::HorseTack, "horse_bridle_handle");

  EXPECT_NE(handle, k_invalid_equipment_handle);
  EXPECT_TRUE(registry->has(handle));
}

TEST_F(EquipmentRegistryTest, HumanoidEquipmentArchetypeResolveIsThreadSafe) {
  registry->register_placeholder_equipment(EquipmentCategory::Armor,
                                           "thread_safe_test_armor");
  registry->register_placeholder_equipment(EquipmentCategory::Helmet,
                                           "thread_safe_test_helmet");
  const auto armor =
      registry->resolve_handle(EquipmentCategory::Armor, "thread_safe_test_armor");
  const auto helmet =
      registry->resolve_handle(EquipmentCategory::Helmet, "thread_safe_test_helmet");
  ASSERT_NE(armor, k_invalid_equipment_handle);
  ASSERT_NE(helmet, k_invalid_equipment_handle);

  HumanoidEquipmentContribution contribution{};
  contribution.build_attachments = &no_attachment_builder;
  contribution.role_count = 1U;
  register_humanoid_equipment_contribution(armor, contribution);
  register_humanoid_equipment_contribution(helmet, contribution);

  constexpr int k_thread_count = 8;
  constexpr int k_iterations = 64;
  std::atomic<bool> failed{false};
  std::vector<Render::Creature::ArchetypeId> ids(
      static_cast<std::size_t>(k_thread_count), Render::Creature::k_invalid_archetype);
  std::vector<std::thread> threads;
  threads.reserve(k_thread_count);

  for (int t = 0; t < k_thread_count; ++t) {
    threads.emplace_back([&, t] {
      for (int i = 0; i < k_iterations; ++i) {
        const std::array<EquipmentHandle, 2> handles{armor, helmet};
        const auto id = resolve_humanoid_equipment_archetype(
            "thread_safe_test_loadout",
            Render::Creature::ArchetypeRegistry::k_humanoid_base,
            handles);
        if (id == Render::Creature::k_invalid_archetype) {
          failed.store(true);
          return;
        }
        if (ids[static_cast<std::size_t>(t)] == Render::Creature::k_invalid_archetype) {
          ids[static_cast<std::size_t>(t)] = id;
        } else if (ids[static_cast<std::size_t>(t)] != id) {
          failed.store(true);
          return;
        }
      }
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }

  ASSERT_FALSE(failed.load());
  const auto expected = ids.front();
  ASSERT_NE(expected, Render::Creature::k_invalid_archetype);
  for (const auto id : ids) {
    EXPECT_EQ(id, expected);
  }
}

TEST_F(EquipmentRegistryTest, HorseEquipmentArchetypeResolveIsThreadSafe) {
  registry->register_placeholder_equipment(EquipmentCategory::HorseTack,
                                           "thread_safe_test_saddle");
  registry->register_placeholder_equipment(EquipmentCategory::HorseArmor,
                                           "thread_safe_test_barding");
  const auto saddle =
      registry->resolve_handle(EquipmentCategory::HorseTack, "thread_safe_test_saddle");
  const auto barding = registry->resolve_handle(EquipmentCategory::HorseArmor,
                                                "thread_safe_test_barding");
  ASSERT_NE(saddle, k_invalid_equipment_handle);
  ASSERT_NE(barding, k_invalid_equipment_handle);

  HorseEquipmentContribution contribution{};
  contribution.build_attachments = &no_attachment_builder;
  contribution.role_count = 1U;
  register_horse_equipment_contribution(saddle, contribution);
  register_horse_equipment_contribution(barding, contribution);

  constexpr int k_thread_count = 8;
  constexpr int k_iterations = 64;
  std::atomic<bool> failed{false};
  std::vector<Render::Creature::ArchetypeId> ids(
      static_cast<std::size_t>(k_thread_count), Render::Creature::k_invalid_archetype);
  std::vector<std::thread> threads;
  threads.reserve(k_thread_count);

  for (int t = 0; t < k_thread_count; ++t) {
    threads.emplace_back([&, t] {
      for (int i = 0; i < k_iterations; ++i) {
        const std::array<EquipmentHandle, 2> handles{saddle, barding};
        const auto id = resolve_horse_equipment_archetype(
            "thread_safe_test_horse_loadout",
            Render::Creature::ArchetypeRegistry::k_horse_base,
            handles);
        if (id == Render::Creature::k_invalid_archetype) {
          failed.store(true);
          return;
        }
        if (ids[static_cast<std::size_t>(t)] == Render::Creature::k_invalid_archetype) {
          ids[static_cast<std::size_t>(t)] = id;
        } else if (ids[static_cast<std::size_t>(t)] != id) {
          failed.store(true);
          return;
        }
      }
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }

  ASSERT_FALSE(failed.load());
  const auto expected = ids.front();
  ASSERT_NE(expected, Render::Creature::k_invalid_archetype);
  for (const auto id : ids) {
    EXPECT_EQ(id, expected);
  }
}
