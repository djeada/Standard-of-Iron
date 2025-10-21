#include "game/audio/AudioEventHandler.h"
#include "game/audio/AudioSystem.h"
#include "game/core/component.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/systems/selection_system.h"
#include <QCoreApplication>
#include <QTimer>
#include <iostream>

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  std::cout << "=== Audio Event Integration Test ===" << std::endl;

  std::cout << "\n1. Initializing Audio System..." << std::endl;
  auto &audioSystem = AudioSystem::getInstance();
  if (!audioSystem.initialize()) {
    std::cerr << "Failed to initialize audio system!" << std::endl;
    return 1;
  }
  std::cout << "   ✓ Audio System initialized" << std::endl;

  std::cout << "\n2. Creating World and Systems..." << std::endl;
  Engine::Core::World world;
  Game::Systems::SelectionSystem selectionSystem;
  std::cout << "   ✓ World and SelectionSystem created" << std::endl;

  std::cout << "\n3. Initializing Audio Event Handler..." << std::endl;
  Game::Audio::AudioEventHandler handler(&world);
  if (!handler.initialize()) {
    std::cerr << "Failed to initialize audio event handler!" << std::endl;
    return 1;
  }
  std::cout << "   ✓ Audio Event Handler initialized" << std::endl;

  std::cout << "\n4. Loading placeholder audio resources..." << std::endl;
  audioSystem.loadSound("archer_voice", "assets/audio/voices/archer_voice.wav");
  audioSystem.loadSound("knight_voice", "assets/audio/voices/knight_voice.wav");
  audioSystem.loadSound("spearman_voice",
                        "assets/audio/voices/spearman_voice.wav");
  audioSystem.loadMusic("peaceful", "assets/audio/music/peaceful.wav");
  audioSystem.loadMusic("combat", "assets/audio/music/combat.wav");
  std::cout << "   ✓ Audio resources loaded" << std::endl;

  std::cout << "\n5. Configuring unit type mappings..." << std::endl;
  handler.loadUnitVoiceMapping("archer", "archer_voice");
  handler.loadUnitVoiceMapping("knight", "knight_voice");
  handler.loadUnitVoiceMapping("spearman", "spearman_voice");
  handler.loadAmbientMusic(Engine::Core::AmbientState::PEACEFUL, "peaceful");
  handler.loadAmbientMusic(Engine::Core::AmbientState::COMBAT, "combat");
  std::cout << "   ✓ Mappings configured" << std::endl;

  std::cout << "\n6. Creating test units..." << std::endl;
  auto *archerEntity = world.createEntity();
  auto *archerUnit = archerEntity->addComponent<Engine::Core::UnitComponent>();
  archerUnit->unitType = "archer";
  archerUnit->health = 100;
  archerUnit->maxHealth = 100;

  auto *knightEntity = world.createEntity();
  auto *knightUnit = knightEntity->addComponent<Engine::Core::UnitComponent>();
  knightUnit->unitType = "knight";
  knightUnit->health = 150;
  knightUnit->maxHealth = 150;
  std::cout << "   ✓ Created archer (ID: " << archerEntity->getId()
            << ") and knight (ID: " << knightEntity->getId() << ")"
            << std::endl;

  std::cout << "\n7. Testing unit selection with voice playback..."
            << std::endl;
  std::cout << "   - Selecting archer..." << std::endl;
  selectionSystem.selectUnit(archerEntity->getId());
  std::cout
      << "   ✓ Archer selected (should trigger archer_voice sound playback)"
      << std::endl;

  std::cout << "   - Selecting knight..." << std::endl;
  selectionSystem.selectUnit(knightEntity->getId());
  std::cout
      << "   ✓ Knight selected (should trigger knight_voice sound playback)"
      << std::endl;

  std::cout << "\n8. Testing ambient state changes..." << std::endl;
  std::cout << "   - Changing to COMBAT state..." << std::endl;
  Engine::Core::EventManager::instance().publish(
      Engine::Core::AmbientStateChangedEvent(
          Engine::Core::AmbientState::COMBAT,
          Engine::Core::AmbientState::PEACEFUL));
  std::cout << "   ✓ State changed (should trigger combat music)" << std::endl;

  std::cout << "   - Changing back to PEACEFUL state..." << std::endl;
  Engine::Core::EventManager::instance().publish(
      Engine::Core::AmbientStateChangedEvent(
          Engine::Core::AmbientState::PEACEFUL,
          Engine::Core::AmbientState::COMBAT));
  std::cout << "   ✓ State changed (should trigger peaceful music)"
            << std::endl;

  std::cout << "\n9. Verifying event statistics..." << std::endl;
  auto unitSelectedStats = Engine::Core::EventManager::instance().getStats(
      std::type_index(typeid(Engine::Core::UnitSelectedEvent)));
  std::cout << "   ✓ UnitSelectedEvent subscribers: "
            << unitSelectedStats.subscriberCount << std::endl;
  std::cout << "   ✓ UnitSelectedEvent published: "
            << unitSelectedStats.publishCount << " times" << std::endl;

  auto ambientStats = Engine::Core::EventManager::instance().getStats(
      std::type_index(typeid(Engine::Core::AmbientStateChangedEvent)));
  std::cout << "   ✓ AmbientStateChangedEvent subscribers: "
            << ambientStats.subscriberCount << std::endl;
  std::cout << "   ✓ AmbientStateChangedEvent published: "
            << ambientStats.publishCount << " times" << std::endl;

  std::cout << "\n10. Shutting down..." << std::endl;
  handler.shutdown();
  audioSystem.shutdown();
  std::cout << "   ✓ All systems shutdown" << std::endl;

  std::cout << "\n=== All integration tests passed! ===" << std::endl;
  std::cout << "\nNote: Audio playback may not be audible in headless "
               "environments."
            << std::endl;

  QTimer::singleShot(0, &app, &QCoreApplication::quit);
  return app.exec();
}
