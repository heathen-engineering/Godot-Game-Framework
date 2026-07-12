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

#include "register_types.h"
#include "SubsystemManagerBridge.h"
#include "editor/SubsystemsDock.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

static SubsystemManagerBridge *subsystem_manager_bridge_singleton = nullptr;

void initialize_foundation_gameframework_module(ModuleInitializationLevel p_level)
{
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE)
    {
        ClassDB::register_class<SubsystemManagerBridge>();

        subsystem_manager_bridge_singleton = memnew(SubsystemManagerBridge);
        Engine::get_singleton()->register_singleton("SubsystemManagerBridge", SubsystemManagerBridge::get_singleton());
    }
    else if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR)
    {
        // Editor-only — Godot never initializes MODULE_INITIALIZATION_LEVEL_EDITOR
        // in exported game builds, so the dock stays entirely out of shipped
        // games automatically, same pattern as every other gem's editor
        // tooling in this project.
        ClassDB::register_class<SubsystemsDock>();
    }
}

void uninitialize_foundation_gameframework_module(ModuleInitializationLevel p_level)
{
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE)
        return;

    if (subsystem_manager_bridge_singleton == nullptr)
        return;

    Engine::get_singleton()->unregister_singleton("SubsystemManagerBridge");
    memdelete(subsystem_manager_bridge_singleton);
    subsystem_manager_bridge_singleton = nullptr;
}

extern "C"
{
    GDE_EXPORT GDExtensionBool foundation_gameframework_init(
        GDExtensionInterfaceGetProcAddress p_get_proc_address,
        GDExtensionClassLibraryPtr p_library,
        GDExtensionInitialization *r_initialization)
    {
        GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

        init_obj.register_initializer(initialize_foundation_gameframework_module);
        init_obj.register_terminator(uninitialize_foundation_gameframework_module);
        init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

        return init_obj.init();
    }
}
