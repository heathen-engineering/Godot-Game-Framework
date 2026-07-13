@tool
extends RefCounted
class_name SubsystemAutoloadSetup

## Ensures the SubsystemBoot autoload registration exists in project.godot —
## called from FoundationGameFrameworkEditorPlugin's _enter_tree() so a dev
## never has to manually wire this up. Same precedent as
## FoundationSteamworks' own SteamAutoloadSetup.gd.
##
## Idempotent and cheap: only writes project.godot when the setting is
## actually missing or wrong, so this doesn't churn a shared,
## version-controlled file on every editor launch.

const AUTOLOAD_SETTING := "autoload/SubsystemBoot"
const SCRIPT_PATH := "res://addons/FoundationGameFramework/editor/SubsystemBootAutoload.gd"

static func ensure_autoload_setup() -> void:
	var desired := "*%s" % SCRIPT_PATH
	if ProjectSettings.get_setting(AUTOLOAD_SETTING, "") == desired:
		return
	ProjectSettings.set_setting(AUTOLOAD_SETTING, desired)
	ProjectSettings.save()
