#include "game/core/event_manager.h"
#include <QCoreApplication>
#include <QTimer>
#include <iostream>
#include <vector>

using namespace Engine::Core;

struct TestResults {
  int totalTests = 0;
  int passedTests = 0;
  std::vector<std::string> failures;

  void recordTest(const std::string &testName, bool passed) {
    totalTests++;
    if (passed) {
      passedTests++;
      std::cout << "  ✓ " << testName << std::endl;
    } else {
      failures.push_back(testName);
      std::cout << "  ✗ " << testName << std::endl;
    }
  }

  void printSummary() {
    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "Total: " << totalTests << " | Passed: " << passedTests
              << " | Failed: " << (totalTests - passedTests) << std::endl;
    if (!failures.empty()) {
      std::cout << "\nFailed tests:" << std::endl;
      for (const auto &failure : failures) {
        std::cout << "  - " << failure << std::endl;
      }
    }
  }

  bool allPassed() const { return totalTests == passedTests; }
};

void testBasicEventPublishSubscribe(TestResults &results) {
  std::cout << "\n1. Testing basic event publish/subscribe..." << std::endl;

  bool unitSelectedReceived = false;
  EntityID receivedUnitId = 0;

  auto handle = EventManager::instance().subscribe<UnitSelectedEvent>(
      [&](const UnitSelectedEvent &event) {
        unitSelectedReceived = true;
        receivedUnitId = event.unitId;
      });

  EventManager::instance().publish(UnitSelectedEvent(42));

  results.recordTest("UnitSelectedEvent received",
                     unitSelectedReceived && receivedUnitId == 42);

  EventManager::instance().unsubscribe<UnitSelectedEvent>(handle);
}

void testMultipleSubscribers(TestResults &results) {
  std::cout << "\n2. Testing multiple subscribers..." << std::endl;

  int callCount = 0;

  auto handle1 = EventManager::instance().subscribe<UnitMovedEvent>(
      [&](const UnitMovedEvent &) { callCount++; });

  auto handle2 = EventManager::instance().subscribe<UnitMovedEvent>(
      [&](const UnitMovedEvent &) { callCount++; });

  EventManager::instance().publish(UnitMovedEvent(1, 10.0f, 20.0f));

  results.recordTest("Multiple subscribers called", callCount == 2);

  EventManager::instance().unsubscribe<UnitMovedEvent>(handle1);
  EventManager::instance().unsubscribe<UnitMovedEvent>(handle2);
}

void testUnsubscribe(TestResults &results) {
  std::cout << "\n3. Testing unsubscribe..." << std::endl;

  int callCount = 0;

  auto handle = EventManager::instance().subscribe<UnitDiedEvent>(
      [&](const UnitDiedEvent &) { callCount++; });

  EventManager::instance().publish(UnitDiedEvent(1, 0, "knight"));
  results.recordTest("Event received before unsubscribe", callCount == 1);

  EventManager::instance().unsubscribe<UnitDiedEvent>(handle);
  EventManager::instance().publish(UnitDiedEvent(2, 0, "archer"));

  results.recordTest("Event not received after unsubscribe", callCount == 1);
}

void testScopedSubscription(TestResults &results) {
  std::cout << "\n4. Testing scoped subscription..." << std::endl;

  int callCount = 0;

  {
    ScopedEventSubscription<UnitSpawnedEvent> subscription(
        [&](const UnitSpawnedEvent &) { callCount++; });

    EventManager::instance().publish(UnitSpawnedEvent(1, 0, "spearman"));
    results.recordTest("Event received with scoped subscription",
                       callCount == 1);
  }

  EventManager::instance().publish(UnitSpawnedEvent(2, 0, "knight"));
  results.recordTest("Event not received after scope exit", callCount == 1);
}

void testBattleEvents(TestResults &results) {
  std::cout << "\n5. Testing battle events..." << std::endl;

  bool battleStarted = false;
  bool battleEnded = false;
  EntityID startAttackerId = 0;
  EntityID endWinnerId = 0;

  auto startHandle = EventManager::instance().subscribe<BattleStartedEvent>(
      [&](const BattleStartedEvent &event) {
        battleStarted = true;
        startAttackerId = event.attackerId;
      });

  auto endHandle = EventManager::instance().subscribe<BattleEndedEvent>(
      [&](const BattleEndedEvent &event) {
        battleEnded = true;
        endWinnerId = event.winnerId;
      });

  EventManager::instance().publish(BattleStartedEvent(10, 20, 15.0f, 25.0f));
  results.recordTest("BattleStartedEvent received",
                     battleStarted && startAttackerId == 10);

  EventManager::instance().publish(BattleEndedEvent(10, 20, true));
  results.recordTest("BattleEndedEvent received",
                     battleEnded && endWinnerId == 10);

  EventManager::instance().unsubscribe<BattleStartedEvent>(startHandle);
  EventManager::instance().unsubscribe<BattleEndedEvent>(endHandle);
}

void testAmbientStateChanged(TestResults &results) {
  std::cout << "\n6. Testing ambient state changed events..." << std::endl;

  bool stateChanged = false;
  AmbientState receivedNewState = AmbientState::PEACEFUL;
  AmbientState receivedPrevState = AmbientState::PEACEFUL;

  auto handle = EventManager::instance().subscribe<AmbientStateChangedEvent>(
      [&](const AmbientStateChangedEvent &event) {
        stateChanged = true;
        receivedNewState = event.newState;
        receivedPrevState = event.previousState;
      });

  EventManager::instance().publish(
      AmbientStateChangedEvent(AmbientState::COMBAT, AmbientState::TENSE));

  results.recordTest("AmbientStateChangedEvent received",
                     stateChanged && receivedNewState == AmbientState::COMBAT &&
                         receivedPrevState == AmbientState::TENSE);

  EventManager::instance().unsubscribe<AmbientStateChangedEvent>(handle);
}

void testAudioEvents(TestResults &results) {
  std::cout << "\n7. Testing audio trigger events..." << std::endl;

  bool soundTriggered = false;
  bool musicTriggered = false;
  std::string receivedSoundId;
  std::string receivedMusicId;

  auto soundHandle = EventManager::instance().subscribe<AudioTriggerEvent>(
      [&](const AudioTriggerEvent &event) {
        soundTriggered = true;
        receivedSoundId = event.soundId;
      });

  auto musicHandle = EventManager::instance().subscribe<MusicTriggerEvent>(
      [&](const MusicTriggerEvent &event) {
        musicTriggered = true;
        receivedMusicId = event.musicId;
      });

  EventManager::instance().publish(
      AudioTriggerEvent("sword_clash", 0.8f, false, 5));
  results.recordTest("AudioTriggerEvent received",
                     soundTriggered && receivedSoundId == "sword_clash");

  EventManager::instance().publish(
      MusicTriggerEvent("battle_theme", 0.7f, true));
  results.recordTest("MusicTriggerEvent received",
                     musicTriggered && receivedMusicId == "battle_theme");

  EventManager::instance().unsubscribe<AudioTriggerEvent>(soundHandle);
  EventManager::instance().unsubscribe<MusicTriggerEvent>(musicHandle);
}

void testEventDataIntegrity(TestResults &results) {
  std::cout << "\n8. Testing event data integrity..." << std::endl;

  float receivedX = 0.0f;
  float receivedY = 0.0f;

  auto handle = EventManager::instance().subscribe<UnitMovedEvent>(
      [&](const UnitMovedEvent &event) {
        receivedX = event.x;
        receivedY = event.y;
      });

  EventManager::instance().publish(UnitMovedEvent(1, 123.45f, 678.90f));

  results.recordTest("Event data preserved",
                     receivedX == 123.45f && receivedY == 678.90f);

  EventManager::instance().unsubscribe<UnitMovedEvent>(handle);
}

void testNoSubscribers(TestResults &results) {
  std::cout << "\n9. Testing event publish with no subscribers..." << std::endl;

  bool noCrash = true;
  try {
    EventManager::instance().publish(UnitDiedEvent(999, 0, "test"));
  } catch (...) {
    noCrash = false;
  }

  results.recordTest("No crash when publishing without subscribers", noCrash);
}

void testComplexEventScenario(TestResults &results) {
  std::cout << "\n10. Testing complex event scenario..." << std::endl;

  int eventSequence = 0;

  auto unitSpawnHandle = EventManager::instance().subscribe<UnitSpawnedEvent>(
      [&](const UnitSpawnedEvent &) {
        if (eventSequence == 0)
          eventSequence = 1;
      });

  auto unitMovedHandle = EventManager::instance().subscribe<UnitMovedEvent>(
      [&](const UnitMovedEvent &) {
        if (eventSequence == 1)
          eventSequence = 2;
      });

  auto battleHandle = EventManager::instance().subscribe<BattleStartedEvent>(
      [&](const BattleStartedEvent &) {
        if (eventSequence == 2)
          eventSequence = 3;
      });

  EventManager::instance().publish(UnitSpawnedEvent(1, 0, "knight"));
  EventManager::instance().publish(UnitMovedEvent(1, 10.0f, 10.0f));
  EventManager::instance().publish(BattleStartedEvent(1, 2, 10.0f, 10.0f));

  results.recordTest("Complex event sequence handled correctly",
                     eventSequence == 3);

  EventManager::instance().unsubscribe<UnitSpawnedEvent>(unitSpawnHandle);
  EventManager::instance().unsubscribe<UnitMovedEvent>(unitMovedHandle);
  EventManager::instance().unsubscribe<BattleStartedEvent>(battleHandle);
}

void testEventStats(TestResults &results) {
  std::cout << "\n11. Testing event statistics..." << std::endl;

  auto handle1 = EventManager::instance().subscribe<UnitDiedEvent>(
      [](const UnitDiedEvent &) {});
  auto handle2 = EventManager::instance().subscribe<UnitDiedEvent>(
      [](const UnitDiedEvent &) {});

  auto stats1 = EventManager::instance().getStats(typeid(UnitDiedEvent));
  results.recordTest("Subscriber count correct", stats1.subscriberCount == 2);

  EventManager::instance().publish(UnitDiedEvent(1, 0, "test"));
  EventManager::instance().publish(UnitDiedEvent(2, 0, "test"));

  auto stats2 = EventManager::instance().getStats(typeid(UnitDiedEvent));
  results.recordTest("Publish count tracked", stats2.publishCount == 2);

  EventManager::instance().unsubscribe<UnitDiedEvent>(handle1);
  auto stats3 = EventManager::instance().getStats(typeid(UnitDiedEvent));
  results.recordTest("Subscriber count updated after unsubscribe",
                     stats3.subscriberCount == 1);

  EventManager::instance().unsubscribe<UnitDiedEvent>(handle2);
}

void testEventTypeNames(TestResults &results) {
  std::cout << "\n12. Testing event type names..." << std::endl;

  UnitSelectedEvent unitEvent(1);
  results.recordTest("UNIT_SELECTED type name",
                     std::string(unitEvent.getTypeName()) == "UNIT_SELECTED");

  BattleStartedEvent battleStart(1, 2);
  results.recordTest("BATTLE_STARTED type name",
                     std::string(battleStart.getTypeName()) ==
                         "BATTLE_STARTED");

  BattleEndedEvent battleEnd(1, 2);
  results.recordTest("BATTLE_ENDED type name",
                     std::string(battleEnd.getTypeName()) == "BATTLE_ENDED");

  AmbientStateChangedEvent ambientEvent(AmbientState::COMBAT,
                                        AmbientState::PEACEFUL);
  results.recordTest("AMBIENT_STATE_CHANGED type name",
                     std::string(ambientEvent.getTypeName()) ==
                         "AMBIENT_STATE_CHANGED");
}

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  std::cout << "=== Event System Test Suite ===" << std::endl;
  std::cout << "Testing EventManager functionality independently\n"
            << std::endl;

  TestResults results;

  testBasicEventPublishSubscribe(results);
  testMultipleSubscribers(results);
  testUnsubscribe(results);
  testScopedSubscription(results);
  testBattleEvents(results);
  testAmbientStateChanged(results);
  testAudioEvents(results);
  testEventDataIntegrity(results);
  testNoSubscribers(results);
  testComplexEventScenario(results);
  testEventStats(results);
  testEventTypeNames(results);

  results.printSummary();

  if (results.allPassed()) {
    std::cout << "\n✓ All event system tests passed!" << std::endl;
  } else {
    std::cout << "\n✗ Some tests failed. Please review." << std::endl;
  }

  QTimer::singleShot(0, &app, &QCoreApplication::quit);
  return results.allPassed() ? 0 : 1;
}
