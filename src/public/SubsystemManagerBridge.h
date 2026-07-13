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
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

using namespace godot;

class SubsystemTicker;

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
    SubsystemTicker *ticker_ = nullptr;
    /// subsystem name -> zero-argument Callable returning a fresh Control.
    /// A Dictionary, not a std::unordered_map, since Variant already holds
    /// Callable natively — no extra plumbing needed to bridge C++ and
    /// GDScript-provided callables through this.
    Dictionary settings_panel_providers_;
    /// subsystem name -> zero-argument Callable that opens/focuses that
    /// subsystem's tool window (an EditorDock, e.g. Ogham's node-graph
    /// editor). Same rationale as settings_panel_providers_ — the bridge
    /// never needs to know what kind of window it is, only that calling the
    /// Callable makes it visible.
    Dictionary tool_window_openers_;
    /// subsystem name -> one-argument Callable(int mode) that persists a
    /// new StartMode (0=Disabled, 1=OnDemand, 2=Automatic) somewhere that
    /// subsystem's own C++ start_mode() override reads back from — e.g.
    /// SteamworksSubsystem reading a ProjectSettings entry. Presence of an
    /// entry here (not a separate bool flag) is what the Subsystems tab
    /// uses to decide "show an editable dropdown for this row" vs. "show a
    /// plain read-only label" — same has_X()-presence idiom as
    /// settings_panel_providers_/tool_window_openers_ above. Subsystems
    /// that never override start_mode() away from the Automatic default
    /// (Ogham/Lexicon/GameplayTags today) simply never register one.
    Dictionary start_mode_setters_;
    /// subsystem name -> zero-argument Callable returning an int build
    /// status (0=Good/green, 1=NeedsAttention/yellow, 2=Error/red) — the
    /// optional "Build" concept (ported from Unity-Game-Framework's
    /// ISettingsGenerator/BuildStatus), e.g. Steamworks' generated
    /// SteamGame.gd going stale against the current settings. Re-invoked
    /// on every get_build_status() call (not cached) so the colour is
    /// always current, not just whatever it was when the row was built.
    Dictionary build_status_providers_;
    /// subsystem name -> zero-argument Callable invoked when the user
    /// clicks that subsystem's build-status button.
    Dictionary build_actions_;

    /// Idempotent. Creates the SubsystemTicker Node under the scene tree's
    /// root the first time boot() runs — deferred via call_deferred if the
    /// scene tree doesn't exist yet (see .cpp for why that's a real case,
    /// not a defensive-only guard).
    void _ensure_ticker();

public:
    SubsystemManagerBridge();
    ~SubsystemManagerBridge();

    static SubsystemManagerBridge *get_singleton();

    /// Boots every registered Global subsystem (dependency-ordered), then
    /// creates the default World if none exists yet. Idempotent. Always
    /// synchronous — callers get an accurate global_subsystem_count() etc.
    /// immediately after this returns.
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
    /// 0=Disabled, 1=OnDemand, 2=Automatic — the subsystem's CURRENT
    /// effective start_mode(), whatever that C++ override returns right
    /// now (default Automatic, or a persisted value it already read back).
    int get_global_subsystem_start_mode(int index) const;
    /// Manually initializes a StartMode::OnDemand subsystem boot() left
    /// uninitialized — the dock-facing counterpart to a dev calling e.g.
    /// SteamApi::InitialiseClient() themselves. False if index is invalid
    /// or it's already initialised.
    bool initialize_global_subsystem(int index);

    /// Hands this subsystem's settings-page UI to the unified Subsystems
    /// Project Settings tab (owned by this addon's own EditorPlugin — see
    /// its README, "Settings-panel handoff"). provider is a zero-argument
    /// Callable that returns a fresh Control when invoked; the bridge never
    /// inspects what's inside, so each gem's settings content stays built
    /// from that gem's own native types, no cross-gem C++ coupling. Called
    /// from a gem's own EditorPlugin, after its gate (if any) has unlocked.
    void register_settings_panel(const String &subsystem_name, const Callable &provider);
    bool has_settings_panel(const String &subsystem_name) const;
    /// Invokes the registered provider and returns its result. Null if
    /// nothing is registered for subsystem_name, or if the provider didn't
    /// return a Control.
    Control *get_settings_panel(const String &subsystem_name) const;

    /// Registers a subsystem's "open tool window" callable — e.g. Ogham's
    /// node-graph EditorDock. This is deliberately "a" place to open the
    /// window (the Subsystems settings tab wires a button to it), not the
    /// only one — the dock itself is still reachable through Godot's normal
    /// dock UI regardless of whether this is ever registered/called.
    void register_tool_window_opener(const String &subsystem_name, const Callable &opener);
    bool has_tool_window(const String &subsystem_name) const;
    /// No-op if nothing is registered for subsystem_name.
    void open_tool_window(const String &subsystem_name) const;

    /// Registers a subsystem's start-mode setter — see start_mode_setters_
    /// doc comment above. Called from a gem's own EditorPlugin, same
    /// pattern/timing as register_settings_panel().
    void register_start_mode_setter(const String &subsystem_name, const Callable &setter);
    bool has_start_mode_setter(const String &subsystem_name) const;
    /// No-op if nothing is registered for subsystem_name (i.e. this
    /// subsystem's start mode isn't user-configurable).
    void set_global_subsystem_start_mode(const String &subsystem_name, int mode) const;

    /// Registers a subsystem's optional "Build" concept — see
    /// build_status_providers_/build_actions_ doc comments above.
    void register_build(const String &subsystem_name, const Callable &status_provider, const Callable &build_action);
    bool has_build(const String &subsystem_name) const;
    /// Re-invokes the registered status_provider every call. 0 (Good) if
    /// nothing is registered for subsystem_name — has_build() is what the
    /// tab checks before deciding whether to show the button at all.
    int get_build_status(const String &subsystem_name) const;
    /// No-op if nothing is registered for subsystem_name.
    void trigger_build(const String &subsystem_name) const;

protected:
    static void _bind_methods();
};
