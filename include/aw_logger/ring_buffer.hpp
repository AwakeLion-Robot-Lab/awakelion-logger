// Copyright 2025 siyiovo
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef RING_BUFFER_HPP
#define RING_BUFFER_HPP

// C++ standard library
#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>

// aw_logger library
#include "aw_logger/exception.hpp"

/***
 * @brief a low-latency, high-throughput and few-dependency logger for `AwakeLion Robot Lab` project
 * @note fundamental structure is inspired by [sylar logger](https://github.com/sylar-yin/sylar) and implement is
 * inspired by [log4j2](https://logging.apache.org/log4j/2.12.x/) and [minilog](https://github.com/archibate/minilog)
 * @author jinhua "siyiovo" deng
 */
namespace aw_logger {
/***
 * @brief a lock-free MPMC ring buffer without `std::mutex` and mirror MSB but with CAS opertaion, support `std::allocator` to manage memory
 * @tparam DataT data type
 * @tparam Allocator allocator type
 * @details inspired by [Vyukov​'​s MPMCQueue](https://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue) and Linux kfifo
 */
template<typename DataT, typename Allocator = std::allocator<DataT>>
class RingBuffer {
public:
    using value_t = DataT;

    /***
     * @brief constructor
     * @param capacity capacity of ring buffer
     */
    explicit RingBuffer(size_t capacity);

    /***
     * @brief destructor
     */
    ~RingBuffer();

    /***
     * @brief push data into ring buffer
     * @tparam U universal reference type
     * @param data data to be pushed, rvalue reference
     * @note if size of pushed data is greater than rest size, it will be discarded
     * @details 'cause loggger is asynchronous, here we use CAS operation to update atomic index
     */
    template<typename U>
    bool push(U&& data);

    /***
     * @brief pop out data from ring buffer, FIFO
     * @param data pop-out data
     */
    bool pop(value_t& data);

    /***
     * @brief get capacity of ring buffer
     * @retval capacity of ring buffer
     */
    inline const size_t getCapacity() const noexcept
    {
        return mask_ + 1;
    }

    /***
     * @brief get size of ring buffer
     * @retval size of ring buffer
     * @details the return means used size
     */
    inline const size_t getSize() const noexcept;

    /***
     * @brief get the rest of size of ring buffer
     */
    inline const size_t getRestSize() const noexcept
    {
        return mask_ + 1 - getSize();
    }

private:
    /***
     * @brief cell structure for ring buffer
     * @param sequence_ atomic sequence counter
     * @param data_ data of specific type
     * @details
     * [IMPORTANT]:
     * sequence is a counter, after write/read operation, it will be updated
     * (1) if according index equals to sequence, it means this cell is ready for write
     * (2) if according index + 1 equals to sequence, it means this cell already written,
     *     which means this cell is ready for read
     *     so the relation of write/read index and sequence is like `std::condition_variable`, after write/read operation,
     *     we can know the state of cell by comparing index and sequence
     * (3) if according index + n (n > 1) equals to sequence, it means this cell has already handled by another thread
     *     but why?
     *     push operation will add sequence by 1
     *     pop operation will add sequence by mask_ + 1, which means in next round
     *     you can quickly figure out whether in next round via pow of 2 mask
     *     so if sequence is greater than index + n, it means this cell has already write and read by another thread
     */
    struct cell_t {
        std::atomic<size_t> sequence_;
        value_t data_;
    };

    /* rebind template of allocator */
    using allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<cell_t>;
    using allocator_trait = typename std::allocator_traits<allocator_type>;

    /***
     * @brief const buffer of cells
     */
    cell_t* const buffer_;

    /***
     * @brief allocator to manage memory
     */
    allocator_type alloc_;

    /***
     * @brief atomic write index
     * @details wIdx and rIdx in different cache line to avoid false sharing
     */
    alignas(64) std::atomic<size_t> wIdx_;

    /***
     * @brief atomic read index
     * @details wIdx and rIdx in different cache line to avoid false sharing
     */
    alignas(64) std::atomic<size_t> rIdx_;

    /***
     * @brief capacity mask for fast modulo operation
     */
    const size_t mask_;

    /***
     * @brief round up to power of 2
     * @param num input number
     * @return rounded number
     */
    inline size_t roundUpPow2(size_t num) const noexcept
    {
        num--;
        num |= num >> 1;
        num |= num >> 2;
        num |= num >> 4;
        num |= num >> 8;
        num |= num >> 16;
#if SIZE_MAX > UINT32_MAX
        num |= num >> 32;
#endif
        num++;
        return num;
    }

    /***
     * @brief convert index to pointer
     * @param idx input index
     * @return real index in buffer
     * @details JUST for power of 2 mask!
     */
    inline const size_t toPtr(size_t idx) const noexcept
    {
        return idx & mask_;
    }
};
} // namespace aw_logger

#endif //! RING_BUFFER_HPP
