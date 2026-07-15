# Game Framework Foundation for Godot

Godot GDExtension wrap for Heathen Engineering's Game Framework, an Unreal-Subsystem-inspired, engine-agnostic runtime for lifecycle-managed, dependency-ordered singletons.

The engine-agnostic logic (Subsystem, SubsystemManager, World, GameMode, GameState, PlayerState) lives in a separate, plain C++ core with zero Godot dependency. This addon is the thin bridge that makes it usable from Godot.

## What it does

Gives your project a real Subsystem architecture: lifecycle-managed, dependency-ordered singletons, either Global (one per process) or World-scoped, with explicit boot ordering instead of ad hoc AutoLoad sequencing.

- Real dependency ordering: a subsystem can declare which other subsystem types must initialize first, resolved by a deterministic topological sort.
- Start Mode per subsystem: Automatic, On Demand, or Disabled, so a gem can ship its subsystem dormant without removing it from the build.
- A Subsystems dock listing every registered Global subsystem: name, scope, running state, and health issues.

Every other Heathen Godot gem that defines a real Subsystem (GameplayTags, Lexicon, Ogham, and more) is built on top of this addon, and depends on it automatically through Extension Resolver for Godot.

## Requirements

- Godot 4.6 or compatible

## Links

- GitHub: [https://github.com/heathen-engineering/Godot-Game-Framework](https://github.com/heathen-engineering/Godot-Game-Framework)
- Support and Discord: [https://discord.gg/xmtRNkW7hW](https://discord.gg/xmtRNkW7hW)
- License: Apache 2.0
