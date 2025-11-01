# Standard of Iron - Test Suite

This directory contains unit and integration tests for the Standard of Iron project using Google Test.

## Structure

```
tests/
├── core/              # Core engine tests
│   └── serialization_test.cpp  # Entity/World serialization tests
├── db/                # Database tests
│   └── save_storage_test.cpp   # SQLite save/load tests
└── CMakeLists.txt     # Test build configuration
```

## Running Tests

### Build and Run All Tests
```bash
make test
```

### Run Tests Directly
```bash
cd build
./bin/standard_of_iron_tests
```

### Run Specific Tests
```bash
# Run only serialization tests
./bin/standard_of_iron_tests --gtest_filter=SerializationTest.*

# Run only database tests
./bin/standard_of_iron_tests --gtest_filter=SaveStorageTest.*

# Run a specific test case
./bin/standard_of_iron_tests --gtest_filter=SerializationTest.EntitySerializationBasic
```

### Verbose Output
```bash
./bin/standard_of_iron_tests --gtest_color=yes
```

## Test Categories

### Core Serialization Tests (`core/serialization_test.cpp`)
Tests for JSON serialization and deserialization of game objects:
- **Entity serialization**: Basic entity save/load
- **Component serialization**: Individual component types (Transform, Unit, Movement, Attack, etc.)
- **Round-trip testing**: Serialize→Deserialize→Verify data integrity
- **Edge cases**: Missing fields, malformed JSON, default values
- **File I/O**: Save to file and load from file

### Database Integration Tests (`db/save_storage_test.cpp`)
Tests for SQLite-based save game storage:
- **Initialization**: In-memory database setup
- **Save/Load**: Store and retrieve save slots
- **Schema management**: Database schema creation and validation
- **Error handling**: Non-existent slots, constraint violations
- **Complex data**: Large saves, complex metadata, special characters
- **CRUD operations**: List, delete, and overwrite slots

## Adding New Tests

### 1. Create a New Test File
```cpp
#include <gtest/gtest.h>
// Include necessary headers

class MyFeatureTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code
    }

    void TearDown() override {
        // Cleanup code
    }
};

TEST_F(MyFeatureTest, TestCase) {
    // Test code
    EXPECT_EQ(expected, actual);
}
```

### 2. Add to CMakeLists.txt
```cmake
add_executable(standard_of_iron_tests
    core/serialization_test.cpp
    db/save_storage_test.cpp
    your_category/your_test.cpp  # Add your test here
)
```

### 3. Rebuild and Run
```bash
make test
```

## Test Conventions

### Naming
- Test suite names should match the class/feature being tested
- Test case names should describe what is being tested
- Use descriptive names: `TEST_F(SerializationTest, EntityDeserializationRoundTrip)`

### Assertions
- Use `EXPECT_*` for non-fatal assertions (test continues)
- Use `ASSERT_*` for fatal assertions (test stops)
- Common assertions:
  - `EXPECT_EQ(a, b)` - equality
  - `EXPECT_TRUE(condition)` - boolean true
  - `EXPECT_FLOAT_EQ(a, b)` - floating-point equality
  - `EXPECT_NE(a, b)` - not equal

### Test Organization
- Group related tests in the same test fixture
- Use descriptive test names
- Test one thing per test case
- Keep tests independent

## Dependencies

The test suite uses:
- **Google Test** (v1.14.0) - Testing framework
- **Qt6/Qt5** - Core, Gui, SQL modules
- **engine_core** - Core game engine library
- **game_systems** - Game systems library

## Continuous Integration

Tests are automatically run in CI when:
- Pull requests are created
- Code is pushed to main branch
- Release builds are created

## Coverage

Current test coverage focuses on:
- ✅ Entity and component serialization
- ✅ World serialization
- ✅ SQLite save/load operations
- ✅ Database CRUD operations
- ✅ Error handling and edge cases

Future coverage should include:
- [ ] Terrain serialization
- [ ] AI system testing
- [ ] Combat system testing
- [ ] Pathfinding tests
- [ ] Production system tests

## Troubleshooting

### Tests Not Found
If tests aren't being discovered:
```bash
rm -rf build
make configure
make build
```

### Database Lock Errors
Tests use in-memory databases (`:memory:`), so file locks shouldn't occur. If they do:
- Check for orphaned test processes
- Restart your terminal

### Qt-related Errors
Ensure Qt development packages are installed:
```bash
make install  # Installs dependencies
```

## References

- [Google Test Documentation](https://google.github.io/googletest/)
- [Google Test Primer](https://google.github.io/googletest/primer.html)
- [Google Test Advanced Guide](https://google.github.io/googletest/advanced.html)
