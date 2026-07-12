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

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/tree.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

/// <summary>
/// Minimal Subsystems dock — lists every registered Global subsystem
/// (SubsystemManagerBridge::get_global_subsystem_*), shows initialised
/// state and any health issues. Direct C++ port of the established dock
/// pattern used throughout this project (GameplayTagsDock, OghamGraphView):
/// Control, lazy _ensure_built() called from _ready() — safe here since
/// nothing calls a method on this dock right after construction, it's
/// entirely self-driving via its own _ready(), matching every other dock's
/// documented reasoning for why that ordering is safe.
///
/// Settings-page navigation (per-subsystem "open its own settings" links)
/// is deliberately deferred — see the plan doc — until a real settings page
/// exists somewhere to test that against; this first pass is list +
/// enable-state + health only.
/// </summary>
class SubsystemsDock : public Control
{
    GDCLASS(SubsystemsDock, Control);

private:
    Tree *tree_ = nullptr;

    void _ensure_built();
    void _rebuild();

public:
    SubsystemsDock() = default;

    void _ready();
    void refresh();

protected:
    static void _bind_methods();
};
