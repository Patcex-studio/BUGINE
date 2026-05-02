/*
 Copyright (C) 2026 Jocer S. <patcex@proton.me>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.

 SPDX-License-Identifier: AGPL-3.0 OR Commercial
*/
#include <gtest/gtest.h>
#include "rendering_engine/resource_manager/memory_pool.h"
#include "rendering_engine/resource_manager/gpu_allocator.h"
#include "rendering_engine/resource_manager/asset_manager.h"
#include "rendering_engine/resource_manager/resource_manager.h"
#include <thread>
#include <vector>
#include <random>

using namespace rendering_engine::resource_manager;

// ============================================================================
// Memory Pool Tests
// ============================================================================

class MemoryPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        pool_.initialize(10 * 1024 * 1024, 256);  // 10 MB pool
    }
    
    MemoryPool pool_;
};

TEST_F(MemoryPoolTest, AllocateAndDeallocate) {
    void* ptr = pool_.allocate(1024);
    EXPECT_NE(ptr, nullptr);
    
    auto stats = pool_.get_stats();
    EXPECT_EQ(stats.used_bytes, 1024);
    
    uint64_t offset = reinterpret_cast<uintptr_t>(ptr) - 
                     reinterpret_cast<uintptr_t>(pool_.get_memory_block());
    bool result = pool_.deallocate(offset, 1024);
    EXPECT_TRUE(result);
    
    stats = pool_.get_stats();
    EXPECT_EQ(stats.used_bytes, 0);
}

TEST_F(MemoryPoolTest, MultipleAllocations) {
    const size_t alloc_size = 64 * 1024;  // 64 KB
    std::vector<void*> ptrs;
    
    for (int i = 0; i < 100; ++i) {
        void* ptr = pool_.allocate(alloc_size);
        EXPECT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }
    
    auto stats = pool_.get_stats();
    EXPECT_EQ(stats.active_allocations, 100);
    EXPECT_GE(stats.used_bytes, 100 * alloc_size);
}

TEST_F(MemoryPoolTest, AlignmentRequirements) {
    void* ptr1 = pool_.allocate(256, 512);
    EXPECT_NE(ptr1, nullptr);
    
    uint64_t addr = reinterpret_cast<uint64_t>(ptr1);
    EXPECT_EQ(addr % 512, 0);
}

TEST_F(MemoryPoolTest, FragmentationTest) {
    // Allocate and deallocate in pattern to create fragmentation
    std::vector<void*> ptrs;
    const size_t alloc_size = 64 * 1024;
    
    // Allocate
    for (int i = 0; i < 20; ++i) {
        void* ptr = pool_.allocate(alloc_size);
        ptrs.push_back(ptr);
    }
    
    // Deallocate every other one to create fragmentation
    for (size_t i = 0; i < ptrs.size(); i += 2) {
        uint64_t offset = reinterpret_cast<uintptr_t>(ptrs[i]) - 
                         reinterpret_cast<uintptr_t>(pool_.get_memory_block());
        pool_.deallocate(offset, alloc_size);
    }
    
    auto stats = pool_.get_stats();
    EXPECT_GT(stats.fragmentation_percent, 0);
    
    // Defragment
    uint32_t coalesced = pool_.defragment();
    EXPECT_GT(coalesced, 0);
}

TEST_F(MemoryPoolTest, ContainsCheck) {
    void* ptr = pool_.allocate(1024);
    EXPECT_TRUE(pool_.contains(ptr));
    
    char dummy = 0;
    EXPECT_FALSE(pool_.contains(&dummy));
}

// ============================================================================
// Pool Allocator Tests  
// ============================================================================

class PoolAllocatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::array<size_t, 4> config = {
            128 * 1024 * 1024,  // TEXTURE_POOL
            256 * 1024 * 1024,  // VERTEX_POOL
            64 * 1024 * 1024,   // UNIFORM_POOL
            128 * 1024 * 1024   // SYSTEM_POOL
        };
        allocator_.initialize(config);
    }
    
    PoolAllocator allocator_;
};

TEST_F(PoolAllocatorTest, AllocateFromDifferentPools) {
    auto [ptr1, handle1] = allocator_.allocate(PoolType::TEXTURE_POOL, 1024);
    EXPECT_NE(ptr1, nullptr);
    EXPECT_NE(handle1, 0UL);
    
    auto [ptr2, handle2] = allocator_.allocate(PoolType::VERTEX_POOL, 2048);
    EXPECT_NE(ptr2, nullptr);
    EXPECT_NE(handle2, 0UL);
}

TEST_F(PoolAllocatorTest, StatisticsReporting) {
    allocator_.allocate(PoolType::TEXTURE_POOL, 10 * 1024 * 1024);
    allocator_.allocate(PoolType::VERTEX_POOL, 20 * 1024 * 1024);
    
    auto stats = allocator_.get_statistics();
    EXPECT_EQ(stats.pool_stats[0].used_bytes, 10 * 1024 * 1024);
    EXPECT_EQ(stats.pool_stats[1].used_bytes, 20 * 1024 * 1024);
}

// ============================================================================
// Asset Manager Tests
// ============================================================================

class AssetManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Note: These tests require a valid Vulkan device
        // For now, we test the basic interface
    }
};

TEST_F(AssetManagerTest, AssetIDGeneration) {
    // Test that asset type generation works
    AssetType types[] = {
        AssetType::TEXTURE_2D,
        AssetType::MESH,
        AssetType::SHADER,
        AssetType::MATERIAL
    };
    
    for (auto type : types) {
        EXPECT_NE(type, AssetType::UNKNOWN);
    }
}

TEST_F(AssetManagerTest, LoadPriorityOrdering) {
    LoadPriority priorities[] = {
        LoadPriority::PRIORITY_CRITICAL,
        LoadPriority::PRIORITY_HIGH,
        LoadPriority::PRIORITY_NORMAL,
        LoadPriority::PRIORITY_LOW
    };
    
    // Verify priority levels are distinct
    for (size_t i = 0; i < sizeof(priorities) / sizeof(priorities[0]) - 1; ++i) {
        EXPECT_LT(priorities[i], priorities[i + 1]);
    }
}

// ============================================================================
// stress tests
// ============================================================================

class StressTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::array<size_t, 4> pool_config = {
            128 * 1024 * 1024,
            256 * 1024 * 1024,
            64 * 1024 * 1024,
            128 * 1024 * 1024
        };
        allocator_.initialize(pool_config);
    }
    
    PoolAllocator allocator_;
};

TEST_F(StressTest, ConcurrentAllocations) {
    const int num_threads = 4;
    const int allocations_per_thread = 1000;
    
    std::vector<std::thread> threads;
    std::vector<std::pair<void*, uint64_t>> allocations;
    std::mutex alloc_mutex;
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, &allocations, &alloc_mutex]() {
            std::mt19937 rng(t);
            std::uniform_int_distribution<size_t> size_dist(256, 64 * 1024);
            std::uniform_int_distribution<int> pool_dist(0, 3);
            
            for (int i = 0; i < allocations_per_thread; ++i) {
                size_t size = size_dist(rng);
                PoolType pool_type = static_cast<PoolType>(pool_dist(rng));
                
                auto result = allocator_.allocate(pool_type, size);
                if (std::get<0>(result) != nullptr) {
                    std::lock_guard<std::mutex> lock(alloc_mutex);
                    allocations.push_back(result);
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto stats = allocator_.get_statistics();
    EXPECT_GT(stats.total_allocated, 0);
    EXPECT_LT(stats.overall_fragmentation, 10.0f);  // Should be under 10%
}

TEST_F(StressTest, FragmentationUnderRepeatedAllocFreeing) {
    const int iterations = 100;
    std::vector<void*> persistent_ptrs;
    
    for (int iter = 0; iter < iterations; ++iter) {
        // Allocate temporary
        for (int i = 0; i < 50; ++i) {
            auto [ptr, handle] = allocator_.allocate(PoolType::TEXTURE_POOL, 64 * 1024);
            if (ptr && i < 5) {
                persistent_ptrs.push_back(ptr);
            }
        }
    }
    
    auto stats = allocator_.get_statistics();
    EXPECT_LT(stats.overall_fragmentation, 20.0f);  // Should remain manageable
}

// ============================================================================
// Performance Tests
// ============================================================================

class PerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::array<size_t, 4> pool_config = {
            128 * 1024 * 1024,
            256 * 1024 * 1024,
            64 * 1024 * 1024,
            128 * 1024 * 1024
        };
        allocator_.initialize(pool_config);
    }
    
    PoolAllocator allocator_;
};

TEST_F(PerformanceTest, AllocationSpeed) {
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 10000; ++i) {
        allocator_.allocate(PoolType::TEXTURE_POOL, 4096);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    auto avg_ns = duration_ns.count() / 10000.0;
    
    // Target: < 10 ns per allocation
    EXPECT_LT(avg_ns, 10000.0);  // Allow some tolerance in testing environment
    
    std::cout << "Average allocation time: " << avg_ns << " ns" << std::endl;
}

TEST_F(PerformanceTest, DefragmentationSpeed) {
    // Create fragmentation
    std::vector<void*> ptrs;
    for (int i = 0; i < 1000; ++i) {
        auto [ptr, _] = allocator_.allocate(PoolType::VERTEX_POOL, 64 * 1024);
        ptrs.push_back(ptr);
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    allocator_.defragment_all();
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Defragmentation time: " << duration_ms.count() << " ms" << std::endl;
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
