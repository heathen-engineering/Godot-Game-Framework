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

var _tab: SubsystemsSettingsTab
var _dependency_dialog: AcceptDialog

func _enter_tree() -> void:
	_tab = SubsystemsSettingsTab.new()
	add_control_to_container(CONTAINER_PROJECT_SETTING_TAB_LEFT, _tab)
	call_deferred("_check_dependencies")
	SubsystemAutoloadSetup.ensure_autoload_setup()

func _exit_tree() -> void:
	if _tab != null:
		remove_control_from_container(CONTAINER_PROJECT_SETTING_TAB_LEFT, _tab)
		_tab.queue_free()
		_tab = null
	if _dependency_dialog != null:
		_dependency_dialog.queue_free()
		_dependency_dialog = null

## Soft-dependency check: every installed addon that ships a
## heathen_manifest.json gets its declared dependencies checked against
## what else is installed. Missing dependencies are never fetched
## automatically — this only ever surfaces a dialog the user must
## explicitly confirm per-item. See dependency_manifest.gd /
## dependency_fetcher.gd for the mechanism.
func _check_dependencies() -> void:
	var installed := HeathenDependencyManifest.scan_installed()
	var missing := HeathenDependencyManifest.find_missing_dependencies(installed)
	if missing.is_empty():
		return

	_dependency_dialog = AcceptDialog.new()
	_dependency_dialog.title = "Missing Dependencies"
	_dependency_dialog.dialog_hide_on_ok = false

	var vbox := VBoxContainer.new()
	var label := Label.new()
	var lines: PackedStringArray = []
	for dep in missing:
		lines.append("- %s (required by %s)" % [dep["id"], dep["required_by"]])
	label.text = "The following addons are missing:\n\n%s\n\nFetch them from GitHub now?" % "\n".join(lines)
	label.autowrap_mode = TextServer.AUTOWRAP_WORD
	vbox.add_child(label)

	var status_label := Label.new()
	vbox.add_child(status_label)

	_dependency_dialog.add_child(vbox)
	var fetch_button := _dependency_dialog.add_button("Fetch All", true, "fetch_all")
	_dependency_dialog.confirmed.connect(func(): _dependency_dialog.hide())
	_dependency_dialog.custom_action.connect(func(action: StringName):
		if action == "fetch_all":
			fetch_button.disabled = true
			await _fetch_all(missing, status_label)
			_dependency_dialog.hide()
	)

	get_editor_interface().get_base_control().add_child(_dependency_dialog)
	_dependency_dialog.popup_centered(Vector2i(480, 320))

func _fetch_all(missing: Array, status_label: Label) -> void:
	for dep in missing:
		var fetcher := HeathenDependencyFetcher.new()
		add_child(fetcher)
		fetcher.progress.connect(func(msg: String): status_label.text = msg)
		var ok: bool = await fetcher.fetch(dep)
		if not ok:
			status_label.text = "Failed to fetch %s: %s" % [dep["id"], fetcher.last_error()]
			push_warning("HeathenDependencyFetcher: %s" % fetcher.last_error())
		fetcher.queue_free()
