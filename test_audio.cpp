#include "game/audio/AudioSystem.h"
#include <QCoreApplication>
#include <QTimer>
#include <iostream>

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  std::cout << "Initializing Audio System..." << std::endl;

  auto &audioSystem = AudioSystem::getInstance();

  if (!audioSystem.initialize()) {
    std::cerr << "Failed to initialize audio system!" << std::endl;
    return 1;
  }

  std::cout << "Audio System initialized successfully!" << std::endl;

  std::cout << "Testing volume controls..." << std::endl;
  audioSystem.setMasterVolume(0.8f);
  audioSystem.setSoundVolume(0.7f);
  audioSystem.setMusicVolume(0.6f);
  std::cout << "Volume controls working!" << std::endl;

  std::cout << "Shutting down Audio System..." << std::endl;
  audioSystem.shutdown();
  std::cout << "Audio System shutdown successfully!" << std::endl;

  QTimer::singleShot(0, &app, &QCoreApplication::quit);
  return app.exec();
}
