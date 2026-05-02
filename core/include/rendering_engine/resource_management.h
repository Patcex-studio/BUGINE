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
#pragma once

#include "resource_manager/resource_manager.h"
#include "resource_manager/asset_manager.h"
#include "resource_manager/memory_pool.h"
#include "resource_manager/gpu_allocator.h"
#include "resource_manager/asset_types.h"

/**
 * @brief Convenience header for exporting resource management system
 * 
 * This header re-exports all resource management components
 * through the rendering_engine namespace for convenience.
 */

namespace rendering_engine {
    
    // Import resource manager types and classes
    using resource_manager::ResourceManager;
    using resource_manager::AssetManager;
    using resource_manager::MemoryPool;
    using resource_manager::PoolAllocator;
    using resource_manager::GPUAllocator;
    using resource_manager::GPUMemoryBlock;
    
    // Asset types
    using resource_manager::AssetID;
    using resource_manager::AssetType;
    using resource_manager::TextureFormat;
    using resource_manager::LoadPriority;
    using resource_manager::AssetRequest;
    using resource_manager::AssetMetadata;
    using resource_manager::Texture;
    using resource_manager::Shader;
    using resource_manager::Material;
    using resource_manager::StreamingContext;
    
    // Memory pool types
    using resource_manager::PoolType;
    using resource_manager::AllocationInfo;
    using resource_manager::FreeBlock;
    
    // GPU allocator types
    using resource_manager::GPUBlockSize;
    using resource_manager::GPUSubAllocation;
    
}  // namespace rendering_engine
