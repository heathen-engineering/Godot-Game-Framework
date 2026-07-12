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

#include "editor/SubsystemsDock.h"

#include "SubsystemManagerBridge.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/tree_item.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

using namespace godot;

void SubsystemsDock::_ready()
{
    _ensure_built();
}

void SubsystemsDock::_ensure_built()
{
    set_name("Subsystems");
    // The bottom panel's own container sizes its tabs via size_flags, not
    // anchors — same fix as every other main-screen/bottom-panel dock in
    // this project (OghamGraphView, GameplayTagsDock).
    set_h_size_flags(Control::SIZE_EXPAND_FILL);
    set_v_size_flags(Control::SIZE_EXPAND_FILL);

    VBoxContainer *root_vbox = memnew(VBoxContainer);
    root_vbox->set_anchors_preset(Control::PRESET_FULL_RECT);
    add_child(root_vbox);

    HBoxContainer *toolbar = memnew(HBoxContainer);
    Button *refresh_btn = memnew(Button);
    refresh_btn->set_text("Refresh");
    refresh_btn->connect("pressed", callable_mp(this, &SubsystemsDock::refresh));
    toolbar->add_child(refresh_btn);
    root_vbox->add_child(toolbar);

    tree_ = memnew(Tree);
    tree_->set_hide_root(true);
    tree_->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    tree_->set_columns(4);
    tree_->set_column_title(0, "Name");
    tree_->set_column_title(1, "Scope");
    tree_->set_column_title(2, "Status");
    tree_->set_column_title(3, "Health");
    tree_->set_column_titles_visible(true);
    tree_->set_column_expand(1, false);
    tree_->set_column_custom_minimum_width(1, 80);
    tree_->set_column_expand(2, false);
    tree_->set_column_custom_minimum_width(2, 90);
    root_vbox->add_child(tree_);

    refresh();
}

void SubsystemsDock::refresh()
{
    SubsystemManagerBridge *bridge = SubsystemManagerBridge::get_singleton();
    if (bridge == nullptr)
        return;

    bridge->boot(); // idempotent — safe to call every refresh, matches SubsystemManager::boot()'s own no-op-if-already-booted contract
    _rebuild();
}

void SubsystemsDock::_rebuild()
{
    SubsystemManagerBridge *bridge = SubsystemManagerBridge::get_singleton();
    if (bridge == nullptr || tree_ == nullptr)
        return;

    tree_->clear();
    TreeItem *root = tree_->create_item();

    int count = bridge->get_global_subsystem_count();
    for (int i = 0; i < count; i++)
    {
        TreeItem *item = tree_->create_item(root);
        item->set_text(0, bridge->get_global_subsystem_name(i));

        int scope = bridge->get_global_subsystem_scope(i);
        item->set_text(1, scope == 0 ? String("Global") : String("World"));

        bool initialised = bridge->get_global_subsystem_initialised(i);
        item->set_text(2, initialised ? String("Running") : String("Stopped"));

        PackedStringArray issues = bridge->get_global_subsystem_health_issues(i);
        if (issues.is_empty())
        {
            item->set_text(3, "OK");
        }
        else
        {
            item->set_text(3, String(", ").join(issues));
            // Texture-free color flag, not an icon — same reasoning as
            // GameplayTagsDock's leaf-color distinction: Button.icon
            // doesn't render in this Godot build (see
            // feedback_godot_button_icon_broken memory), so anything that
            // needs to stand out uses a plain solid-color text draw
            // instead of an icon/texture.
            item->set_custom_color(3, Color(1.0, 0.55, 0.3));
        }
    }
}

void SubsystemsDock::_bind_methods() {}
