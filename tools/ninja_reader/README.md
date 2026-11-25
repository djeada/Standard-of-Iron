# Ninja Reader

## Introduction

**Ninja Reader** (`ninja_reader`) represents a command-line interface
development tool that is responsible for reading Ninja logs and getting
source files taking the longest to compile.

**Maintainer**: dywoq <dywoq.contact@gmail.com> (https://github.com/dywoq/)

## Commands

### `read`

**Synopsis**:
```
ninja_reader read
```

**Description**: This command is responsible for fetching compilation results
of the `.ninja_log` file, printing the total/average compilation time and top
slowest source files.

**Options**:
- `--top` (`number`, default: 20): This flag constraints the count of printed
  top slowest source files.

- `--build` (`string`): This flag specifies the build directory where
  `.ninja_log` is located. It is required.

**Failure Cases**:
- The `.ninja_log` file does not exist in the specified build directory, or
  the build directory does not exist itself.

- The `.ninja_log` is invalid, meaning it does not follow the standard layout
  of `.ninja_log` files.

### `help`

**Synopsis**:
```
ninja_reader help
```

**Description**: This command is responsible for printing available commands,
including itself.

**Options**: _None_

**Failure Cases**: _None_

## License

```
Copyright 2026 dywoq - MIT License
https://github.com/djeada/Standard-of-Iron
```
