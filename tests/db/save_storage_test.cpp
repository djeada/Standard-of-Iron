#include <gtest/gtest.h>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include "systems/save_storage.h"

using namespace Game::Systems;

class SaveStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        storage = std::make_unique<SaveStorage>(":memory:");
        QString error;
        bool initialized = storage->initialize(&error);
        ASSERT_TRUE(initialized) << "Failed to initialize: " << error.toStdString();
    }

    void TearDown() override {
        storage.reset();
    }

    std::unique_ptr<SaveStorage> storage;
};

TEST_F(SaveStorageTest, InitializationSuccess) {
    EXPECT_NE(storage, nullptr);
}

TEST_F(SaveStorageTest, SaveSlotBasic) {
    QString slot_name = "test_slot";
    QString title = "Test Save Game";
    
    QJsonObject metadata;
    metadata["level"] = 5;
    metadata["score"] = 1000;
    
    QByteArray world_state("world_state_data");
    QByteArray screenshot("screenshot_data");
    
    QString error;
    bool saved = storage->saveSlot(slot_name, title, metadata, world_state, screenshot, &error);
    
    EXPECT_TRUE(saved) << "Failed to save: " << error.toStdString();
}

TEST_F(SaveStorageTest, SaveAndLoadSlot) {
    QString slot_name = "save_load_test";
    QString original_title = "Original Title";
    
    QJsonObject original_metadata;
    original_metadata["player_name"] = "TestPlayer";
    original_metadata["game_time"] = 3600;
    original_metadata["difficulty"] = "hard";
    
    QByteArray original_world_state("test_world_state_content");
    QByteArray original_screenshot("test_screenshot_content");
    
    QString error;
    bool saved = storage->saveSlot(slot_name, original_title, original_metadata, 
                                    original_world_state, original_screenshot, &error);
    ASSERT_TRUE(saved) << "Save failed: " << error.toStdString();
    
    QByteArray loaded_world_state;
    QJsonObject loaded_metadata;
    QByteArray loaded_screenshot;
    QString loaded_title;
    
    bool loaded = storage->loadSlot(slot_name, loaded_world_state, loaded_metadata,
                                     loaded_screenshot, loaded_title, &error);
    
    ASSERT_TRUE(loaded) << "Load failed: " << error.toStdString();
    
    EXPECT_EQ(loaded_title, original_title);
    EXPECT_EQ(loaded_world_state, original_world_state);
    EXPECT_EQ(loaded_screenshot, original_screenshot);
    
    EXPECT_EQ(loaded_metadata["player_name"].toString(), QString("TestPlayer"));
    EXPECT_EQ(loaded_metadata["game_time"].toInt(), 3600);
    EXPECT_EQ(loaded_metadata["difficulty"].toString(), QString("hard"));
}

TEST_F(SaveStorageTest, OverwriteExistingSlot) {
    QString slot_name = "overwrite_test";
    QString title1 = "First Save";
    QString title2 = "Second Save";
    
    QJsonObject metadata1;
    metadata1["version"] = 1;
    
    QJsonObject metadata2;
    metadata2["version"] = 2;
    
    QByteArray world_state1("state1");
    QByteArray world_state2("state2");
    
    QString error;
    
    bool saved1 = storage->saveSlot(slot_name, title1, metadata1, world_state1, 
                                     QByteArray(), &error);
    ASSERT_TRUE(saved1) << "First save failed: " << error.toStdString();
    
    bool saved2 = storage->saveSlot(slot_name, title2, metadata2, world_state2,
                                     QByteArray(), &error);
    ASSERT_TRUE(saved2) << "Second save failed: " << error.toStdString();
    
    QByteArray loaded_world_state;
    QJsonObject loaded_metadata;
    QByteArray loaded_screenshot;
    QString loaded_title;
    
    bool loaded = storage->loadSlot(slot_name, loaded_world_state, loaded_metadata,
                                     loaded_screenshot, loaded_title, &error);
    
    ASSERT_TRUE(loaded) << "Load failed: " << error.toStdString();
    
    EXPECT_EQ(loaded_title, title2);
    EXPECT_EQ(loaded_world_state, world_state2);
    EXPECT_EQ(loaded_metadata["version"].toInt(), 2);
}

TEST_F(SaveStorageTest, LoadNonExistentSlot) {
    QString slot_name = "nonexistent_slot";
    
    QByteArray loaded_world_state;
    QJsonObject loaded_metadata;
    QByteArray loaded_screenshot;
    QString loaded_title;
    QString error;
    
    bool loaded = storage->loadSlot(slot_name, loaded_world_state, loaded_metadata,
                                     loaded_screenshot, loaded_title, &error);
    
    EXPECT_FALSE(loaded);
    EXPECT_FALSE(error.isEmpty());
}

TEST_F(SaveStorageTest, ListSlots) {
    QString error;
    
    QByteArray non_empty_data("test_data");
    storage->saveSlot("slot1", "Title 1", QJsonObject(), non_empty_data, QByteArray(), &error);
    storage->saveSlot("slot2", "Title 2", QJsonObject(), non_empty_data, QByteArray(), &error);
    storage->saveSlot("slot3", "Title 3", QJsonObject(), non_empty_data, QByteArray(), &error);
    
    QVariantList slot_list = storage->listSlots(&error);
    
    EXPECT_TRUE(error.isEmpty()) << "List failed: " << error.toStdString();
    EXPECT_EQ(slot_list.size(), 3);
    
    bool found_slot1 = false;
    bool found_slot2 = false;
    bool found_slot3 = false;
    
    for (const QVariant& slot_variant : slot_list) {
        QVariantMap slot = slot_variant.toMap();
        QString slot_name = slot["slotName"].toString();
        
        if (slot_name == "slot1") {
            found_slot1 = true;
            EXPECT_EQ(slot["title"].toString(), QString("Title 1"));
        } else if (slot_name == "slot2") {
            found_slot2 = true;
            EXPECT_EQ(slot["title"].toString(), QString("Title 2"));
        } else if (slot_name == "slot3") {
            found_slot3 = true;
            EXPECT_EQ(slot["title"].toString(), QString("Title 3"));
        }
    }
    
    EXPECT_TRUE(found_slot1);
    EXPECT_TRUE(found_slot2);
    EXPECT_TRUE(found_slot3);
}

TEST_F(SaveStorageTest, DeleteSlot) {
    QString slot_name = "delete_test";
    QString error;
    
    QByteArray non_empty_data("test_data");
    storage->saveSlot(slot_name, "Title", QJsonObject(), non_empty_data, QByteArray(), &error);
    
    QVariantList slots_before = storage->listSlots(&error);
    EXPECT_EQ(slots_before.size(), 1);
    
    bool deleted = storage->deleteSlot(slot_name, &error);
    EXPECT_TRUE(deleted) << "Delete failed: " << error.toStdString();
    
    QVariantList slots_after = storage->listSlots(&error);
    EXPECT_EQ(slots_after.size(), 0);
}

TEST_F(SaveStorageTest, DeleteNonExistentSlot) {
    QString slot_name = "nonexistent_delete";
    QString error;
    
    bool deleted = storage->deleteSlot(slot_name, &error);
    
    EXPECT_FALSE(deleted);
    EXPECT_FALSE(error.isEmpty());
}

TEST_F(SaveStorageTest, EmptyMetadataSave) {
    QString slot_name = "empty_metadata";
    QJsonObject empty_metadata;
    
    QString error;
    bool saved = storage->saveSlot(slot_name, "Title", empty_metadata, 
                                    QByteArray("data"), QByteArray(), &error);
    
    EXPECT_TRUE(saved) << "Failed to save: " << error.toStdString();
    
    QByteArray loaded_world_state;
    QJsonObject loaded_metadata;
    QByteArray loaded_screenshot;
    QString loaded_title;
    
    bool loaded = storage->loadSlot(slot_name, loaded_world_state, loaded_metadata,
                                     loaded_screenshot, loaded_title, &error);
    
    EXPECT_TRUE(loaded) << "Failed to load: " << error.toStdString();
}

TEST_F(SaveStorageTest, EmptyWorldStateSave) {
    QString slot_name = "empty_world_state";
    QByteArray minimal_world_state(" ");
    
    QString error;
    bool saved = storage->saveSlot(slot_name, "Title", QJsonObject(), 
                                    minimal_world_state, QByteArray(), &error);
    
    EXPECT_TRUE(saved) << "Failed to save: " << error.toStdString();
    
    QByteArray loaded_world_state;
    QJsonObject loaded_metadata;
    QByteArray loaded_screenshot;
    QString loaded_title;
    
    bool loaded = storage->loadSlot(slot_name, loaded_world_state, loaded_metadata,
                                     loaded_screenshot, loaded_title, &error);
    
    EXPECT_TRUE(loaded) << "Failed to load: " << error.toStdString();
    EXPECT_EQ(loaded_world_state, minimal_world_state);
}

TEST_F(SaveStorageTest, LargeDataSave) {
    QString slot_name = "large_data";
    
    QByteArray large_world_state(1024 * 1024, 'A');
    QByteArray large_screenshot(512 * 1024, 'B');
    
    QJsonObject metadata;
    metadata["size"] = "large";
    
    QString error;
    bool saved = storage->saveSlot(slot_name, "Large Data Test", metadata, 
                                    large_world_state, large_screenshot, &error);
    
    EXPECT_TRUE(saved) << "Failed to save large data: " << error.toStdString();
    
    QByteArray loaded_world_state;
    QJsonObject loaded_metadata;
    QByteArray loaded_screenshot;
    QString loaded_title;
    
    bool loaded = storage->loadSlot(slot_name, loaded_world_state, loaded_metadata,
                                     loaded_screenshot, loaded_title, &error);
    
    EXPECT_TRUE(loaded) << "Failed to load large data: " << error.toStdString();
    EXPECT_EQ(loaded_world_state.size(), 1024 * 1024);
    EXPECT_EQ(loaded_screenshot.size(), 512 * 1024);
}

TEST_F(SaveStorageTest, SpecialCharactersInSlotName) {
    QString slot_name = "slot_with_special_chars_123";
    QString title = "Title with special chars: !@#$%^&*()";
    
    QJsonObject metadata;
    metadata["description"] = "Test with special characters: <>&\"'";
    
    QString error;
    bool saved = storage->saveSlot(slot_name, title, metadata, 
                                    QByteArray("data"), QByteArray(), &error);
    
    EXPECT_TRUE(saved) << "Failed to save: " << error.toStdString();
    
    QByteArray loaded_world_state;
    QJsonObject loaded_metadata;
    QByteArray loaded_screenshot;
    QString loaded_title;
    
    bool loaded = storage->loadSlot(slot_name, loaded_world_state, loaded_metadata,
                                     loaded_screenshot, loaded_title, &error);
    
    EXPECT_TRUE(loaded) << "Failed to load: " << error.toStdString();
    EXPECT_EQ(loaded_title, title);
}

TEST_F(SaveStorageTest, ComplexMetadataSave) {
    QString slot_name = "complex_metadata";
    
    QJsonObject metadata;
    metadata["int_value"] = 42;
    metadata["double_value"] = 3.14159;
    metadata["string_value"] = "test_string";
    metadata["bool_value"] = true;
    
    QJsonObject nested_object;
    nested_object["nested_field"] = "nested_value";
    metadata["nested"] = nested_object;
    
    QJsonArray array;
    array.append(1);
    array.append(2);
    array.append(3);
    metadata["array"] = array;
    
    QString error;
    bool saved = storage->saveSlot(slot_name, "Complex Metadata Test", metadata,
                                    QByteArray("data"), QByteArray(), &error);
    
    EXPECT_TRUE(saved) << "Failed to save: " << error.toStdString();
    
    QByteArray loaded_world_state;
    QJsonObject loaded_metadata;
    QByteArray loaded_screenshot;
    QString loaded_title;
    
    bool loaded = storage->loadSlot(slot_name, loaded_world_state, loaded_metadata,
                                     loaded_screenshot, loaded_title, &error);
    
    EXPECT_TRUE(loaded) << "Failed to load: " << error.toStdString();
    
    EXPECT_EQ(loaded_metadata["int_value"].toInt(), 42);
    EXPECT_DOUBLE_EQ(loaded_metadata["double_value"].toDouble(), 3.14159);
    EXPECT_EQ(loaded_metadata["string_value"].toString(), QString("test_string"));
    EXPECT_TRUE(loaded_metadata["bool_value"].toBool());
    
    QJsonObject loaded_nested = loaded_metadata["nested"].toObject();
    EXPECT_EQ(loaded_nested["nested_field"].toString(), QString("nested_value"));
    
    QJsonArray loaded_array = loaded_metadata["array"].toArray();
    EXPECT_EQ(loaded_array.size(), 3);
    EXPECT_EQ(loaded_array[0].toInt(), 1);
    EXPECT_EQ(loaded_array[1].toInt(), 2);
    EXPECT_EQ(loaded_array[2].toInt(), 3);
}

TEST_F(SaveStorageTest, MultipleSavesAndDeletes) {
    QString error;
    
    for (int i = 0; i < 10; i++) {
        QString slot_name = QString("slot_%1").arg(i);
        storage->saveSlot(slot_name, QString("Title %1").arg(i), 
                         QJsonObject(), QByteArray("data"), QByteArray(), &error);
    }
    
    QVariantList slot_list = storage->listSlots(&error);
    EXPECT_EQ(slot_list.size(), 10);
    
    for (int i = 0; i < 5; i++) {
        QString slot_name = QString("slot_%1").arg(i);
        storage->deleteSlot(slot_name, &error);
    }
    
    slot_list = storage->listSlots(&error);
    EXPECT_EQ(slot_list.size(), 5);
    
    for (const QVariant& slot_variant : slot_list) {
        QVariantMap slot = slot_variant.toMap();
        QString slot_name = slot["slotName"].toString();
        int slot_num = slot_name.mid(5).toInt();
        EXPECT_GE(slot_num, 5);
        EXPECT_LT(slot_num, 10);
    }
}
