#include <array>
#include <atomic>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>

#include "render/equipment/equipment_registry.h"
#include "render/equipment/equipment_submit.h"
#include "render/equipment/horse/tack/bridle_renderer.h"
#include "render/equipment/horse_equipment_archetype.h"
#include "render/equipment/humanoid_equipment_archetype.h"
#include "render/equipment/i_equipment_renderer.h"

using namespace Render::GL;

namespace {

class MockEquipmentRenderer : public IEquipmentRenderer {
public:
  explicit MockEquipmentRenderer(std::string name)
      : m_name(std::move(name)) {}

  void render(const DrawContext&,
              const BodyFrames&,
              const HumanoidPalette&,
              const HumanoidAnimationContext&,
              EquipmentBatch&) override {}

  [[nodiscard]] auto getName() const -> const std::string& { return m_name; }

private:
  std::string m_name;
};

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

TEST_F(EquipmentRegistryTest, RegisterAndGetHelmet) {
  auto helmet = std::make_shared<MockEquipmentRenderer>("test_helmet");
  registry->register_equipment(EquipmentCategory::Helmet, "iron_helmet", helmet);

  auto retrieved = registry->get(EquipmentCategory::Helmet, "iron_helmet");

  ASSERT_NE(retrieved, nullptr);
  EXPECT_EQ(retrieved, helmet);
}

TEST_F(EquipmentRegistryTest, RegisterAndGetArmor) {
  auto armor = std::make_shared<MockEquipmentRenderer>("test_armor");
  registry->register_equipment(EquipmentCategory::Armor, "chainmail", armor);

  auto retrieved = registry->get(EquipmentCategory::Armor, "chainmail");

  ASSERT_NE(retrieved, nullptr);
  EXPECT_EQ(retrieved, armor);
}

TEST_F(EquipmentRegistryTest, RegisterAndGetWeapon) {
  auto weapon = std::make_shared<MockEquipmentRenderer>("test_weapon");
  registry->register_equipment(EquipmentCategory::Weapon, "longsword", weapon);

  auto retrieved = registry->get(EquipmentCategory::Weapon, "longsword");

  ASSERT_NE(retrieved, nullptr);
  EXPECT_EQ(retrieved, weapon);
}

TEST_F(EquipmentRegistryTest, GetNonExistentEquipment) {
  auto retrieved = registry->get(EquipmentCategory::Helmet, "non_existent_helmet");

  EXPECT_EQ(retrieved, nullptr);
}

TEST_F(EquipmentRegistryTest, HasEquipment) {
  auto helmet = std::make_shared<MockEquipmentRenderer>("test_helmet");
  registry->register_equipment(EquipmentCategory::Helmet, "steel_helmet", helmet);

  EXPECT_TRUE(registry->has(EquipmentCategory::Helmet, "steel_helmet"));
  EXPECT_FALSE(registry->has(EquipmentCategory::Helmet, "bronze_helmet"));
  EXPECT_FALSE(registry->has(EquipmentCategory::Armor, "steel_helmet"));
}

TEST_F(EquipmentRegistryTest, RegisterMultipleInSameCategory) {
  auto helmet1 = std::make_shared<MockEquipmentRenderer>("helmet_1");
  auto helmet2 = std::make_shared<MockEquipmentRenderer>("helmet_2");

  registry->register_equipment(EquipmentCategory::Helmet, "helmet_1", helmet1);
  registry->register_equipment(EquipmentCategory::Helmet, "helmet_2", helmet2);

  auto retrieved1 = registry->get(EquipmentCategory::Helmet, "helmet_1");
  auto retrieved2 = registry->get(EquipmentCategory::Helmet, "helmet_2");

  ASSERT_NE(retrieved1, nullptr);
  ASSERT_NE(retrieved2, nullptr);
  EXPECT_EQ(retrieved1, helmet1);
  EXPECT_EQ(retrieved2, helmet2);
}

TEST_F(EquipmentRegistryTest, RegisterAcrossDifferentCategories) {
  auto helmet = std::make_shared<MockEquipmentRenderer>("helmet");
  auto armor = std::make_shared<MockEquipmentRenderer>("armor");
  auto weapon = std::make_shared<MockEquipmentRenderer>("weapon");

  registry->register_equipment(EquipmentCategory::Helmet, "item_1", helmet);
  registry->register_equipment(EquipmentCategory::Armor, "item_2", armor);
  registry->register_equipment(EquipmentCategory::Weapon, "item_3", weapon);

  EXPECT_TRUE(registry->has(EquipmentCategory::Helmet, "item_1"));
  EXPECT_TRUE(registry->has(EquipmentCategory::Armor, "item_2"));
  EXPECT_TRUE(registry->has(EquipmentCategory::Weapon, "item_3"));

  EXPECT_FALSE(registry->has(EquipmentCategory::Helmet, "item_2"));
  EXPECT_FALSE(registry->has(EquipmentCategory::Armor, "item_1"));
}

TEST_F(EquipmentRegistryTest, RegisterNullRenderer) {
  registry->register_equipment(EquipmentCategory::Helmet, "null_helmet", nullptr);

  auto retrieved = registry->get(EquipmentCategory::Helmet, "null_helmet");
  EXPECT_EQ(retrieved, nullptr);
  EXPECT_FALSE(registry->has(EquipmentCategory::Helmet, "null_helmet"));
}

TEST_F(EquipmentRegistryTest, OverwriteExistingEquipment) {
  auto helmet1 = std::make_shared<MockEquipmentRenderer>("helmet_v1");
  auto helmet2 = std::make_shared<MockEquipmentRenderer>("helmet_v2");

  registry->register_equipment(EquipmentCategory::Helmet, "helmet", helmet1);
  registry->register_equipment(EquipmentCategory::Helmet, "helmet", helmet2);

  auto retrieved = registry->get(EquipmentCategory::Helmet, "helmet");

  ASSERT_NE(retrieved, nullptr);
  EXPECT_EQ(retrieved, helmet2);
}

TEST_F(EquipmentRegistryTest, NationSpecificWeapons) {
  auto sword_carthage = std::make_shared<MockEquipmentRenderer>("sword_carthage");
  auto sword_roman = std::make_shared<MockEquipmentRenderer>("sword_roman");

  registry->register_equipment(
      EquipmentCategory::Weapon, "sword_carthage", sword_carthage);
  registry->register_equipment(EquipmentCategory::Weapon, "sword_roman", sword_roman);

  EXPECT_TRUE(registry->has(EquipmentCategory::Weapon, "sword_carthage"));
  EXPECT_TRUE(registry->has(EquipmentCategory::Weapon, "sword_roman"));

  auto retrieved_carthage = registry->get(EquipmentCategory::Weapon, "sword_carthage");
  auto retrieved_roman = registry->get(EquipmentCategory::Weapon, "sword_roman");

  ASSERT_NE(retrieved_carthage, nullptr);
  ASSERT_NE(retrieved_roman, nullptr);

  EXPECT_EQ(retrieved_carthage, sword_carthage);
  EXPECT_EQ(retrieved_roman, sword_roman);
}

TEST_F(EquipmentRegistryTest, ResolveHandleForRegisteredEquipment) {
  auto helmet = std::make_shared<MockEquipmentRenderer>("handle_helmet");
  registry->register_equipment(EquipmentCategory::Helmet, "handle_helmet", helmet);

  const auto handle =
      registry->resolve_handle(EquipmentCategory::Helmet, "handle_helmet");

  EXPECT_NE(handle, k_invalid_equipment_handle);
  EXPECT_TRUE(registry->has(handle));
  EXPECT_EQ(registry->get(handle), helmet);
}

TEST_F(EquipmentRegistryTest, ResolveHandleForMissingEquipment) {
  const auto handle =
      registry->resolve_handle(EquipmentCategory::Weapon, "missing_weapon");

  EXPECT_EQ(handle, k_invalid_equipment_handle);
  EXPECT_FALSE(registry->has(handle));
  EXPECT_EQ(registry->get(handle), nullptr);
}

TEST_F(EquipmentRegistryTest, OverwriteKeepsStableHandle) {
  auto armor_v1 = std::make_shared<MockEquipmentRenderer>("armor_v1");
  auto armor_v2 = std::make_shared<MockEquipmentRenderer>("armor_v2");
  registry->register_equipment(EquipmentCategory::Armor, "stable_armor", armor_v1);
  const auto handle_v1 =
      registry->resolve_handle(EquipmentCategory::Armor, "stable_armor");

  registry->register_equipment(EquipmentCategory::Armor, "stable_armor", armor_v2);
  const auto handle_v2 =
      registry->resolve_handle(EquipmentCategory::Armor, "stable_armor");

  EXPECT_EQ(handle_v1, handle_v2);
  EXPECT_NE(handle_v1, k_invalid_equipment_handle);
  EXPECT_EQ(registry->get(handle_v2), armor_v2);
}

TEST_F(EquipmentRegistryTest, PlaceholderEquipmentResolvesStableHandleWithoutRenderer) {
  registry->register_placeholder_equipment(EquipmentCategory::Armor,
                                           "placeholder_robe");

  const auto handle =
      registry->resolve_handle(EquipmentCategory::Armor, "placeholder_robe");

  EXPECT_NE(handle, k_invalid_equipment_handle);
  EXPECT_TRUE(registry->has(EquipmentCategory::Armor, "placeholder_robe"));
  EXPECT_FALSE(registry->has(handle));
  EXPECT_EQ(registry->get(handle), nullptr);
}

TEST_F(EquipmentRegistryTest, RegisterAndGetHorseEquipment) {
  auto horse_bridle = std::make_shared<BridleRenderer>();
  registry->register_horse_equipment(
      EquipmentCategory::HorseTack, "horse_bridle_test", horse_bridle);

  auto retrieved =
      registry->get_horse(EquipmentCategory::HorseTack, "horse_bridle_test");
  ASSERT_NE(retrieved, nullptr);
  EXPECT_EQ(retrieved, horse_bridle);
}

TEST_F(EquipmentRegistryTest, HorseEquipmentResolvesHandleFromSameRegistry) {
  auto horse_bridle = std::make_shared<BridleRenderer>();
  registry->register_horse_equipment(
      EquipmentCategory::HorseTack, "horse_bridle_handle", horse_bridle);

  const auto handle =
      registry->resolve_handle(EquipmentCategory::HorseTack, "horse_bridle_handle");

  EXPECT_NE(handle, k_invalid_equipment_handle);
  EXPECT_EQ(registry->get_horse(handle), horse_bridle);
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
