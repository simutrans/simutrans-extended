/*
 * Copyright (c) 1997 - 2001 Hansjörg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#ifndef gui_halt_detail_h
#define gui_halt_detail_h

#include "components/gui_textarea.h"
#include "components/gui_tab_panel.h"
#include "components/gui_container.h"
#include "gui_frame.h"
#include "components/gui_scrollpane.h"
#include "components/gui_label.h"
#include "components/action_listener.h"

#include "../halthandle_t.h"
#include "../utils/cbuffer_t.h"
#include "../gui/simwin.h"

#include "../simfab.h"
#include "components/gui_button.h"

class player_t;

// tab1 @Ranran May, 2019
class halt_detail_pas_t : public gui_container_t
{
private:
	halthandle_t halt;
	karte_t *welt;

	cbuffer_t pas_info;

public:
	halt_detail_pas_t(halthandle_t halt);

	void set_halt(halthandle_t h) { halt = h; }

	void draw(scr_coord offset);

	// draw pax or mail classes waiting amount information table
	void draw_class_table(scr_coord offset, const uint8 class_name_cell_width, const goods_desc_t *warentyp);
};


// tab2 @Ranran
class halt_detail_goods_t : public gui_world_component_t
{
private:
	halthandle_t halt;
	karte_t *welt;

	//slist_tpl<button_t *>posbuttons;
	//button_t *pb;

	//slist_tpl<char*> label_names;
	//gui_container_t cont;
	const slist_tpl<fabrik_t*> fab_list;
	uint32 line_selected;


	cbuffer_t goods_info;

public:
	halt_detail_goods_t(halthandle_t halt);


	void set_halt(halthandle_t h) { halt = h; }

	void draw(scr_coord offset);
	//bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;

	bool infowin_event(const event_t * ev) OVERRIDE;

	//~halt_detail_goods_t();
};


// tab3
class halt_detail_line_t : public gui_container_t, action_listener_t
{
private:
	halthandle_t halt;
	karte_t *welt;

	player_t *cached_active_player; // So that, if different from current, change line links
	uint32 cached_line_count;
	uint32 cached_convoy_count;

	slist_tpl<button_t *>posbuttons;
	//slist_tpl<gui_label_t *>linelabels;
	slist_tpl<button_t *>linebuttons;
	//slist_tpl<gui_label_t *> convoylabels;
	slist_tpl<button_t *> convoybuttons;
	//slist_tpl<char*> label_names;

	gui_container_t cont;

	uint32 line_selected;

	cbuffer_t line_info;

public:
	halt_detail_line_t(halthandle_t halt);

	~halt_detail_line_t();

	void set_halt(halthandle_t h) { halt = h; }

	void draw(scr_coord offset);

	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;
};

/**
 * Window with destination information for a stop
 * @author Hj. Malthaner
 */

class halt_detail_t : public gui_frame_t, action_listener_t
{
private:
	halthandle_t halt;
	player_t *cached_active_player; // So that, if different from current, change line links
	uint32 cached_line_count;
	uint32 cached_convoy_count;
	uint32 update_time;

	cbuffer_t buf;

	gui_scrollpane_t scrolly, scrolly_goods, scrolly_lines;
	//gui_textarea_t txt_info;
	halt_detail_pas_t pas;
	halt_detail_goods_t goods;
	halt_detail_line_t lines;
	gui_tab_panel_t tabs;

	//gui_container_t cont;
	//slist_tpl<button_t *>posbuttons;

public:
	halt_detail_t(halthandle_t halt);

	~halt_detail_t();

	// only defined to update schedule, if changed
	void draw( scr_coord pos, scr_size size );

	//void halt_detail_info();

	/**
	 * Set the window associated helptext
	 * @return the filename for the helptext, or NULL
	 * @author Hj. Malthaner
	 */
	const char * get_help_filename() const { return "station_details.txt"; }

	// Set window size and adjust component sizes and/or positions accordingly
	virtual void set_windowsize(scr_size size);

	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;


	// this constructor is only used during loading
	halt_detail_t();

	void rdwr( loadsave_t *file );

	uint32 get_rdwr_id() { return magic_halt_detail; }
};


#endif
