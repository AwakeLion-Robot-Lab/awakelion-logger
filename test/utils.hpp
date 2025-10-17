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

#ifndef TEST__UTILS_HPP
#define TEST__UTILS_HPP

// C++ standard library
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <ratio>
#include <vector>

/***
 * @brief namespace for test utilities
 * @author jinhua "siyiovo" deng
 */
namespace aw_test {
/***
 * @brief tictoc class for time measurement
 */
class TicToc {
public:
    /***
     * @brief constructor
     */
    explicit TicToc() = default;

    /***
     * @brief destructor
     */
    ~TicToc() = default;

    /***
     * @brief record start timepoint
     */
    void tic()
    {
        start_ = std::chrono::high_resolution_clock::now();
    }

    /***
     * @brief record duration
     * @return duration in nanoseconds
     */
    inline long long toc() const noexcept
    {
        const auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_).count();
    }

private:
    /***
     * @brief start time point
     */
    std::chrono::high_resolution_clock::time_point start_;
};

/***
 * @brief Latency class for API response time latency
 * @note
 * the latency indicators are follow statistics \n
 * `min` - the fastest stats \n
 * `avg` - the average stats \n
 * `p50` - the median stats \n
 * `p95` - the 95% stats \n
 * `p99` - the 99% stats \n
 * `max` - the lowest stats
 *
 */
class Latency {
public:
    /***
     * @brief add latency sample
     * @param latency_ns latency in nanoseconds
     */
    void add(long long latency_ns)
    {
        latencies_.push_back(latency_ns);
    }

    /***
     * @brief print statistics
     * @param test_name test name
     * @param os output stream (default: std::cout)
     */
    void print(const std::string& test_name, std::ostream& os = std::cout)
    {
        if (latencies_.empty())
            return;

        std::sort(latencies_.begin(), latencies_.end());

        long long sum = std::accumulate(latencies_.begin(), latencies_.end(), 0LL);
        double avg = static_cast<double>(sum) / latencies_.size();
        long long min = latencies_.front();
        long long max = latencies_.back();
        long long p50 = latencies_[latencies_.size() / 2];
        long long p95 = latencies_[static_cast<size_t>(latencies_.size() * 0.95)];
        long long p99 = latencies_[static_cast<size_t>(latencies_.size() * 0.99)];

        os << "\n========== " << test_name << " ==========\n";
        os << "Count:      " << latencies_.size() << " calls\n";
        os << std::fixed << std::setprecision(3);
        os << "Min:        " << min << " ns\n";
        os << "Avg:        " << avg << " ns\n";
        os << "P50:        " << p50 << " ns\n";
        os << "P95:        " << p95 << " ns\n";
        os << "P99:        " << p99 << " ns\n";
        os << "Max:        " << max << " ns\n";

        double throughput = 1e9 / avg;
        os << "Throughput: " << static_cast<long long>(throughput) << " calls/sec\n";
        os << "=======================================\n";
    }

    /***
     * @brief clear all samples
     */
    void clear()
    {
        latencies_.clear();
    }

    /***
     * @brief get sample count
     */
    size_t count() const
    {
        return latencies_.size();
    }

private:
    /***
     * @brief latencies vector
     */
    std::vector<long long> latencies_;
};

} // namespace aw_test

#endif //! TEST__UTILS_HPP
