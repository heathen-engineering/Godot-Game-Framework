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

#pragma once

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/h_split_container.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/tree.hpp>
#include <godot_cpp/classes/tree_item.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>

using namespace godot;

/// <summary>
/// The single, unified "Subsystems" Project Settings tab — added once via
/// add_control_to_container(CONTAINER_PROJECT_SETTING_TAB_LEFT, ...) from
/// FoundationGameFrameworkEditorPlugin.gd. Replaces the old SubsystemsDock
/// (a bottom-panel dock that was, in practice, never enabled/used) — same
/// underlying data (SubsystemManagerBridge's get_global_subsystem_* query
/// surface), presented as a left-side list + right-side detail pane instead
/// of a standalone dock, matching the reference Unity Project Settings ▸
/// Subsystems layout this is built from.
///
/// List entries with a registered settings panel (see
/// SubsystemManagerBridge::has_settings_panel()) render as clickable —
/// selecting one swaps the detail pane to that subsystem's own settings UI,
/// built by that gem's own registered Callable (see
/// SubsystemManagerBridge::register_settings_panel()'s doc comment for why
/// this addon never needs to know what's inside).
/// </summary>
class SubsystemsSettingsTab : public VBoxContainer
{
    GDCLASS(SubsystemsSettingsTab, VBoxContainer);

private:
    HSplitContainer *split_ = nullptr;
    /// Wraps tree_ so a StyleBoxFlat border/background can be applied — a
    /// plain Tree has no border of its own, so the left/right divide used
    /// to be visible only via the thin HSplitContainer drag handle. This is
    /// also now the control that owns custom_minimum_size/persisted width
    /// (see _on_split_dragged), not tree_ directly.
    PanelContainer *tree_panel_ = nullptr;
    Tree *tree_ = nullptr;
    Control *detail_container_ = nullptr;
    /// subsystem name -> built Control, cached lazily so switching away from
    /// and back to a subsystem's page preserves its own internal state
    /// (scroll position, expanded tree nodes, in-progress edits, ...)
    /// instead of rebuilding a fresh, blank instance every click.
    Dictionary built_panels_;

    void _ensure_built();
    void _rebuild_list();
    void _on_item_selected();
    void _show_panel_for(const String &subsystem_name);
    void _on_button_clicked(Object *item, int column, int id, int mouse_button_index);
    /// Persists the divider's actual pixel position (tree_'s resulting
    /// width, not the raw HSplitContainer offset — see the .cpp for why)
    /// so it survives closing/reopening Project Settings and editor
    /// restarts.
    void _on_split_dragged(int offset);
    /// ok=true -> checkmark glyph; ok=false -> warning-triangle glyph
    /// (hard_error picks red over amber). Replaces the old flat-color-square
    /// icon — same generated-ImageTexture mechanism (TreeItem::add_button()
    /// renders it correctly; this project's Button.icon does not), just a
    /// hand-plotted/procedural shape instead of a solid fill, so status
    /// reads without relying on color alone (colorblind-friendly).
    static Ref<ImageTexture> _status_icon(bool ok, bool hard_error = false);
    /// mode: 0=Disabled (stop square), 1=On Demand (play triangle),
    /// 2=Automatic (fast-forward double-triangle). editable=false renders a
    /// dim/gray glyph for the ~most rows where StartMode can't be changed;
    /// editable=true colors it (red/blue/green) since it's now an
    /// interactive click-to-cycle button, not a dropdown.
    static Ref<ImageTexture> _start_mode_icon(int mode, bool editable);
    /// EditorSettings-backed (per-user editor state, not project data —
    /// this is UI layout memory, the same category as e.g. which dock tabs
    /// were open, not something that belongs committed to the project).
    static Variant _get_editor_setting(const String &key, const Variant &default_value);
    static void _set_editor_setting(const String &key, const Variant &value);

public:
    SubsystemsSettingsTab() = default;

    void _ready();
    void refresh();
    /// Re-runs refresh() whenever this tab becomes visible again (e.g. the
    /// user closes and reopens Project Settings) — a live status readout
    /// instead of a manual "Refresh" button nobody has a reason to press
    /// while looking straight at it.
    void _on_visibility_changed();

protected:
    static void _bind_methods();
};
