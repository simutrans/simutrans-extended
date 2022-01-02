/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "../simworld.h"
#include "../display/simimg.h"
#include "../display/viewport.h"
#include "../simware.h"
#include "../simfab.h"
#include "../simhalt.h"
#include "../simconvoi.h"

#include "../descriptor/goods_desc.h"
#include "../bauer/goods_manager.h"

#include "../dataobj/environment.h"
#include "../dataobj/translator.h"
#include "../dataobj/loadsave.h"
#include "../dataobj/koord.h"

#include "../utils/simstring.h"

#include "halt_detail.h"
#include "components/gui_label.h"


#define GOODS_SYMBOL_CELL_WIDTH 14
#define GOODS_WAITING_CELL_WIDTH 64
#define GOODS_WAITING_BAR_BASE_WIDTH 128
#define GOODS_LEAVING_BAR_HEIGHT 2
#define BARCOL_TRANSFER_IN color_idx_to_rgb(10)
#define MAX_CATEGORY_COLS 16

sint16 halt_detail_t::tabstate = -1;

halt_detail_t::halt_detail_t(halthandle_t halt_) :
	gui_frame_t(""),
	halt(halt_),
	line_number(halt_),
	pas(halt_),
	goods(halt_),
	cont_service(halt_),
	scrolly_pas(&pas),
	scrolly_goods(&cont_goods),
	scrolly_service(&cont_service),
	scrolly_route(&cont_desinations),
	nearby_factory(halt_),
	destinations(halt_, selected_route_catg_index)
{
	if (halt.is_bound()) {
		init();
	}
}


#define WARE_TYPE_IMG_BUTTON_WIDTH (D_BUTTON_HEIGHT * 3)
#define GOODS_TEXT_BUTTON_WIDTH (80)
#define CLASS_TEXT_BUTTON_WIDTH ((D_DEFAULT_WIDTH - D_MARGINS_X - WARE_TYPE_IMG_BUTTON_WIDTH)/5)
#define CATG_IMG_BUTTON_WIDTH (D_BUTTON_HEIGHT * 2)
void halt_detail_t::init()
{
	gui_frame_t::set_name(halt->get_name());
	gui_frame_t::set_owner(halt->get_owner());

	set_table_layout(1, 0);
	set_margin(scr_size(D_MARGIN_LEFT, 0), scr_size(D_MARGIN_RIGHT, 0));
	set_spacing(scr_size(0, D_V_SPACE));

	add_component(&line_number);
	waiting_bar = new_component<gui_halt_waiting_indicator_t>(halt);

	tabs.set_pos(scr_coord(0, LINESPACE*4 + D_MARGIN_TOP + D_V_SPACE));
	set_min_windowsize(scr_size(D_DEFAULT_WIDTH, D_TITLEBAR_HEIGHT + tabs.get_pos().y + D_TAB_HEADER_HEIGHT));

	old_factory_count = 0;
	old_catg_count = 0;
	cached_active_player=NULL;
	cached_size_y = 0;
	show_pas_info = false;
	show_freight_info = false;

	lb_nearby_factory.init("Fabrikanschluss"/* (en)Connected industries */, scr_coord(D_MARGIN_LEFT, 0),
		color_idx_to_rgb(halt->get_owner()->get_player_color1()), color_idx_to_rgb(halt->get_owner()->get_player_color1()+2), 1);

	// fill buffer with halt detail
	goods.recalc_size();
	nearby_factory.recalc_size();
	update_components();

	tabs.add_listener(this);
	add_component(&tabs);

	cont_goods.set_pos(scr_coord(0, 0));
	cont_goods.add_component(&goods);
	cont_goods.add_component(&lb_nearby_factory);
	cont_goods.add_component(&nearby_factory);

	// route tab components
	cont_route.set_table_layout(1,0);
	cont_route.add_table(4,1)->set_spacing(scr_size(0,0));
	{
		cont_route.set_margin(scr_size(D_H_SPACE, D_V_SPACE), scr_size(0, 0));

		bt_by_station.init(button_t::roundbox_left_state, "hd_btn_by_station", scr_coord(0, 0), D_WIDE_BUTTON_SIZE);
		bt_by_category.init(button_t::roundbox_right_state, "hd_btn_by_category", scr_coord(0, 0), D_WIDE_BUTTON_SIZE);
		bt_by_station.add_listener(this);
		bt_by_category.add_listener(this);
		bt_by_station.pressed = false;
		bt_by_category.pressed = true;
		cont_route.new_component<gui_fill_t>();
		cont_route.add_component(&bt_by_station);
		cont_route.add_component(&bt_by_category);
		cont_route.new_component<gui_margin_t>(D_MARGIN_RIGHT);
	}
	cont_route.end_table();
	lb_serve_catg.init("lb_served_goods_and_classes", scr_coord(0, 0),
		color_idx_to_rgb(halt->get_owner()->get_player_color1()), color_idx_to_rgb(halt->get_owner()->get_player_color1()+2), 1);
	cont_route.add_component(&lb_serve_catg);



	// goods button table
	cont_route.add_table(2,0);
	uint8 classes;
	// * passenger
	// ** catg button
	button_t *bp = new button_t();
	bp->init(button_t::roundbox_state, NULL);
	bp->set_image(goods_manager_t::get_info_catg_index(goods_manager_t::INDEX_PAS)->get_catg_symbol());
	bp->set_tooltip(translator::translate(goods_manager_t::get_info_catg_index(goods_manager_t::INDEX_PAS)->get_catg_name()));
	bp->add_listener(this);
	catg_buttons.append(bp);
	cont_route.add_component(bp);
	// *** class buttons
	classes = goods_manager_t::passengers->get_number_of_classes();
	// Button is not required if there is only one class
	if (classes > 0) {
		cont_route.add_table(classes, 1)->set_spacing(scr_size(0, 0));
		{
			for (uint8 c = 0; c < classes; c++) {
				button_t *cb = new button_t();
				cb->init(button_t::roundbox_state, goods_manager_t::get_translated_wealth_name(goods_manager_t::INDEX_PAS, c), scr_coord(0, 0), scr_size(CLASS_TEXT_BUTTON_WIDTH, D_BUTTON_HEIGHT));
				if (classes>1) {
					if (c == 0) { cb->set_typ(button_t::roundbox_left_state);  }
					else if(c == classes-1) { cb->set_typ(button_t::roundbox_right_state); }
					else { cb->set_typ(button_t::roundbox_middle_state); }
				}
				cb->disable();
				cb->add_listener(this);
				cont_route.add_component(cb);
				pas_class_buttons.append(cb);
			}
		}
		cont_route.end_table();
	}
	else {
		cont_route.new_component<gui_empty_t>();
	}

	// * mail
	// ** catg button
	button_t *bm = new button_t();
	bm->init(button_t::roundbox_state, NULL);
	bm->set_image(goods_manager_t::get_info_catg_index(goods_manager_t::INDEX_MAIL)->get_catg_symbol());
	bm->set_tooltip(translator::translate(goods_manager_t::get_info_catg_index(goods_manager_t::INDEX_MAIL)->get_catg_name()));
	bm->add_listener(this);
	catg_buttons.append(bm);
	cont_route.add_component(bm);
	// *** class buttons
	classes = goods_manager_t::mail->get_number_of_classes();
	// Button is not required if there is only one class
	if (classes > 0) {
		cont_route.add_table(classes, 1)->set_spacing(scr_size(0, 0));
		{
			for (uint8 c = 0; c < classes; c++) {
				button_t *cb = new button_t();
				cb->init(button_t::roundbox_state, goods_manager_t::get_translated_wealth_name(goods_manager_t::INDEX_MAIL, c), scr_coord(0, 0), scr_size(CLASS_TEXT_BUTTON_WIDTH, D_BUTTON_HEIGHT));
				if (classes > 1) {
					if (c == 0) { cb->set_typ(button_t::roundbox_left_state); }
					else if (c == classes - 1) { cb->set_typ(button_t::roundbox_right_state); }
					else { cb->set_typ(button_t::roundbox_middle_state); }
				}
				cb->disable();
				cb->add_listener(this);
				cont_route.add_component(cb);
				mail_class_buttons.append(cb);
			}
		}
		cont_route.end_table();
	}
	else {
		cont_route.new_component<gui_empty_t>();
	}

	// *goods
	// ** freight group image
	cont_route.new_component<gui_empty_t>();
	// *** catg buttons
	cont_route.add_table(8, 0)->set_spacing(scr_size(0, 0));
	{
		uint8 cnt = 0;
		for (uint8 i = 0; i < goods_manager_t::get_max_catg_index(); i++) {
			if (goods_manager_t::get_info_catg_index(i) == goods_manager_t::none
				|| i == goods_manager_t::INDEX_PAS || i == goods_manager_t::INDEX_MAIL) {
				continue;
			}
			// make goods category buttons
			button_t *b = new button_t();
			if (goods_manager_t::get_info_catg_index(i)->get_catg_symbol() == IMG_EMPTY || goods_manager_t::get_info_catg_index(i)->get_catg_symbol() == skinverwaltung_t::goods->get_image_id(0)) {
				// category text button (pakset does not have symbol)
				b->init(button_t::roundbox_state, goods_manager_t::get_info_catg_index(i)->get_catg_name(), scr_coord(0,0), scr_size(GOODS_TEXT_BUTTON_WIDTH, D_BUTTON_HEIGHT));
			}
			else {
				// category symbol button
				b->init(button_t::roundbox_state, NULL, scr_coord(0,0), scr_size(CATG_IMG_BUTTON_WIDTH, D_BUTTON_HEIGHT));
				b->set_image(goods_manager_t::get_info_catg_index(i)->get_catg_symbol());
			}
			b->set_tooltip(translator::translate(goods_manager_t::get_info_catg_index(i)->get_catg_name()));
			b->add_listener(this);
			catg_buttons.append(b);
			cont_route.add_component(b);
			cnt++;
		}

		if (!cnt) {
			cont_route.new_component<gui_empty_t>();
		}
	}
	cont_route.end_table();

	cont_route.end_table(); // button table end

	lb_routes.init("Direkt erreichbare Haltestellen", scr_coord(0, 0),
		color_idx_to_rgb(halt->get_owner()->get_player_color1()), color_idx_to_rgb(halt->get_owner()->get_player_color1()+2), 1);
	cont_route.add_component(&lb_routes);
	cont_route.add_component(&scrolly_route);

	// list (inside the scroll panel)
	lb_selected_route_catg.set_pos(scr_coord(0, D_V_SPACE));
	lb_selected_route_catg.set_size(D_BUTTON_SIZE);
	destinations.set_pos(scr_coord(0, lb_selected_route_catg.get_pos().y + D_BUTTON_HEIGHT));
	destinations.recalc_size();
	cont_desinations.add_component(&lb_selected_route_catg);
	cont_desinations.add_component(&destinations);


	// Adjust window to optimal size
	set_tab_opened();

	tabs.set_size(get_client_windowsize() - tabs.get_pos() - scr_size(0, 1));

	set_resizemode(diagonal_resize);
	resize(scr_coord(0,0));
}

halt_detail_t::~halt_detail_t()
{
	while (!catg_buttons.empty()) {
		button_t *b = catg_buttons.remove_first();
		cont_desinations.remove_component(b);
		delete b;
	}
	while (!pas_class_buttons.empty()) {
		button_t *b = pas_class_buttons.remove_first();
		cont_desinations.remove_component(b);
		delete b;
	}
	while (!mail_class_buttons.empty()) {
		button_t *b = mail_class_buttons.remove_first();
		cont_desinations.remove_component(b);
		delete b;
	}
}


koord3d halt_detail_t::get_weltpos(bool)
{
	return halt->get_basis_pos3d();
}


bool halt_detail_t::is_weltpos()
{
	return (welt->get_viewport()->is_on_center(get_weltpos(false)));
}

// update all buffers
void halt_detail_t::update_components()
{
	waiting_bar->update();
	bool reset_tab = false;
	int old_tab = tabs.get_active_tab_index();
	if (!show_pas_info && (halt->get_pax_enabled() || halt->get_mail_enabled())) {
		show_pas_info = true;
		reset_tab = true;
		old_tab++;
	}
	if (!show_freight_info && halt->get_ware_enabled())
	{
		show_freight_info = true;
		reset_tab = true;
		old_tab++;
	}
	if (reset_tab) {
		tabs.clear();
		if (show_pas_info) {
			scrolly_pas.set_size(pas.get_size());
			tabs.add_tab(&scrolly_pas, translator::translate("Passengers/mail"));
		}
		if (show_freight_info) {
			tabs.add_tab(&scrolly_goods, translator::translate("Freight"));
		}
		tabs.add_tab(&scrolly_service, translator::translate("Services"));
		tabs.add_tab(&cont_route, translator::translate("Route_tab"));
		if (tabstate != -1) {
			tabs.set_active_tab_index(old_tab);
			tabstate = old_tab;
		}
		else {
			tabs.set_active_tab_index(0);
			tabstate = 0;
		}
	}

	// tab3
	if (halt.is_bound()) {
		if (cached_active_player != welt->get_active_player() || halt->registered_lines.get_count() != cached_line_count ||
			halt->registered_convoys.get_count() != cached_convoy_count ||
			welt->get_ticks() - update_time > 10000) {
			// fill buffer with halt detail
			cached_active_player = welt->get_active_player();
			update_time = (uint32)welt->get_ticks();
		}
	}

	// tab1
	if (cached_size_y != pas.get_size().h) {
		cached_size_y = pas.get_size().h;
		if (tabstate==0) {
			set_tab_opened();
		}
	}

	// tab2
	scr_coord_val y = 0; // calc for layout
	if (goods.active_freight_catg != old_catg_count)
	{
		goods.recalc_size();
		old_catg_count = goods.active_freight_catg;
	}
	y += goods.get_size().h;

	lb_nearby_factory.set_pos(scr_coord(D_H_SPACE, y));
	y += D_HEADING_HEIGHT + D_V_SPACE*2;
	nearby_factory.set_pos(scr_coord(0, y));
	if (halt->get_fab_list().get_count() != old_factory_count)
	{
		nearby_factory.recalc_size();
		old_factory_count = halt->get_fab_list().get_count();
	}
	y += nearby_factory.get_size().h;

	if (y != cont_goods.get_size().h) {
		cont_goods.set_size(scr_size(400, y));
	}

	// tab4
	// catg buttons
	if (!list_by_station) {
		bool chk_pressed = (selected_route_catg_index == goods_manager_t::INDEX_NONE) ? false : true;
		uint8 i = 0;
		FORX(slist_tpl<button_t *>, btn, catg_buttons, i++) {
			uint8 catg_index = i >= goods_manager_t::INDEX_NONE ? i + 1 : i;
			btn->disable();
			if (halt->is_enabled(catg_index)) {
				// check station handled goods category
				if (catg_index == goods_manager_t::INDEX_PAS || catg_index == goods_manager_t::INDEX_MAIL)
				{
					btn->enable();
					bool all_class_btn_pressed = false;
					if (!chk_pressed) {
						// Initialization
						btn->pressed = true;
						chk_pressed = true;
						selected_route_catg_index = catg_index;
						// Set the most higher wealth class by default. Because the higher class can choose the route of all classes
						selected_class = goods_manager_t::get_info_catg_index(catg_index)->get_number_of_classes() - 1;
						lb_selected_route_catg.set_text(goods_manager_t::get_info_catg_index(selected_route_catg_index)->get_catg_name());
						all_class_btn_pressed = true;
						destinations.build_halt_list(selected_route_catg_index, selected_class, list_by_station);
					}
					else if (selected_route_catg_index != catg_index) {
						btn->pressed = false;
					}
					uint8 cl = 0;
					FORX(slist_tpl<button_t *>, cl_btn, catg_index == goods_manager_t::INDEX_PAS ? pas_class_buttons : mail_class_buttons, cl++) {
						cl_btn->pressed = selected_class >= cl;
						if (btn->enabled() == false) {
							cl_btn->disable();
							continue;
						}
						haltestelle_t::connexions_map *connexions = halt->get_connexions(catg_index, cl);
						if (!connexions->empty())
						{
							cl_btn->enable();
						}
						if (btn->pressed == false) {
							cl_btn->pressed = false;
						}
					}
				}
				else {
					uint8 g_class = goods_manager_t::get_classes_catg_index(catg_index) - 1;
					haltestelle_t::connexions_map *connexions = halt->get_connexions(catg_index, g_class);
					if (!connexions->empty())
					{
						btn->enable();
						if (!chk_pressed) {
							btn->pressed = true;
							chk_pressed = true;
							selected_route_catg_index = catg_index;
							lb_selected_route_catg.set_text(goods_manager_t::get_info_catg_index(selected_route_catg_index)->get_catg_name());
							destinations.build_halt_list(selected_route_catg_index, selected_class, list_by_station);
						}
						else if (selected_route_catg_index != catg_index) {
							btn->pressed = false;
						}
					}
				}
			}
			else if (catg_index == goods_manager_t::INDEX_PAS || catg_index == goods_manager_t::INDEX_MAIL) {
				selected_class = 0;
				uint8 cl = goods_manager_t::get_info_catg_index(catg_index)->get_number_of_classes() - 1;
				FORX(slist_tpl<button_t *>, cl_btn, catg_index == goods_manager_t::INDEX_PAS ? pas_class_buttons : mail_class_buttons, cl--) {
					cl_btn->pressed = false;
					cl_btn->disable();
				}
			}
		}
	}
	destinations.recalc_size();
	cont_desinations.set_size(scr_size(max(get_windowsize().w, destinations.get_size().w), destinations.get_pos().y + destinations.get_size().h));

	resize(scr_coord(0, 0));

	set_dirty();
}



bool halt_detail_t::action_triggered( gui_action_creator_t *comp, value_t /*extra*/)
{
	if (tabstate != tabs.get_active_tab_index() || get_windowsize().h == get_min_windowsize().h) {
		set_tab_opened();
	}
	else if (comp == &bt_by_category) {
		if (list_by_station) {
			list_by_station = false;
			destinations.build_halt_list(selected_route_catg_index, selected_class, list_by_station);
			open_close_catg_buttons();
		}
	}
	else if (comp == &bt_by_station) {
		if (!list_by_station) {
			list_by_station = true;
			destinations.build_halt_list(goods_manager_t::INDEX_NONE, 0, list_by_station);
			open_close_catg_buttons();
		}
	}
	else if(comp != &tabs){
		uint8 i = 0;
		uint8 cl = 0;
		FORX(slist_tpl<button_t *>, btn, catg_buttons,i++) {
			bool flag_all_classes_btn_on = false;
			bool flag_upper_classes_btn_off = false;
			uint8 catg_index = i >= goods_manager_t::INDEX_NONE ? i + 1 : i;
			if (comp == btn) {
				selected_class = 0;
				if ((catg_index == goods_manager_t::INDEX_PAS || catg_index == goods_manager_t::INDEX_MAIL)) {
					flag_all_classes_btn_on = true;
					// Set the most higher wealth class by default. Because the higher class can choose the route of all classes
					selected_class = goods_manager_t::get_info_catg_index(catg_index)->get_number_of_classes() - 1;
				}
				btn->pressed = true; // Don't release when press the same button
				selected_route_catg_index = catg_index;
				lb_selected_route_catg.set_text(goods_manager_t::get_info_catg_index(catg_index)->get_catg_name());
				destinations.build_halt_list(selected_route_catg_index, selected_class, list_by_station);
				destinations.recalc_size();
			}
			if (catg_index == goods_manager_t::INDEX_PAS || catg_index == goods_manager_t::INDEX_MAIL)
			{
				cl = 0;
				FORX(slist_tpl<button_t *>, cl_btn, catg_index == goods_manager_t::INDEX_PAS ? pas_class_buttons : mail_class_buttons, cl++) {
					if (flag_all_classes_btn_on) {
						cl_btn->pressed = true;
						continue;
					}
					if (comp == cl_btn) {
						flag_upper_classes_btn_off = true;
						selected_class = cl;
						if (btn->pressed == false) {
							btn->pressed = true;
							selected_route_catg_index = catg_index;
							cl_btn->pressed = true;
						}
						else if(flag_upper_classes_btn_off) {
							cl_btn->pressed = false;
						}
						lb_selected_route_catg.set_text(goods_manager_t::get_info_catg_index(catg_index)->get_catg_name());
						destinations.build_halt_list(selected_route_catg_index, selected_class, list_by_station);
						destinations.recalc_size();
					}
					else if(btn->pressed == false){
						cl_btn->pressed = false;
					}
					else{
						cl_btn->pressed = flag_upper_classes_btn_off;
					}
				}
			}
		}
		update_components();
	}
	return true;
}


bool halt_detail_t::infowin_event(const event_t *ev)
{
	if (ev->ev_class == EVENT_KEYBOARD && ev->ev_code == SIM_KEY_DOWN) {
		set_tab_opened();
	}
	if (ev->ev_class == EVENT_KEYBOARD && ev->ev_code == SIM_KEY_UP) {
		set_windowsize(get_min_windowsize());
	}
	return gui_frame_t::infowin_event(ev);
}


void halt_detail_t::open_close_catg_buttons()
{
	bt_by_category.pressed = !list_by_station;
	bt_by_station.pressed = list_by_station;
	// Show or hide buttons and labels
	lb_serve_catg.set_visible(!list_by_station);
	lb_selected_route_catg.set_visible(!list_by_station);
	uint8 i = 0;
	uint8 cl = 0;
	FORX(slist_tpl<button_t *>, btn, catg_buttons, i++) {
		btn->set_visible(!list_by_station);
		uint8 catg_index = i >= goods_manager_t::INDEX_NONE ? i + 1 : i;
		if (catg_index == goods_manager_t::INDEX_PAS || catg_index == goods_manager_t::INDEX_MAIL)
		{
			cl = goods_manager_t::get_info_catg_index(i)->get_number_of_classes() - 1;
			FORX(slist_tpl<button_t *>, cl_btn, catg_index == goods_manager_t::INDEX_PAS ? pas_class_buttons : mail_class_buttons, cl--) {
				cl_btn->set_visible(!list_by_station);
			}
		}
	}
	// Shift the list position
	if (list_by_station) {
		destinations.set_pos(scr_coord(0, 0));
	}
	else {
		destinations.set_pos(scr_coord(0, D_BUTTON_HEIGHT+D_V_SPACE));
	}
	cont_route.set_size(scr_size(cont_route.get_size().w, scrolly_route.get_pos().y + scrolly_route.get_size().h));
	cont_desinations.set_size(scr_size(max(get_windowsize().w, destinations.get_size().w), destinations.get_pos().y + destinations.get_size().h));
}

void halt_detail_t::set_tab_opened()
{
	update_components();
	tabstate = tabs.get_active_tab_index();
	uint selected_tab = tabstate;
	if (!show_pas_info) {
		selected_tab++;
	}
	else if (!show_freight_info && selected_tab) {
		selected_tab++;
	}

	int margin_above_tab = tabs.get_pos().y + D_TAB_HEADER_HEIGHT + D_MARGINS_Y + D_V_SPACE;
	switch (selected_tab)
	{
		case 0: // pas
		default:
			set_windowsize(scr_size(get_windowsize().w, min(display_get_height() - margin_above_tab, margin_above_tab + cached_size_y)));
			break;
		case 1: // goods
			set_windowsize(scr_size(get_windowsize().w, min(display_get_height() - margin_above_tab, margin_above_tab + cont_goods.get_size().h)));
			break;
		case 2: // info
			set_windowsize(scr_size(get_windowsize().w, min(display_get_height() - margin_above_tab, margin_above_tab + cont_service.get_size().h)));
			break;
		case 3: // route
			destinations.recalc_size();
			cont_desinations.set_size(scr_size(max(get_windowsize().w, destinations.get_size().w), destinations.get_pos().y + destinations.get_size().h));
			cont_route.set_size(scr_size(cont_route.get_size().w, scrolly_route.get_pos().y + scrolly_route.get_size().h));
			set_windowsize(scr_size(get_windowsize().w, min(display_get_height() - margin_above_tab, margin_above_tab + cont_route.get_size().h)));
			break;
	}
}


void halt_detail_t::draw(scr_coord pos, scr_size size)
{
	update_components();
	gui_frame_t::draw( pos, size );
}



void halt_detail_t::set_windowsize(scr_size size)
{
	gui_frame_t::set_windowsize(size);
	tabs.set_size(get_client_windowsize() - tabs.get_pos() - scr_size(0, 1));
}



void halt_detail_t::rdwr(loadsave_t *file)
{
	koord3d halt_pos;
	scr_size size = get_windowsize();
	if(  file->is_saving()  ) {
		halt_pos = halt->get_basis_pos3d();
	}
	halt_pos.rdwr( file );
	size.rdwr( file );
	uint8 selected_tab = tabs.get_active_tab_index();
	if( file->is_version_ex_atleast(14,41) ) {
		file->rdwr_byte( selected_tab );
	}
	if(  file->is_loading()  ) {
		halt = welt->lookup( halt_pos )->get_halt();
		// now we can open the window ...
		scr_coord const& pos = win_get_pos(this);
		halt_detail_t *w = new halt_detail_t(halt);
		create_win(pos.x, pos.y, w, w_info, magic_halt_detail + halt.get_id());
		w->set_windowsize( size );
		w->tabs.set_active_tab_index(selected_tab);
		destroy_win( this );
	}
}


halt_detail_pas_t::halt_detail_pas_t(halthandle_t halt)
{
	this->halt = halt;
}

void halt_detail_pas_t::draw_class_table(scr_coord offset, const uint8 class_name_cell_width, const goods_desc_t *good_category)
{
	if (good_category != goods_manager_t::mail && good_category != goods_manager_t::passengers) {
		return; // this table is for pax and mail
	}
	scr_coord_val y = offset.y;

	uint base_capacity = halt->get_capacity(good_category->get_catg_index()) ? max(halt->get_capacity(good_category->get_catg_index()), 10) : 10; // Note that capacity may have 0 even if pax_enabled
	uint transferring_sum = 0;
	bool served = false;
	int left = 0;

	display_color_img(good_category == goods_manager_t::mail ? skinverwaltung_t::mail->get_image_id(0) : skinverwaltung_t::passengers->get_image_id(0), offset.x, y + FIXED_SYMBOL_YOFF, 0, false, false);
	for (uint8 i = 0; i < good_category->get_number_of_classes(); i++)
	{
		if (halt->get_connexions(good_category->get_catg_index(), i)->empty())
		{
			served = false;
		}
		else {
			served = true;
		}
		pas_info.clear();
		pas_info.append(goods_manager_t::get_translated_wealth_name(good_category->get_catg_index(), i));
		display_proportional_clip_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH, y, pas_info, ALIGN_LEFT, SYSCOL_TEXT, true);
		pas_info.clear();

		const uint32 waiting_sum_by_class = halt->get_ware_summe(good_category, i);
		const uint32 waiting_commuter_sum_by_class = (good_category == goods_manager_t::passengers) ? halt->get_ware_summe(good_category, i, true) : 0;
		if (served || waiting_sum_by_class) {
			if (good_category == goods_manager_t::passengers) {
				pas_info.append(waiting_sum_by_class);
#ifdef DEBUG
				pas_info.printf("(%u)", waiting_commuter_sum_by_class);
#endif
			}
			else {
				pas_info.append(waiting_sum_by_class);
			}
		}
		else {
			pas_info.append("-");
		}
		display_proportional_clip_rgb(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH, y, pas_info, ALIGN_RIGHT, SYSCOL_TEXT, true);
		pas_info.clear();

		uint transfers_out = halt->get_leaving_goods_sum(good_category, i);
		uint transfers_in = halt->get_transferring_goods_sum(good_category, i) - transfers_out;
		if (served || halt->get_transferring_goods_sum(good_category, i))
		{
			pas_info.printf("%4u/%4u", transfers_in, transfers_out);
		}
		else {
			pas_info.append("-");
		}
		left += display_proportional_clip_rgb(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 5, y, pas_info, ALIGN_RIGHT, SYSCOL_TEXT, true);
		pas_info.clear();

		// color bar
		uint bar_width = ((waiting_sum_by_class*GOODS_WAITING_BAR_BASE_WIDTH) + (base_capacity-1)) / base_capacity;
		// transferring bar
		display_fillbox_wh_clip_rgb(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 10, y + GOODS_COLOR_BOX_YOFF + 1, (transfers_in * GOODS_WAITING_BAR_BASE_WIDTH / base_capacity) + bar_width, 6, BARCOL_TRANSFER_IN, true);
		transferring_sum += halt->get_transferring_goods_sum(good_category, i);
		// leaving bar
		display_fillbox_wh_clip_rgb(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 10, y + GOODS_COLOR_BOX_YOFF + 8, transfers_out * GOODS_WAITING_BAR_BASE_WIDTH / base_capacity, GOODS_LEAVING_BAR_HEIGHT, color_idx_to_rgb(MN_GREY0), true);
		// waiting bar
		display_cylinderbar_wh_clip_rgb(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 10, y + GOODS_COLOR_BOX_YOFF + 1, bar_width, 6, good_category->get_color(), true);
		if (waiting_commuter_sum_by_class) {
			// commuter bar
			bar_width = ((waiting_commuter_sum_by_class*GOODS_WAITING_BAR_BASE_WIDTH) + (base_capacity-1)) / base_capacity;
			display_cylinderbar_wh_clip_rgb(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 10, y + GOODS_COLOR_BOX_YOFF + 1, bar_width, 6, color_idx_to_rgb(COL_COMMUTER), true);
		}

		y += LINESPACE + GOODS_LEAVING_BAR_HEIGHT + 1;

	}
	y += 2;
	display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH, y, offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width, y, color_idx_to_rgb(MN_GREY1));
	display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + 4, y, offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + GOODS_WAITING_CELL_WIDTH, y, color_idx_to_rgb(MN_GREY1));
	display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + GOODS_WAITING_CELL_WIDTH + 4, y, offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + GOODS_WAITING_CELL_WIDTH * 2 + 5, y, color_idx_to_rgb(MN_GREY1));
	display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + GOODS_WAITING_CELL_WIDTH * 2 + 5 + 4, y, offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + GOODS_WAITING_CELL_WIDTH * 2 + 5 + GOODS_WAITING_BAR_BASE_WIDTH, y, color_idx_to_rgb(MN_GREY1));
	y += 4;

	// total
	PIXVAL color;
	color = halt->is_overcrowded(good_category->get_catg_index()) ? color_idx_to_rgb(COL_DARK_PURPLE) : SYSCOL_TEXT;
	pas_info.append(halt->get_ware_summe(good_category));
	display_proportional_clip_rgb(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH, y, pas_info, ALIGN_RIGHT, color, true);
	pas_info.clear();

	pas_info.append(transferring_sum);
	left += display_proportional_clip_rgb(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 5, y, pas_info, ALIGN_RIGHT, SYSCOL_TEXT, true);
	pas_info.clear();

	pas_info.printf("  %u ", halt->get_ware_summe(good_category) + transferring_sum);
	pas_info.printf(translator::translate("(max: %u)"), halt->get_capacity(good_category->get_catg_index()));
	left += display_proportional_clip_rgb(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 10, y, pas_info, ALIGN_LEFT, SYSCOL_TEXT, true);
	pas_info.clear();
}


#define TOTAL_CELL_OFFSET_RIGHT (proportional_string_width(" (00.0%)"))
#define DEMANDS_CELL_WIDTH (max(100, proportional_string_width(" 00000") + TOTAL_CELL_OFFSET_RIGHT))
#define GENERATED_CELL_WIDTH (proportional_string_width(" 000,000,000"))
void halt_detail_pas_t::draw(scr_coord offset)
{
	// keep previous maximum width
	int x_size = get_size().w - 51 - pos.x;
	int top = D_MARGIN_TOP;
	offset.x += D_MARGIN_LEFT;
	sint16 left = 0;

	if (halt.is_bound()) {
		// Calculate width of class name cell
		uint8 class_name_cell_width = 0;
		uint8 pass_classes = goods_manager_t::passengers->get_number_of_classes();
		uint8 mail_classes = goods_manager_t::mail->get_number_of_classes();
		for (uint8 i = 0; i < pass_classes; i++) {
			class_name_cell_width = max(proportional_string_width(goods_manager_t::get_translated_wealth_name(goods_manager_t::INDEX_PAS, i)), class_name_cell_width);
		}
		for (uint8 i = 0; i < mail_classes; i++) {
			class_name_cell_width = max(proportional_string_width(goods_manager_t::get_translated_wealth_name(goods_manager_t::INDEX_MAIL, i)), class_name_cell_width);
		}

		display_proportional_clip_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH, offset.y + top, translator::translate("hd_wealth"), ALIGN_LEFT, SYSCOL_TEXT, true);
		display_proportional_clip_rgb(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH, offset.y + top, translator::translate("hd_waiting"), ALIGN_RIGHT, SYSCOL_TEXT, true);
		display_proportional_clip_rgb(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 5, offset.y + top, translator::translate("hd_transfers"), ALIGN_RIGHT, SYSCOL_TEXT, true);
		top += LINESPACE + 2;

		display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH, offset.y + top, offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width, offset.y + top, color_idx_to_rgb(MN_GREY1));
		display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + 4, offset.y + top, offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + GOODS_WAITING_CELL_WIDTH, offset.y + top, color_idx_to_rgb(MN_GREY1));
		display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + GOODS_WAITING_CELL_WIDTH + 4, offset.y + top, offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + GOODS_WAITING_CELL_WIDTH * 2 + 5, offset.y + top, color_idx_to_rgb(MN_GREY1));
		display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + GOODS_WAITING_CELL_WIDTH * 2 + 5 + 4, offset.y + top, offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + GOODS_WAITING_CELL_WIDTH * 2 + 5 + GOODS_WAITING_BAR_BASE_WIDTH, offset.y + top, color_idx_to_rgb(MN_GREY1));
		top += 4;

		if (halt->get_pax_enabled()) {
			draw_class_table(scr_coord(offset.x, offset.y + top), class_name_cell_width, goods_manager_t::passengers);
			top += (pass_classes + 1) * (LINESPACE + GOODS_LEAVING_BAR_HEIGHT + 1) + 6;
			top += LINESPACE + D_V_SPACE;
		}

		if (halt->get_mail_enabled()) {
			draw_class_table(scr_coord(offset.x, offset.y + top), class_name_cell_width, goods_manager_t::mail);
			top += (mail_classes + 1) * (LINESPACE + GOODS_LEAVING_BAR_HEIGHT + 1) + 6;
			top += LINESPACE;
		}

		// bar color description
		left = D_MARGIN_LEFT;
		const PIXVAL col_passenger = goods_manager_t::passengers->get_color();
		if (halt->get_pax_enabled()) {
			display_colorbox_with_tooltip(offset.x + left, offset.y + top + GOODS_COLOR_BOX_YOFF, 8, 8, col_passenger, true, translator::translate("visitors"));
			left += 10;
			pas_info.clear();
			pas_info.append(translator::translate("visitors"));
			left += display_proportional_clip_rgb(offset.x + left, offset.y + top, pas_info, ALIGN_LEFT, SYSCOL_TEXT, true) + D_H_SPACE * 2;

			display_colorbox_with_tooltip(offset.x + left, offset.y + top + GOODS_COLOR_BOX_YOFF, 8, 8, color_idx_to_rgb(COL_COMMUTER), true, translator::translate("commuters"));
			left += 10;
			pas_info.clear();
			pas_info.append(translator::translate("commuters"));
			left += display_proportional_clip_rgb(offset.x + left, offset.y + top, pas_info, ALIGN_LEFT, SYSCOL_TEXT, true) + D_H_SPACE * 2;
		}

		if (halt->get_mail_enabled()) {
			display_colorbox_with_tooltip(offset.x + left, offset.y + top + GOODS_COLOR_BOX_YOFF, 8, 8, goods_manager_t::mail->get_color(), true, goods_manager_t::mail->get_name());
			left += 10;
			pas_info.clear();
			pas_info.append( translator::translate(goods_manager_t::mail->get_name()) );
			left += display_proportional_clip_rgb(offset.x + left, offset.y + top, pas_info, ALIGN_LEFT, SYSCOL_TEXT, true) + D_H_SPACE * 2;
		}

		if (halt->get_pax_enabled() || halt->get_mail_enabled()) {
			top += LINESPACE * 2;
		}

		// passenger demands info
		const uint32 arround_population = halt->get_around_population();
		const uint32 arround_job_demand = halt->get_around_job_demand();
		if (halt->get_pax_enabled()) {
			top += LINESPACE;
			display_heading_rgb(offset.x, offset.y + top, D_DEFAULT_WIDTH - D_MARGINS_X, D_HEADING_HEIGHT,
				color_idx_to_rgb(halt->get_owner()->get_player_color1()), color_idx_to_rgb(halt->get_owner()->get_player_color1()+2), translator::translate("Around passenger demands"), true, 1);
			top += D_HEADING_HEIGHT + D_V_SPACE*2;
			display_proportional_clip_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH, offset.y + top, translator::translate("hd_wealth"), ALIGN_LEFT, SYSCOL_TEXT, true);
			display_proportional_clip_rgb(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + 4, offset.y + top, translator::translate("Population"), ALIGN_LEFT, SYSCOL_TEXT, true);
			display_proportional_clip_rgb(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + DEMANDS_CELL_WIDTH + 4, offset.y + top, translator::translate("Visitor demand"), ALIGN_LEFT, SYSCOL_TEXT, true);
			display_proportional_clip_rgb(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + DEMANDS_CELL_WIDTH * 2 + 5 + 4, offset.y + top, translator::translate("Jobs"), ALIGN_LEFT, SYSCOL_TEXT, true);
			top += LINESPACE + 2;

			display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH, offset.y + top, offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width, offset.y + top, color_idx_to_rgb(MN_GREY1));
			display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + 4, offset.y + top, offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + DEMANDS_CELL_WIDTH, offset.y + top, color_idx_to_rgb(MN_GREY1));
			display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + DEMANDS_CELL_WIDTH + 4, offset.y + top, offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + DEMANDS_CELL_WIDTH * 2 + 5, offset.y + top, color_idx_to_rgb(MN_GREY1));
			display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + DEMANDS_CELL_WIDTH * 2 + 5 + 4, offset.y + top, offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + DEMANDS_CELL_WIDTH * 3 + 5, offset.y + top, color_idx_to_rgb(MN_GREY1));
			top += 4;

			const uint32 arround_visitor_demand = halt->get_around_visitor_demand();
			for (uint8 i = 0; i < pass_classes; i++) {
				// wealth name
				pas_info.clear();
				pas_info.append(goods_manager_t::get_translated_wealth_name(goods_manager_t::INDEX_PAS, i));
				display_proportional_clip_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH, offset.y + top, pas_info, ALIGN_LEFT, SYSCOL_TEXT, true);

				// population
				const uint32 arround_population_by_class = halt->get_around_population(i);
				pas_info.clear();
				pas_info.printf("%u (%4.1f%%)", arround_population_by_class, arround_population ? 100.0 * arround_population_by_class / arround_population : 0.0);
				uint colored_with = arround_population ? ((DEMANDS_CELL_WIDTH*arround_population_by_class) + (arround_population-1)) / arround_population : 0;
				display_linear_gradient_wh_rgb(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH+5, offset.y + top+1, colored_with, LINESPACE - 3, color_idx_to_rgb(COL_DARK_GREEN+1), 70, 15, true);
				display_proportional_clip_rgb(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + DEMANDS_CELL_WIDTH, offset.y + top, pas_info, ALIGN_RIGHT, SYSCOL_TEXT, true);

				// visitor demand
				const uint32 arround_visitor_demand_by_class = halt->get_around_visitor_demand(i);
				pas_info.clear();
				pas_info.printf("%u (%4.1f%%)", arround_visitor_demand_by_class, arround_visitor_demand ? 100.0 * arround_visitor_demand_by_class / arround_visitor_demand : 0.0);
				colored_with = arround_visitor_demand ? ((DEMANDS_CELL_WIDTH*arround_visitor_demand_by_class) + (arround_visitor_demand-1)) / arround_visitor_demand : 0;
				display_linear_gradient_wh_rgb(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + DEMANDS_CELL_WIDTH+5, offset.y + top + 1, colored_with, LINESPACE - 3, col_passenger, 70, 15, true);
				display_proportional_clip_rgb(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + DEMANDS_CELL_WIDTH *2 + 5, offset.y + top, pas_info, ALIGN_RIGHT, SYSCOL_TEXT, true);

				// job demand
				const uint32 arround_job_demand_by_class = halt->get_around_job_demand(i);
				pas_info.clear();
				pas_info.printf("%u (%4.1f%%)", arround_job_demand_by_class, arround_job_demand ? 100.0 * arround_job_demand_by_class / arround_job_demand : 0.0);
				colored_with = arround_job_demand ? ((DEMANDS_CELL_WIDTH*arround_job_demand_by_class) + (arround_job_demand-1)) / arround_job_demand : 0;
				display_linear_gradient_wh_rgb(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + (DEMANDS_CELL_WIDTH+5) *2, offset.y + top + 1, colored_with, LINESPACE - 3, color_idx_to_rgb(COL_COMMUTER-1), 70, 15, true);
				display_proportional_clip_rgb(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + DEMANDS_CELL_WIDTH *3 + 5 + 4, offset.y + top, pas_info, ALIGN_RIGHT, SYSCOL_TEXT, true);

				top += LINESPACE;
			}
			top += 2;
			display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH, offset.y + top, offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width, offset.y + top, color_idx_to_rgb(MN_GREY1));
			display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + 4, offset.y + top, offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + DEMANDS_CELL_WIDTH, offset.y + top, color_idx_to_rgb(MN_GREY1));
			display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + DEMANDS_CELL_WIDTH + 4, offset.y + top, offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + DEMANDS_CELL_WIDTH * 2 + 5, offset.y + top, color_idx_to_rgb(MN_GREY1));
			display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + DEMANDS_CELL_WIDTH * 2 + 5 + 4, offset.y + top, offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + DEMANDS_CELL_WIDTH * 3 + 5, offset.y + top, color_idx_to_rgb(MN_GREY1));

			top += 4;
			pas_info.clear();
			pas_info.printf("%u", arround_population);
			display_proportional_clip_rgb(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH - TOTAL_CELL_OFFSET_RIGHT + DEMANDS_CELL_WIDTH, offset.y + top, pas_info, ALIGN_RIGHT, SYSCOL_TEXT, true);
			pas_info.clear();
			pas_info.printf("%u", arround_visitor_demand);
			display_proportional_clip_rgb(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH - TOTAL_CELL_OFFSET_RIGHT + DEMANDS_CELL_WIDTH * 2 + 5, offset.y + top, pas_info, ALIGN_RIGHT, SYSCOL_TEXT, true);
			pas_info.clear();
			pas_info.printf("%u", arround_job_demand);
			display_proportional_clip_rgb(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH - TOTAL_CELL_OFFSET_RIGHT + DEMANDS_CELL_WIDTH * 3 + 5 + 4, offset.y + top, pas_info, ALIGN_RIGHT, SYSCOL_TEXT, true);

			top += LINESPACE;
		}

		// transportation status
		if ((halt->get_pax_enabled() && arround_population) || halt->get_mail_enabled()) {
			top += LINESPACE;
			display_heading_rgb(offset.x, offset.y + top, D_DEFAULT_WIDTH - D_MARGINS_X, D_HEADING_HEIGHT,
				color_idx_to_rgb(halt->get_owner()->get_player_color1()), color_idx_to_rgb(halt->get_owner()->get_player_color1()+2), translator::translate("Transportation status around this stop"), true, 1);
			top += D_HEADING_HEIGHT + D_V_SPACE * 2;
			// header
			display_proportional_clip_rgb(offset.x + D_BUTTON_WIDTH + GOODS_SYMBOL_CELL_WIDTH + 4, offset.y + top, translator::translate("hd_generated"), ALIGN_LEFT, SYSCOL_TEXT, true);
			display_proportional_clip_rgb(offset.x + D_BUTTON_WIDTH + GOODS_SYMBOL_CELL_WIDTH + GENERATED_CELL_WIDTH + D_H_SPACE + 4, offset.y + top, translator::translate("Success rate"), ALIGN_LEFT, SYSCOL_TEXT, true);
			top += LINESPACE + 2;
			display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH, offset.y + top, offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH, offset.y + top, color_idx_to_rgb(MN_GREY1));
			display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + 4, offset.y + top, offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GENERATED_CELL_WIDTH, offset.y + top, color_idx_to_rgb(MN_GREY1));
			display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GENERATED_CELL_WIDTH + 4, offset.y + top, offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH * 2 + GENERATED_CELL_WIDTH + 5, offset.y + top, color_idx_to_rgb(MN_GREY1));
			top += 4;
			if (halt->get_pax_enabled() && arround_population) {
				// These are information about RES building
				// visiting trip
				display_colorbox_with_tooltip(offset.x, offset.y + top + GOODS_COLOR_BOX_YOFF, 8, 8, col_passenger, true, translator::translate("visitors"));
				pas_info.clear();
				pas_info.append(translator::translate("Visiting trip")); // Note: "Visiting" is used in chart button. "visiting" is used in cargo list
				display_proportional_clip_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH, offset.y + top, pas_info, ALIGN_LEFT, SYSCOL_TEXT, true);
				pas_info.clear();
				pas_info.append(halt->get_around_visitor_generated(), 0);
				display_proportional_clip_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GENERATED_CELL_WIDTH, offset.y + top, pas_info, ALIGN_RIGHT, SYSCOL_TEXT, true);
				pas_info.clear();
				pas_info.printf(" %5.1f%%", halt->get_around_visitor_generated() ? 100.0 * halt->get_around_succeeded_visiting() / halt->get_around_visitor_generated() : 0.0);
				display_proportional_clip_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GENERATED_CELL_WIDTH + D_H_SPACE, offset.y + top, pas_info, ALIGN_LEFT, SYSCOL_TEXT, true);
				top += LINESPACE;

				// commuting trip
				display_colorbox_with_tooltip(offset.x, offset.y + top + GOODS_COLOR_BOX_YOFF, 8, 8, color_idx_to_rgb(COL_COMMUTER), true, translator::translate("commuters"));
				pas_info.clear();
				pas_info.append(translator::translate("Commuting trip")); // Note: "Commuting" is used in chart button. "commuting" is used in cargo list
				display_proportional_clip_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH, offset.y + top, pas_info, ALIGN_LEFT, SYSCOL_TEXT, true);
				pas_info.clear();
				pas_info.append(halt->get_around_commuter_generated(), 0);
				display_proportional_clip_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GENERATED_CELL_WIDTH, offset.y + top, pas_info, ALIGN_RIGHT, SYSCOL_TEXT, true);
				pas_info.clear();
				pas_info.printf(" %5.1f%%", halt->get_around_commuter_generated() ? 100.0 * halt->get_around_succeeded_commuting() / halt->get_around_commuter_generated() : 0.0);
				display_proportional_clip_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GENERATED_CELL_WIDTH + D_H_SPACE, offset.y + top, pas_info, ALIGN_LEFT, SYSCOL_TEXT, true);
				top += LINESPACE;
			}

			if (halt->get_mail_enabled()) {
				display_colorbox_with_tooltip(offset.x, offset.y + top + GOODS_COLOR_BOX_YOFF, 8, 8, goods_manager_t::mail->get_color(), true, goods_manager_t::mail->get_name());
				pas_info.clear();
				pas_info.append(translator::translate("hd_mailing"));
				display_proportional_clip_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH, offset.y + top, pas_info, ALIGN_LEFT, SYSCOL_TEXT, true);
				pas_info.clear();
				pas_info.append(halt->get_around_mail_generated(), 0);
				display_proportional_clip_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GENERATED_CELL_WIDTH, offset.y + top, pas_info, ALIGN_RIGHT, SYSCOL_TEXT, true);
				pas_info.clear();
				pas_info.printf(" %5.1f%%", halt->get_around_mail_generated() ? 100.0 * halt->get_around_mail_delivery_succeeded() / halt->get_around_mail_generated() : 0.0);
				display_proportional_clip_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GENERATED_CELL_WIDTH + D_H_SPACE, offset.y + top, pas_info, ALIGN_LEFT, SYSCOL_TEXT, true);

				top += LINESPACE;
			}

			if (halt->get_pax_enabled() && arround_job_demand) {
				top += LINESPACE;
				// Staffing rate
				pas_info.clear();
				pas_info.append(translator::translate("Jobs"));
				display_proportional_clip_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH, offset.y + top, pas_info, ALIGN_LEFT, SYSCOL_TEXT, true);
				pas_info.clear();
				pas_info.append(arround_job_demand, 0);
				display_proportional_clip_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GENERATED_CELL_WIDTH, offset.y + top, pas_info, ALIGN_RIGHT, SYSCOL_TEXT, true);
				pas_info.clear();
				pas_info.printf(" %5.1f%%", 100.0 * halt->get_around_employee_factor() / arround_job_demand);
				display_proportional_clip_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GENERATED_CELL_WIDTH + D_H_SPACE, offset.y + top, pas_info, ALIGN_LEFT, SYSCOL_TEXT, true);

			}
		}
		top += LINESPACE + D_MARGIN_BOTTOM;

		scr_size size(max(x_size + pos.x, get_size().w), top);
		if (size != get_size()) {
			set_size(size);
		}
	}
}



halt_detail_goods_t::halt_detail_goods_t(halthandle_t halt)
{
	this->halt = halt;
}


void halt_detail_goods_t::draw(scr_coord offset)
{
	// keep previous maximum width
	int x_size = get_size().w - 51 - pos.x;
	int top = D_MARGIN_TOP;
	offset.x += D_MARGIN_LEFT;
	int left = GOODS_SYMBOL_CELL_WIDTH;

	//goods_info.clear();
	if (halt.is_bound()) {
		active_freight_catg = 0;
		if (halt->get_ware_enabled())
		{
			uint base_capacity = halt->get_capacity(2) ? max(halt->get_capacity(2), 10) : 10; // Note that capacity may have 0 even if enabled
			uint waiting_sum = 0;
			uint transship_sum = 0;

			goods_info.clear();

			// [Waiting goods info]
			display_proportional_clip_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH, offset.y + top, translator::translate("hd_category"), ALIGN_LEFT, SYSCOL_TEXT, true);
			display_proportional_clip_rgb(offset.x + D_BUTTON_WIDTH + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH, offset.y + top, translator::translate("hd_waiting"), ALIGN_RIGHT, SYSCOL_TEXT, true);
			display_proportional_clip_rgb(offset.x + D_BUTTON_WIDTH + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 5, offset.y + top, translator::translate("hd_transship"), ALIGN_RIGHT, SYSCOL_TEXT, true);
			top += LINESPACE + 2;

			display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH, offset.y + top, offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH, offset.y + top, color_idx_to_rgb(MN_GREY1));
			display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + 4, offset.y + top, offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GOODS_WAITING_CELL_WIDTH, offset.y + top, color_idx_to_rgb(MN_GREY1));
			display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GOODS_WAITING_CELL_WIDTH + 4, offset.y + top, offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 5, offset.y + top, color_idx_to_rgb(MN_GREY1));
			display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 5 + 4, offset.y + top, offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 5 + GOODS_WAITING_BAR_BASE_WIDTH, offset.y + top, color_idx_to_rgb(MN_GREY1));
			top += 4;

			for (uint8 i = 0; i < goods_manager_t::get_max_catg_index(); i++) {
				if (i == goods_manager_t::INDEX_PAS || i == goods_manager_t::INDEX_MAIL)
				{
					continue;
				}

				const goods_desc_t* info = goods_manager_t::get_info_catg_index(i);
				goods_info.append(translator::translate(info->get_catg_name()));

				const goods_desc_t *wtyp = goods_manager_t::get_info(i);
				if (halt->gibt_ab(info)) {
					uint32 waiting_sum_catg = 0;
					uint32 transship_in_catg = 0;
					uint32 leaving_sum_catg = 0;

					// category symbol
					display_color_img(info->get_catg_symbol(), offset.x, offset.y + top + FIXED_SYMBOL_YOFF, 0, false, false);

					display_proportional_clip_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH, offset.y + top, goods_info, ALIGN_LEFT, SYSCOL_TEXT, true);
					goods_info.clear();

					int bar_offset_left = 0;
					switch (i) {
					case 0:
						waiting_sum_catg = halt->get_ware_summe(wtyp);
						display_fillbox_wh_clip_rgb(offset.x + D_BUTTON_WIDTH + bar_offset_left + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 10, offset.y + top + GOODS_COLOR_BOX_YOFF + 1, halt->get_ware_summe(wtyp) * GOODS_WAITING_BAR_BASE_WIDTH / base_capacity, 6, wtyp->get_color(), true);
						display_blend_wh_rgb(offset.x + D_BUTTON_WIDTH + bar_offset_left + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 10, offset.y + top + GOODS_COLOR_BOX_YOFF + 6, halt->get_ware_summe(wtyp) * GOODS_WAITING_BAR_BASE_WIDTH / base_capacity, 1, color_idx_to_rgb(COL_BLACK), 10);
						bar_offset_left = halt->get_ware_summe(wtyp) * GOODS_WAITING_BAR_BASE_WIDTH / base_capacity;
						leaving_sum_catg = halt->get_leaving_goods_sum(wtyp, 0);
						transship_in_catg = halt->get_transferring_goods_sum(wtyp, 0) - leaving_sum_catg;
						break;
					default:
						const uint8  count = goods_manager_t::get_count();
						for (uint8 j = 3; j < count; j++) {
							goods_desc_t const* const wtyp2 = goods_manager_t::get_info(j);
							if (wtyp2->get_catg_index() != i) {
								continue;
							}
							waiting_sum_catg += halt->get_ware_summe(wtyp2);
							leaving_sum_catg += halt->get_leaving_goods_sum(wtyp2, 0);
							transship_in_catg += halt->get_transferring_goods_sum(wtyp2, 0) - halt->get_leaving_goods_sum(wtyp2, 0);

							// color bar
							uint bar_width = halt->get_ware_summe(wtyp2) * GOODS_WAITING_BAR_BASE_WIDTH / base_capacity;

							// waiting bar
							if (bar_width) {
								display_cylinderbar_wh_clip_rgb(offset.x + D_BUTTON_WIDTH + bar_offset_left + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 10, offset.y + top + GOODS_COLOR_BOX_YOFF + 1, bar_width, 6, wtyp2->get_color(), true);
							}
							bar_offset_left += bar_width;
						}
						break;
					}
					transship_sum += leaving_sum_catg + transship_in_catg;
					// transferring bar
					display_fillbox_wh_clip_rgb(offset.x + D_BUTTON_WIDTH + bar_offset_left + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 10, offset.y + top + GOODS_COLOR_BOX_YOFF + 1, transship_in_catg * GOODS_WAITING_BAR_BASE_WIDTH / base_capacity, 6, BARCOL_TRANSFER_IN, true);
					// leaving bar
					display_fillbox_wh_clip_rgb(offset.x + D_BUTTON_WIDTH + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 10, offset.y + top + GOODS_COLOR_BOX_YOFF + 8, leaving_sum_catg * GOODS_WAITING_BAR_BASE_WIDTH / base_capacity, GOODS_LEAVING_BAR_HEIGHT, color_idx_to_rgb(MN_GREY0), true);

					//waiting
					goods_info.append(waiting_sum_catg);
					display_proportional_clip_rgb(offset.x + D_BUTTON_WIDTH + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH, offset.y + top, goods_info, ALIGN_RIGHT, SYSCOL_TEXT, true);
					goods_info.clear();
					waiting_sum += waiting_sum_catg;

					//transfer
					goods_info.printf("%4u/%4u", transship_in_catg, leaving_sum_catg);
					//goods_info.append(transship_sum_catg);
					left += display_proportional_clip_rgb(offset.x + D_BUTTON_WIDTH + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 5, offset.y + top, goods_info, ALIGN_RIGHT, SYSCOL_TEXT, true);
					goods_info.clear();
					top += LINESPACE + GOODS_LEAVING_BAR_HEIGHT + 1;
					active_freight_catg++;
				}
				goods_info.clear();

			}
			if (!active_freight_catg) {
				// There is no data until connection data is updated, or no freight service has been operated
				if (skinverwaltung_t::alerts) {
					display_color_img(skinverwaltung_t::alerts->get_image_id(2), offset.x + D_BUTTON_WIDTH, offset.y + top + FIXED_SYMBOL_YOFF, 0, false, false);
				}
				display_proportional_clip_rgb(offset.x + D_BUTTON_WIDTH + GOODS_SYMBOL_CELL_WIDTH, offset.y + top, translator::translate("no data"), ALIGN_LEFT, color_idx_to_rgb(MN_GREY0), true);
				top += LINESPACE;
			}

			top += 2;

			display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH, offset.y + top, offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH, offset.y + top, color_idx_to_rgb(MN_GREY1));
			display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + 4, offset.y + top, offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GOODS_WAITING_CELL_WIDTH, offset.y + top, color_idx_to_rgb(MN_GREY1));
			display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GOODS_WAITING_CELL_WIDTH + 4, offset.y + top, offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 5, offset.y + top, color_idx_to_rgb(MN_GREY1));
			display_direct_line_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 5 + 4, offset.y + top, offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 5 + GOODS_WAITING_BAR_BASE_WIDTH, offset.y + top, color_idx_to_rgb(MN_GREY1));
			top += 4;

			// total
			PIXVAL color;
			color = halt->is_overcrowded(2) ? color_idx_to_rgb(COL_DARK_PURPLE) : SYSCOL_TEXT;
			goods_info.append(waiting_sum);
			display_proportional_clip_rgb(offset.x + D_BUTTON_WIDTH + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH, offset.y + top, goods_info, ALIGN_RIGHT, color, true);
			goods_info.clear();

			goods_info.append(transship_sum);
			left += display_proportional_clip_rgb(offset.x + D_BUTTON_WIDTH + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 5, offset.y + top, goods_info, ALIGN_RIGHT, SYSCOL_TEXT, true);
			goods_info.clear();

			goods_info.printf("  %u ", waiting_sum + transship_sum);
			goods_info.printf(translator::translate("(max: %u)"), halt->get_capacity(2));
			left += display_proportional_clip_rgb(offset.x + D_BUTTON_WIDTH + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 10, offset.y + top, goods_info, ALIGN_LEFT, SYSCOL_TEXT, true);
			goods_info.clear();

			top += LINESPACE * 2;

			goods_info.clear();

		}
		goods_info.clear();

		scr_size size(max(x_size + pos.x, get_size().w), top);
		if (size != get_size()) {
			set_size(size);
		}
	}
}


void halt_detail_goods_t::recalc_size()
{
	set_size(scr_size(400, D_MARGIN_TOP + active_freight_catg * (LINESPACE + GOODS_LEAVING_BAR_HEIGHT + 1) + LINESPACE * 4 + 12));
}


gui_halt_nearby_factory_info_t::gui_halt_nearby_factory_info_t(const halthandle_t & halt)
{
	this->halt = halt;

	line_selected = 0xFFFFFFFFu;
}

void gui_halt_nearby_factory_info_t::recalc_size()
{
	int yoff = 0;
	if (halt.is_bound()) {
		// set for default size
		yoff = halt->get_fab_list().get_count() * (LINESPACE + 1);
		if (yoff) { yoff += LINESPACE; }
		yoff += LINESPACE * 2;

		yoff += LINESPACE * max(required_material.get_count(), active_product.get_count() + inactive_product.get_count());

		set_size(scr_size(400, yoff));
	}
}


void gui_halt_nearby_factory_info_t::draw(scr_coord offset)
{
	static cbuffer_t buf;
	int xoff = pos.x;
	int yoff = pos.y;

	const slist_tpl<fabrik_t*> & fab_list = halt->get_fab_list();

	uint32 sel = line_selected;
	FORX(const slist_tpl<fabrik_t*>, const fab, fab_list, yoff += LINESPACE + 1) {
		xoff = D_POS_BUTTON_WIDTH + D_H_SPACE;
		// [status color bar]
		const PIXVAL col_val = color_idx_to_rgb(fabrik_t::status_to_color[fab->get_status() % fabrik_t::staff_shortage]);
		display_fillbox_wh_clip_rgb(offset.x + xoff + 1, offset.y + yoff + GOODS_COLOR_BOX_YOFF + 3, D_INDICATOR_WIDTH / 2 - 1, D_INDICATOR_HEIGHT, col_val, true);
		xoff += D_INDICATOR_WIDTH / 2 + 3;

		// [name]
		buf.clear();
		buf.printf("%s (%d,%d)", fab->get_name(), fab->get_pos().x, fab->get_pos().y);
		xoff += display_proportional_clip_rgb(offset.x + xoff, offset.y + yoff, buf, ALIGN_LEFT, SYSCOL_TEXT, true);

		xoff = max(xoff, D_BUTTON_WIDTH * 2 + D_INDICATOR_WIDTH);

		uint has_input_output = 0;
		FOR(array_tpl<ware_production_t>, const& i, fab->get_input()) {
			goods_desc_t const* const ware = i.get_typ();
			if (skinverwaltung_t::input_output && !has_input_output) {
				display_color_img(skinverwaltung_t::input_output->get_image_id(0), offset.x + xoff, offset.y + yoff + FIXED_SYMBOL_YOFF, 0, false, false);
				xoff += D_FIXED_SYMBOL_WIDTH + D_V_SPACE;
			}
			// input goods color square box
			ware->get_name();
			display_colorbox_with_tooltip(offset.x + xoff, offset.y + yoff + GOODS_COLOR_BOX_YOFF, GOODS_COLOR_BOX_HEIGHT, GOODS_COLOR_BOX_HEIGHT, ware->get_color(), true, translator::translate(ware->get_name()));
			xoff += D_FIXED_SYMBOL_WIDTH;

			required_material.append_unique(ware);
			has_input_output++;
		}

		if (has_input_output < 4) {
			xoff += has_input_output ? D_FIXED_SYMBOL_WIDTH * (4 - has_input_output) : D_FIXED_SYMBOL_WIDTH * 5 + D_V_SPACE;
		}
		has_input_output = 0;
		FOR(array_tpl<ware_production_t>, const& i, fab->get_output()) {
			goods_desc_t const* const ware = i.get_typ();
			if (skinverwaltung_t::input_output && !has_input_output) {
				display_color_img(skinverwaltung_t::input_output->get_image_id(1), offset.x + xoff, offset.y + yoff + FIXED_SYMBOL_YOFF, 0, false, false);
				xoff += D_FIXED_SYMBOL_WIDTH + D_V_SPACE;
			}
			// output goods color square box
			display_colorbox_with_tooltip(offset.x + xoff, offset.y + yoff + GOODS_COLOR_BOX_YOFF, GOODS_COLOR_BOX_HEIGHT, GOODS_COLOR_BOX_HEIGHT, ware->get_color(), true, translator::translate(ware->get_name()));
			xoff += D_FIXED_SYMBOL_WIDTH;

			if (!active_product.is_contained(ware)) {
				if ((fab->get_status() % fabrik_t::no_material || fab->get_status() % fabrik_t::material_shortage) && !i.menge) {
					// this factory is not in operation
					inactive_product.append_unique(ware);
				}
				else {
					active_product.append(ware);
				}
			}
			has_input_output++;
		}
		if (fab->get_sector() == fabrik_t::power_plant) {
			xoff += D_FIXED_SYMBOL_WIDTH;
			display_color_img(skinverwaltung_t::electricity->get_image_id(0), offset.x + xoff, offset.y + yoff + FIXED_SYMBOL_YOFF, 0, false, false);
		}


		// goto button
		bool selected = sel == 0 || welt->get_viewport()->is_on_center(fab->get_pos());
		display_img_aligned(gui_theme_t::pos_button_img[selected], scr_rect(offset.x + D_H_SPACE, offset.y + yoff, D_POS_BUTTON_WIDTH, LINESPACE), ALIGN_CENTER_V | ALIGN_CENTER_H, true);
		sel--;

		if (win_get_magic((ptrdiff_t)fab)) {
			display_blend_wh_rgb(offset.x, offset.y + yoff, size.w, LINESPACE, SYSCOL_TEXT, 20);
		}
	}
	if (!halt->get_fab_list().get_count()) {
		display_proportional_clip_rgb(offset.x + D_MARGIN_LEFT, offset.y + yoff, translator::translate("keine"), ALIGN_LEFT, color_idx_to_rgb(MN_GREY0), true);
		yoff += LINESPACE;
	}
	yoff += LINESPACE;

	// [Goods needed by nearby industries]
	xoff = GOODS_SYMBOL_CELL_WIDTH + (int)(D_BUTTON_WIDTH*1.5); // 2nd col x offset
	offset.x += D_MARGIN_LEFT;
	if ((!required_material.empty() || !active_product.empty() || !inactive_product.empty()) && halt->get_ware_enabled()) {
		uint8 input_cnt = 0;
		uint8 output_cnt = 0;
		if (skinverwaltung_t::input_output) {
			display_color_img_with_tooltip(skinverwaltung_t::input_output->get_image_id(0), offset.x, offset.y + yoff + FIXED_SYMBOL_YOFF, 0, false, false, translator::translate("Angenommene Waren"));
			display_color_img(skinverwaltung_t::input_output->get_image_id(1), offset.x + xoff, offset.y + yoff + FIXED_SYMBOL_YOFF, 0, false, false);
		}
		display_proportional_clip_rgb(skinverwaltung_t::input_output ? offset.x + GOODS_SYMBOL_CELL_WIDTH : offset.x, offset.y + yoff, translator::translate("Needed"), ALIGN_LEFT, SYSCOL_TEXT, true);
		display_proportional_clip_rgb(skinverwaltung_t::input_output ? offset.x + GOODS_SYMBOL_CELL_WIDTH + xoff : offset.x + xoff, offset.y + yoff, translator::translate("Products"), ALIGN_LEFT, SYSCOL_TEXT, true);
		yoff += LINESPACE;
		yoff += 2;

		display_direct_line_rgb(offset.x, offset.y + yoff, offset.x + xoff - 4, offset.y + yoff, color_idx_to_rgb(MN_GREY1));
		display_direct_line_rgb(offset.x + xoff, offset.y + yoff, offset.x + xoff + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH * 1.5, offset.y + yoff, color_idx_to_rgb(MN_GREY1));
		yoff += 4;

		for (uint8 i = 0; i < goods_manager_t::get_count(); i++) {
			const goods_desc_t *ware = goods_manager_t::get_info(i);
			// inuput
			if (required_material.is_contained(ware)) {
				// category symbol
				display_color_img(ware->get_catg_symbol(), offset.x, offset.y + yoff + LINESPACE * input_cnt + FIXED_SYMBOL_YOFF, 0, false, false);
				// goods color
				display_colorbox_with_tooltip(offset.x + GOODS_SYMBOL_CELL_WIDTH, offset.y + yoff + LINESPACE * input_cnt + GOODS_COLOR_BOX_YOFF, GOODS_COLOR_BOX_HEIGHT, GOODS_COLOR_BOX_HEIGHT, ware->get_color(), false);
				// goods name
				display_proportional_clip_rgb(offset.x + GOODS_SYMBOL_CELL_WIDTH * 2 - 2, offset.y + yoff + LINESPACE * input_cnt, translator::translate(ware->get_name()), ALIGN_LEFT, SYSCOL_TEXT, true);
				input_cnt++;
			}

			//output
			if (active_product.is_contained(ware) || inactive_product.is_contained(ware)) {
				// category symbol
				display_color_img(ware->get_catg_symbol(), offset.x + xoff, offset.y + yoff + LINESPACE * output_cnt + FIXED_SYMBOL_YOFF, 0, false, false);
				// goods color
				display_colorbox_with_tooltip(offset.x + xoff + GOODS_SYMBOL_CELL_WIDTH, offset.y + yoff + LINESPACE * output_cnt + GOODS_COLOR_BOX_YOFF, GOODS_COLOR_BOX_HEIGHT, GOODS_COLOR_BOX_HEIGHT, ware->get_color(), false);
				// goods name
				PIXVAL text_color;
				display_proportional_clip_rgb(offset.x + xoff + GOODS_SYMBOL_CELL_WIDTH * 2 - 2, offset.y + yoff + LINESPACE * output_cnt, translator::translate(ware->get_name()), ALIGN_LEFT, text_color = active_product.is_contained(ware) ? SYSCOL_TEXT : SYSCOL_TEXT_WEAK, true);
				output_cnt++;
			}
		}

		if (required_material.empty()) {
			display_proportional_clip_rgb(offset.x + D_MARGIN_LEFT, offset.y + yoff, translator::translate("keine"), ALIGN_LEFT, color_idx_to_rgb(MN_GREY0), true);
		}
		if (active_product.empty() && inactive_product.empty()) {
			display_proportional_clip_rgb(offset.x + xoff + D_MARGIN_LEFT, offset.y + yoff, translator::translate("keine"), ALIGN_LEFT, color_idx_to_rgb(MN_GREY0), true);
		}
		yoff += LINESPACE * max(input_cnt, output_cnt) + 1;
	}

	set_size(scr_size(400, yoff - pos.y));
}


bool gui_halt_nearby_factory_info_t::infowin_event(const event_t * ev)
{
	const unsigned int line = (ev->cy) / (LINESPACE + 1);
	line_selected = 0xFFFFFFFFu;
	if (line >= halt->get_fab_list().get_count()) {
		return false;
	}

	fabrik_t * fab = halt->get_fab_list().at(line);
	const koord3d fab_pos = fab->get_pos();

	if (!fab) {
		welt->get_viewport()->change_world_position(fab_pos);
		return false;
	}

	if (IS_LEFTRELEASE(ev)) {
		if (ev->cx > 0 && ev->cx < 15) {
			welt->get_viewport()->change_world_position(fab_pos);
		}
		else {
			fab->show_info();
		}
	}
	else if (IS_RIGHTRELEASE(ev)) {
		welt->get_viewport()->change_world_position(fab_pos);
	}
	return false;
}


gui_halt_service_info_t::gui_halt_service_info_t(halthandle_t halt)
	: scrolly(&container)
{
	if (halt.is_bound()) {
		init(halt);
	}
}

void gui_halt_service_info_t::init(halthandle_t halt)
{
	this->halt = halt;
	cached_line_count = 0xFFFFFFFFul;
	cached_convoy_count = 0xFFFFFFFFul;

	update_connections(halt);
}

void gui_halt_service_info_t::draw(scr_coord offset)
{

	update_connections(halt);

	scrolly.set_size(scrolly.get_size());

	gui_aligned_container_t::draw(offset);
}

void gui_halt_service_info_t::update_connections(halthandle_t /*h*/)
{
	if (!halt.is_bound()) {
		// first call, or invalid handle
		remove_all();
		return;
	}

	if (halt->registered_lines.get_count() == cached_line_count && halt->registered_convoys.get_count() == cached_convoy_count) {
		// all current, so do nothing
		return;
	}

	// update connections from here
	remove_all();

	set_table_layout(1, 0);
	set_margin(scr_size(D_MARGIN_LEFT, D_V_SPACE), scr_size(D_MARGIN_RIGHT, D_V_SPACE));

	// add lines that serve this stop
	new_component<gui_heading_t>("Lines serving this stop", color_idx_to_rgb(halt->get_owner()->get_player_color1()), color_idx_to_rgb(halt->get_owner()->get_player_color1()+2), 1);
	add_table(6,0)->set_spacing(scr_size(D_H_SPACE, 2));
	if (halt->registered_lines.empty()) {
		insert_show_nothing();
	}
	else {
		for (uint8 lt = 1; lt < simline_t::MAX_LINE_TYPE; lt++) {
			uint waytype_line_cnt = 0;
			for (uint32 i = 0; i < halt->registered_lines.get_count(); i++) {
				const linehandle_t line = halt->registered_lines[i];
				if (line->get_linetype() != lt) {
					continue;
				}
				// Linetype if it is the first
				if (!waytype_line_cnt) {
					new_component_span<gui_empty_t>(5); new_component<gui_margin_t>(0, D_V_SPACE);

					new_component<gui_image_t>()->set_image(line->get_linetype_symbol(), true);
					new_component_span<gui_label_t>(translator::translate(line->get_linetype_name()),5);

				}

				// Line buttons only if owner ...
				new_component<gui_line_button_t>(line);

				// Line labels with color of player
				gui_label_buf_t *lb = new_component<gui_label_buf_t>(PLAYER_FLAG | color_idx_to_rgb(line->get_owner()->get_player_color1() + env_t::gui_player_color_dark));
				lb->buf().append(line->get_name());
				lb->update();

				new_component<gui_empty_t>();
				if (!skinverwaltung_t::service_frequency) {
					new_component<gui_empty_t>();
				}
				else {
					new_component<gui_image_t>()->set_image(skinverwaltung_t::service_frequency->get_image_id(0), true);
				}
				const sint64 service_frequency = line->get_service_frequency();
				gui_label_buf_t *lb_frequency = new_component<gui_label_buf_t>();
				if (service_frequency) {
					char as_clock[32];
					world()->sprintf_ticks(as_clock, sizeof(as_clock), service_frequency);
					lb_frequency->buf().printf(" %s", as_clock);
					if (line->get_state() & simline_t::line_missing_scheduled_slots) {
						lb_frequency->set_color(color_idx_to_rgb(COL_DARK_TURQUOISE));
					}
				}
				else {
					lb_frequency->buf().append("--:--:--");
					lb_frequency->set_color(COL_INACTIVE);
				}
				lb_frequency->set_align(gui_label_t::right);
				lb_frequency->update();
				lb_frequency->set_fixed_width( proportional_string_width("--:--:--") );

				// convoy count
				gui_label_buf_t *lb_convoy_count = new_component<gui_label_buf_t>();
				if (line->get_convoys().get_count() == 1) {
					lb_convoy_count->buf().append(translator::translate("1 convoi"));
				}
				else if(line->get_convoys().get_count()) {
					lb_convoy_count->buf().printf(translator::translate("%d convois"), line->get_convoys().get_count());
				}
				else {
					lb_convoy_count->buf().append(translator::translate("no convois")); // error
				}
				lb_convoy_count->set_color(line->has_overcrowded() ? color_idx_to_rgb(COL_DARK_PURPLE) : SYSCOL_TEXT);
				lb_convoy_count->update();

				waytype_line_cnt++;
			}
		}
	}
	end_table();

	// add lineless convoys which serve this stop
	new_component<gui_margin_t>(0, D_V_SPACE);
	new_component<gui_heading_t>("Lineless convoys serving this stop", color_idx_to_rgb(halt->get_owner()->get_player_color1()), color_idx_to_rgb(halt->get_owner()->get_player_color1() + 2), 1);
	add_table(6, 0)->set_spacing(scr_size(D_H_SPACE, 2));
	if (halt->registered_convoys.empty()) {
		insert_show_nothing();
	}
	else {
		for (uint8 lt = 1; lt < simline_t::MAX_LINE_TYPE; lt++) {
			uint lineless_convoy_cnt = 0;
			for (uint32 i = 0; i < halt->registered_convoys.get_count(); ++i) {
				const convoihandle_t cnv = halt->registered_convoys[i];
				if (cnv->get_schedule()->get_type() != lt) {
					continue;
				}
				// Linetype if it is the first
				if (!lineless_convoy_cnt) {
					new_component_span<gui_empty_t>(5); new_component<gui_margin_t>(0, D_V_SPACE);

					new_component<gui_image_t>()->set_image(cnv->get_schedule()->get_schedule_type_symbol(), true);
					new_component_span<gui_label_t>(translator::translate(cnv->get_schedule()->get_schedule_type_name()),5);
				}

				// Convoy buttons
				new_component<gui_convoi_button_t>(cnv);

				// Line labels with color of player
				gui_label_buf_t *lb = new_component<gui_label_buf_t>(PLAYER_FLAG | color_idx_to_rgb(cnv->get_owner()->get_player_color1() + env_t::gui_player_color_dark));
				lb->buf().append(cnv->get_name());
				lb->update();

				new_component<gui_empty_t>();
				if (!skinverwaltung_t::service_frequency) {
					new_component<gui_empty_t>();
				}
				else {
					new_component<gui_image_t>()->set_image(skinverwaltung_t::service_frequency->get_image_id(0), true);
				}
				const sint64 average_round_trip_time = cnv->get_average_round_trip_time();
				gui_label_buf_t *lb_triptime = new_component<gui_label_buf_t>();
				if (average_round_trip_time) {
					char as_clock[32];
					world()->sprintf_ticks(as_clock, sizeof(as_clock), average_round_trip_time);
					lb_triptime->buf().printf(" %s", as_clock);
				}
				else {
					lb_triptime->buf().append("--:--:--");
					lb_triptime->set_color(COL_INACTIVE);
				}
				lb_triptime->set_align(gui_label_t::right);
				lb_triptime->update();
				lb_triptime->set_fixed_width(proportional_string_width("--:--:--"));

				new_component<gui_empty_t>();

				lineless_convoy_cnt++;
			}
		}
	}
	end_table();
	new_component<gui_margin_t>(0, D_MARGIN_BOTTOM);

	// ok, we have now this counter for pending updates
	cached_line_count = halt->registered_lines.get_count();
	cached_convoy_count = halt->registered_convoys.get_count();

	set_size(get_min_size());
}



class RelativeDistanceOrdering
{
private:
	const koord m_origin;

public:
	RelativeDistanceOrdering(const koord& origin)
		: m_origin(origin)
	{ /* nothing */
	}

	/// @returns true if @p a is closer to the origin than @p b, otherwise false.
	bool operator()(const halthandle_t &a, const halthandle_t &b) const
	{
		if (!a.is_bound() || !b.is_bound())
		{
			return false;
		}
		return koord_distance(m_origin, a->get_basis_pos()) < koord_distance(m_origin, b->get_basis_pos());
	}
};


gui_halt_route_info_t::gui_halt_route_info_t(const halthandle_t & halt, uint8 catg_index, bool station_mode)
{
	this->halt = halt;
	station_display_mode = station_mode;
	build_halt_list(catg_index, selected_class, station_display_mode);
	line_selected = 0xFFFFFFFFu;
}


void gui_halt_route_info_t::build_halt_list(uint8 catg_index, uint8 g_class, bool station_mode)
{
	halt_list.clear();
	station_display_mode = station_mode;
	if (!halt.is_bound() ||
		(!station_display_mode && (!halt->is_enabled(catg_index) || catg_index == goods_manager_t::INDEX_NONE || catg_index >= goods_manager_t::get_max_catg_index()))) {
		return;
	}

	if (station_display_mode) {
		// all connected stations
		for (uint8 i = 0; i < goods_manager_t::get_max_catg_index(); i++) {
			haltestelle_t::connexions_map *connexions = halt->get_connexions(i, goods_manager_t::get_classes_catg_index(i) - 1);
			if (!connexions->empty()) {
				for(auto &iter : *connexions) {
					halthandle_t a_halt = iter.key;
					if (a_halt.is_bound()) {
						halt_list.insert_unique_ordered(a_halt, RelativeDistanceOrdering(halt->get_basis_pos()));
					}
				}
			}
		}
	}
	else{
		selected_route_catg_index = catg_index;
		if (catg_index != goods_manager_t::INDEX_PAS && catg_index != goods_manager_t::INDEX_MAIL) {
			selected_class = 0;
		}
		else if (g_class == 255) {
			// initialize for passengers and mail
			// Set the most higher wealth class by default. Because the higher class can choose the route of all classes
			selected_class = goods_manager_t::get_info_catg_index(catg_index)->get_number_of_classes() - 1;
		}
		else {
			selected_class = g_class;
		}


		haltestelle_t::connexions_map *connexions = halt->get_connexions(selected_route_catg_index, selected_class);
		if (!connexions->empty()) {
			for(auto &iter : *connexions) {
				halthandle_t a_halt = iter.key;
				if (a_halt.is_bound()) {
					halt_list.insert_unique_ordered(a_halt, RelativeDistanceOrdering(halt->get_basis_pos()));
				}
			}
		}
	}
	halt_list.resize(halt_list.get_count());
}


bool gui_halt_route_info_t::infowin_event(const event_t * ev)
{
	if (!station_display_mode) {
		const unsigned int line = (ev->cy) / (LINESPACE + 1);
		line_selected = 0xFFFFFFFFu;
		if (line >= halt_list.get_count()) {
			return false;
		}

		halthandle_t halt = halt_list[line];
		const koord3d halt_pos = halt->get_basis_pos3d();
		if (!halt.is_bound()) {
			welt->get_viewport()->change_world_position(halt_pos);
			return false;
		}

		if (IS_LEFTRELEASE(ev)) {
			if (ev->cx > 0 && ev->cx < 15) {
				welt->get_viewport()->change_world_position(halt_pos);
			}
			else {
				halt->show_info();
			}
		}
		else if (IS_RIGHTRELEASE(ev)) {
			welt->get_viewport()->change_world_position(halt_pos);
		}
	}
	return false;
}


void gui_halt_route_info_t::recalc_size()
{
	if (size==scr_size(0,0)) {
		const scr_coord_val y_size = halt_list.get_count() ? halt_list.get_count() * (LINESPACE + 1) + D_MARGIN_BOTTOM : LINESPACE*2;
		set_size(scr_size(400, y_size));
	}
	else {
		set_size(size);
	}
}


void gui_halt_route_info_t::draw(scr_coord offset)
{
	build_halt_list(selected_route_catg_index, selected_class, station_display_mode);

	if (station_display_mode) {
		draw_list_by_dest(offset);
	}
	else {
		draw_list_by_catg(offset);
	}
}

void gui_halt_route_info_t::draw_list_by_dest(scr_coord offset)
{
	static cbuffer_t buf;
	int xoff = 0;
	int yoff = 0;
	int max_x = D_DEFAULT_WIDTH;

	FOR(const vector_tpl<halthandle_t>, const dest, halt_list) {
		xoff = D_POS_BUTTON_WIDTH + D_H_SPACE;
		yoff += D_V_SPACE;

		// [distance]
		buf.clear();
		const double km_to_halt = welt->tiles_to_km(shortest_distance(halt->get_next_pos(dest->get_basis_pos()), dest->get_next_pos(halt->get_basis_pos())));

		if (km_to_halt < 1000.0) {
			buf.printf("%.2fkm", km_to_halt);
		}
		else if (km_to_halt < 10000.0) {
			buf.printf("%.1fkm", km_to_halt);
		}
		else {
			buf.printf("%ukm", (uint)km_to_halt);
		}
		xoff += proportional_string_width("000.00km");
		display_proportional_clip_rgb(offset.x + xoff, offset.y + yoff, buf, ALIGN_RIGHT, SYSCOL_TEXT, true);
		xoff += D_H_SPACE;

		// [stop name]
		display_proportional_clip_rgb(offset.x + xoff, offset.y + yoff, dest->get_name(), ALIGN_LEFT, color_idx_to_rgb(dest->get_owner()->get_player_color1()+env_t::gui_player_color_dark), true);

		//TODO: set pos button here

		if (win_get_magic(magic_halt_info + dest.get_id())) {
			display_blend_wh_rgb(offset.x, offset.y + yoff, D_DEFAULT_WIDTH, LINESPACE, SYSCOL_TEXT, 20);
		}

		yoff += LINESPACE;

		for (uint8 i = 0; i < goods_manager_t::get_max_catg_index(); i++) {
			haltestelle_t::connexion* cnx = halt->get_connexions(i, goods_manager_t::get_classes_catg_index(i) - 1)->get(dest);
			if (cnx) {
				const bool is_walking = halt->is_within_walking_distance_of(dest) && !cnx->best_convoy.is_bound() && !cnx->best_line.is_bound();
				uint16 catg_xoff = GOODS_SYMBOL_CELL_WIDTH;
				// [goods catgory]
				const goods_desc_t* info = goods_manager_t::get_info_catg_index(i);
				display_color_img(info->get_catg_symbol(), offset.x + xoff, offset.y + yoff + FIXED_SYMBOL_YOFF, 0, false, false);
				display_proportional_clip_rgb(offset.x + xoff + catg_xoff, offset.y + yoff, translator::translate(info->get_catg_name()), ALIGN_LEFT, SYSCOL_TEXT, true);
				// [travel time]
				catg_xoff += D_BUTTON_WIDTH;
				buf.clear();
				char travelling_time_as_clock[32];
				const uint32 journey_time = cnx->journey_time;
				welt->sprintf_time_tenths(travelling_time_as_clock, sizeof(travelling_time_as_clock), journey_time);

				if(const skin_desc_t* icon = is_walking ? skinverwaltung_t::on_foot : skinverwaltung_t::travel_time) {
					buf.printf("%5s", travelling_time_as_clock);
					display_color_img_with_tooltip(icon->get_image_id(0),offset.x + xoff + catg_xoff,offset.y + yoff + FIXED_SYMBOL_YOFF,0, false, false,translator::translate(is_walking ? "Walking time" : "Travel time and travel speed"));
					catg_xoff += GOODS_SYMBOL_CELL_WIDTH;
				} else {
					buf.printf(translator::translate(is_walking ? "%s mins. walking" : "%s mins. travelling"), travelling_time_as_clock);
					buf.append(", ");
				}
				catg_xoff += display_proportional_clip_rgb(offset.x + xoff + catg_xoff, offset.y + yoff, buf, ALIGN_LEFT, SYSCOL_TEXT, true);

				// [average speed]
				buf.clear();
				buf.printf(" (%2ukm/h) ", journey_time ? kmh_from_meters_and_tenths((int)(km_to_halt * 1000), journey_time) : 0);
				catg_xoff += display_proportional_clip_rgb(offset.x + xoff + catg_xoff, offset.y + yoff, buf, ALIGN_LEFT, SYSCOL_TEXT_WEAK, true);

				// [waiting time]
				buf.clear();
				if (!is_walking) {
					if (skinverwaltung_t::waiting_time) {
						display_color_img_with_tooltip(skinverwaltung_t::waiting_time->get_image_id(0), offset.x + xoff + catg_xoff, offset.y + yoff + FIXED_SYMBOL_YOFF, 0, false, false, translator::translate("Route average waiting time and speed evaluation"));
						catg_xoff += GOODS_SYMBOL_CELL_WIDTH;
					}
					const uint32 waiting_time = cnx->waiting_time;
					if (waiting_time > 0) {
						char waiting_time_as_clock[32];
						welt->sprintf_time_tenths(waiting_time_as_clock, sizeof(waiting_time_as_clock), waiting_time);
						if (!skinverwaltung_t::waiting_time) {
							buf.printf(translator::translate("%s mins. waiting"), waiting_time_as_clock);
						}
						else {
							buf.printf("%5s", waiting_time_as_clock);
						}
					}
					else {
						if (skinverwaltung_t::waiting_time) {
							buf.append("--:--");
						}
						else {
							buf.append(translator::translate("waiting time unknown"));
						}
					}
					catg_xoff += display_proportional_clip_rgb(offset.x + xoff + catg_xoff, offset.y + yoff, buf, ALIGN_LEFT, SYSCOL_TEXT, true);

					// [average schedule speed]
					buf.clear();
					buf.printf(" (%2ukm/h) ", journey_time+waiting_time ? kmh_from_meters_and_tenths((int)(km_to_halt * 1000), journey_time + waiting_time) : 0);
					catg_xoff += display_proportional_clip_rgb(offset.x + xoff + catg_xoff, offset.y + yoff, buf, ALIGN_LEFT, SYSCOL_TEXT_WEAK, true);
					max_x = max(max_x, catg_xoff);
				}
#ifdef DEBUG
				// Who offers the best service?
				{
					// [best line]
					buf.clear();
					player_t *victor = NULL;
					linehandle_t preferred_line = halt->get_preferred_line(dest, i, goods_manager_t::get_classes_catg_index(i) - 1);
					if (preferred_line.is_bound()) {
						buf.printf(translator::translate(" DBG: Preferred line : %s"), preferred_line->get_name());
						victor = preferred_line->get_owner();
					}
					else {
						convoihandle_t preferred_convoy = halt->get_preferred_convoy(dest, i, goods_manager_t::get_classes_catg_index(i) - 1);
						if (preferred_convoy.is_bound()) {
							buf.printf(translator::translate("  DBG: Preferred convoy : %s"), preferred_convoy->get_name());
							victor = preferred_convoy->get_owner();
						}
						else if(!is_walking) {
							buf.append(" DBG: Preferred convoy : ** NOT FOUND! **");
						}
					}
					display_proportional_clip_rgb(offset.x + xoff + catg_xoff, offset.y + yoff, buf, ALIGN_LEFT, victor == NULL ? COL_DANGER : color_idx_to_rgb(victor->get_player_color1()+env_t::gui_player_color_dark), true);
				}
#endif
				yoff += LINESPACE;
			}
		}
		yoff += D_V_SPACE;
	}
	yoff += D_MARGIN_BOTTOM;

	set_size(scr_size(max_x, yoff));
}

void gui_halt_route_info_t::draw_list_by_catg(scr_coord offset)
{
	static cbuffer_t buf;
	int xoff = pos.x;
	int yoff = pos.y;
	int max_x = D_DEFAULT_WIDTH;

	uint8 g_class = goods_manager_t::get_classes_catg_index(selected_route_catg_index) - 1;

	FORX(const vector_tpl<halthandle_t>, const dest, halt_list, yoff += LINESPACE + 1)
	{
		if (!dest.is_bound())
		{
			continue;
		}
		haltestelle_t::connexion* cnx = halt->get_connexions(selected_route_catg_index, g_class)->get(dest);
		xoff = D_POS_BUTTON_WIDTH + D_H_SPACE;

		// [distance]
		buf.clear();
		const double km_to_halt = welt->tiles_to_km(shortest_distance(halt->get_next_pos(dest->get_basis_pos()), dest->get_next_pos(halt->get_basis_pos())));
		const bool is_walking = cnx ? halt->is_within_walking_distance_of(dest) && !cnx->best_convoy.is_bound() && !cnx->best_line.is_bound() : halt->is_within_walking_distance_of(dest);
		if (km_to_halt < 1000.0) {
			buf.printf("%.2fkm", km_to_halt);
		}
		else if(km_to_halt < 10000.0) {
			buf.printf("%.1fkm", km_to_halt);
		}
		else {
			buf.printf("%ukm", (uint)km_to_halt);
		}
		xoff += proportional_string_width("000.00km");
		display_proportional_clip_rgb(offset.x + xoff, offset.y + yoff, buf, ALIGN_RIGHT, is_walking ? SYSCOL_TEXT_WEAK : SYSCOL_TEXT, true);
		if (is_walking && skinverwaltung_t::on_foot) {
			display_color_img(skinverwaltung_t::on_foot->get_image_id(0), offset.x + D_MARGIN_LEFT, offset.y + yoff + FIXED_SYMBOL_YOFF, 0, false, false);
		}
		xoff += D_H_SPACE;

		// [name]
		xoff += max(display_proportional_clip_rgb(offset.x + xoff, offset.y + yoff, dest->get_name(), ALIGN_LEFT, color_idx_to_rgb(dest->get_owner()->get_player_color1()+env_t::gui_player_color_dark), true), D_BUTTON_WIDTH * 2);

		if (!cnx) {
			continue; // After that, it will not be displayed until the connection data is updated.
		}

		// [travel time]
		buf.clear();
		char travelling_time_as_clock[32];
		welt->sprintf_time_tenths(travelling_time_as_clock, sizeof(travelling_time_as_clock), cnx->journey_time);
		if (is_walking) {
			if (skinverwaltung_t::on_foot) {
				buf.printf("%5s", travelling_time_as_clock);
				display_color_img_with_tooltip(skinverwaltung_t::on_foot->get_image_id(0), offset.x + xoff, offset.y + yoff + FIXED_SYMBOL_YOFF, 0, false, false, translator::translate("Walking time"));
				xoff += GOODS_SYMBOL_CELL_WIDTH;
			}
			else {
				buf.printf(translator::translate("%s mins. walking"), travelling_time_as_clock);
				buf.append(", ");
			}
		}
		else {
			if (skinverwaltung_t::travel_time) {
				buf.printf("%5s", travelling_time_as_clock);
				display_color_img(skinverwaltung_t::travel_time->get_image_id(0), offset.x + xoff, offset.y + yoff + FIXED_SYMBOL_YOFF, 0, false, false);
				xoff += GOODS_SYMBOL_CELL_WIDTH;
			}
			else {
				buf.printf(translator::translate("%s mins. travelling"), travelling_time_as_clock);
				buf.append(", ");
			}
		}
		xoff += display_proportional_clip_rgb(offset.x + xoff, offset.y + yoff, buf, ALIGN_LEFT, SYSCOL_TEXT, true);
#ifdef DEBUG
		// [avarage speed]
		buf.clear();
		sint64 average_speed = kmh_from_meters_and_tenths((int)(km_to_halt*1000), cnx->journey_time);
		buf.printf(" (%2ukm/h)", average_speed);
		xoff += display_proportional_clip_rgb(offset.x + xoff, offset.y + yoff, buf, ALIGN_LEFT, SYSCOL_TEXT_WEAK, true);
#endif

		xoff += D_H_SPACE*2;
		// [waiting time]
		buf.clear();
		if (!is_walking){
			if (skinverwaltung_t::waiting_time) {
				display_color_img(skinverwaltung_t::waiting_time->get_image_id(0), offset.x + xoff, offset.y + yoff + FIXED_SYMBOL_YOFF, 0, false, false);
				xoff += GOODS_SYMBOL_CELL_WIDTH;
			}
			if (cnx->waiting_time > 0) {
				char waiting_time_as_clock[32];
				welt->sprintf_time_tenths(waiting_time_as_clock, sizeof(waiting_time_as_clock), cnx->waiting_time);
				if (!skinverwaltung_t::waiting_time) {
					buf.printf(translator::translate("%s mins. waiting"), waiting_time_as_clock);
				}
				else {
					buf.printf("%5s", waiting_time_as_clock);
				}
			}
			else {
				if (skinverwaltung_t::waiting_time) {
					buf.append("--:--");
				}
				else {
					buf.append(translator::translate("waiting time unknown"));
				}
			}
			xoff += display_proportional_clip_rgb(offset.x + xoff, offset.y + yoff, buf, ALIGN_LEFT, SYSCOL_TEXT, true);
#ifdef DEBUG
			// [avarage schedule speed]
			buf.clear();
			sint64 schedule_speed = kmh_from_meters_and_tenths((int)(km_to_halt * 1000), cnx->journey_time + cnx->waiting_time);
			buf.printf(" (%2ukm/h) ", schedule_speed);
			xoff += display_proportional_clip_rgb(offset.x + xoff, offset.y + yoff, buf, ALIGN_LEFT, SYSCOL_TEXT_WEAK, true);
#endif

#ifdef DEBUG
			// Who offers the best service?
			{
				// [best line]
				buf.clear();
				player_t *victor = NULL;
				linehandle_t preferred_line = halt->get_preferred_line(dest, selected_route_catg_index, goods_manager_t::get_classes_catg_index(selected_route_catg_index)-1);
				if (preferred_line.is_bound()) {
					buf.printf(translator::translate(" DBG: Preferred line : %s"), preferred_line->get_name());
					victor = preferred_line->get_owner();
				}
				else {
					convoihandle_t preferred_convoy = halt->get_preferred_convoy(dest, selected_route_catg_index, goods_manager_t::get_classes_catg_index(selected_route_catg_index)-1);
					if (preferred_convoy.is_bound()) {
						buf.printf(translator::translate("  DBG: Preferred convoy : %s"), preferred_convoy->get_name());
						victor = preferred_convoy->get_owner();
					}
					else {
						buf.append(" DBG: Preferred convoy : ** NOT FOUND! **");
					}
				}
				display_proportional_clip_rgb(offset.x + xoff, offset.y + yoff, buf, ALIGN_LEFT, victor == NULL ? COL_DANGER : color_idx_to_rgb(victor->get_player_color1()+env_t::gui_player_color_dark), true);
			}
#endif

		}
		max_x = max(max_x, xoff);
		if (win_get_magic(magic_halt_info + dest.get_id())) {
			display_blend_wh_rgb(offset.x, offset.y + yoff, max_x, LINESPACE, SYSCOL_TEXT, 20);
		}
	}

	if (!halt_list.get_count()) {
		display_proportional_clip_rgb(offset.x + D_MARGIN_LEFT, offset.y + yoff, translator::translate("keine"), ALIGN_LEFT, MN_GREY0, true);
		yoff += LINESPACE;
	}
	yoff += D_MARGIN_BOTTOM;

	set_size(scr_size(max_x, yoff - pos.y));
}
