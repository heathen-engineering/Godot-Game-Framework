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
#include "SubsystemTicker.h"
#include "editor/SubsystemsSettingsTab.h"

#include <gameframework/SubsystemManager.h>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

static SubsystemManagerBridge *subsystem_manager_bridge_singleton = nullptr;

void initialize_foundation_gameframework_module(ModuleInitializationLevel p_level)
{
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE)
    {
        ClassDB::register_class<SubsystemManagerBridge>();
        ClassDB::register_class<SubsystemTicker>();

        subsystem_manager_bridge_singleton = memnew(SubsystemManagerBridge);
        Engine::get_singleton()->register_singleton("SubsystemManagerBridge", SubsystemManagerBridge::get_singleton());

        // Deliberately NOT auto-booting from module init. Tried it two ways
        // (boot() straight from here; boot() deferred until the scene tree
        // exists) and both crashed — the first deterministically (iterating
        // gameframework::SubsystemManager's list while another still-loading
        // extension was concurrently registering into it — "scene tree
        // exists" turned out not to reliably mean "every extension's
        // SCENE-level init has finished"), the second non-deterministically
        // (racing against explicit boot() callers — a dock, a test script —
        // and crashing during scene teardown when the deferred one-shot
        // fired after quit()). GDExtension module-init has no reliable hook
        // for "the whole engine has actually finished starting," and
        // improvising one with call_deferred/signal chains kept finding new
        // ways to race. Real game startup instead needs a project autoload
        // (see editor/SubsystemBootAutoload.gd, registered into
        // project.godot by editor/SubsystemAutoloadSetup.gd — same
        // precedent as FoundationSteamworks' own SteamAutoloadSetup.gd) —
        // Godot's autoload mechanism has no such ambiguity, it's guaranteed
        // to run once the scene tree is genuinely ready, no invented
        // plumbing needed.
    }
    else if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR)
    {
        // Editor-only — Godot never initializes MODULE_INITIALIZATION_LEVEL_EDITOR
        // in exported game builds, so the tab stays entirely out of shipped
        // games automatically, same pattern as every other gem's editor
        // tooling in this project.
        ClassDB::register_class<SubsystemsSettingsTab>();
    }
}

void uninitialize_foundation_gameframework_module(ModuleInitializationLevel p_level)
{
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE)
        return;

    // release_all(), not shutdown() — must actually DESTROY every registered
    // Subsystem instance here, not just deinitialize it. Every instance's concrete
    // type (and vtable) lives inside whichever GDExtension .so registered it —
    // GameplayTags/Lexicon/Ogham/etc, not this one. Godot calls every extension's
    // uninitialize callback while all extensions are still loaded, THEN dlcloses
    // them; raw process exit() (when SubsystemManager's own destructor would
    // otherwise destroy its leftover entries_) happens well after that unloading.
    // Destroying a Subsystem — a virtual call, ~Subsystem() — through a vtable
    // whose owning .so has already been dlclose'd is a segfault, not just
    // theoretical UB: confirmed twice with gdb, first in do_deinitialize() (fixed
    // by calling shutdown() here), then in ~Entry()'s implicit unique_ptr<Subsystem>
    // destruction (fixed by this release_all() call actually clearing entries_ here
    // instead of leaving them for ~SubsystemManager() to find already-unloaded).
    gameframework::SubsystemManager::instance().release_all();

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
