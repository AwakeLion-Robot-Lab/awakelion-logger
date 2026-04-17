<div align="center">

# Awakelion-Logger

A low-latency, high-throughput and few-dependencies logger for `AwakeLion Robot Lab` project. It's highly based on modern C++ standard library (C++20).

![img](./docs/log_format_type.png "log_format_types")

[![build-and-test](https://github.com/AwakeLion-Robot-Lab/awakelion-logger/actions/workflows/ci.yml/badge.svg)](https://github.com/AwakeLion-Robot-Lab/awakelion-logger/actions/workflows/ci.yml) [![cpp-linter](https://github.com/AwakeLion-Robot-Lab/awakelion-logger/actions/workflows/cpp-linter.yml/badge.svg)](https://github.com/AwakeLion-Robot-Lab/awakelion-logger/actions/workflows/cpp-linter.yml) [![docs](https://github.com/AwakeLion-Robot-Lab/awakelion-logger/actions/workflows/docs.yml/badge.svg)](https://github.com/AwakeLion-Robot-Lab/awakelion-logger/actions/workflows/docs.yml)

English | [简体中文](./docs/README.zh_CN.md)

[github-pages](https://awakelion-robot-lab.github.io/awakelion-logger/) for API docs

</div>

---

## Features

### Pipeline

```mermaid
flowchart LR
  subgraph LoggerManager["LoggerManager"]
    Root["Root Logger"]
    LoggerA["Logger A"]
    LoggerB["Logger B"]
    LoggerC["Logger C"]

    LoggerA --> Root
    LoggerB --> Root
    LoggerC --> Root
  end

  Root -->|"submit(event)"| Submit[Logger::submit]

  subgraph Frontend["Frontend Threads"]
    Macro[Log Macro / Caller] --> Wrap[Create LogEvent]
    Wrap --> Submit
  end

  Submit --> Filter{Level ≥ Threshold?}
    Filter -->|No| Drop[Drop Event]
    Filter -->|Yes| Enqueue[RingBuffer::push]
  Enqueue --> Notify[Notify Worker]

  subgraph Backend["Worker Thread"]
    Notify --> Worker[Wait / Loop]
    Worker --> Pop[RingBuffer::pop]
    Pop -->|Success| Format[Formatter::formatComponents]
    Pop -->|Empty| Wait[Wait for Signal]
    Wait --> Worker

    Format --> AppSel{Iterate Appenders}
    AppSel --> Console[ConsoleAppender]
    AppSel --> File[FileAppender]
    AppSel --> Web[WebSocketAppender]

    Console --> Stdout[`std::cout` / `std::cerr`]
    File --> LogFile[Rotating Log File]
    Web --> Clients[WebSocket Clients]
  end
```

### Structure

* Awakelion-Logger is based on async-logger(MPSC) and sync-appender(SPSC) mode, which is inspired from [log4j2](https://logging.apache.org/log4j/2.12.x/).
* Whole strcuture is based on [sylar-logger](https://github.com/sylar-yin/sylar/blob/master/sylar%2Flog.h), which means that use logger manager singleton class to manage multi-loggers in multi-threads. Besides, modern c++ function is inspired from [minilog](https://github.com/archibate/minilog) and [fmtlib](https://github.com/fmtlib).
* The design of appenders are inspired by `sink` in [spdlog](https://github.com/gabime/spdlog/tree/v1.x/include/spdlog/sinks).
* Customize log format at runtime with pattern strings (see [hello_aw_logger](./test/hello_aw_logger.cpp)); hundreds of colors are [built in](include/aw_logger/fmt_base.hpp).

### Core of asynchronous

The core of implementation about asynchronous is **MPMC ringbuffer**, which is lock-free and with mirrored index memory. I take in a lot of reference below:

* Deeply inspired by  [Vyukov&#39;s MPMCQueue](https://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue), which is a better way to adapt MPMC model.
* [kfifo](https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/tree/lib/kfifo.c) for mirrored index memory.
* Use `std::allocator` as standard of memory allocation, like placement new.

> [!NOTE]
> I already found a helpful [blog](https://pskrgag.github.io/post/mpmc_vuykov/) to explain Vyukov's MPMCQueue, and here I provide my thought.

**The core of Vyukov's MPMCQueue is the sequence of each cell**, here cell is the base element of ringbuffer, which includes sequence and input `DataT` data.

In fact, sequence is an atomic counter, according to source code, **it indicates the status of between cell and operator thread**.

#### Key parameters

* `curr_wIdx / curr_rIdx`: **write index / read index in current thread.**
* `curr_seq`: **sequence of current cell in current thread.**

#### How it update

|                 |                  `push()`                  |                          `pop()`                           |
| :-------------: | :----------------------------------------: | :--------------------------------------------------------: |
| **description** | add to `curr_wIdx + 1`, move to next cell. | add to `curr_rIdx + capacity`, move to next mirror memory. |
| **expression**  |         `curr_seq = curr_wIdx + 1`         |             `curr_seq = curr_rIdx + mask_ + 1`             |

#### Constructor

```cpp
buffer_ = allocator_trait::allocate(alloc_, r_capacity);
    for (size_t i = 0; i < r_capacity; i++)
    {
        /* construct empty cell */
        allocator_trait::construct(alloc_, buffer_ + i);
        /* initialize sequence */
        (buffer_ + i)->sequence_.store(i, std::memory_order_relaxed);
    }
```

#### Producer perspective

|     status      |                                                    available                                                     |                             pending                              |                                                                             unavailable                                                                             |
| :-------------: | :--------------------------------------------------------------------------------------------------------------: | :--------------------------------------------------------------: | :-----------------------------------------------------------------------------------------------------------------------------------------------------------------: |
| **description** | default to its index,<br />producer can write.<br />after update, it signal<br />to consumer for `ready` status. | occupied by another producer,<br />wait for write and try again. | this cell already wrap-around(property of unsigned int),<br />but write index not, that means all cells are written,<br /> which also means the ringbuffer is full. |
| **expression**  |                                                  `== curr_wIdx`                                                  |                          `> curr_wIdx`                           |                                                                            `< curr_wIdx`                                                                            |

#### Consumer perspective

|     status      |                                                         available                                                          |                                                   pending                                                   |                                 unavailable                                 |
| :-------------: | :------------------------------------------------------------------------------------------------------------------------: | :---------------------------------------------------------------------------------------------------------: | :-------------------------------------------------------------------------: |
| **description** | equal to value after `push()` update,<br />it means it's time to read,<br />which is similar to `std::condition_variable`. | this cell has already<br />read, try to load <br />`curr_rIdx` status again<br />for a next read operation. | data in all cells have been read,<br />which means the ringbuffer is empty. |
| **expression**  |                                                     `== curr_rIdx + 1`                                                     |                                              `> curr_rIdx + 1`                                              |                              `< curr_rIdx + 1`                              |

## Dependencies

### nlohmann JSON

A flexible and lightweight JSON C++ library kept for optional structured logging; runtime pattern configuration no longer depends on external JSON files. Included in `include/3rdparty/nlohmann` (version 3.12.0).

### IXWebSocket

A lightweight C++ WebSocket library for real-time log streaming.

## Installation

> Awakelion-Logger is a **header-only library**. Include the headers and set your pattern in code—no external config file required.

### Requirements

- gcc 13+
- [xmake](https://xmake.io/) 2.9.8+

### Quick Setup with xmake

this project is built via `xmake`, manage via `xrepo`.

#### For xmake Direct Users (Recommended)
you can install in your project via command `xmake install awakelion-logger`, or make builtin integration in `xmake.lua` of your project like:
```bash
-- ...exist codes
add_repositories("awakelion-xmake-repo https://github.com/AwakeLion-Robot-Lab/awakelion-xmake-repo.git")
add_requires("awakelion-logger")
```

#### For xmake Source Coders

```bash
git clone https://github.com/AwakeLion-Robot-Lab/awakelion-logger.git
cd awakelion-logger

# download requirement
sudo apt install -y libssl-dev
xmake build -y

# build and run tests (optional)
xmake f --test=y -m release -y
xmake test
```

#### For cmake Users

If you prefer cmake, just follow the normal way within prebuild `CMakeLists.txt`:

```bash
# make and test file
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
nproc
make -j<nproc-num>
ctest --output-on-failure
```

> [!NOTE]
> you can auto-update `CMakeLists.txt` within xmake command `xmake project -k cmakelists`.

And you just make it! Now just include in your C++ files like below:

```cpp
#include "aw_logger/aw_logger.hpp"
```

### Quick Start Example

You can start with code as below:

```cpp
#include "aw_logger/aw_logger.hpp"

int main() {
    auto logger = aw_logger::getLogger("hello_aw_logger");

    AW_LOG_INFO(logger, "Hello aw_logger!");
    AW_LOG_FMT_INFO(logger, "Value: {}", 42);

    return 0;
}
```

#### Color Control

Colors are precomputed in the formatter; you can customize or disable them per appender:

```cpp
// customize level colors and keep console colorized
auto factory = std::make_unique<aw_logger::ComponentFactory>();
auto formatter = std::make_unique<aw_logger::Formatter>(std::move(factory));
formatter->setLevelColor(aw_logger::LogLevel::level::INFO, "cyan");
formatter->setLevelColor(aw_logger::LogLevel::level::WARN, "orange");
formatter->setDebugColor("violet");

auto console_appender = std::make_shared<aw_logger::ConsoleAppender>(std::move(formatter));
console_appender->enableColor(true);

auto logger = aw_logger::getLogger("colorful");
logger->setAppender(console_appender);

AW_LOG_INFO(logger, "Info is cyan");
AW_LOG_WARN(logger, "Warn is orange");
AW_LOG_DEBUG(logger, "Debug is violet");

// file appender defaults to no ANSI color; you can also enforce disable
auto file_appender = std::make_shared<aw_logger::FileAppender>("logs/app.log");
file_appender->enableColor(false);
```

#### Custom Pattern Format

You can customize the log output format using pattern strings. Here are the available format specifiers:

| Specifier | Description                                         |
| :-------: | :-------------------------------------------------- |
|   `%t`    | Timestamp                                           |
|   `%p`    | Log level (DEBUG, INFO, WARN, etc.)                 |
|   `%i`    | Thread ID                                           |
|   `%f`    | Source location - file name                         |
|   `%n`    | Source location - function name                     |
|   `%l`    | Source location - line number                       |
|   `%m`    | Log message                                         |
|   text    | Any text not prefixed with `%` will be output as-is |

and example is below:

```cpp
#include "aw_logger/aw_logger.hpp"

int main() {
    // Create custom pattern: [timestamp] <level> message
    auto factory = std::make_shared<aw_logger::ComponentFactory>("[%t] <%p> %m");
    auto formatter = std::make_shared<aw_logger::Formatter>(factory);
    auto appender = std::make_shared<aw_logger::ConsoleAppender>(formatter);

    auto logger = aw_logger::getLogger("custom");
    logger->setAppender(appender);

    AW_LOG_INFO(logger, "Custom format example");
    // Output: [2025-10-29 22:35:38.456244408] <INFO> Custom format example

    return 0;
}
```

You can set or change patterns directly in code at runtime (see [hello_aw_logger.cpp](./test/hello_aw_logger.cpp) for examples).

### Benchmark Stats

Performance tests conducted on the following environment:

- Platform: Linux, VMware Workstation 17pro
- Performance: 4 core CPU(usage < 20%), <1GB avaliable memory
- Test tool: GoogleTest with [custom utilities](./test/utils.hpp)

#### Multi-threaded Performance (Console Output)

|     Metric     |               Value                |
| :------------: | :--------------------------------: |
|    Threads     |                 4                  |
|   Total Logs   |       100,000 * 4 = 400,000        |
|    Log Size    | 130-150 bytes(without `file_name`) |
|  Average Time  |        2185.2 ms (5 rounds)        |
| **Throughput** |       **~183,000 logs/sec**        |

*Note: log size is includes all the format except for the `file_name`*

#### `Valgrind` Memory Leak Test

test report for `hello_aw_logger`:

```bash
==53296== Memcheck, a memory error detector
==53296== Copyright (C) 2002-2017, and GNU GPL'd, by Julian Seward et al.
==53296== Using Valgrind-3.18.1 and LibVEX; rerun with -h for copyright info
==53296== Command: ./build/linux/arm64/debug/fosu-awakelion/awakelion-logger-test-hello_aw_logger
==53296== Parent PID: 21910
==53296==
==53296== Warning: invalid file descriptor -1 in syscall read()
==53296== Warning: invalid file descriptor -1 in syscall read()
==53296==
==53296== HEAP SUMMARY:
==53296==     in use at exit: 0 bytes in 0 blocks
==53296==   total heap usage: 66,936 allocs, 66,936 frees, 6,521,737 bytes allocated
==53296==
==53296== All heap blocks were freed -- no leaks are possible
==53296==
==53296== For lists of detected and suppressed errors, rerun with: -s
==53296== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```

test report for `load_benchmark`:

```bash
==61077== Memcheck, a memory error detector
==61077== Copyright (C) 2002-2017, and GNU GPL'd, by Julian Seward et al.
==61077== Using Valgrind-3.18.1 and LibVEX; rerun with -h for copyright info
==61077== Command: ./build/linux/arm64/debug/fosu-awakelion/awakelion-logger-test-load_benchmark
==61077== Parent PID: 21910
==61077==
==61077==
==61077== HEAP SUMMARY:
==61077==     in use at exit: 0 bytes in 0 blocks
==61077==   total heap usage: 2,345,651 allocs, 2,345,651 frees, 243,733,771 bytes allocated
==61077==
==61077== All heap blocks were freed -- no leaks are possible
==61077==
==61077== For lists of detected and suppressed errors, rerun with: -s
==61077== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```

it's seemed that no memory leak at all, if your outcome is opposite to mine, please send PR and I will check it out when I'm free!

## TODO

- [X] support `ComponentFactory` class which is used to manage component registration. @done(25-10-11 23:19)
- [X] support `LoggerManager` singleton class to manager loggers in multi-threads. @started(25-10-11 23:19) @done(25-10-12 22:35)
- [X] support websocket for monitoring log information in real time, considering library as [IXWebSocket](https://github.com/machinezone/IXWebSocket). @started(25-10-15 03:33) @high @done(25-11-21 23:59) @lasted(5w2d20h26m48s)
- [X] process ringbuffer load test and appenders latency test. @started(25-10-11 23:19) @high @done(25-10-18 00:08) @lasted(6d49m31s)
- [X] support `%` as format specifier in `ComponentFactory` class. @low @done(25-10-29 22:40)
- [X] after load test, consider to support double ringbuffer to reduce lock time. @low @done(25-10-18 03:02) [siyiya]: no need for now.
- [X] support formatter on cpp server, including uploading ANSI color and parse patterns like `Formatter` class. @low @done(25-11-23 20:33)
