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

#include "editor/SubsystemsSettingsTab.h"

#include "SubsystemManagerBridge.h"

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_settings.hpp>
#include <godot_cpp/classes/h_split_container.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/tree_item.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

using namespace godot;

namespace
{
// Column layout — [Build] [Name] [Start Mode], matching the reference
// mock: a small colored build-status button, the subsystem's name, and
// either a read-only "Automatic" label or an editable dropdown for
// subsystems that expose one (see SubsystemManagerBridge::
// has_start_mode_setter()'s doc comment for why that's the deciding
// factor, not a hardcoded per-subsystem list here).
constexpr int COL_BUILD = 0;
constexpr int COL_NAME = 1;
constexpr int COL_START_MODE = 2;
constexpr int BUILD_BUTTON_ID = 1;
constexpr int START_MODE_BUTTON_ID = 2;

const char *SETTING_TREE_WIDTH = "subsystems/tree_panel_width";
const char *SETTING_LAST_SELECTED = "subsystems/last_selected_subsystem";
constexpr int DEFAULT_TREE_WIDTH = 248;

const char *START_MODE_TOOLTIP =
    ">> = Automatic, Initialise at boot\n"
    "> = On Demand, Initialise when requested from script\n"
    "[] = Disabled, cannot be Initialised";

// ── Status/StartMode glyphs ─────────────────────────────────────────────
// Small solid-color glyph bitmaps generated procedurally (not hand-authored
// pixel art — easier to verify correct than an ASCII-art mask), the same
// approach used for Godot-Extension-Resolver's settings_tab.gd equivalent
// this session. This project's Button.icon doesn't render in this editor
// build (see feedback_godot_button_icon_broken) — every glyph here goes
// through TreeItem::add_button(), the same mechanism the original
// flat-color status square used, confirmed to render correctly.
constexpr int GLYPH_SIZE = 12;

Ref<Image> new_glyph_image()
{
    Ref<Image> img = Image::create(GLYPH_SIZE, GLYPH_SIZE, false, Image::FORMAT_RGBA8);
    img->fill(Color(0, 0, 0, 0));
    return img;
}

// Isoceles triangle, apex pointing up — the warning glyph.
void fill_triangle_up(Ref<Image> img, const Color &color, int apex_x, int apex_y, int base_y, int half_width)
{
    int height = base_y - apex_y;
    if (height <= 0)
        return;
    for (int y = apex_y; y <= base_y; y++)
    {
        float t = float(y - apex_y) / float(height);
        int half_w = int(t * half_width + 0.5f);
        for (int x = apex_x - half_w; x <= apex_x + half_w; x++)
        {
            if (x >= 0 && x < GLYPH_SIZE && y >= 0 && y < GLYPH_SIZE)
                img->set_pixel(x, y, color);
        }
    }
}

// Apex pointing right — the Play/On-Demand glyph, and (drawn twice, offset)
// the Fast-Forward/Automatic glyph.
void fill_triangle_right(Ref<Image> img, const Color &color, int x0, int x1, int center_y, int half_height)
{
    int width = x1 - x0;
    if (width <= 0)
        return;
    for (int x = x0; x <= x1; x++)
    {
        float t = float(x1 - x) / float(width);
        int half_h = int(t * half_height + 0.5f);
        for (int y = center_y - half_h; y <= center_y + half_h; y++)
        {
            if (x >= 0 && x < GLYPH_SIZE && y >= 0 && y < GLYPH_SIZE)
                img->set_pixel(x, y, color);
        }
    }
}

// Filled square — the Stop/Disabled glyph.
void fill_rect(Ref<Image> img, const Color &color, int x0, int y0, int x1, int y1)
{
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (x >= 0 && x < GLYPH_SIZE && y >= 0 && y < GLYPH_SIZE)
                img->set_pixel(x, y, color);
}

// Hand-picked points, not procedural — a checkmark isn't a simple geometric
// primitive the way the triangle/square glyphs are. Each point drawn 2px
// wide so the stroke reads at 12px scale instead of vanishing to single
// pixels.
void plot_checkmark(Ref<Image> img, const Color &color)
{
    static const int points[][2] = {
        {1, 6}, {2, 7}, {3, 8}, {4, 9}, {5, 8}, {6, 7}, {7, 6}, {8, 5}, {9, 4}, {10, 3}, {10, 2},
    };
    for (const auto &p : points)
    {
        int x = p[0], y = p[1];
        if (x >= 0 && x < GLYPH_SIZE && y >= 0 && y < GLYPH_SIZE)
            img->set_pixel(x, y, color);
        if (x + 1 < GLYPH_SIZE)
            img->set_pixel(x + 1, y, color);
    }
}
} // namespace

void SubsystemsSettingsTab::_ready()
{
    _ensure_built();
}

void SubsystemsSettingsTab::_ensure_built()
{
    set_name("Subsystems");
    // A direct VBoxContainer (not a plain Control with an internal
    // anchored VBoxContainer, which this used to be) — a plain Control's
    // get_minimum_size() doesn't account for anchor-positioned children, so
    // whatever container the Project Settings dialog placed this tab page
    // inside sized it down to ~0 and only size flags stretched it, which
    // wasn't reliable for height. Extending VBoxContainer directly means
    // every ancestor container sizes this the same well-tested way it
    // sizes every other Container, top to bottom.
    set_h_size_flags(Control::SIZE_EXPAND_FILL);
    set_v_size_flags(Control::SIZE_EXPAND_FILL);
    connect("visibility_changed", callable_mp(this, &SubsystemsSettingsTab::_on_visibility_changed));

    split_ = memnew(HSplitContainer);
    split_->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    split_->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    split_->connect("dragged", callable_mp(this, &SubsystemsSettingsTab::_on_split_dragged));
    add_child(split_);

    // tree_panel_ wraps the Tree so a StyleBoxFlat border/background can be
    // applied — a plain Tree has no border of its own, so the left/right
    // divide used to be visible only via the thin HSplitContainer drag
    // handle. Border on the side facing the detail pane, plus a slightly
    // darker fill than the editor's default panel color (Unity Package
    // Manager's "sunken list" cue) so the two halves read as distinct
    // regions at a glance.
    tree_panel_ = memnew(PanelContainer);
    tree_panel_->set_h_size_flags(Control::SIZE_FILL);
    tree_panel_->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    Ref<StyleBoxFlat> panel_style;
    panel_style.instantiate();
    panel_style->set_bg_color(Color(0, 0, 0, 0.12));
    panel_style->set_border_color(Color(0, 0, 0, 0.4));
    panel_style->set_border_width(SIDE_RIGHT, 2);
    panel_style->set_content_margin(SIDE_LEFT, 4);
    panel_style->set_content_margin(SIDE_TOP, 4);
    panel_style->set_content_margin(SIDE_BOTTOM, 4);
    tree_panel_->add_theme_stylebox_override("panel", panel_style);
    // custom_minimum_size moves to the PANEL now — it's the control that
    // actually drives HSplitContainer's SIZE_FILL sizing math (see the
    // class-level comment on tree_'s own sizing below); the Tree fills the
    // panel completely. If the user has dragged the divider before, restore
    // that width instead of the default — see _on_split_dragged().
    int saved_width = int(_get_editor_setting(SETTING_TREE_WIDTH, DEFAULT_TREE_WIDTH));
    tree_panel_->set_custom_minimum_size(Vector2(saved_width > 0 ? saved_width : DEFAULT_TREE_WIDTH, 0));
    split_->add_child(tree_panel_);

    tree_ = memnew(Tree);
    tree_->set_hide_root(true);
    // Horizontal is EXPAND_FILL here (unlike before) since tree_panel_, not
    // tree_ itself, is now the SIZE_FILL control HSplitContainer sizes off
    // of — the Tree just fills whatever width the panel ends up with.
    // (An HSplitContainer's split_offset is measured from the container's
    // auto-computed *center*, not its left edge, which is why an earlier
    // attempt at a fixed offset put the divider past the middle of the
    // whole dialog instead of near the panel's minimum width — hence
    // sizing via custom_minimum_size + SIZE_FILL instead of split_offset.)
    tree_->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    tree_->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    tree_->set_columns(3);
    tree_->set_column_titles_visible(false);
    tree_->set_column_expand(COL_BUILD, false);
    tree_->set_column_custom_minimum_width(COL_BUILD, 28);
    tree_->set_column_expand(COL_NAME, true);
    tree_->set_column_custom_minimum_width(COL_NAME, 110);
    tree_->set_column_expand(COL_START_MODE, false);
    tree_->set_column_custom_minimum_width(COL_START_MODE, 90);
    tree_->connect("item_selected", callable_mp(this, &SubsystemsSettingsTab::_on_item_selected));
    tree_->connect("button_clicked", callable_mp(this, &SubsystemsSettingsTab::_on_button_clicked));
    tree_panel_->add_child(tree_);

    detail_container_ = memnew(Control);
    detail_container_->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    detail_container_->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    split_->add_child(detail_container_);

    Label *hint = memnew(Label);
    hint->set_text("Select a subsystem on the left to view its settings.");
    hint->set_anchors_preset(Control::PRESET_CENTER);
    detail_container_->add_child(hint);

    refresh();
}

void SubsystemsSettingsTab::_on_visibility_changed()
{
    if (is_visible_in_tree())
        refresh();
}

void SubsystemsSettingsTab::refresh()
{
    SubsystemManagerBridge *bridge = SubsystemManagerBridge::get_singleton();
    if (bridge == nullptr)
        return;

    bridge->boot(); // idempotent — safe to call every refresh.
    _rebuild_list();
}

void SubsystemsSettingsTab::_rebuild_list()
{
    SubsystemManagerBridge *bridge = SubsystemManagerBridge::get_singleton();
    if (bridge == nullptr || tree_ == nullptr)
        return;

    // Selection doesn't survive tree_->clear() — TreeItem instances are
    // freed with it — so remember which subsystem was selected by name and
    // re-select the matching new row once the list is rebuilt. Falls back
    // to the last selection from a previous editor session (EditorSettings)
    // when there's no in-session selection yet — i.e. the very first
    // _rebuild_list() call after opening Project Settings.
    TreeItem *previously_selected = tree_->get_selected();
    String reselect_name = previously_selected != nullptr
        ? previously_selected->get_text(COL_NAME)
        : String(_get_editor_setting(SETTING_LAST_SELECTED, String()));

    tree_->clear();
    TreeItem *root = tree_->create_item();
    TreeItem *to_select = nullptr;
    TreeItem *first_item = nullptr;

    int count = bridge->get_global_subsystem_count();
    for (int i = 0; i < count; i++)
    {
        String name = bridge->get_global_subsystem_name(i);
        TreeItem *item = tree_->create_item(root);

        // Build status — a checkmark (good) or warning-triangle (needs
        // attention/amber, or error/red) glyph button, only present at all
        // for subsystems that registered one (see SubsystemManagerBridge::
        // has_build()'s doc comment — this concept, ported from
        // Unity-Game-Framework's ISettingsGenerator/BuildStatus, is opt-in;
        // most subsystems have nothing to build). Fresh Ref<ImageTexture>
        // generated per rebuild rather than cached — deliberately, see
        // _status_icon()'s own comment for why a static cache here would be
        // a mistake.
        if (bridge->has_build(name))
        {
            int status = bridge->get_build_status(name);
            item->add_button(COL_BUILD, _status_icon(status == 0, status == 2), BUILD_BUTTON_ID, false,
                              status == 1 ? "Needs attention — click to build" : status == 2 ? "Error — click to build" : "Up to date — click to rebuild");
        }

        item->set_text(COL_NAME, name);
        // The "blue button" look from the reference Unity screenshots —
        // texture-free (this project's Button.icon doesn't render), a
        // plain colored label is what distinguishes "this has a settings
        // page, click it" from a plain reporting-only entry.
        if (bridge->has_settings_panel(name))
            item->set_custom_color(COL_NAME, Color(0.45, 0.65, 1.0));

        // Start Mode — click-to-cycle icon button instead of a dropdown
        // (Play=On Demand, Fast-Forward=Automatic, Stop=Disabled), dim/gray
        // and disabled for the ~most rows where it isn't configurable at
        // all. See _on_button_clicked() for the cycling logic.
        int mode = bridge->get_global_subsystem_start_mode(i);
        bool editable = bridge->has_start_mode_setter(name);
        String tooltip = String(START_MODE_TOOLTIP) + (editable ? "\n\nClick to change — takes effect next time the project loads." : "");
        item->add_button(COL_START_MODE, _start_mode_icon(mode, editable), START_MODE_BUTTON_ID, !editable, tooltip);

        if (first_item == nullptr)
            first_item = item;
        if (name == reselect_name)
            to_select = item;
    }

    // Nothing matched (first-ever run, or the remembered subsystem is gone)
    // — default to the top row rather than leaving the detail pane blank.
    if (to_select == nullptr)
        to_select = first_item;

    if (to_select != nullptr)
    {
        to_select->select(COL_NAME);
        // TreeItem::select() doesn't emit "item_selected" (that signal is
        // reserved for user interaction), so the detail pane needs an
        // explicit push here too — otherwise a first-ever open would select
        // the top row visually but still show the "select a subsystem"
        // placeholder.
        _show_panel_for(to_select->get_text(COL_NAME));
        _set_editor_setting(SETTING_LAST_SELECTED, to_select->get_text(COL_NAME));
    }
}

void SubsystemsSettingsTab::_on_item_selected()
{
    TreeItem *selected = tree_->get_selected();
    if (selected == nullptr)
        return;
    _show_panel_for(selected->get_text(COL_NAME));
    _set_editor_setting(SETTING_LAST_SELECTED, selected->get_text(COL_NAME));
}

void SubsystemsSettingsTab::_on_button_clicked(Object *item, int column, int id, int mouse_button_index)
{
    (void)mouse_button_index;
    SubsystemManagerBridge *bridge = SubsystemManagerBridge::get_singleton();
    TreeItem *tree_item = Object::cast_to<TreeItem>(item);
    if (bridge == nullptr || tree_item == nullptr)
        return;

    if (column == COL_BUILD && id == BUILD_BUTTON_ID)
    {
        bridge->trigger_build(tree_item->get_text(COL_NAME));
        _rebuild_list();
        return;
    }

    if (column == COL_START_MODE && id == START_MODE_BUTTON_ID)
    {
        String name = tree_item->get_text(COL_NAME);
        // Disabled buttons (the !has_start_mode_setter(name) rows) don't
        // fire button_clicked at all, but guard anyway rather than trust
        // that Godot version behavior implicitly.
        if (!bridge->has_start_mode_setter(name))
            return;

        int current = -1;
        int count = bridge->get_global_subsystem_count();
        for (int i = 0; i < count; i++)
        {
            if (bridge->get_global_subsystem_name(i) == name)
            {
                current = bridge->get_global_subsystem_start_mode(i);
                break;
            }
        }
        if (current < 0)
            return;

        int next_mode = (current + 1) % 3; // Disabled(0) -> On Demand(1) -> Automatic(2) -> Disabled(0)
        bridge->set_global_subsystem_start_mode(name, next_mode);
        // Changing StartMode only takes effect on the next boot() — this
        // project's Subsystems are constructed/dependency-ordered once at
        // extension-load time (see gameframework::SubsystemManager::boot()'s
        // own doc comment), so an already-Automatic subsystem that's already
        // initialised can't be retroactively un-initialised from here. Same
        // real-world constraint Unity's own ApplyHint concept exists to
        // communicate — surfaced via the tooltip text set in _rebuild_list().
        _rebuild_list();
    }
}

void SubsystemsSettingsTab::_on_split_dragged(int offset)
{
    (void)offset;
    // Deliberately not persisting the raw "offset" the signal hands us —
    // HSplitContainer's own split_offset is measured from the container's
    // auto-computed center, not a pixel width, which is exactly the
    // confusion that caused the "extremely wide on reopen" bug this feature
    // is built on top of. tree_panel_'s actual on-screen width after the
    // drag has already settled by the time this signal fires, so read that
    // directly and persist it as the same custom_minimum_size value
    // _ensure_built() knows how to apply — no offset math to get wrong a
    // second time. (Reads tree_panel_, not tree_, since the panel is now
    // the SIZE_FILL control that drives sizing — see _ensure_built().)
    _set_editor_setting(SETTING_TREE_WIDTH, int(tree_panel_->get_size().x));
}

Variant SubsystemsSettingsTab::_get_editor_setting(const String &key, const Variant &default_value)
{
    EditorInterface *interface_ = EditorInterface::get_singleton();
    if (interface_ == nullptr)
        return default_value;
    Ref<EditorSettings> settings = interface_->get_editor_settings();
    if (settings.is_null() || !settings->has_setting(key))
        return default_value;
    return settings->get_setting(key);
}

void SubsystemsSettingsTab::_set_editor_setting(const String &key, const Variant &value)
{
    EditorInterface *interface_ = EditorInterface::get_singleton();
    if (interface_ == nullptr)
        return;
    Ref<EditorSettings> settings = interface_->get_editor_settings();
    if (settings.is_null())
        return;
    settings->set_setting(key, value);
}

Ref<ImageTexture> SubsystemsSettingsTab::_status_icon(bool ok, bool hard_error)
{
    // NOT cached in a static — a static Ref<ImageTexture> here would hold a
    // GPU resource past RenderingServer's own teardown at editor shutdown,
    // crashing on destruction ("Parameter RenderingServer::get_singleton()
    // is null"), exactly the bug already found and fixed in
    // OghamGraphView::_swatch_icon_cache. There are at most a handful of
    // rows redrawn on an explicit refresh/click, not a hot path — recreating
    // a 12x12 texture each time costs nothing worth caching for.
    Ref<Image> img = new_glyph_image();
    if (ok)
        plot_checkmark(img, Color(0.30, 0.72, 0.35));
    else
        fill_triangle_up(img, hard_error ? Color(0.85, 0.32, 0.32) : Color(1.0, 0.72, 0.10), 6, 1, 10, 5);
    return ImageTexture::create_from_image(img);
}

Ref<ImageTexture> SubsystemsSettingsTab::_start_mode_icon(int mode, bool editable)
{
    Ref<Image> img = new_glyph_image();
    Color color;
    if (!editable)
        color = Color(0.5, 0.5, 0.5, 0.6); // dim/gray — not configurable for this subsystem
    else
        color = mode == 0 ? Color(0.85, 0.32, 0.32) // Disabled -> red
              : mode == 1 ? Color(0.30, 0.55, 0.85) // On Demand -> blue
                          : Color(0.30, 0.72, 0.35); // Automatic -> green

    switch (mode)
    {
        case 0: // Disabled -> stop square
            fill_rect(img, color, 3, 3, 8, 8);
            break;
        case 1: // On Demand -> play triangle
            fill_triangle_right(img, color, 2, 9, 6, 5);
            break;
        default: // Automatic -> fast-forward, two triangles
            fill_triangle_right(img, color, 1, 6, 6, 5);
            fill_triangle_right(img, color, 6, 11, 6, 5);
            break;
    }
    return ImageTexture::create_from_image(img);
}

void SubsystemsSettingsTab::_show_panel_for(const String &subsystem_name)
{
    SubsystemManagerBridge *bridge = SubsystemManagerBridge::get_singleton();
    if (bridge == nullptr || detail_container_ == nullptr)
        return;

    for (int i = 0; i < detail_container_->get_child_count(); i++)
    {
        Control *child = Object::cast_to<Control>(detail_container_->get_child(i));
        if (child != nullptr)
            child->set_visible(false);
    }

    if (!bridge->has_settings_panel(subsystem_name))
    {
        Label *placeholder = memnew(Label);
        placeholder->set_text(subsystem_name + String(" has no settings page."));
        detail_container_->add_child(placeholder);
        return;
    }

    if (!built_panels_.has(subsystem_name))
    {
        Control *panel = bridge->get_settings_panel(subsystem_name);
        if (panel == nullptr)
            return;
        panel->set_anchors_preset(Control::PRESET_FULL_RECT);
        detail_container_->add_child(panel);
        built_panels_[subsystem_name] = panel;
    }

    Object *cached = built_panels_[subsystem_name];
    Control *panel = Object::cast_to<Control>(cached);
    if (panel != nullptr)
        panel->set_visible(true);
}

void SubsystemsSettingsTab::_bind_methods() {}
