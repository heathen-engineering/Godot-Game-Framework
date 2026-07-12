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

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

using namespace godot;

/// <summary>
/// The single point of contact between Godot and Game Framework's
/// engine-agnostic core (gameframework::SubsystemManager) — see this
/// repo's README and the Game-Framework core's own Subsystem.h header
/// comment for why the architecture is shaped this way (cross-extension
/// GDExtension class inheritance is unsupported by Godot's ClassDB;
/// Subsystem/World/GameMode/GameState/PlayerState are all plain,
/// engine-agnostic C++, never GDCLASS types).
///
/// Deliberately thin: every method here just marshals a call/result to and
/// from gameframework::SubsystemManager::instance(), converting between its
/// plain-C++ types (std::string, std::vector, std::type_index) and Godot's
/// Variant-friendly ones (String, PackedStringArray, Dictionary, int) —
/// no logic of its own. Registered as an Engine singleton
/// ("SubsystemManagerBridge"), the same pattern already used throughout
/// this project's other gems (GameplayTagRegistry, Storyteller, ...).
/// </summary>
class SubsystemManagerBridge : public Object
{
    GDCLASS(SubsystemManagerBridge, Object);

private:
    static SubsystemManagerBridge *singleton_;

public:
    SubsystemManagerBridge();
    ~SubsystemManagerBridge();

    static SubsystemManagerBridge *get_singleton();

    /// Boots every registered Global subsystem (dependency-ordered), then
    /// creates the default World if none exists yet. Idempotent.
    void boot();
    /// Tears down every Global subsystem (and, transitively via
    /// WorldManagerSubsystem, every World) in reverse order.
    void shutdown();

    int get_global_subsystem_count() const;
    /// display_name() if the subsystem overrides it, else a generic
    /// "Subsystem #<index>" placeholder — see Subsystem::display_name()'s
    /// own doc comment for why there's no automatic fallback derived from
    /// the C++ type itself.
    String get_global_subsystem_name(int index) const;
    /// 0 = Global, 1 = World — always 0 here (this method only iterates
    /// Global-scope subsystems), exposed for forward-compatibility once a
    /// World-subsystem-browsing surface exists on this bridge too.
    int get_global_subsystem_scope(int index) const;
    bool get_global_subsystem_initialised(int index) const;
    PackedStringArray get_global_subsystem_health_issues(int index) const;
    /// (label, value) pairs as a flat Dictionary.
    Dictionary get_global_subsystem_debug_info(int index) const;

protected:
    static void _bind_methods();
};
