/*
 * Copyright (c) 2026 Heathen Engineering Limited
 * Irish Registered Company #556277
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

/// <summary>
/// The one thing in the whole framework that has to be a real Node — every
/// Subsystem is plain, engine-agnostic C++ with no per-frame hook of its
/// own, and gameframework::SubsystemManager::tick_all(delta) needs SOMEONE
/// in the scene tree calling it once a frame. This is that someone: created
/// and added under the main loop's root by SubsystemManagerBridge::boot()
/// (see that method's own comment), never constructed directly by anything
/// else.
/// </summary>
class SubsystemTicker : public Node
{
    GDCLASS(SubsystemTicker, Node);

public:
    void _process(double delta) override;

protected:
    static void _bind_methods();
};
