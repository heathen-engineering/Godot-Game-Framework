@tool
extends EditorPlugin

## Activates the Game Framework editor tooling: the unified "Subsystems"
## Project Settings tab. This is the one file in the whole addon that HAS to
## be GDScript — Godot's plugin.cfg "script=" field only accepts a .gd path,
## not a C++ class (a hard platform constraint, not a style choice).
## Everything it does is a single .new() + container registration; every
## other class it references (SubsystemsSettingsTab) is a C++ GDCLASS
## resolved transparently by name.
##
## Deliberately the ONLY place in the whole framework that calls
## add_control_to_container(CONTAINER_PROJECT_SETTING_TAB_LEFT/RIGHT, ...) —
## confirmed (via godotengine/godot#98210) that this adds a genuine new
## top-level Project Settings tab per call, with no automatic merging across
## plugins the way Unity's SettingsProvider tree has. If every gem called
## this independently, Project Settings would end up with a separate
## "GameplayTags"/"Lexicon"/"Ogham"/"Steamworks" tab each, not one unified
## "Subsystems" tab — exactly the scattered layout this whole pass exists to
## fix. Every other gem hands its settings UI to THIS tab instead (see
## SubsystemManagerBridge::register_settings_panel()).
##
## No longer does its own project-wide dependency check (the old
## HeathenDependencyManifest.scan_installed() + "Fetch All" dialog, removed
## along with dependency_manifest.gd/dependency_fetcher.gd) — that
## responsibility now belongs to Extension Resolver for Godot's own Project
## Settings tab, which every manifest-bearing addon (this one included, see
## extension.manifest.json at this repo's root) is visible to without this
## plugin needing to scan anything itself. Game-Framework has no
## dependencies of its own and ships ungated (no .gdextension.available —
## nothing needs to wait on it), so it never needed a gate call here either.

var _tab: SubsystemsSettingsTab

func _enter_tree() -> void:
	_tab = SubsystemsSettingsTab.new()
	add_control_to_container(CONTAINER_PROJECT_SETTING_TAB_LEFT, _tab)
	SubsystemAutoloadSetup.ensure_autoload_setup()

func _exit_tree() -> void:
	if _tab != null:
		remove_control_from_container(CONTAINER_PROJECT_SETTING_TAB_LEFT, _tab)
		_tab.queue_free()
		_tab = null
