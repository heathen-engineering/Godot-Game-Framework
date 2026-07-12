@tool
extends EditorPlugin

## Activates the Game Framework editor tooling: the Subsystems dock. This is
## the one file in the whole addon that HAS to be GDScript — Godot's
## plugin.cfg "script=" field only accepts a .gd path, not a C++ class (a
## hard platform constraint, not a style choice). Everything it does is a
## single .new() + dock registration; every other class it references
## (SubsystemsDock) is a C++ GDCLASS resolved transparently by name.

var _dock: SubsystemsDock
var _dependency_dialog: AcceptDialog

func _enter_tree() -> void:
	_dock = SubsystemsDock.new()
	add_control_to_bottom_panel(_dock, "Subsystems")
	call_deferred("_check_dependencies")

func _exit_tree() -> void:
	if _dock != null:
		remove_control_from_bottom_panel(_dock)
		_dock.queue_free()
		_dock = null
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
