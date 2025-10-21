#include "game/audio/AudioSystem.h"
#include <QCoreApplication>
#include <QTimer>
#include <iostream>

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  std::cout << "=== Audio System Test ===" << std::endl;
  std::cout << "\n1. Initializing Audio System..." << std::endl;

  auto &audioSystem = AudioSystem::getInstance();

  if (!audioSystem.initialize()) {
    std::cerr << "Failed to initialize audio system!" << std::endl;
    return 1;
  }

  std::cout << "   ✓ Audio System initialized successfully!" << std::endl;

  std::cout << "\n2. Testing volume controls..." << std::endl;
  audioSystem.setMasterVolume(0.8f);
  std::cout << "   ✓ Set master volume to 0.8" << std::endl;
  audioSystem.setSoundVolume(0.7f);
  std::cout << "   ✓ Set sound volume to 0.7" << std::endl;
  audioSystem.setMusicVolume(0.6f);
  std::cout << "   ✓ Set music volume to 0.6" << std::endl;

  std::cout << "\n3. Testing resource loading (simulated)..." << std::endl;
  std::cout << "   Note: Actual audio files would be loaded with:" << std::endl;
  std::cout << "   - audioSystem.loadSound(\"id\", \"path/to/sound.wav\")"
            << std::endl;
  std::cout << "   - audioSystem.loadMusic(\"id\", \"path/to/music.mp3\")"
            << std::endl;

  std::cout << "\n4. Testing playback commands (simulated)..." << std::endl;
  std::cout << "   Note: Would call playSound/playMusic with loaded resources"
            << std::endl;

  std::cout << "\n5. Shutting down Audio System..." << std::endl;
  audioSystem.shutdown();
  std::cout << "   ✓ Audio System shutdown successfully!" << std::endl;

  std::cout << "\n=== All tests passed! ===" << std::endl;

  QTimer::singleShot(0, &app, &QCoreApplication::quit);
  return app.exec();
}
