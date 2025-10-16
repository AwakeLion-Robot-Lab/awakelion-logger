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
#include <chrono>
#include <iostream>

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
    explicit TicToc() = default;

    ~TicToc() = default;

    void tic()
    {
        start_ = std::chrono::high_resolution_clock::now();
    }

    void toc()
    {
        end_ = std::chrono::high_resolution_clock::now();
        std::cout << "Time elapsed: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(end_ - start_).count()
                  << " ms" << std::endl;
    }

private:
    /***
     * @brief start time point
     */
    std::chrono::high_resolution_clock::time_point start_;

    /***
     * @brief end time point
     */
    std::chrono::high_resolution_clock::time_point end_;
};
} // namespace aw_test

#endif //! TEST__UTILS_HPP
