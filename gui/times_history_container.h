/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_TIMES_HISTORY_CONTAINER_H
#define GUI_TIMES_HISTORY_CONTAINER_H


#include "components/gui_component.h"
#include "gui_frame.h"
#include "components/gui_label.h"
#include "times_history_entry.h"

// @author: suitougreentea
class times_history_container_t :
	public gui_container_t,
	public action_listener_t
{
private:
	const player_t *owner;
	schedule_t *schedule;
	times_history_map *map;

	bool mirrored;
	bool reversed;

	gui_label_t lbl_order;
	gui_label_t lbl_name_header;
	gui_label_t lbl_time_header;
	gui_label_t lbl_average_header;

	slist_tpl<button_t *> buttons;
	slist_tpl<gui_label_t *> name_labels;
	slist_tpl<times_history_entry_t *> entry_labels;

	slist_tpl<uint8> *last_schedule_indices;

	static class karte_ptr_t welt;

	// Create list of entries displayed.
	// This depends on mirror/reverse states of line/convoi
	void construct_data(slist_tpl<uint8> *schedule_indices, slist_tpl<departure_point_t *> *time_keys);

	bool updated_schedule_indices(slist_tpl<uint8> *schedule_indices);

	void delete_buttons();
	void delete_labels();

public:
	times_history_container_t() {}
	times_history_container_t(const player_t *owner, schedule_t *schedule, times_history_map *map, bool mirrored, bool reversed);
	~times_history_container_t();

	void update_container();

	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;

	void draw(scr_coord offset) OVERRIDE;

	inline void set_schedule(schedule_t *s) { schedule = s; }
};


#include "../linehandle_t.h"

class gui_times_history_t : public gui_aligned_container_t
{
	//const player_t *owner;
	player_t* player;

	// if it is a history about convoi, line is null
	linehandle_t line;
	// if it is a history about line, convoi is null
	convoihandle_t convoy;

	times_history_map *map;

	bool mirrored;
	bool reversed = false;

	schedule_t *schedule;

	// for update
	uint8 old_current_stop = -1;
	uint32 update_time;

	void init();

	// Create list of entries displayed.
	// This depends on mirror/reverse states of line/convoy
	void construct_data(slist_tpl<uint8> *schedule_indices, slist_tpl<departure_point_t *> *time_keys);

	void build_table();

public:
	gui_times_history_t(linehandle_t line, convoihandle_t convoi, bool line_reversed_display = false);

	// for reload from the save
	void set_line(linehandle_t line_) { line = line_; init(); }
	void set_convoy(convoihandle_t convoi) { convoy = convoi; init(); };

	void draw(scr_coord offset) OVERRIDE;
};

#endif
