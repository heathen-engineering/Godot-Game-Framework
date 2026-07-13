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

#include <godot_cpp/classes/h_split_container.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/label.hpp>
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

const char *START_MODE_CHOICES = "Disabled,On Demand,Automatic";

String start_mode_label(int mode)
{
    switch (mode)
    {
        case 0: return "Disabled";
        case 1: return "On Demand";
        default: return "Automatic";
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

    HSplitContainer *split = memnew(HSplitContainer);
    split->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    split->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    add_child(split);

    tree_ = memnew(Tree);
    tree_->set_hide_root(true);
    // Both EXPAND_FILL — an earlier version only set the vertical flag, so
    // HSplitContainer gave the Tree only its minimum width and handed
    // literally everything else to detail_container_, cramming the whole
    // list into the top-left corner with a huge dead space to the right.
    tree_->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    tree_->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    tree_->set_custom_minimum_size(Vector2(320, 0));
    tree_->set_columns(3);
    tree_->set_column_titles_visible(false);
    tree_->set_column_expand(COL_BUILD, false);
    tree_->set_column_custom_minimum_width(COL_BUILD, 28);
    tree_->set_column_expand(COL_NAME, true);
    tree_->set_column_custom_minimum_width(COL_NAME, 120);
    tree_->set_column_expand(COL_START_MODE, false);
    tree_->set_column_custom_minimum_width(COL_START_MODE, 110);
    tree_->connect("item_selected", callable_mp(this, &SubsystemsSettingsTab::_on_item_selected));
    tree_->connect("item_edited", callable_mp(this, &SubsystemsSettingsTab::_on_item_edited));
    tree_->connect("button_clicked", callable_mp(this, &SubsystemsSettingsTab::_on_button_clicked));
    split->add_child(tree_);

    detail_container_ = memnew(Control);
    detail_container_->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    detail_container_->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    split->add_child(detail_container_);

    Label *hint = memnew(Label);
    hint->set_text("Select a subsystem on the left to view its settings.");
    hint->set_anchors_preset(Control::PRESET_CENTER);
    detail_container_->add_child(hint);

    // HSplitContainer splits by absolute pixel offset from the left edge,
    // not a ratio — without this it defaults to 0, which (combined with
    // both children now correctly requesting EXPAND_FILL) let the split
    // collapse back to detail_container_ taking nearly everything again.
    split->set_split_offset(340);

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
    // re-select the matching new row once the list is rebuilt.
    TreeItem *previously_selected = tree_->get_selected();
    String reselect_name = previously_selected != nullptr ? previously_selected->get_text(COL_NAME) : String();

    tree_->clear();
    TreeItem *root = tree_->create_item();

    int count = bridge->get_global_subsystem_count();
    for (int i = 0; i < count; i++)
    {
        String name = bridge->get_global_subsystem_name(i);
        TreeItem *item = tree_->create_item(root);

        // Build status — a small colored square button (green/good, amber/
        // needs attention, red/error), only present at all for subsystems
        // that registered one (see SubsystemManagerBridge::has_build()'s
        // doc comment — this concept, ported from Unity-Game-Framework's
        // ISettingsGenerator/BuildStatus, is opt-in; most subsystems have
        // nothing to build). Fresh Ref<ImageTexture> generated per rebuild
        // rather than cached — deliberately, see _status_icon()'s own
        // comment for why a static cache here would be a mistake.
        if (bridge->has_build(name))
        {
            int status = bridge->get_build_status(name);
            Color status_color = status == 1 ? Color(1.0, 0.72, 0.10) // NeedsAttention/amber
                                : status == 2 ? Color(0.85, 0.32, 0.32) // Error/red
                                              : Color(0.30, 0.72, 0.35); // Good/green
            item->add_button(COL_BUILD, _status_icon(status_color), BUILD_BUTTON_ID, false,
                              status == 1 ? "Needs attention — click to build" : status == 2 ? "Error — click to build" : "Up to date — click to rebuild");
        }

        item->set_text(COL_NAME, name);
        // The "blue button" look from the reference Unity screenshots —
        // texture-free (this project's Button.icon doesn't render), a
        // plain colored label is what distinguishes "this has a settings
        // page, click it" from a plain reporting-only entry.
        if (bridge->has_settings_panel(name))
            item->set_custom_color(COL_NAME, Color(0.45, 0.65, 1.0));

        int mode = bridge->get_global_subsystem_start_mode(i);
        if (bridge->has_start_mode_setter(name))
        {
            item->set_cell_mode(COL_START_MODE, TreeItem::CELL_MODE_RANGE);
            item->set_text(COL_START_MODE, START_MODE_CHOICES);
            item->set_editable(COL_START_MODE, true);
            item->set_range(COL_START_MODE, mode);
        }
        else
        {
            item->set_text(COL_START_MODE, start_mode_label(mode));
        }

        if (name == reselect_name)
            item->select(COL_NAME);
    }
}

void SubsystemsSettingsTab::_on_item_selected()
{
    TreeItem *selected = tree_->get_selected();
    if (selected == nullptr)
        return;
    _show_panel_for(selected->get_text(COL_NAME));
}

void SubsystemsSettingsTab::_on_item_edited()
{
    TreeItem *item = tree_->get_edited();
    SubsystemManagerBridge *bridge = SubsystemManagerBridge::get_singleton();
    if (item == nullptr || bridge == nullptr || tree_->get_edited_column() != COL_START_MODE)
        return;

    String name = item->get_text(COL_NAME);
    int mode = int(item->get_range(COL_START_MODE));
    bridge->set_global_subsystem_start_mode(name, mode);
    // Changing StartMode only takes effect on the next boot() — this
    // project's Subsystems are constructed/dependency-ordered once at
    // extension-load time (see gameframework::SubsystemManager::boot()'s
    // own doc comment), so an already-Automatic subsystem that's already
    // initialised can't be retroactively un-initialised from here. Same
    // real-world constraint Unity's own ApplyHint concept exists to
    // communicate, just surfaced as a tooltip instead of a separate label.
    item->set_tooltip_text(COL_START_MODE, "Takes effect next time the project loads.");
}

void SubsystemsSettingsTab::_on_button_clicked(Object *item, int column, int id, int mouse_button_index)
{
    (void)mouse_button_index;
    SubsystemManagerBridge *bridge = SubsystemManagerBridge::get_singleton();
    TreeItem *tree_item = Object::cast_to<TreeItem>(item);
    if (bridge == nullptr || tree_item == nullptr || column != COL_BUILD || id != BUILD_BUTTON_ID)
        return;

    bridge->trigger_build(tree_item->get_text(COL_NAME));
    _rebuild_list();
}

Ref<ImageTexture> SubsystemsSettingsTab::_status_icon(const Color &color)
{
    // NOT cached in a static — a static Ref<ImageTexture> here would hold a
    // GPU resource past RenderingServer's own teardown at editor shutdown,
    // crashing on destruction ("Parameter RenderingServer::get_singleton()
    // is null"), exactly the bug already found and fixed in
    // OghamGraphView::_swatch_icon_cache. There are at most a handful of
    // rows redrawn on an explicit refresh/click, not a hot path — recreating
    // a 12x12 texture each time costs nothing worth caching for.
    int size = 12;
    Ref<Image> img = Image::create(size, size, false, Image::FORMAT_RGBA8);
    img->fill(color);
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
