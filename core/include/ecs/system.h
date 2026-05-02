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

#include "entity_manager.h"

#include <tuple>
#include <vector>

namespace ecs {

template<typename... ComponentTypes>
class QuerySystem : public SystemBase {
public:
    using QueryTuple = std::tuple<ComponentTypes...>;

    void update(float delta_time) override {
        current_delta_time_ = delta_time;
        if (!entity_manager_) {
            return;
        }

        entity_manager_->template query_components<ComponentTypes...>([this](EntityID entity, ComponentTypes&... components) {
            process_entity(entity, components..., current_delta_time_);
        });
    }

protected:
    virtual void process_entity(EntityID entity, ComponentTypes&... components, float delta_time) = 0;
};

} // namespace ecs
