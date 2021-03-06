/*
 * ct_actions_tree.cc
 *
 * Copyright 2017-2020 Giuseppe Penone <giuspen@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "ct_actions.h"
#include <gtkmm/dialog.h>
#include <gtkmm/stock.h>
#include "ct_image.h"
#include "ct_dialogs.h"
#include "ct_doc_rw.h"
#include <ctime>

bool CtActions::_is_there_selected_node_or_error()
{
    if (_pCtMainWin->curr_tree_iter()) return true;
    CtDialogs::warning_dialog(_("No Node is Selected"), *_pCtMainWin);
    return false;
}

bool CtActions::_is_tree_not_empty_or_error()
{
    if (!_pCtMainWin->curr_tree_store().get_iter_first()) {
        CtDialogs::error_dialog(_("The Tree is Empty!"), *_pCtMainWin);
        return false;
    }
    return true;
}

bool CtActions::_is_curr_node_not_read_only_or_error()
{
    if (_pCtMainWin->curr_tree_iter().get_node_read_only()) {
        CtDialogs::error_dialog(_("The Selected Node is Read Only"), *_pCtMainWin);
        return false;
    }
    return true;
}

// Returns True if ok (no syntax highlighting) or False and prompts error dialog
bool CtActions::_is_curr_node_not_syntax_highlighting_or_error(bool plain_text_ok /*=false*/)
{
    if (_pCtMainWin->curr_tree_iter().get_node_syntax_highlighting() == CtConst::RICH_TEXT_ID
        || (plain_text_ok && _pCtMainWin->curr_tree_iter().get_node_syntax_highlighting() == CtConst::PLAIN_TEXT_ID))
        return true;
    if (!plain_text_ok)
        CtDialogs::warning_dialog(_("This Feature is Available Only in Rich Text Nodes"), *_pCtMainWin);
    else
        CtDialogs::warning_dialog(_("This Feature is Not Available in Automatic Syntax Highlighting Nodes"), *_pCtMainWin);
    return false;
}

// Returns True if ok (there's a selection) or False and prompts error dialog
bool CtActions::_is_there_text_selection_or_error()
{
    if (!_is_there_selected_node_or_error()) return false;
    if (!_curr_buffer()->get_has_selection())
    {
        CtDialogs::error_dialog(_("No Text is Selected"), *_pCtMainWin);
        return false;
    }
    return true;
}

// Put Selection Upon the achrored widget
void CtActions::object_set_selection(CtAnchoredWidget* widget)
{
    Gtk::TextIter iter_object = _curr_buffer()->get_iter_at_child_anchor(widget->getTextChildAnchor());
    Gtk::TextIter iter_bound = iter_object;
    iter_bound.forward_char();
    if (dynamic_cast<CtImage*>(widget))
        _pCtMainWin->get_text_view().grab_focus();
    _curr_buffer()->select_range(iter_object, iter_bound);
}

// Returns True if there's not a node selected or is not rich text
bool CtActions::_node_sel_and_rich_text()
{
    if (!_is_there_selected_node_or_error()) return false;
    if (!_is_curr_node_not_syntax_highlighting_or_error()) return false;
    return true;
}

void CtActions::_node_add(bool duplicate, bool add_child)
{
    CtNodeData nodeData;
    if (duplicate)
    {
        if (!_is_there_selected_node_or_error()) return;
        _pCtMainWin->curr_tree_store().get_node_data(_pCtMainWin->curr_tree_iter(), nodeData);

        if (nodeData.syntax != CtConst::RICH_TEXT_ID) {
            nodeData.rTextBuffer = _pCtMainWin->get_new_text_buffer(nodeData.syntax, nodeData.rTextBuffer->get_text());
            nodeData.anchoredWidgets.clear();
        } else {
            // todo:
            //state = self.state_machine.requested_state_previous(self.treestore[tree_iter_from][3])
            //self.load_buffer_from_state(state, given_tree_iter=new_node_iter)

            // todo: temporary solution
            nodeData.anchoredWidgets.clear();
            nodeData.rTextBuffer = _pCtMainWin->get_new_text_buffer(nodeData.syntax, nodeData.rTextBuffer->get_text());
        }
    }
    else
    {
        if (add_child && !_is_there_selected_node_or_error()) return;
        std::string title = add_child ? _("New Child Node Properties") : _("New Node Properties");
        nodeData.isBold = false;
        nodeData.customIconId = 0;
        nodeData.syntax = CtConst::RICH_TEXT_ID;
        nodeData.isRO = false;
        if (not CtDialogs::node_prop_dialog(title, _pCtMainWin, nodeData, _pCtMainWin->curr_tree_store().get_used_tags()))
            return;
    }
    _node_add_with_data(_pCtMainWin->curr_tree_iter(), nodeData, add_child);
}

void CtActions::_node_add_with_data(Gtk::TreeIter curr_iter, CtNodeData& nodeData, bool add_child)
{
    if (!nodeData.rTextBuffer)
        nodeData.rTextBuffer = _pCtMainWin->get_new_text_buffer(nodeData.syntax);
    nodeData.tsCreation = std::time(nullptr);
    nodeData.tsLastSave = nodeData.tsCreation;
    nodeData.nodeId = _pCtMainWin->curr_tree_store().node_id_get();

    _pCtMainWin->update_window_save_needed();
    _pCtMainWin->get_ct_config()->syntaxHighlighting = nodeData.syntax;

    Gtk::TreeIter nodeIter;
    if (add_child) {
        nodeIter = _pCtMainWin->curr_tree_store().appendNode(&nodeData, &curr_iter /* as parent */);
    } else if (curr_iter)
        nodeIter = _pCtMainWin->curr_tree_store().insertNode(&nodeData, curr_iter /* after */);
    else
        nodeIter = _pCtMainWin->curr_tree_store().appendNode(&nodeData);

    _pCtMainWin->curr_tree_store().to_ct_tree_iter(nodeIter).pending_new_db_node();
    _pCtMainWin->curr_tree_store().nodes_sequences_fix(curr_iter ? curr_iter->parent() : Gtk::TreeIter(), false);
    _pCtMainWin->curr_tree_store().update_node_aux_icon(nodeIter);
    _pCtMainWin->curr_tree_view().set_cursor_safe(nodeIter);
    _pCtMainWin->get_text_view().grab_focus();
}

void CtActions::_node_child_exist_or_create(Gtk::TreeIter parentIter, const std::string& nodeName)
{
    Gtk::TreeIter childIter = parentIter ? parentIter->children().begin() : _pCtMainWin->curr_tree_store().get_iter_first();
    for (; childIter; ++childIter)
        if (_pCtMainWin->curr_tree_store().to_ct_tree_iter(childIter).get_node_name() == nodeName) {
            _pCtMainWin->curr_tree_view().set_cursor_safe(childIter);
            return;
        }
    CtNodeData nodeData;
    nodeData.name = nodeName;
    nodeData.isBold = false;
    nodeData.customIconId = 0;
    nodeData.syntax = CtConst::RICH_TEXT_ID;
    nodeData.isRO = false;
    _node_add_with_data(parentIter, nodeData, true);
}

// Move a node to a parent and after a sibling
void CtActions::_node_move_after(Gtk::TreeIter iter_to_move, Gtk::TreeIter father_iter,
                                 Gtk::TreeIter brother_iter /*= Gtk::TreeIter()*/, bool set_first /*= false*/)
{
    Gtk::TreeIter new_node_iter;
    if (brother_iter)   new_node_iter = _pCtMainWin->curr_tree_store().get_store()->insert_after(brother_iter);
    else if (set_first) new_node_iter = _pCtMainWin->curr_tree_store().get_store()->prepend(father_iter->children());
    else                new_node_iter = _pCtMainWin->curr_tree_store().get_store()->append(father_iter->children());

    // we move also all the children
    std::function<void(Gtk::TreeIter&,Gtk::TreeIter&)> node_move_data_and_children;
    node_move_data_and_children = [this, &node_move_data_and_children](Gtk::TreeIter& old_iter,Gtk::TreeIter& new_iter) {
        CtNodeData node_data;
        _pCtMainWin->curr_tree_store().get_node_data(old_iter, node_data);
        _pCtMainWin->curr_tree_store().update_node_data(new_iter, node_data);
        for (Gtk::TreeIter child: old_iter->children()) {
            Gtk::TreeIter new_child = _pCtMainWin->curr_tree_store().get_store()->append(new_iter->children());
            node_move_data_and_children(child, new_child);
        }
    };
    node_move_data_and_children(iter_to_move, new_node_iter);

    // now we can remove the old iter (and all children)
    _pCtMainWin->curr_tree_store().get_store()->erase(iter_to_move);
    //todo: self.ctdb_handler.pending_edit_db_node_hier(self.treestore[new_node_iter][3])
    _pCtMainWin->curr_tree_store().nodes_sequences_fix(Gtk::TreeIter(), true);
    if (father_iter)
        _pCtMainWin->curr_tree_view().expand_row(_pCtMainWin->curr_tree_store().get_path(father_iter), false);
    else
        _pCtMainWin->curr_tree_view().expand_row(_pCtMainWin->curr_tree_store().get_path(new_node_iter), false);
    Gtk::TreePath new_node_path = _pCtMainWin->curr_tree_store().get_path(new_node_iter);
    _pCtMainWin->curr_tree_view().collapse_row(new_node_path);
    _pCtMainWin->curr_tree_view().set_cursor(new_node_path);
    _pCtMainWin->update_window_save_needed();
}

bool CtActions::_need_node_swap(Gtk::TreeIter& leftIter, Gtk::TreeIter& rightIter, bool ascending)
{
    Glib::ustring left_node_name = _pCtMainWin->curr_tree_store().to_ct_tree_iter(leftIter).get_node_name().lowercase();
    Glib::ustring right_node_name = _pCtMainWin->curr_tree_store().to_ct_tree_iter(rightIter).get_node_name().lowercase();
    //int cmp = left_node_name.compare(right_node_name);
    int cmp = CtStrUtil::natural_compare(left_node_name, right_node_name);

    return ascending ? cmp > 0 : cmp < 0;
}

bool CtActions::_tree_sort_level_and_sublevels(const Gtk::TreeNodeChildren& children, bool ascending)
{
    auto need_swap = [this,&ascending](Gtk::TreeIter& l, Gtk::TreeIter& r) { return _need_node_swap(l, r, ascending); };
    bool swap_excecuted = CtMiscUtil::node_siblings_sort_iteration(_pCtMainWin->curr_tree_store().get_store(), children, need_swap);
    for (auto& child: children)
        if (_tree_sort_level_and_sublevels(child.children(), ascending))
            swap_excecuted = true;
    return swap_excecuted;
}

void CtActions::node_edit()
{
    if (!_is_there_selected_node_or_error()) return;
    CtNodeData nodeData;
    _pCtMainWin->curr_tree_store().get_node_data(_pCtMainWin->curr_tree_iter(), nodeData);
    CtNodeData newData = nodeData;
    if (not CtDialogs::node_prop_dialog(_("Node Properties"), _pCtMainWin, newData, _pCtMainWin->curr_tree_store().get_used_tags()))
        return;

    _pCtMainWin->get_ct_config()->syntaxHighlighting = newData.syntax;
    if (nodeData.syntax !=  newData.syntax) {
        if (nodeData.syntax == CtConst::RICH_TEXT_ID) {
            // leaving rich text
            if (!CtDialogs::question_dialog(_("Leaving the Node Type Rich Text you will Lose all Formatting for This Node, Do you want to Continue?"), *_pCtMainWin)) {
                return;
            }
            // todo:
            // SWITCH TextBuffer -> SourceBuffer
            //self.switch_buffer_text_source(self.curr_buffer, self.curr_tree_iter, self.syntax_highlighting, self.treestore[self.curr_tree_iter][4])
            //self.curr_buffer = self.treestore[self.curr_tree_iter][2]
            //self.state_machine.delete_states(self.get_node_id_from_tree_iter(self.curr_tree_iter))
        } else if (newData.syntax == CtConst::RICH_TEXT_ID) {
            // going to rich text
            // SWITCH SourceBuffer -> TextBuffer
            //self.switch_buffer_text_source(self.curr_buffer, self.curr_tree_iter, self.syntax_highlighting, self.treestore[self.curr_tree_iter][4])
            //self.curr_buffer = self.treestore[self.curr_tree_iter][2]
        } else if (nodeData.syntax == CtConst::PLAIN_TEXT_ID) {
            // plain text to code
            //self.sourceview.modify_font(pango.FontDescription(self.code_font))
        } else if (newData.syntax == CtConst::PLAIN_TEXT_ID) {
            // code to plain text
            // self.sourceview.modify_font(pango.FontDescription(self.pt_font))
        }
    }
    _pCtMainWin->curr_tree_store().update_node_data(_pCtMainWin->curr_tree_iter(), newData);
    //todo: if self.syntax_highlighting not in [cons.RICH_TEXT_ID, cons.PLAIN_TEXT_ID]:
    //  self.set_sourcebuffer_syntax_highlight(self.curr_buffer, self.syntax_highlighting)
    _pCtMainWin->get_text_view().set_editable(!newData.isRO);
    _pCtMainWin->update_selected_node_statusbar_info();
    _pCtMainWin->curr_tree_store().update_node_aux_icon(_pCtMainWin->curr_tree_iter());
    _pCtMainWin->window_header_update();
    _pCtMainWin->window_header_update_lock_icon(newData.isRO);
    _pCtMainWin->update_window_save_needed(CtSaveNeededUpdType::npro);
    _pCtMainWin->get_text_view().grab_focus();
}

void CtActions::node_toggle_read_only()
{
    if (!_is_there_selected_node_or_error()) return;
    bool node_is_ro = !_pCtMainWin->curr_tree_iter().get_node_read_only();
    _pCtMainWin->curr_tree_iter().set_node_read_only(node_is_ro);
    _pCtMainWin->get_text_view().set_editable(!node_is_ro);
    _pCtMainWin->window_header_update_lock_icon(node_is_ro);
    _pCtMainWin->update_selected_node_statusbar_info();
    _pCtMainWin->curr_tree_store().update_node_aux_icon(_pCtMainWin->curr_tree_iter());
    _pCtMainWin->update_window_save_needed(CtSaveNeededUpdType::npro);
    _pCtMainWin->get_text_view().grab_focus();
}

void CtActions::node_date()
{
    time_t time = std::time(nullptr);

    std::string year = str::time_format("%Y", time);
    std::string month = str::time_format("%B", time);
    std::string day = str::time_format("%d %a", time);

    _node_child_exist_or_create(Gtk::TreeIter(), year);
    _node_child_exist_or_create(_pCtMainWin->curr_tree_iter(), month);
    _node_child_exist_or_create(_pCtMainWin->curr_tree_iter(), day);
}

void CtActions::node_up()
{
    if (!_is_there_selected_node_or_error()) return;
    auto prev_iter = --_pCtMainWin->curr_tree_iter();
    if (!prev_iter) return;
    _pCtMainWin->curr_tree_store().get_store()->iter_swap(_pCtMainWin->curr_tree_iter(), prev_iter);
    //todo: self.nodes_sequences_swap(self.curr_tree_iter, prev_iter)
    //self.ctdb_handler.pending_edit_db_node_hier(self.treestore[self.curr_tree_iter][3])
    //self.ctdb_handler.pending_edit_db_node_hier(self.treestore[prev_iter][3])
    _pCtMainWin->curr_tree_view().set_cursor(_pCtMainWin->curr_tree_store().get_path(_pCtMainWin->curr_tree_iter()));
    _pCtMainWin->update_window_save_needed();
}

void CtActions::node_down()
{
    if (!_is_there_selected_node_or_error()) return;
    auto next_iter = ++_pCtMainWin->curr_tree_iter();
    if (!next_iter) return;
    _pCtMainWin->curr_tree_store().get_store()->iter_swap(_pCtMainWin->curr_tree_iter(), next_iter);
    //todo: self.nodes_sequences_swap(self.curr_tree_iter, subseq_iter)
    //self.ctdb_handler.pending_edit_db_node_hier(self.treestore[self.curr_tree_iter][3])
    //self.ctdb_handler.pending_edit_db_node_hier(self.treestore[subseq_iter][3])
    _pCtMainWin->curr_tree_view().set_cursor(_pCtMainWin->curr_tree_store().get_path(_pCtMainWin->curr_tree_iter()));
    _pCtMainWin->update_window_save_needed();
}

void CtActions::node_right()
{
    if (!_is_there_selected_node_or_error()) return;
    auto prev_iter = --_pCtMainWin->curr_tree_iter();
    if (!prev_iter) return;
    _node_move_after(_pCtMainWin->curr_tree_iter(), prev_iter);
    //todo: if (_pCtMainWin->get_ct_config()->nodesIcons == "c") self.treeview_refresh(change_icon=True)
}

void CtActions::node_left()
{
    if (!_is_there_selected_node_or_error()) return;
    Gtk::TreeIter father_iter = _pCtMainWin->curr_tree_iter()->parent();
    if (!father_iter) return;
    _node_move_after(_pCtMainWin->curr_tree_iter(), father_iter->parent(), father_iter);
    //todo: if (_pCtMainWin->get_ct_config()->nodesIcons == "c") self.treeview_refresh(change_icon=True)
}

void CtActions::node_change_father()
{
    if (!_is_there_selected_node_or_error()) return;
    CtTreeIter old_father_iter = _pCtMainWin->curr_tree_iter().parent();
    CtTreeIter father_iter = _pCtMainWin->curr_tree_store().to_ct_tree_iter(CtDialogs::choose_node_dialog(_pCtMainWin,
                                   _pCtMainWin->curr_tree_view(), _("Select the New Parent"), &_pCtMainWin->curr_tree_store(), _pCtMainWin->curr_tree_iter()));
    if (!father_iter) return;
    gint64 curr_node_id = _pCtMainWin->curr_tree_iter().get_node_id();
    gint64 old_father_node_id = old_father_iter.get_node_id();
    gint64 new_father_node_id = father_iter.get_node_id();
    if (curr_node_id == new_father_node_id) {
        CtDialogs::error_dialog(_("The new parent can't be the very node to move!"), *_pCtMainWin);
        return;
    }
    if (old_father_node_id != -1 && new_father_node_id == old_father_node_id) {
        CtDialogs::info_dialog(_("The new chosen parent is still the old parent!"), *_pCtMainWin);
        return;
    }
    for (CtTreeIter move_towards_top_iter = father_iter.parent(); move_towards_top_iter; move_towards_top_iter = move_towards_top_iter.parent())
        if (move_towards_top_iter.get_node_id() == curr_node_id) {
            CtDialogs::error_dialog(_("The new parent can't be one of his children!"), *_pCtMainWin);
            return;
        }

    _node_move_after(_pCtMainWin->curr_tree_iter(), father_iter);
    //todo: if self.nodes_icons == "c": self.treeview_refresh(change_icon=True)
}

//"""Sorts the Tree Ascending"""
void CtActions::tree_sort_ascending()
{
    if (_tree_sort_level_and_sublevels(_pCtMainWin->curr_tree_store().get_store()->children(), true)) {
        _pCtMainWin->curr_tree_store().nodes_sequences_fix(Gtk::TreeIter(), true);
        _pCtMainWin->update_window_save_needed();
    }
}

//"""Sorts the Tree Ascending"""
void CtActions::tree_sort_descending()
{
    if (_tree_sort_level_and_sublevels(_pCtMainWin->curr_tree_store().get_store()->children(), false)) {
        _pCtMainWin->curr_tree_store().nodes_sequences_fix(Gtk::TreeIter(), true);
        _pCtMainWin->update_window_save_needed();
    }
}

//"""Sorts all the Siblings of the Selected Node Ascending"""
void CtActions::node_siblings_sort_ascending()
{
    if (!_is_there_selected_node_or_error()) return;
    Gtk::TreeIter father_iter = _pCtMainWin->curr_tree_iter()->parent();
    const Gtk::TreeNodeChildren& children = father_iter ? father_iter->children() : _pCtMainWin->curr_tree_store().get_store()->children();
    auto need_swap = [this](Gtk::TreeIter& l, Gtk::TreeIter& r) { return _need_node_swap(l, r, true); };
    if (CtMiscUtil::node_siblings_sort_iteration(_pCtMainWin->curr_tree_store().get_store(), children, need_swap)) {
        _pCtMainWin->curr_tree_store().nodes_sequences_fix(father_iter, true);
        _pCtMainWin->update_window_save_needed();
    }
}

//"""Sorts all the Siblings of the Selected Node Descending"""
void CtActions::node_siblings_sort_descending()
{
    if (!_is_there_selected_node_or_error()) return;
    Gtk::TreeIter father_iter = _pCtMainWin->curr_tree_iter()->parent();
    const Gtk::TreeNodeChildren& children = father_iter ? father_iter->children() : _pCtMainWin->curr_tree_store().get_store()->children();
    auto need_swap = [this](Gtk::TreeIter& l, Gtk::TreeIter& r) { return _need_node_swap(l, r, false); };
    if (CtMiscUtil::node_siblings_sort_iteration(_pCtMainWin->curr_tree_store().get_store(), children, need_swap)) {
        _pCtMainWin->curr_tree_store().nodes_sequences_fix(father_iter, true);
        _pCtMainWin->update_window_save_needed();
    }
}

void CtActions::bookmark_curr_node()
{
    if (!_is_there_selected_node_or_error()) return;
    gint64 node_id = _pCtMainWin->curr_tree_iter().get_node_id();

    if (_pCtMainWin->curr_tree_store().onRequestAddBookmark(node_id)) {
        _pCtMainWin->set_bookmarks_menu_items();
        _pCtMainWin->curr_tree_store().update_node_aux_icon(_pCtMainWin->curr_tree_iter());
        _pCtMainWin->update_window_save_needed(CtSaveNeededUpdType::book);
        _pCtMainWin->menu_tree_update_for_bookmarked_node(true);
    }
}

void CtActions::bookmark_curr_node_remove()
{
    if (!_is_there_selected_node_or_error()) return;
    gint64 node_id = _pCtMainWin->curr_tree_iter().get_node_id();

    if (_pCtMainWin->curr_tree_store().onRequestRemoveBookmark(node_id)) {
        _pCtMainWin->set_bookmarks_menu_items();
        _pCtMainWin->curr_tree_store().update_node_aux_icon(_pCtMainWin->curr_tree_iter());
        _pCtMainWin->update_window_save_needed(CtSaveNeededUpdType::book);
        _pCtMainWin->menu_tree_update_for_bookmarked_node(false);
    }

}

void CtActions::bookmarks_handle()
{
    CtDialogs::bookmarks_handle_dialog(_pCtMainWin);
}
