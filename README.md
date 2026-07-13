# Godot-Game-Framework

Godot GDExtension wrap for Heathen Engineering's Game Framework — an Unreal-Subsystem-inspired,
engine-agnostic runtime for lifecycle-managed, dependency-ordered singletons. The engine-agnostic
logic (`Subsystem`, `SubsystemManager`, `World`, `GameMode`, `GameState`, `PlayerState`) lives in
the separate [`Game-Framework`](https://github.com/heathen-engineering/Game-Framework) repo, built
as plain C++ with zero Godot dependency. This repo is the thin bridge that makes it usable from
Godot: `SubsystemManagerBridge` (an `Engine` singleton) and the Subsystems dock.

Every other Heathen Godot gem (GameplayTags, Lexicon, Ogham, ...) that defines real `Subsystem`
subclasses is built on top of this addon. If you're building one of those, or your own tool in the
same spirit, read on — the linking model below is not optional boilerplate, it's the one thing that
has to be done correctly for cross-gem `Subsystem` visibility to work at all.

## Why a separate core repo, and why C++ types instead of Godot classes

Two things Godot's `ClassDB` genuinely cannot do ruled out the obvious designs:

1. **A GDExtension class cannot inherit from a `ClassDB`-registered class defined in a *different*
   GDExtension.** Confirmed empirically (`ERROR: Unimplemented yet` at
   `_register_extension_class_internal`) and by a Godot maintainer ("not officially supported",
   tracked as unresolved `godot-proposals#13997`). So `Subsystem` cannot be a `GDCLASS` that
   GameplayTags/Lexicon/Ogham each subclass across extension boundaries.
2. **Godot's per-extension instance-binding bookkeeping doesn't resolve a typed pointer for an
   object registered by a *different* extension**, even when the two `.so`s are correctly linked
   and `Object::cast_to<T>()` finds the class fine at the `ClassDB` level — `dynamic_cast` on the
   resulting instance binding still fails. Ruled out a design where `SubsystemManager` alone is the
   `GDCLASS` bridge and consumers call its ordinary methods across the boundary.

What's left, and what this repo actually does: `Subsystem`/`SubsystemManager` are **plain C++,
zero Godot dependency**, shipped as an ordinary shared library — exactly the way any C++ plugin-core
system works, engine-agnostic by construction (Godot today, O3DE or a bare command-line tool later).
`SubsystemManagerBridge` is the *only* `GDCLASS` in the whole picture; it's a thin marshaling layer,
not where the real logic lives.

## The linking model — read this before adding a new gem's Subsystem

This is the part that silently breaks if you get it wrong, because the failure mode is *not* a
crash or a compile error — it's each `.so` quietly getting its own private, disconnected
`SubsystemManager::instance()`, so your subsystem never shows up in the dock and cross-gem
`depends_on()` ordering does nothing.

**Every GDExtension that registers a `Subsystem` must dynamically link the same physical shared
library file** — `Game-Framework`'s `gameframework::shared` target (the `gameframework_shared`
CMake target), never `gameframework::gameframework` (the static target). Static linking gives each
`.so` its own copy of the Meyer's singleton's storage; two statically-linked `.so`s in the same
Godot process do **not** share state just because they're the same process — ELF/PE/Mach-O linking
duplicates static data per shared object, not per process. This was verified with a real two-`.so`
test (a throwaway third extension, `Probe`, registering a `Subsystem` with nothing but a runtime
dependency on this repo's shipped `libgameframework.*` — its registration showed up correctly via
`SubsystemManagerBridge`, proving cross-extension sharing genuinely works when done this way).

Concretely, for a new gem (call it `FoundationExample`):

1. **Vendor `Game-Framework` as a submodule** (`extern/Game-Framework`), the same way this repo
   does — you need its headers to compile against `Subsystem`/`SubsystemManager`, and you build
   `gameframework::shared` from it locally to satisfy the linker/import-stub at compile time.
2. **Link `gameframework::shared`, not `gameframework::gameframework`.** Give the
   `gameframework_shared` CMake target the *exact same* `OUTPUT_NAME` convention this repo uses —
   `libgameframework.<platform>.<config>.<arch>` — so the `DT_NEEDED`/import-library name your
   `.so` embeds matches byte-for-byte what's actually shipped. A mismatched name (e.g. leaving the
   CMake default `libgameframework_shared.so`) means the loader treats it as a completely different
   library and nothing shares.
3. **Do not package that local build byproduct.** Your CI/packaging step should copy only your own
   `FoundationExample.*` into the shipped addon — never your own copy of `libgameframework.*`. The
   *only* place that ships (and should ever ship) `libgameframework.*` is this repo's own
   `addons/FoundationGameFramework/bin/`. This is deliberate, not an oversight: it's what makes
   Godot-Game-Framework a real, singular runtime dependency other gems have, which is exactly what
   the dependency-manifest system below is for.
4. **Set a relative RPATH**, not the CMake default. Without `BUILD_WITH_INSTALL_RPATH TRUE` +
   `INSTALL_RPATH "$ORIGIN"` (macOS: `"@loader_path"`; Windows needs nothing — DLL search order
   already checks the loading DLL's own directory), CMake bakes an *absolute* build-tree path into
   the binary. It works by accident on the machine that built it and silently breaks for every CI
   artifact, every other developer, and every end user. (Caught exactly this way in this repo —
   `readelf -d` showed an absolute `/home/.../build` RUNPATH before this was fixed.)
5. **Declare the runtime dependency explicitly** in your own `.gdextension`'s `[dependencies]`
   section, pointing at this repo's shipped path:
   ```ini
   [dependencies]
   linux.debug.x86_64 = {
     "res://addons/FoundationGameFramework/bin/libgameframework.linux.debug.x86_64.so": ""
   }
   ```
   Repeat per platform/config/arch. This makes correctness independent of extension load order —
   don't rely on your addon folder happening to sort alphabetically after
   `FoundationGameFramework`.
6. **Register your subsystem explicitly at module init**, not via any discovery mechanism:
   ```cpp
   void initialize_foundation_example_module(ModuleInitializationLevel p_level) {
       if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
       gameframework::SubsystemManager::instance().register_subsystem<ExampleSubsystem>();
   }
   ```

## No reflection-based discovery — and why that's fine

Unity's original `com.heathen.gameframework` finds every `Subsystem` subclass by reflecting over
*all loaded assemblies* at boot (`SubsystemDiscovery.cs`). C++ has no equivalent — you can't scan
"every loaded `.so`" for subclasses across a DLL boundary. That's not a gap here: each gem already
knows exactly which subsystem(s) it provides, so it registers them explicitly at its own module
init (`register_subsystem<T>()`), the same way `ClassDB::register_class<T>()` already works
per-extension. No scan needed, nothing to cache, nothing that gets stale.

## Dependency ordering and priority

`Subsystem::depends_on()` returns the `std::type_index`s of other subsystem types that must
initialize first — real, type-safe, no hand-typed strings, resolved by `DependencyOrder`'s
deterministic topological sort (`SubsystemManager::boot()` sorts by this before initializing
anything). This works identically whether the dependency is declared by a subsystem in the *same*
`.so* or a different one — dependency ordering doesn't care where a `Subsystem` type physically
lives, only that its concrete instance is registered with the same shared `SubsystemManager`.

`Scope` (`Global` vs `World`) is a second, coarser ordering tier: all `Global` subsystems boot
before any `World` is created, so a `World`-scoped subsystem can freely assume every `Global`
subsystem it might want is already up, without needing an explicit `depends_on()` for it (this
mirrors Unity's own `GameplayTagsSubsystem`/`LexiconSubsystem` (`Global`) vs `StorytellerSubsystem`
(`World`) split — `StorytellerSubsystem` doesn't declare `DependsOn` either, it just relies on scope
tiering).

`StartMode` (`Automatic` / `OnDemand` / `Disabled`) and the resulting `should_create()` let a gem
ship its subsystem "dormant" — present in the build, not booted — the same affordance Unity's
version has for e.g. shipping Steamworks assemblies into a non-Steam build without ever
initializing Steam.

## What's different from Unity here — and the QoL problem that creates

In Unity, `com.heathen.gameframework` and every gem built on it are Unity Packages, and **Unity
Package Manager already resolves package dependencies** (a `manifest.json`/`package.json`
dependency graph) — install `com.heathen.gameplaytagsfoundation` and UPM pulls in
`com.heathen.gameframework` for you. Reflection then finds every `Subsystem` subclass across
whatever got installed, with no extra registration step.

**Godot has no equivalent.** There's no dependency field in `plugin.cfg`, no dependency graph in
the Asset Library, and multiple long-open, unresolved `godot-proposals` asking for one. If a user
installs `FoundationGameplayTags` without `FoundationGameFramework` present, the addon's `.so` would
otherwise just fail to load — `Can't open shared object file` for `libgameframework.*`, a confusing
error pointing at a file the user has never heard of, from an addon they never installed on purpose.

This gap is closed by a separate, standalone tool — **[Extension Resolver for
Godot](https://github.com/heathen-engineering/Godot-Extension-Resolver)** — not by this addon
itself. Every `Subsystem`-registering gem, this one included, ships an `extension.manifest.json`
at its addon root declaring its dependencies and where to fetch them from; Extension Resolver
reads those, checks real version constraints (not just presence), and — **always via a confirm
dialog, never silent, never automatic** — offers to fetch anything missing or out of range. This
addon's own gate (`gate/extension_resolver_gate.gd`, present in every gem that depends on this
one) is the thin per-addon integration point: "is Extension Resolver installed? If not, fetch
just that; then hand off dependency resolution to it." See that tool's own
`docs/manifest-schema.md` for the manifest format, and its README for why the resolution logic
lives in one shared, generic tool instead of duplicated per gem the way earlier versions of this
addon (`HeathenDependencyManifest`/`HeathenDependencyFetcher`, since removed) did it.

## Contents

- `src/public/SubsystemManagerBridge.h` / `src/private/SubsystemManagerBridge.cpp` — the one
  `GDCLASS`, an `Engine` singleton forwarding to `gameframework::SubsystemManager::instance()`.
- `src/public/editor/SubsystemsDock.h` / `src/private/editor/SubsystemsDock.cpp` — bottom-panel dock
  listing every registered Global subsystem: name, scope, running/stopped, health issues.
- `editor/FoundationGameFrameworkEditorPlugin.gd` — the `plugin.cfg`-mandated GDScript entry point
  (Godot requires this one file to be GDScript; everything it does is instantiate C++ `GDCLASS`es).
- `editor/dependency_manifest.gd` / `editor/dependency_fetcher.gd` — the manifest scan and
  fetch-on-confirm mechanism described above.
- `extern/Game-Framework` — the engine-agnostic core, as a git submodule.
- `godot-cpp` — Godot's C++ bindings, as a git submodule.
