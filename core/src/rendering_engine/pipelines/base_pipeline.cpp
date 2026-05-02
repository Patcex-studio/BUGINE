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
#include "rendering_engine/pipelines/base_pipeline.h"

namespace rendering_engine {

// Base pipeline virtual destructor implementation
BasePipeline::~BasePipeline() = default;

uint32_t BasePipeline::get_light_count() const {
    return static_cast<uint32_t>(current_lights_.size());
}

uint32_t BasePipeline::get_render_queue_size() const {
    return static_cast<uint32_t>(render_queue_.size());
}

} // namespace rendering_engine
