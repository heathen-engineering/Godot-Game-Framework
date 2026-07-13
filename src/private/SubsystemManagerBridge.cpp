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

#include "SubsystemManagerBridge.h"

#include "SubsystemTicker.h"

#include <gameframework/Subsystem.h>
#include <gameframework/SubsystemManager.h>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/array.hpp>

using namespace godot;

SubsystemManagerBridge *SubsystemManagerBridge::singleton_ = nullptr;

SubsystemManagerBridge::SubsystemManagerBridge()
{
    singleton_ = this;
}

SubsystemManagerBridge::~SubsystemManagerBridge()
{
    singleton_ = nullptr;
}

SubsystemManagerBridge *SubsystemManagerBridge::get_singleton()
{
    return singleton_;
}

void SubsystemManagerBridge::boot()
{
    gameframework::SubsystemManager::instance().boot();
    _ensure_ticker();
}

void SubsystemManagerBridge::_ensure_ticker()
{
    if (ticker_ != nullptr)
        return;

    // The scene tree doesn't exist yet if boot() was called this early —
    // e.g. from initialize_foundation_gameframework_module() itself, at
    // MODULE_INITIALIZATION_LEVEL_SCENE, well before the engine has a
    // MainLoop/SceneTree/root Window. Retry once deferred-call processing
    // actually starts (which only happens once the engine is far enough
    // along for a SceneTree to exist), rather than silently giving up —
    // tick_all() would otherwise never run in a real shipped game, only
    // when something happens to call boot() again later (e.g. a dock
    // refresh) by which point real gameplay frames may already have been
    // missed.
    SceneTree *tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
    if (tree == nullptr)
    {
        call_deferred("_ensure_ticker");
        return;
    }

    ticker_ = memnew(SubsystemTicker);
    // Deferred, not called directly — boot() (and therefore this) can run
    // from inside another autoload's own _ready(), while Godot is still
    // batch-adding every autoload as a child of root. A direct add_child()
    // here hits that "Parent node is busy setting up children" guard and
    // fails outright (confirmed: exactly this error, from exactly this
    // call site, when SubsystemBootAutoload.gd's own _ready() called
    // boot() -> _ensure_ticker() while root was still mid-setup for the
    // autoload after it in the list).
    tree->get_root()->call_deferred("add_child", ticker_);
}

void SubsystemManagerBridge::shutdown()
{
    gameframework::SubsystemManager::instance().shutdown();
}

int SubsystemManagerBridge::get_global_subsystem_count() const
{
    return gameframework::SubsystemManager::instance().global_subsystem_count();
}

String SubsystemManagerBridge::get_global_subsystem_name(int index) const
{
    gameframework::Subsystem *sys = gameframework::SubsystemManager::instance().get_global_subsystem_at(index);
    if (sys == nullptr)
        return String();

    std::string name = sys->display_name();
    if (!name.empty())
        return String(name.c_str());

    return String("Subsystem #{0}").format(Array::make(index));
}

int SubsystemManagerBridge::get_global_subsystem_scope(int index) const
{
    gameframework::Subsystem *sys = gameframework::SubsystemManager::instance().get_global_subsystem_at(index);
    return sys == nullptr ? -1 : int(sys->scope());
}

bool SubsystemManagerBridge::get_global_subsystem_initialised(int index) const
{
    gameframework::Subsystem *sys = gameframework::SubsystemManager::instance().get_global_subsystem_at(index);
    return sys != nullptr && sys->is_initialised();
}

PackedStringArray SubsystemManagerBridge::get_global_subsystem_health_issues(int index) const
{
    PackedStringArray result;
    gameframework::Subsystem *sys = gameframework::SubsystemManager::instance().get_global_subsystem_at(index);
    if (sys == nullptr)
        return result;

    for (const std::string &issue : sys->health_issues())
        result.push_back(String(issue.c_str()));
    return result;
}

Dictionary SubsystemManagerBridge::get_global_subsystem_debug_info(int index) const
{
    Dictionary result;
    gameframework::Subsystem *sys = gameframework::SubsystemManager::instance().get_global_subsystem_at(index);
    if (sys == nullptr)
        return result;

    for (const std::pair<std::string, std::string> &kv : sys->debug_info())
        result[String(kv.first.c_str())] = String(kv.second.c_str());
    return result;
}

int SubsystemManagerBridge::get_global_subsystem_start_mode(int index) const
{
    gameframework::Subsystem *sys = gameframework::SubsystemManager::instance().get_global_subsystem_at(index);
    return sys == nullptr ? int(gameframework::Subsystem::StartMode::Automatic) : int(sys->start_mode());
}

bool SubsystemManagerBridge::initialize_global_subsystem(int index)
{
    return gameframework::SubsystemManager::instance().initialize_global_subsystem_at(index);
}

void SubsystemManagerBridge::register_settings_panel(const String &subsystem_name, const Callable &provider)
{
    settings_panel_providers_[subsystem_name] = provider;
}

bool SubsystemManagerBridge::has_settings_panel(const String &subsystem_name) const
{
    return settings_panel_providers_.has(subsystem_name);
}

Control *SubsystemManagerBridge::get_settings_panel(const String &subsystem_name) const
{
    if (!settings_panel_providers_.has(subsystem_name))
        return nullptr;

    Callable provider = settings_panel_providers_[subsystem_name];
    if (!provider.is_valid())
        return nullptr;

    return Object::cast_to<Control>(provider.call());
}

void SubsystemManagerBridge::register_start_mode_setter(const String &subsystem_name, const Callable &setter)
{
    start_mode_setters_[subsystem_name] = setter;
}

bool SubsystemManagerBridge::has_start_mode_setter(const String &subsystem_name) const
{
    return start_mode_setters_.has(subsystem_name);
}

void SubsystemManagerBridge::set_global_subsystem_start_mode(const String &subsystem_name, int mode) const
{
    if (!start_mode_setters_.has(subsystem_name))
        return;

    Callable setter = start_mode_setters_[subsystem_name];
    if (setter.is_valid())
        setter.call(mode);
}

void SubsystemManagerBridge::register_build(const String &subsystem_name, const Callable &status_provider, const Callable &build_action)
{
    build_status_providers_[subsystem_name] = status_provider;
    build_actions_[subsystem_name] = build_action;
}

bool SubsystemManagerBridge::has_build(const String &subsystem_name) const
{
    return build_status_providers_.has(subsystem_name);
}

int SubsystemManagerBridge::get_build_status(const String &subsystem_name) const
{
    if (!build_status_providers_.has(subsystem_name))
        return 0;

    Callable provider = build_status_providers_[subsystem_name];
    if (!provider.is_valid())
        return 0;

    return int(provider.call());
}

void SubsystemManagerBridge::trigger_build(const String &subsystem_name) const
{
    if (!build_actions_.has(subsystem_name))
        return;

    Callable action = build_actions_[subsystem_name];
    if (action.is_valid())
        action.call();
}

void SubsystemManagerBridge::register_tool_window_opener(const String &subsystem_name, const Callable &opener)
{
    tool_window_openers_[subsystem_name] = opener;
}

bool SubsystemManagerBridge::has_tool_window(const String &subsystem_name) const
{
    return tool_window_openers_.has(subsystem_name);
}

void SubsystemManagerBridge::open_tool_window(const String &subsystem_name) const
{
    if (!tool_window_openers_.has(subsystem_name))
        return;

    Callable opener = tool_window_openers_[subsystem_name];
    if (opener.is_valid())
        opener.call();
}

void SubsystemManagerBridge::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("boot"), &SubsystemManagerBridge::boot);
    ClassDB::bind_method(D_METHOD("shutdown"), &SubsystemManagerBridge::shutdown);
    ClassDB::bind_method(D_METHOD("get_global_subsystem_count"), &SubsystemManagerBridge::get_global_subsystem_count);
    ClassDB::bind_method(D_METHOD("get_global_subsystem_name", "index"), &SubsystemManagerBridge::get_global_subsystem_name);
    ClassDB::bind_method(D_METHOD("get_global_subsystem_scope", "index"), &SubsystemManagerBridge::get_global_subsystem_scope);
    ClassDB::bind_method(D_METHOD("get_global_subsystem_initialised", "index"), &SubsystemManagerBridge::get_global_subsystem_initialised);
    ClassDB::bind_method(D_METHOD("get_global_subsystem_health_issues", "index"), &SubsystemManagerBridge::get_global_subsystem_health_issues);
    ClassDB::bind_method(D_METHOD("get_global_subsystem_debug_info", "index"), &SubsystemManagerBridge::get_global_subsystem_debug_info);
    ClassDB::bind_method(D_METHOD("get_global_subsystem_start_mode", "index"), &SubsystemManagerBridge::get_global_subsystem_start_mode);
    ClassDB::bind_method(D_METHOD("initialize_global_subsystem", "index"), &SubsystemManagerBridge::initialize_global_subsystem);
    ClassDB::bind_method(D_METHOD("register_start_mode_setter", "subsystem_name", "setter"), &SubsystemManagerBridge::register_start_mode_setter);
    ClassDB::bind_method(D_METHOD("has_start_mode_setter", "subsystem_name"), &SubsystemManagerBridge::has_start_mode_setter);
    ClassDB::bind_method(D_METHOD("set_global_subsystem_start_mode", "subsystem_name", "mode"), &SubsystemManagerBridge::set_global_subsystem_start_mode);
    ClassDB::bind_method(D_METHOD("register_build", "subsystem_name", "status_provider", "build_action"), &SubsystemManagerBridge::register_build);
    ClassDB::bind_method(D_METHOD("has_build", "subsystem_name"), &SubsystemManagerBridge::has_build);
    ClassDB::bind_method(D_METHOD("get_build_status", "subsystem_name"), &SubsystemManagerBridge::get_build_status);
    ClassDB::bind_method(D_METHOD("trigger_build", "subsystem_name"), &SubsystemManagerBridge::trigger_build);
    ClassDB::bind_method(D_METHOD("register_settings_panel", "subsystem_name", "provider"), &SubsystemManagerBridge::register_settings_panel);
    ClassDB::bind_method(D_METHOD("has_settings_panel", "subsystem_name"), &SubsystemManagerBridge::has_settings_panel);
    ClassDB::bind_method(D_METHOD("get_settings_panel", "subsystem_name"), &SubsystemManagerBridge::get_settings_panel);
    ClassDB::bind_method(D_METHOD("register_tool_window_opener", "subsystem_name", "opener"), &SubsystemManagerBridge::register_tool_window_opener);
    ClassDB::bind_method(D_METHOD("has_tool_window", "subsystem_name"), &SubsystemManagerBridge::has_tool_window);
    ClassDB::bind_method(D_METHOD("open_tool_window", "subsystem_name"), &SubsystemManagerBridge::open_tool_window);
    // Bound (despite the leading underscore, Godot's own "framework calls
    // this" convention) because call_deferred("_ensure_ticker") resolves
    // through the same ClassDB method-bind lookup as any other deferred
    // call — an unbound method can't be reached that way.
    ClassDB::bind_method(D_METHOD("_ensure_ticker"), &SubsystemManagerBridge::_ensure_ticker);
}
