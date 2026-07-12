@tool
class_name HeathenDependencyManifest
extends RefCounted

## Reads and reasons about `heathen_manifest.json` files. Every gem that wants
## its dependencies checked (Godot-Game-Framework included) ships one of
## these at its addon root:
##
##   {
##       "id": "FoundationGameFramework",
##       "display_name": "Foundation for Game Framework",
##       "version": "1.0.0",
##       "repo": "heathen-engineering/Godot-Game-Framework",
##       "release_asset_pattern": "FoundationGameFramework-{version}.zip",
##       "dependencies": [
##           { "id": "FoundationXxHash", "min_version": "1.0.0",
##             "repo": "heathen-engineering/Godot-xxHash",
##             "release_asset_pattern": "FoundationXxHash-{version}.zip" }
##       ]
##   }
##
## This is soft-dependency bookkeeping only — nothing here ever fetches or
## installs anything on its own. It is deliberately engine-tooling-only
## (HTTPRequest / EditorFileSystem live in HeathenDependencyFetcher), so it
## stays out of the engine-agnostic Game-Framework core by design.

const MANIFEST_FILENAME := "heathen_manifest.json"

## Scans res://addons/*/heathen_manifest.json and returns a Dictionary of
## id -> parsed manifest Dictionary for every addon that ships one. Addons
## without a manifest are simply not represented — they are not part of
## this dependency graph at all.
static func scan_installed() -> Dictionary:
	var result: Dictionary = {}
	var addons_dir := DirAccess.open("res://addons")
	if addons_dir == null:
		return result

	addons_dir.list_dir_begin()
	var entry := addons_dir.get_next()
	while entry != "":
		if addons_dir.current_is_dir() and not entry.begins_with("."):
			var manifest_path := "res://addons/%s/%s" % [entry, MANIFEST_FILENAME]
			var manifest := _read_manifest(manifest_path)
			if manifest != null and manifest.has("id"):
				result[manifest["id"]] = manifest
		entry = addons_dir.get_next()
	addons_dir.list_dir_end()

	return result

static func _read_manifest(path: String) -> Variant:
	if not FileAccess.file_exists(path):
		return null

	var file := FileAccess.open(path, FileAccess.READ)
	if file == null:
		return null

	var text := file.get_as_text()
	var parsed: Variant = JSON.parse_string(text)
	if typeof(parsed) != TYPE_DICTIONARY:
		push_warning("HeathenDependencyManifest: malformed manifest at %s" % path)
		return null

	return parsed

## Given the result of scan_installed(), returns an Array of missing
## dependency descriptors — one Dictionary per (required_by, dependency)
## pair whose "id" isn't present in installed. Each descriptor carries
## everything HeathenDependencyFetcher needs to offer a fetch:
## { id, repo, release_asset_pattern, min_version, required_by }.
## Version satisfaction is not checked here — an installed dependency of
## any version counts as present; a stricter check is a future addition,
## not a gap being worked around now.
static func find_missing_dependencies(installed: Dictionary) -> Array:
	var missing: Array = []

	for manifest_id in installed:
		var manifest: Dictionary = installed[manifest_id]
		var deps: Array = manifest.get("dependencies", [])
		for dep in deps:
			if typeof(dep) != TYPE_DICTIONARY or not dep.has("id"):
				continue
			if installed.has(dep["id"]):
				continue

			missing.append({
				"id": dep["id"],
				"repo": dep.get("repo", ""),
				"release_asset_pattern": dep.get("release_asset_pattern", ""),
				"min_version": dep.get("min_version", ""),
				"required_by": manifest_id,
			})

	return missing
