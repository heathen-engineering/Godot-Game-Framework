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

#include <gameframework/Subsystem.h>
#include <gameframework/SubsystemManager.h>

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
}
