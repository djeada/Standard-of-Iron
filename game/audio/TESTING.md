# Audio System Testing Guide

This document outlines the testing procedures for the enhanced audio system.

## Manual Testing Checklist

### Volume Controls
- [ ] **Master Volume**: Adjust master volume slider in settings - all audio should be affected
- [ ] **Music Volume**: Adjust music volume slider - only background music should be affected
- [ ] **SFX Volume**: Adjust SFX volume slider - only sound effects should be affected
- [ ] **Voice Volume**: Adjust voice volume slider - only unit voices should be affected
- [ ] **Volume Persistence**: All volume settings should persist between game sessions

### Channel Management
- [ ] **Max Channels**: Play multiple sounds simultaneously (up to 32 default)
- [ ] **Priority Eviction**: When channels are full, lower priority sounds should be stopped for higher priority ones
- [ ] **Channel Tracking**: Active channel count should be accurate

### Resource Management
- [ ] **Load Sound**: Load sounds with different categories (SFX, VOICE)
- [ ] **Load Music**: Load background music tracks
- [ ] **Unload Sound**: Unload specific sound - should free memory and not crash
- [ ] **Unload Music**: Unload specific music track - should free memory and not crash
- [ ] **Unload All Sounds**: Bulk unload all sounds - should complete without errors
- [ ] **Unload All Music**: Bulk unload all music - should complete without errors
- [ ] **Reload Resources**: Load previously unloaded resources - should work correctly

### Thread Safety & Stability
- [ ] **Concurrent Playback**: Play multiple sounds from different threads simultaneously
- [ ] **Shutdown**: Application should close cleanly without segfaults
- [ ] **Rapid Volume Changes**: Rapidly adjust volume sliders - no crashes or audio glitches
- [ ] **Resource Loading While Playing**: Load/unload resources while audio is playing
- [ ] **Stop During Playback**: Stop sounds/music while they're playing - no crashes

### Memory Safety
- [ ] **Memory Leaks**: Run valgrind or similar tools to check for memory leaks
- [ ] **Null Checks**: All QCoreApplication::instance() calls should be null-checked
- [ ] **Thread Cleanup**: Audio thread should be properly joined on shutdown
- [ ] **Resource Cleanup**: All unique_ptrs should be properly released

### Performance
- [ ] **No Audio Stutters**: Audio should play smoothly without interruptions
- [ ] **Low Latency**: Sound effects should play immediately when triggered
- [ ] **CPU Usage**: Audio system should not consume excessive CPU
- [ ] **Memory Usage**: Memory usage should be reasonable and not grow over time

## Automated Testing (Future)

### Unit Tests
```cpp
// Test volume controls
TEST(AudioSystem, VolumeControls) {
    auto& audio = AudioSystem::getInstance();
    audio.setMasterVolume(0.5f);
    EXPECT_FLOAT_EQ(audio.getMasterVolume(), 0.5f);
    
    audio.setSoundVolume(0.7f);
    EXPECT_FLOAT_EQ(audio.getSoundVolume(), 0.7f);
    
    audio.setMusicVolume(0.6f);
    EXPECT_FLOAT_EQ(audio.getMusicVolume(), 0.6f);
    
    audio.setVoiceVolume(0.8f);
    EXPECT_FLOAT_EQ(audio.getVoiceVolume(), 0.8f);
}

// Test channel management
TEST(AudioSystem, ChannelManagement) {
    auto& audio = AudioSystem::getInstance();
    audio.setMaxChannels(5);
    
    // Load test sounds
    for (int i = 0; i < 10; i++) {
        audio.loadSound("sound" + std::to_string(i), "test.wav");
    }
    
    // Play more sounds than max channels
    for (int i = 0; i < 10; i++) {
        audio.playSound("sound" + std::to_string(i), 1.0f, false, i);
    }
    
    // Should not exceed max channels
    EXPECT_LE(audio.getActiveChannelCount(), 5);
}

// Test resource management
TEST(AudioSystem, ResourceManagement) {
    auto& audio = AudioSystem::getInstance();
    
    EXPECT_TRUE(audio.loadSound("test_sfx", "test.wav", AudioCategory::SFX));
    EXPECT_TRUE(audio.loadSound("test_voice", "test.wav", AudioCategory::VOICE));
    EXPECT_TRUE(audio.loadMusic("test_music", "test.wav"));
    
    audio.unloadSound("test_sfx");
    audio.unloadSound("test_voice");
    audio.unloadMusic("test_music");
    
    // Resources should be properly freed
}

// Test thread safety
TEST(AudioSystem, ThreadSafety) {
    auto& audio = AudioSystem::getInstance();
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.emplace_back([&audio, i]() {
            audio.playSound("sound" + std::to_string(i), 1.0f);
            audio.setMasterVolume(0.5f);
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Should complete without crashes
}
```

## Performance Benchmarks

### Target Metrics
- **Audio Latency**: < 50ms from playSound() to actual sound playback
- **Channel Switch Time**: < 10ms to evict and play new sound
- **Memory Overhead**: < 100MB for 100 loaded sounds
- **CPU Usage**: < 5% CPU when playing 32 simultaneous sounds

### Stress Tests
- Load 1000 sounds and verify memory usage
- Play 100 sounds simultaneously for 1 hour
- Rapidly load/unload resources for 10 minutes
- Adjust volumes 1000 times per second for 1 minute

## Known Issues

### Current Limitations
- Maximum 32 simultaneous sounds (configurable)
- No 3D positional audio yet
- No audio occlusion
- Crossfade between music tracks uses simple stop/play

### Future Improvements
- Add fade-in/fade-out effects
- Implement 3D audio with HRTF
- Add audio compression for large files
- Implement voice line queuing system
