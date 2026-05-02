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

#include <cstdint>
#include <chrono>

class TickBudget {
    uint64_t budget_ns;          // сколько времени выделено на всех ИИ (например, 1.5ms → 1'500'000 ns)
    uint64_t start_ns;          // время начала кадра
public:
    TickBudget(float ms) : budget_ns(static_cast<uint64_t>(ms * 1e6f)) { reset(); }
    void reset() { start_ns = now_ns(); }
    bool allow() const noexcept { return (now_ns() - start_ns) < budget_ns; }
private:
    static uint64_t now_ns() noexcept {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    }
};