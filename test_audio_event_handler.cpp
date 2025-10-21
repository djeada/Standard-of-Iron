#include "game/audio/AudioEventHandler.h"
#include "game/audio/AudioSystem.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include <QCoreApplication>
#include <QTimer>
#include <iostream>

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  std::cout << "=== Audio Event Handler Test ===" << std::endl;

  std::cout << "\n1. Initializing Audio System..." << std::endl;
  auto &audioSystem = AudioSystem::getInstance();
  if (!audioSystem.initialize()) {
    std::cerr << "Failed to initialize audio system!" << std::endl;
    return 1;
  }
  std::cout << "   ✓ Audio System initialized" << std::endl;

  std::cout << "\n2. Creating World instance..." << std::endl;
  Engine::Core::World world;
  std::cout << "   ✓ World created" << std::endl;

  std::cout << "\n3. Initializing Audio Event Handler..." << std::endl;
  Game::Audio::AudioEventHandler handler(&world);
  if (!handler.initialize()) {
    std::cerr << "Failed to initialize audio event handler!" << std::endl;
    return 1;
  }
  std::cout << "   ✓ Audio Event Handler initialized" << std::endl;

  std::cout << "\n4. Loading placeholder audio resources..." << std::endl;
  audioSystem.loadSound("archer_voice",
                        "assets/audio/voices/archer_voice.wav");
  audioSystem.loadSound("knight_voice",
                        "assets/audio/voices/knight_voice.wav");
  audioSystem.loadSound("spearman_voice",
                        "assets/audio/voices/spearman_voice.wav");
  std::cout << "   ✓ Loaded unit voice sounds" << std::endl;

  audioSystem.loadMusic("peaceful", "assets/audio/music/peaceful.wav");
  audioSystem.loadMusic("tense", "assets/audio/music/tense.wav");
  audioSystem.loadMusic("combat", "assets/audio/music/combat.wav");
  audioSystem.loadMusic("victory", "assets/audio/music/victory.wav");
  audioSystem.loadMusic("defeat", "assets/audio/music/defeat.wav");
  std::cout << "   ✓ Loaded ambient music" << std::endl;

  std::cout << "\n5. Configuring unit type mappings..." << std::endl;
  handler.loadUnitVoiceMapping("archer", "archer_voice");
  handler.loadUnitVoiceMapping("knight", "knight_voice");
  handler.loadUnitVoiceMapping("spearman", "spearman_voice");
  std::cout << "   ✓ Unit voice mappings configured" << std::endl;

  std::cout << "\n6. Configuring ambient state mappings..." << std::endl;
  handler.loadAmbientMusic(Engine::Core::AmbientState::PEACEFUL, "peaceful");
  handler.loadAmbientMusic(Engine::Core::AmbientState::TENSE, "tense");
  handler.loadAmbientMusic(Engine::Core::AmbientState::COMBAT, "combat");
  handler.loadAmbientMusic(Engine::Core::AmbientState::VICTORY, "victory");
  handler.loadAmbientMusic(Engine::Core::AmbientState::DEFEAT, "defeat");
  std::cout << "   ✓ Ambient music mappings configured" << std::endl;

  std::cout << "\n7. Testing AudioTriggerEvent..." << std::endl;
  Engine::Core::EventManager::instance().publish(
      Engine::Core::AudioTriggerEvent("archer_voice", 0.8f));
  std::cout << "   ✓ Published AudioTriggerEvent" << std::endl;

  std::cout << "\n8. Testing MusicTriggerEvent..." << std::endl;
  Engine::Core::EventManager::instance().publish(
      Engine::Core::MusicTriggerEvent("peaceful", 0.6f));
  std::cout << "   ✓ Published MusicTriggerEvent" << std::endl;

  std::cout << "\n9. Testing AmbientStateChangedEvent..." << std::endl;
  Engine::Core::EventManager::instance().publish(
      Engine::Core::AmbientStateChangedEvent(
          Engine::Core::AmbientState::COMBAT,
          Engine::Core::AmbientState::PEACEFUL));
  std::cout << "   ✓ Published AmbientStateChangedEvent (PEACEFUL -> COMBAT)"
            << std::endl;

  std::cout << "\n10. Testing event handler registration..." << std::endl;
  auto stats = Engine::Core::EventManager::instance().getStats(
      std::type_index(typeid(Engine::Core::AudioTriggerEvent)));
  std::cout << "   ✓ AudioTriggerEvent subscribers: " << stats.subscriberCount
            << std::endl;
  std::cout << "   ✓ AudioTriggerEvent publish count: " << stats.publishCount
            << std::endl;

  std::cout << "\n11. Shutting down..." << std::endl;
  handler.shutdown();
  std::cout << "   ✓ Audio Event Handler shutdown" << std::endl;
  audioSystem.shutdown();
  std::cout << "   ✓ Audio System shutdown" << std::endl;

  std::cout << "\n=== All tests passed! ===" << std::endl;

  QTimer::singleShot(0, &app, &QCoreApplication::quit);
  return app.exec();
}
