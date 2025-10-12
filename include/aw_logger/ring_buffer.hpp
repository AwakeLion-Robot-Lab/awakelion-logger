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

// C standard library
#include <stddef.h>
#include <stdio.h>

// C++ standard library
#include <atomic>
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
 * @brief a lock-free ring buffer without `std::mutex` and mirror MSB, also allocate memory via `std::allocator`
 * @tparam DataT data type
 * @tparam Allocator `std::allocator` type
 * @details inspired by https://github.com/bobwenstudy/simple_ringbuffer and Linux kfifo(it's lock-free but its capacity must be
 * power of 2)
 */
template<typename DataT, typename Allocator = std::allocator<DataT>>
class RingBuffer {
public:
    using value_type = DataT;
    using allocator_type = Allocator;
    using allocator_trait = typename std::allocator_traits<allocator_type>;

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
    bool pop(value_type& data);

    /***
     * @brief get capacity of ring buffer
     * @retval capacity of ring buffer
     */
    inline const size_t getCapacity() const noexcept
    {
        return capacity_;
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
        return capacity_ - getSize();
    }

private:
    /***
     * @brief buffer of specific type
     */
    value_type* buffer_;

    /***
     * @brief allocator
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
     * @brief the number of specific type of ring buffer
     */
    size_t capacity_;

    /***
     * @brief mirror capacity, which is `2 * capacity_`
     */
    size_t mirror_capacity_;

#ifndef ALIGN4
    /***
     * @brief Round up to nearest multiple of 4, for unsigned integers
     * @details
     *   copied from https://github.com/bobwenstudy/simple_ringbuffer
     *   The addition of 3 forces x into the next multiple of 4. This is responsible
     *   for the rounding in the the next step, to be Up.
     *   For ANDing of ~3: We observe y & (~3) == (y>>2)<<2, and we recognize
     *   (y>>2) as a floored division, which is almost undone by the left-shift. The
     *   flooring can't be undone so have achieved a rounding.
     *
     *   Examples:
     *    MROUND( 0) =  0
     *    MROUND( 1) =  4
     *    MROUND( 2) =  4
     *    MROUND( 3) =  4
     *    MROUND( 4) =  4
     *    MROUND( 5) =  8
     *    MROUND( 8) =  8
     *    MROUND( 9) = 12
     *    MROUND(13) = 16
     */
    #define ALIGN4(x) (((uint32_t)(x) + 3u) & (~((uint32_t)3)))
#endif //! ALIGN4

    /***
     * @brief index to pointer which in real physical memory
     * @param idx index
     * @details based on the way of mirror memory and wrapping of unsigned int type, index is in range of [0, 2 * capacity - 1], and pointer is in range of [0, capacity - 1]
     */
    inline const size_t toPtr(size_t idx) const noexcept
    {
        return (size_t)((idx >= capacity_) ? (idx - capacity_) : idx);
    }
};
} // namespace aw_logger

#endif //! RING_BUFFER_HPP
