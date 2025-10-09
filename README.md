# Awakelion-Logger

a low-latency, high-throughput and few-dependency logger for `AwakeLion Robot Lab` project. It's highly base on modern C++ standard library (C++17 and later).

---

## Dependencies

### nlohmann JSON

a flexible and light-weighted JSON C++ library for log pattern customization. It's already inside `include/nlohmann` folder.

### uwebsockets

a light-weighted C++ websocket library for monitoring log information so that you can watch debugs in real time remotely.

## Features

> * Awakelion-Logger is based on async-logger(MPSC) and sync-appender(SPSC) mode, which is inspired from [log4j2](https://logging.apache.org/log4j/2.12.x/).
> * whole strcuture is based on [sylar-logger](https://github.com/sylar-yin/sylar/blob/master/sylar%2Flog.h), which means that use logger manager singleton class to manage multi-loggers in multi-threads. besides, modern c++ function is inspired from [minilog](https://github.com/archibate/minilog) and [fmtlib](https://github.com/fmtlib).
> * the core of implementation about asynchronous is ringbuffer, which is lock-free, mirrored index memory and no pow-of-2 limitation. I take up a lot of reference below:
>
>> * [kfifo](https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/tree/lib/kfifo.c) for mirrored index memory.
>> * [simple_ringbuffer](https://github.com/bobwenstudy/simple_ringbuffer) for no pow-of-2 limitation.
>> * [c++ ringbuffer](https://b23.tv/W79kaS4) for lock-free (atomic manipulation and memory order).
>> * also use `std::allocator` as standard of memory allocation, like placement new and buffer destruct.
>>


> [!CAUTION]
> for now, the ringbuffer just for SPSC because CAS operation is too complicate for me :<, however, there is a better way to do it within [Vyukov&#39;s MPMCQueue](https://int08h.com/post/ode-to-a-vyukov-queue/).
