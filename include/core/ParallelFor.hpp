#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

namespace core {

template <typename Func>
inline void parallelFor(std::size_t count, std::size_t minChunkSize, Func&& func)
{
    if (count == 0) {
        return;
    }

    const unsigned int hardwareThreads = std::thread::hardware_concurrency();
    const std::size_t maxThreads = hardwareThreads == 0 ? 1u : static_cast<std::size_t>(hardwareThreads);

    if (maxThreads <= 1 || count < minChunkSize) {
        for (std::size_t i = 0; i < count; ++i) {
            func(i);
        }
        return;
    }

    const std::size_t maxUsableThreads = (count + minChunkSize - 1) / minChunkSize;
    const std::size_t threadCount = std::min(maxThreads, std::max<std::size_t>(1, maxUsableThreads));

    if (threadCount <= 1) {
        for (std::size_t i = 0; i < count; ++i) {
            func(i);
        }
        return;
    }

    std::atomic_size_t index{0};
    auto worker = [&]() {
        while (true) {
            const std::size_t i = index.fetch_add(1, std::memory_order_relaxed);
            if (i >= count) {
                break;
            }
            func(i);
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(threadCount - 1);
    for (std::size_t t = 0; t + 1 < threadCount; ++t) {
        threads.emplace_back(worker);
    }
    worker();

    for (auto& thread : threads) {
        thread.join();
    }
}

} // namespace core
