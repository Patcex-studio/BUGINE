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

#include <any>
#include <functional>
#include <mutex>
#include <vector>

namespace ui {

enum class CommandType : uint32_t {
    SPAWN_ENTITY,
    DESTROY_ENTITY,
    APPLY_FORCE,
    SET_PARAMETER,
    LOAD_MISSION,
    // ... extend as needed
};

struct Command {
    CommandType type;
    std::any payload;   // specific data depends on type
};

class CommandQueue {
public:
    static CommandQueue& Instance() {
        static CommandQueue instance;
        return instance;
    }

    void Push(Command&& cmd);
    void FlushTo(std::vector<Command>& outBuffer);

private:
    CommandQueue() = default;
    std::vector<Command> buffer_;
    std::mutex mutex_;   // protection for multi-threaded access
};

} // namespace ui