@tool
class_name HeathenDependencyFetcher
extends Node

## Fetches a single missing dependency described by a
## HeathenDependencyManifest.find_missing_dependencies() descriptor:
## queries the GitHub "latest release" API, finds the matching release
## asset, downloads it, and extracts it into res://addons/<id>/.
##
## This node never runs unattended — every call site is expected to have
## already gotten explicit user confirmation (see
## FoundationGameFrameworkEditorPlugin.gd's dependency-check dialog). This
## class only does the mechanical fetch/extract/rescan once told to.
##
## Usage: add as a child of an editor-context node, then:
##   var fetcher := HeathenDependencyFetcher.new()
##   add_child(fetcher)
##   var ok: bool = await fetcher.fetch(dependency_descriptor)

signal progress(message: String)

const GITHUB_API_ROOT := "https://api.github.com/repos/"

var _last_error := ""

func last_error() -> String:
	return _last_error

## Returns true on success (files extracted, filesystem rescanned), false
## on any failure — check last_error() for details.
func fetch(dep: Dictionary) -> bool:
	_last_error = ""

	var repo: String = dep.get("repo", "")
	var id: String = dep.get("id", "")
	var asset_pattern: String = dep.get("release_asset_pattern", "")

	if repo.is_empty() or id.is_empty():
		_last_error = "Dependency descriptor is missing 'repo' or 'id'."
		return false

	progress.emit("Looking up latest release of %s..." % repo)
	var release := await _fetch_latest_release(repo)
	if release == null:
		return false

	var version: String = release.get("tag_name", "").trim_prefix("v")
	var assets: Array = release.get("assets", [])
	var wanted_name := asset_pattern.replace("{version}", version) if not asset_pattern.is_empty() else ""

	var download_url := ""
	for asset in assets:
		if typeof(asset) != TYPE_DICTIONARY:
			continue
		var name: String = asset.get("name", "")
		if (not wanted_name.is_empty() and name == wanted_name) or (wanted_name.is_empty() and name.begins_with(id)):
			download_url = asset.get("browser_download_url", "")
			break

	if download_url.is_empty():
		_last_error = "No release asset matching '%s' found in latest release of %s." % [wanted_name, repo]
		return false

	progress.emit("Downloading %s..." % download_url.get_file())
	var zip_bytes := await _download(download_url)
	if zip_bytes.is_empty():
		return false

	progress.emit("Extracting into res://addons/%s..." % id)
	if not _extract_zip(zip_bytes, id):
		return false

	progress.emit("Rescanning filesystem...")
	if Engine.is_editor_hint():
		EditorInterface.get_resource_filesystem().scan()

	return true

func _fetch_latest_release(repo: String) -> Variant:
	var http := HTTPRequest.new()
	add_child(http)
	var err := http.request(GITHUB_API_ROOT + repo + "/releases/latest", ["Accept: application/vnd.github+json"])
	if err != OK:
		_last_error = "Failed to start request for %s (error %d)." % [repo, err]
		http.queue_free()
		return null

	var result: Array = await http.request_completed
	http.queue_free()

	var response_code: int = result[1]
	var body: PackedByteArray = result[3]
	if response_code != 200:
		_last_error = "GitHub API returned %d for %s." % [response_code, repo]
		return null

	var parsed: Variant = JSON.parse_string(body.get_string_from_utf8())
	if typeof(parsed) != TYPE_DICTIONARY:
		_last_error = "GitHub API response for %s was not valid JSON." % repo
		return null

	return parsed

func _download(url: String) -> PackedByteArray:
	var http := HTTPRequest.new()
	add_child(http)
	var err := http.request(url)
	if err != OK:
		_last_error = "Failed to start download of %s (error %d)." % [url, err]
		http.queue_free()
		return PackedByteArray()

	var result: Array = await http.request_completed
	http.queue_free()

	var response_code: int = result[1]
	var body: PackedByteArray = result[3]
	if response_code != 200:
		_last_error = "Download of %s returned HTTP %d." % [url, response_code]
		return PackedByteArray()

	return body

func _extract_zip(zip_bytes: PackedByteArray, dependency_id: String) -> bool:
	var tmp_path := "user://%s_download.zip" % dependency_id
	var tmp_file := FileAccess.open(tmp_path, FileAccess.WRITE)
	if tmp_file == null:
		_last_error = "Could not write temp file %s." % tmp_path
		return false
	tmp_file.store_buffer(zip_bytes)
	tmp_file.close()

	var reader := ZIPReader.new()
	if reader.open(tmp_path) != OK:
		_last_error = "Could not open downloaded archive as a zip."
		DirAccess.remove_absolute(ProjectSettings.globalize_path(tmp_path))
		return false

	var dest_root := "res://addons/%s" % dependency_id
	if not DirAccess.dir_exists_absolute(ProjectSettings.globalize_path(dest_root)):
		DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(dest_root))

	for entry_path in reader.get_files():
		if entry_path.ends_with("/"):
			continue

		# Archives are expected to be rooted at the addon folder itself
		# (i.e. contain "FoundationXxHash/plugin.cfg", not "plugin.cfg" at
		# the top level) — strip the first path component so contents land
		# directly under res://addons/<dependency_id>/.
		var relative_path := entry_path
		var first_slash := relative_path.find("/")
		if first_slash != -1:
			relative_path = relative_path.substr(first_slash + 1)
		if relative_path.is_empty():
			continue

		var dest_path := "%s/%s" % [dest_root, relative_path]
		var dest_dir := dest_path.get_base_dir()
		var globalized_dir := ProjectSettings.globalize_path(dest_dir)
		if not DirAccess.dir_exists_absolute(globalized_dir):
			DirAccess.make_dir_recursive_absolute(globalized_dir)

		var out_file := FileAccess.open(dest_path, FileAccess.WRITE)
		if out_file == null:
			_last_error = "Could not write extracted file %s." % dest_path
			reader.close()
			DirAccess.remove_absolute(ProjectSettings.globalize_path(tmp_path))
			return false
		out_file.store_buffer(reader.read_file(entry_path))
		out_file.close()

	reader.close()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(tmp_path))
	return true
