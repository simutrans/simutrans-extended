/*
 * Copyright (c) 1997 - 2001 Hansjörg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#include "../simworld.h"
#include "../display/simimg.h"
#include "../display/viewport.h"
#include "../simware.h"
#include "../simhalt.h"
#include "../simline.h"
#include "../simconvoi.h"

#include "../descriptor/goods_desc.h"
#include "../bauer/goods_manager.h"

#include "../dataobj/translator.h"
#include "../dataobj/loadsave.h"

#include "../player/simplay.h"

#include "../utils/simstring.h"

#include "halt_detail.h"
#include "../vehicle/simvehicle.h"

#define GOODS_SYMBOL_CELL_WIDTH 14
#define GOODS_WAITING_CELL_WIDTH 60
#define GOODS_WAITING_BAR_BASE_WIDTH 160
#define GOODS_LEAVING_BAR_HEIGHT 2

halt_detail_t::halt_detail_t(halthandle_t halt_) :
	gui_frame_t(halt_->get_name(), halt_->get_owner()),
	halt(halt_),
	scrolly(&pas),
	scrolly_goods(&goods),
	scrolly_lines(&lines),
	pas(halt_),
	goods(halt_),
	lines(halt_)
{
	//this->halt = halt;
	//cont.add_component(&txt_info);

	// fill buffer with halt detail
	//halt_detail_info();
	//txt_info.set_pos(scr_coord(D_MARGIN_LEFT,D_MARGIN_TOP));

	scrolly.set_show_scroll_x(false);

	if(halt->get_pax_enabled() || halt->get_mail_enabled()){
		tabs.add_tab(&scrolly, translator::translate("Passengers/mail"));
	}
	tabs.add_tab(&scrolly_goods, translator::translate("Goods"));
	tabs.add_tab(&scrolly_lines, translator::translate("Services"));
	tabs.set_pos(scr_coord(0, 0));

	add_component(&tabs);
	tabs.add_listener(this);

	set_windowsize(scr_size(D_DEFAULT_WIDTH, D_TITLEBAR_HEIGHT+4+22*(LINESPACE)+D_SCROLLBAR_HEIGHT+2));
	set_min_windowsize(scr_size(D_DEFAULT_WIDTH, D_TITLEBAR_HEIGHT+4+3*(LINESPACE)+D_SCROLLBAR_HEIGHT+2));

	set_resizemode(diagonal_resize);
	resize(scr_coord(0,0));

	cached_active_player=NULL;
}



halt_detail_t::~halt_detail_t()
{
}

/*
halt_detail_goods_t::~halt_detail_goods_t()
{
	while (!posbuttons.empty()) {
		button_t *b = posbuttons.remove_first();
		cont.remove_component(b);
		delete b;
	}
}*/

halt_detail_line_t::~halt_detail_line_t()
{
	/*
	while (!linelabels.empty()) {
		gui_label_t *l = linelabels.remove_first();
		cont.remove_component(l);
		delete l;
	}
	while (!linebuttons.empty()) {
		button_t *b = linebuttons.remove_first();
		cont.remove_component(b);
		delete b;
	}
	while (!convoylabels.empty()) {
		gui_label_t *l = convoylabels.remove_first();
		cont.remove_component(l);
		delete l;
	}
	while (!convoybuttons.empty()) {
		button_t *b = convoybuttons.remove_first();
		cont.remove_component(b);
		delete b;
	}
	while (!label_names.empty()) {
		free(label_names.remove_first());
	}
	*/
}

/*
void halt_detail_t::halt_detail_info()
{
	if (!halt.is_bound()) {
		return;
	}

	//txt_info.recalc_size();
	//cont.set_size( txt_info.get_size() );

	// ok, we have now this counter for pending updates
	cached_line_count = halt->registered_lines.get_count();
	cached_convoy_count = halt->registered_convoys.get_count();
}*/



bool halt_detail_t::action_triggered( gui_action_creator_t *, value_t extra)
{
	if (halt.is_bound()) {
		/*
		if (extra.i & ~1) {
			koord k = *(const koord *)extra.p;
			if (k.x >= 0) {
				// goto button pressed
				welt->get_viewport()->change_world_position(koord3d(k, welt->max_hgt(k)));
			}
		}
		*/
		
	}
	return false;
}




void halt_detail_t::draw(scr_coord pos, scr_size size)
{
	if(halt.is_bound()) {

		if( cached_active_player != welt->get_active_player()	||  halt->registered_lines.get_count()!=cached_line_count  ||
			halt->registered_convoys.get_count()!=cached_convoy_count  ||
		    welt->get_ticks() - update_time > 10000) {
			// fill buffer with halt detail
			//halt_detail_info();

			cached_active_player=welt->get_active_player();
			update_time = welt->get_ticks();
		}
	}
	gui_frame_t::draw(pos, size);
}



void halt_detail_t::set_windowsize(scr_size size)
{
	gui_frame_t::set_windowsize(size);
	tabs.set_size(get_client_windowsize() - tabs.get_pos() - scr_size(2,1));
}



halt_detail_t::halt_detail_t():
	gui_frame_t("", NULL),
	scrolly(&pas),
	scrolly_goods(&goods),
	scrolly_lines(&lines),
	pas(halthandle_t()),
	goods(halthandle_t()),
	lines(halthandle_t())
{
	halt = (halthandle_t());
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
	KOORD_VAL y = offset.y;

	int base_capacity = halt->get_capacity(good_category->get_catg_index()) ? max(halt->get_capacity(good_category->get_catg_index()), 10) : 10; // Note that capacity may have 0 even if pax_enabled
	int transferring_sum = 0;
	bool served = false;
	int left = 0;

	display_color_img(good_category == goods_manager_t::mail ? skinverwaltung_t::mail->get_image_id(0) : skinverwaltung_t::passengers->get_image_id(0), offset.x, y, 0, false, false);
	for (int i = 0; i < good_category->get_number_of_classes(); i++)
	{
		if (halt->get_connexions(good_category->get_catg_index(), i)->empty())
		{
			served = false;
		}
		else {
			served = true;
		}
		pas_info.append(good_category->get_translated_wealth_name(i));
		display_proportional_clip(offset.x + GOODS_SYMBOL_CELL_WIDTH, y, pas_info, ALIGN_LEFT, SYSCOL_TEXT, true);
		pas_info.clear();

		if (served || halt->get_ware_summe(good_category, i)) {
			pas_info.append(halt->get_ware_summe(good_category, i));
		}
		else {
			pas_info.append("-");
		}
		display_proportional_clip(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH, y, pas_info, ALIGN_RIGHT, SYSCOL_TEXT, true);
		pas_info.clear();

		int transfers_out = halt->get_leaving_goods_sum(good_category, i);
		int transfers_in = halt->get_transferring_goods_sum(good_category, i) - transfers_out;
		if (served || halt->get_transferring_goods_sum(good_category, i))
		{
			pas_info.printf("%4u/%4u", transfers_in, transfers_out);
		}
		else {
			pas_info.append("-");
		}
		left += display_proportional_clip(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 5, y, pas_info, ALIGN_RIGHT, SYSCOL_TEXT, true);
		pas_info.clear();

		// color bar
		COLOR_VAL overlay_color = i < good_category->get_number_of_classes()/2 ? COL_BLACK : COL_WHITE;
		uint8 overlay_transparency = abs(good_category->get_number_of_classes()/2 - i) * 7;
		int bar_width = (halt->get_ware_summe(good_category, i) * GOODS_WAITING_BAR_BASE_WIDTH) / base_capacity;
		// transferring bar
		display_fillbox_wh_clip(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 10, y + 1, (transfers_in * GOODS_WAITING_BAR_BASE_WIDTH / base_capacity) + bar_width, 6, MN_GREY0, true);
		transferring_sum += halt->get_transferring_goods_sum(good_category, i);
		// leaving bar
		display_fillbox_wh_clip(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 10, y + 8, transfers_out * GOODS_WAITING_BAR_BASE_WIDTH / base_capacity, GOODS_LEAVING_BAR_HEIGHT, MN_GREY0, true);
		// waiting bar
		display_fillbox_wh_clip(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 10, y + 1, bar_width, 6, good_category->get_color(), true);
		display_blend_wh(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 10, y + 1, bar_width, 6, overlay_color, overlay_transparency);
		display_blend_wh(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 10, y + 6, bar_width, 1, COL_BLACK, 10);

		y += LINESPACE + GOODS_LEAVING_BAR_HEIGHT+1;

	}
	y += 2;
	display_direct_line(offset.x + GOODS_SYMBOL_CELL_WIDTH, y, offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width, y, MN_GREY1);
	display_direct_line(offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + 4, y, offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + GOODS_WAITING_CELL_WIDTH, y, MN_GREY1);
	display_direct_line(offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + GOODS_WAITING_CELL_WIDTH + 4, y, offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + GOODS_WAITING_CELL_WIDTH * 2 + 5, y, MN_GREY1);
	display_direct_line(offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + GOODS_WAITING_CELL_WIDTH * 2 + 5 + 4, y, offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + GOODS_WAITING_CELL_WIDTH * 2 + 5 + GOODS_WAITING_BAR_BASE_WIDTH, y, MN_GREY1);
	y += 4;

	// total
	COLOR_VAL color;
	color = halt->is_overcrowded(good_category->get_catg_index()) ? COL_DARK_PURPLE : SYSCOL_TEXT;
	pas_info.append(halt->get_ware_summe(good_category));
	display_proportional_clip(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH, y, pas_info, ALIGN_RIGHT, color, true);
	pas_info.clear();

	pas_info.append(transferring_sum);
	left += display_proportional_clip(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 5, y, pas_info, ALIGN_RIGHT, SYSCOL_TEXT, true);
	pas_info.clear();

	pas_info.printf("  %u ", halt->get_ware_summe(good_category) + transferring_sum);
	pas_info.printf(translator::translate("(max: %u)"), halt->get_capacity(good_category->get_catg_index()));
	left += display_proportional_clip(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 10, y, pas_info, ALIGN_LEFT, SYSCOL_TEXT, true);
	pas_info.clear();
}


void halt_detail_pas_t::draw(scr_coord offset)
{
	// keep previous maximum width
	int x_size = get_size().w - 51 - pos.x;
	int top = offset.y + D_MARGIN_TOP;
	offset.x+=D_MARGIN_LEFT;
	int left = 0;

	if (halt.is_bound()) {
		//transfertime
		pas_info.clear();
		pas_info.append(translator::translate("Transfer time: "));
		char transfer_time_as_clock[32];
		welt->sprintf_time_tenths(transfer_time_as_clock, sizeof(transfer_time_as_clock), (halt->get_transfer_time()));
		pas_info.append(transfer_time_as_clock);
		display_proportional_clip(offset.x, top, pas_info, ALIGN_LEFT, SYSCOL_TEXT, true);
		pas_info.clear();
		top += LINESPACE * 2;

		// Calculate width of class name cell
		uint8 class_name_cell_width = 0;
		uint8 pass_classes = goods_manager_t::passengers->get_number_of_classes();
		uint8 mail_classes = goods_manager_t::mail->get_number_of_classes();
		for (uint8 i = 0; i < pass_classes; i++) {
			class_name_cell_width = max(proportional_string_width(goods_manager_t::passengers->get_translated_wealth_name(i)), class_name_cell_width);
		}
		for (uint8 i = 0; i < mail_classes; i++) {
			class_name_cell_width = max(proportional_string_width(goods_manager_t::mail->get_translated_wealth_name(i)), class_name_cell_width);
		}

		display_proportional_clip(offset.x + GOODS_SYMBOL_CELL_WIDTH, top, translator::translate("hd_wealth"), ALIGN_LEFT, SYSCOL_TEXT, true);
		display_proportional_clip(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH, top, translator::translate("hd_waiting"), ALIGN_RIGHT, SYSCOL_TEXT, true);
		display_proportional_clip(offset.x + class_name_cell_width + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH*2 + 5, top, translator::translate("hd_transfers"), ALIGN_RIGHT, SYSCOL_TEXT, true);
		top += LINESPACE + 2;

		display_direct_line(offset.x + GOODS_SYMBOL_CELL_WIDTH, top, offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width, top, MN_GREY1);
		display_direct_line(offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width+4, top, offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + GOODS_WAITING_CELL_WIDTH, top, MN_GREY1);
		display_direct_line(offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + GOODS_WAITING_CELL_WIDTH+4, top, offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + GOODS_WAITING_CELL_WIDTH*2 + 5, top, MN_GREY1);
		display_direct_line(offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + GOODS_WAITING_CELL_WIDTH*2 + 5+4, top, offset.x + GOODS_SYMBOL_CELL_WIDTH + class_name_cell_width + GOODS_WAITING_CELL_WIDTH*2 + 5 + GOODS_WAITING_BAR_BASE_WIDTH, top, MN_GREY1);
		top += 4;

		if (halt->get_pax_enabled()) {
			draw_class_table(scr_coord(offset.x, top), class_name_cell_width, goods_manager_t::passengers);
			top += (pass_classes+1) * (LINESPACE + GOODS_LEAVING_BAR_HEIGHT +1) + 6;

			top += LINESPACE * 2;
		}

		if (halt->get_mail_enabled()) {
			draw_class_table(scr_coord(offset.x, top), class_name_cell_width, goods_manager_t::mail);
			top += (mail_classes + 1) * (LINESPACE + GOODS_LEAVING_BAR_HEIGHT + 1) + 6;
		}
		
		top += D_MARGIN_TOP;

		scr_size size(max(x_size + pos.x, get_size().w), top - offset.y);
		if (size != get_size()) {
			set_size(size);
		}
	}
}


halt_detail_goods_t::halt_detail_goods_t(halthandle_t halt)
{
	this->halt = halt;
}

bool halt_detail_goods_t::infowin_event(const event_t * ev)
{
	const unsigned int line = (ev->cy) / (LINESPACE + 1);
	line_selected = 0xFFFFFFFFu;
	//line_selected = -1;
	if (line >= fab_list.get_count()) {
		return false;
	}

	//const koord3d fab_pos = (line<fab_list->at(line))->get_pos();
	fabrik_t* fab = fab_list.at(line);
	//fabrik_t* fab = line<fab_list>;
	//const koord3d fab_pos = fab_list.at(line)->get_pos();
	const koord3d fab_pos = fab->get_pos();
	if (fab_pos == koord3d::invalid) {
		return false;
	}

	// un-press goto button
	if (ev->button_state > 0 && ev->cx > 0 && ev->cx < 15) {
		line_selected = line;
	}

	if (IS_LEFTRELEASE(ev)) {
		if (ev->cx > 0 && ev->cx < 15) {
			welt->get_viewport()->change_world_position(fab_pos);
		}
		else {
			fab_list.at(line)->show_info();
		}
	}
	else if (IS_RIGHTRELEASE(ev)) {
		welt->get_viewport()->change_world_position(fab_pos);
	}
	return false;
}

void halt_detail_goods_t::draw(scr_coord offset)
{
	// TODO: change this to "nearby" info and add nearby atraction building info. and more population and jobs info


	// keep previous maximum width
	int x_size = get_size().w - 51 - pos.x;
	int top = offset.y + D_MARGIN_TOP;
	offset.x += D_MARGIN_LEFT;
	int left = GOODS_SYMBOL_CELL_WIDTH;

	line_selected = 0xFFFFFFFFu;

	goods_info.clear();
	if (halt.is_bound()) {
		if (halt->get_ware_enabled())
		{
			int base_capacity = halt->get_capacity(2) ? max(halt->get_capacity(2),10) : 10; // Note that capacity may have 0 even if enabled
			int waiting_sum = 0;
			int transferring_sum = 0;
			int catg_cnt = 0;

			// transfertime
			goods_info.clear();
			goods_info.append(translator::translate("Transshipment time: "));
			char transshipment_time_as_clock[32];
			welt->sprintf_time_tenths(transshipment_time_as_clock, sizeof(transshipment_time_as_clock), (halt->get_transshipment_time()));
			goods_info.append(transshipment_time_as_clock);
			display_proportional_clip(offset.x, top, goods_info, ALIGN_LEFT, SYSCOL_TEXT, true);
			goods_info.clear();
			top += LINESPACE * 2;

			// Waiting goods info
			display_proportional_clip(offset.x + GOODS_SYMBOL_CELL_WIDTH, top, translator::translate("hd_category"), ALIGN_LEFT, SYSCOL_TEXT, true);
			display_proportional_clip(offset.x + D_BUTTON_WIDTH + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH, top, translator::translate("hd_waiting"), ALIGN_RIGHT, SYSCOL_TEXT, true);
			display_proportional_clip(offset.x + D_BUTTON_WIDTH + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 5, top, translator::translate("hd_transship"), ALIGN_RIGHT, SYSCOL_TEXT, true);
			top += LINESPACE + 2;

			display_direct_line(offset.x + GOODS_SYMBOL_CELL_WIDTH, top, offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH, top, MN_GREY1);
			display_direct_line(offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + 4, top, offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GOODS_WAITING_CELL_WIDTH, top, MN_GREY1);
			display_direct_line(offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GOODS_WAITING_CELL_WIDTH + 4, top, offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 5, top, MN_GREY1);
			display_direct_line(offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 5 + 4, top, offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 5 + GOODS_WAITING_BAR_BASE_WIDTH, top, MN_GREY1);
			top += 4;

			uint32 max_capacity = halt->get_capacity(2);
			const uint8 max_classes = max(goods_manager_t::passengers->get_number_of_classes(), goods_manager_t::mail->get_number_of_classes());
			for (uint i = 0; i < goods_manager_t::get_max_catg_index(); i++) {
				if (i == goods_manager_t::INDEX_PAS || i == goods_manager_t::INDEX_MAIL)
				{
					continue;
				}

				const goods_desc_t* info = goods_manager_t::get_info_catg_index(i);
				goods_info.append(translator::translate(info->get_catg_name()));

				const goods_desc_t *wtyp = goods_manager_t::get_info(i);
				if (halt->gibt_ab(info)) {
					uint32 waiting_sum_catg = 0;
					uint32 transship_sum = 0;
					uint32 transship_in_catg = 0;
					uint32 leaving_sum_catg = 0;

					// category symbol
					display_color_img(info->get_catg_symbol(), offset.x, top, 0, false, false);

					display_proportional_clip(offset.x + GOODS_SYMBOL_CELL_WIDTH, top, goods_info, ALIGN_LEFT, SYSCOL_TEXT, true);
					goods_info.clear();

					int bar_offset_left = 0;
					switch(i) {
						case 0:
							waiting_sum_catg = halt->get_ware_summe(wtyp);
							display_fillbox_wh_clip(offset.x + D_BUTTON_WIDTH + bar_offset_left + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 10, top + 1, halt->get_ware_summe(wtyp) * GOODS_WAITING_BAR_BASE_WIDTH / base_capacity, 6, wtyp->get_color(), true);
							display_blend_wh(offset.x + D_BUTTON_WIDTH + bar_offset_left + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 10, top + 6, halt->get_ware_summe(wtyp) * GOODS_WAITING_BAR_BASE_WIDTH / base_capacity, 1, COL_BLACK, 10);
							bar_offset_left = halt->get_ware_summe(wtyp) * GOODS_WAITING_BAR_BASE_WIDTH / base_capacity;
							leaving_sum_catg = halt->get_leaving_goods_sum(wtyp, 0);
							transship_in_catg = halt->get_transferring_goods_sum(wtyp, 0) - leaving_sum_catg;
							break;
						default:
							const uint8  count = goods_manager_t::get_count();
							for (uint32 j = 3; j < count; j++) {
								goods_desc_t const* const wtyp2 = goods_manager_t::get_info(j);
								if (wtyp2->get_catg_index() != i) {
									continue;
								}
								waiting_sum_catg += halt->get_ware_summe(wtyp2);
								leaving_sum_catg += halt->get_leaving_goods_sum(wtyp2, 0);
								transship_in_catg += halt->get_transferring_goods_sum(wtyp2, 0) - halt->get_leaving_goods_sum(wtyp2, 0);

								// color bar
								int bar_width = halt->get_ware_summe(wtyp2) * GOODS_WAITING_BAR_BASE_WIDTH / base_capacity;

								// waiting bar
								if (bar_width) {
									display_fillbox_wh_clip(offset.x + D_BUTTON_WIDTH + bar_offset_left + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 10, top + 1, bar_width, 6, wtyp2->get_color(), true);
									display_blend_wh(offset.x + D_BUTTON_WIDTH + bar_offset_left + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 10, top + 6, bar_width, 1, COL_BLACK, 10);
								}
								bar_offset_left += bar_width;
							}
							break;
					}
					transship_sum += leaving_sum_catg + transship_in_catg;
					// transferring bar
					display_fillbox_wh_clip(offset.x + D_BUTTON_WIDTH + bar_offset_left + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 10, top + 1, transship_in_catg * GOODS_WAITING_BAR_BASE_WIDTH / base_capacity, 6, MN_GREY0, true);
					// leaving bar
					display_fillbox_wh_clip(offset.x + D_BUTTON_WIDTH + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 10, top + 8, leaving_sum_catg * GOODS_WAITING_BAR_BASE_WIDTH / base_capacity, GOODS_LEAVING_BAR_HEIGHT, MN_GREY0, true);

					//waiting
					goods_info.append(waiting_sum_catg);
					display_proportional_clip(offset.x + D_BUTTON_WIDTH + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH, top, goods_info, ALIGN_RIGHT, SYSCOL_TEXT, true);
					goods_info.clear();
					waiting_sum += waiting_sum_catg;

					//transfer
					goods_info.printf("%4u/%4u", transship_in_catg, leaving_sum_catg);
					//goods_info.append(transship_sum_catg);
					left += display_proportional_clip(offset.x + D_BUTTON_WIDTH + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 5, top, goods_info, ALIGN_RIGHT, SYSCOL_TEXT, true);
					goods_info.clear();
					top += LINESPACE + GOODS_LEAVING_BAR_HEIGHT + 1;
					catg_cnt++;
				}
				goods_info.clear();
			}
			if (!catg_cnt) {
				// There is no data until connection data is updated, or no freight service has been operated
				if (skinverwaltung_t::alerts) {
					display_color_img(skinverwaltung_t::alerts->get_image_id(2), offset.x + D_BUTTON_WIDTH, top, 0, false, false);
				}
				display_proportional_clip(offset.x + D_BUTTON_WIDTH + GOODS_SYMBOL_CELL_WIDTH, top, translator::translate("no data"), ALIGN_LEFT, MN_GREY0, true);
				top += LINESPACE;
			}

			top += 2;

			display_direct_line(offset.x + GOODS_SYMBOL_CELL_WIDTH, top, offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH, top, MN_GREY1);
			display_direct_line(offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + 4, top, offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GOODS_WAITING_CELL_WIDTH, top, MN_GREY1);
			display_direct_line(offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GOODS_WAITING_CELL_WIDTH + 4, top, offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 5, top, MN_GREY1);
			display_direct_line(offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 5 + 4, top, offset.x + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 5 + GOODS_WAITING_BAR_BASE_WIDTH, top, MN_GREY1);
			top += 4;

			// total
			COLOR_VAL color;
			color = halt->is_overcrowded(2) ? COL_DARK_PURPLE : SYSCOL_TEXT;
			goods_info.append(waiting_sum);
			display_proportional_clip(offset.x + D_BUTTON_WIDTH + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH, top, goods_info, ALIGN_RIGHT, color, true);
			goods_info.clear();

			goods_info.append(transferring_sum);
			left += display_proportional_clip(offset.x + D_BUTTON_WIDTH + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 5, top, goods_info, ALIGN_RIGHT, SYSCOL_TEXT, true);
			goods_info.clear();

			goods_info.printf("  %u ", waiting_sum + transferring_sum);
			goods_info.printf(translator::translate("(max: %u)"), halt->get_capacity(2));
			left += display_proportional_clip(offset.x + D_BUTTON_WIDTH + GOODS_SYMBOL_CELL_WIDTH + GOODS_WAITING_CELL_WIDTH * 2 + 10, top, goods_info, ALIGN_LEFT, SYSCOL_TEXT, true);
			goods_info.clear();

			top += LINESPACE * 2;
		}

		/*
		while (!posbuttons.empty()) {
			button_t *b = posbuttons.remove_first();
			cont.remove_component(b);
			delete b;
		}
		while (!label_names.empty()) {
			free(label_names.remove_first());
		}*/
		goods_info.clear();

		const slist_tpl<fabrik_t *> & fab_list = halt->get_fab_list();
		slist_tpl<const goods_desc_t *> nimmt_an; // accept
		slist_tpl<const goods_desc_t *> active_product;
		slist_tpl<const goods_desc_t *> inactive_product;

		// Connected industries
		display_proportional_clip(offset.x, top, translator::translate("Fabrikanschluss"), ALIGN_LEFT, SYSCOL_TEXT, true);
		top += LINESPACE;


		left = D_POS_BUTTON_WIDTH;

		if (!fab_list.empty()) {
			clip_dimension const cd = display_get_clip_wh();
			const int start = cd.y - LINESPACE - 1;
			const int end = cd.yy + LINESPACE + 1;

			//int xoff = offset.x;
			//int yoff = top;

			uint32 sel = line_selected; // dummy
			FORX(slist_tpl<fabrik_t*>, const fab, fab_list, top += LINESPACE + 1) {
				if (top >= end) break;
				// skip invisible lines
				if (top < start) continue;

				//make_buttons(posbuttons, fab->get_lieferziele(), yoff, fab_list, this);

				//FOR(slist_tpl<fabrik_t*>, const fab, fab_list) {
				//const koord3d fab_pos = fab->get_pos();
				// target button ...
				//pb = new button_t();
				//pb->init(button_t::posbutton, NULL, scr_coord(offset.x, yoff));
				//pb->set_typ(button_t::posbutton);
				//pb->set_targetpos(fab_pos);
				//pb->set_pos(scr_coord(offset.x, top));
				//pb->add_listener(this);
				//posbuttons.append(pb);
				//cont.add_component(pb);
				//
				// goto button
				//bool selected = welt->get_viewport()->is_on_center(fab_pos);
				//bool selected = sel == 0 || welt->get_viewport()->is_on_center(fab->get_pos());
				display_img_aligned(gui_theme_t::pos_button_img[ sel==0 ], scr_rect(offset.x, top, D_POS_BUTTON_WIDTH, LINESPACE), ALIGN_CENTER_V | ALIGN_CENTER_H, true);
				sel --;

				unsigned indikatorfarbe = fabrik_t::status_to_color[fab->get_status() % fabrik_t::staff_shortage];

				if (fab->get_status() >= fabrik_t::staff_shortage) {
					display_ddd_box_clip(offset.x + left + 1, top + 2, D_INDICATOR_WIDTH, D_INDICATOR_HEIGHT+2, COL_STAFF_SHORTAGE, COL_STAFF_SHORTAGE);
				}
				display_fillbox_wh_clip(offset.x + left + 2, top + 3, D_INDICATOR_WIDTH-2, D_INDICATOR_HEIGHT, indikatorfarbe, true);

				goods_info.printf("%s <%d, %d>", translator::translate(fab->get_name()), fab->get_pos().x, fab->get_pos().y);
				left+=display_proportional_clip(offset.x + D_POS_BUTTON_WIDTH + D_INDICATOR_WIDTH + D_H_SPACE, top, goods_info, ALIGN_LEFT, SYSCOL_TEXT, true) + D_INDICATOR_WIDTH + D_H_SPACE*2;
				goods_info.clear();

				left = max(left, D_BUTTON_WIDTH*2 + D_INDICATOR_WIDTH);

				int has_input_output=0;
				FOR(array_tpl<ware_production_t>, const& i, fab->get_input()) {
					goods_desc_t const* const ware = i.get_typ();
					if (skinverwaltung_t::input_output && !has_input_output) {
						display_color_img(skinverwaltung_t::input_output->get_image_id(0), offset.x + left, top, 0, false, false);
						left += GOODS_SYMBOL_CELL_WIDTH;
					}
					// input goods color square box
					display_ddd_box_clip(offset.x + left, top, 8, 8, MN_GREY0, MN_GREY4);
					display_fillbox_wh_clip(offset.x + left + 1, top + 1, 6, 6, ware->get_color(), true);
					left += GOODS_SYMBOL_CELL_WIDTH-2;

					if (!nimmt_an.is_contained(ware)) {
						nimmt_an.append(ware);
					}
					has_input_output++;
				}

				if (has_input_output<4) {
					left += GOODS_SYMBOL_CELL_WIDTH * (4- has_input_output);
				}
				has_input_output = 0;
				FOR(array_tpl<ware_production_t>, const& i, fab->get_output()) {
					goods_desc_t const* const ware = i.get_typ();
					if (skinverwaltung_t::input_output && !has_input_output) {
						display_color_img(skinverwaltung_t::input_output->get_image_id(1), offset.x + left, top, 0, false, false);
						left += GOODS_SYMBOL_CELL_WIDTH;
					}
					// output goods color square box
					display_ddd_box_clip(offset.x + left, top, 8, 8, MN_GREY0, MN_GREY4);
					display_fillbox_wh_clip(offset.x + left + 1, top + 1, 6, 6, ware->get_color(), true);
					left += GOODS_SYMBOL_CELL_WIDTH - 2;

					if (!active_product.is_contained(ware)) {

						if ((fab->get_status() % fabrik_t::no_material || fab->get_status() % fabrik_t::material_shortage) && !i.menge) {
							// this factory is not in operation
							if (!inactive_product.is_contained(ware)) {
								inactive_product.append(ware);
							}
						}
						else {
							active_product.append(ware);
						}
					}
					has_input_output++;
				}

				left = D_POS_BUTTON_WIDTH;
				//top += LINESPACE+1;
			}
			//cont.draw(scr_coord(offset.x, top));
		}
		else {
			display_proportional_clip(offset.x + left + D_MARGIN_LEFT, top, translator::translate("keine"), ALIGN_LEFT, MN_GREY0, true);
			top += LINESPACE;
		}

		top += LINESPACE;

		// Goods needed by nearby industries
		//goods_info.append(translator::translate("Angenommene Waren"));
		//display_proportional_clip(offset.x, top, goods_info, ALIGN_LEFT, SYSCOL_TEXT, true);
		//goods_info.clear();
		//top += LINESPACE;

		left = GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH*1.5; // 2nd col x offset
		if ((!nimmt_an.empty() || !active_product.empty() || !inactive_product.empty()) && halt->get_ware_enabled()) {
			int input_cnt = 0;
			int output_cnt = 0;
			if (skinverwaltung_t::input_output) {
				display_color_img(skinverwaltung_t::input_output->get_image_id(0), offset.x, top, 0, false, false);
				display_color_img(skinverwaltung_t::input_output->get_image_id(1), offset.x + left, top, 0, false, false);
			}
			display_proportional_clip(skinverwaltung_t::input_output ? offset.x + GOODS_SYMBOL_CELL_WIDTH :  offset.x, top, translator::translate("Needed"), ALIGN_LEFT, SYSCOL_TEXT, true);
			display_proportional_clip(skinverwaltung_t::input_output ? offset.x + GOODS_SYMBOL_CELL_WIDTH + left : offset.x + left, top, translator::translate("Products"), ALIGN_LEFT, SYSCOL_TEXT, true);
			top += LINESPACE;
			top += 2;

			display_direct_line(offset.x, top, offset.x + left-4, top, MN_GREY1);
			display_direct_line(offset.x + left, top, offset.x + left + GOODS_SYMBOL_CELL_WIDTH + D_BUTTON_WIDTH * 1.5, top, MN_GREY1);
			top += 4;

			for (uint32 i = 0; i < goods_manager_t::get_count(); i++) {
				const goods_desc_t *ware = goods_manager_t::get_info(i);
				// inuput
				if (nimmt_an.is_contained(ware)) {
					// category symbol
					display_color_img(ware->get_catg_symbol(), offset.x, top + LINESPACE*input_cnt, 0, false, false);
					// goods color
					display_ddd_box_clip(offset.x + GOODS_SYMBOL_CELL_WIDTH, top + LINESPACE * input_cnt, 8, 8, MN_GREY0, MN_GREY4);
					display_fillbox_wh_clip(offset.x + GOODS_SYMBOL_CELL_WIDTH + 1, top + 1 + LINESPACE * input_cnt, 6, 6, ware->get_color(), true);
					// goods name
					display_proportional_clip(offset.x + GOODS_SYMBOL_CELL_WIDTH*2 - 2, top + LINESPACE * input_cnt, translator::translate(ware->get_name()), ALIGN_LEFT, SYSCOL_TEXT, true);
					input_cnt++;
				}

				//output
				if (active_product.is_contained(ware) || inactive_product.is_contained(ware)) {
					// category symbol
					display_color_img(ware->get_catg_symbol(), offset.x + left, top + LINESPACE * output_cnt, 0, false, false);
					// goods color
					display_ddd_box_clip(offset.x + left + GOODS_SYMBOL_CELL_WIDTH, top + LINESPACE * output_cnt, 8, 8, MN_GREY0, MN_GREY4);
					display_fillbox_wh_clip(offset.x + left + GOODS_SYMBOL_CELL_WIDTH + 1, top + 1 + LINESPACE * output_cnt, 6, 6, ware->get_color(), true);
					// goods name
					COLOR_VAL text_color;
					display_proportional_clip(offset.x + left + GOODS_SYMBOL_CELL_WIDTH*2 - 2, top + LINESPACE * output_cnt, translator::translate(ware->get_name()), ALIGN_LEFT, text_color = active_product.is_contained(ware) ? SYSCOL_TEXT : MN_GREY0, true);
					output_cnt++;
				}
			}

			if (nimmt_an.empty()) {
				display_proportional_clip(offset.x + D_MARGIN_LEFT, top, translator::translate("keine"), ALIGN_LEFT, MN_GREY0, true);
			}
			if (active_product.empty() && inactive_product.empty()) {
				display_proportional_clip(offset.x + left + D_MARGIN_LEFT, top, translator::translate("keine"), ALIGN_LEFT, MN_GREY0, true);
			}
			top += LINESPACE * max(input_cnt, output_cnt)+1;
		}
		top += LINESPACE;
		goods_info.clear();

		scr_size size(max(x_size + pos.x, get_size().w), top-offset.y);
		if (size != get_size()) {
			set_size(size);
		}
	}
}



/*
bool halt_detail_goods_t::action_triggered(gui_action_creator_t *, value_t extra)
{

	if (extra.i & ~1) {
		koord k = *(const koord *)extra.p;
		if (k.x >= 0) {
			// goto button pressed
			welt->get_viewport()->change_world_position(koord3d(k, welt->max_hgt(k)));
		}
	}
	return true;
}*/


halt_detail_line_t::halt_detail_line_t(halthandle_t halt)
{
	this->halt = halt;
	//posbuttons = NULL;
}

/*
 * Draw the component
 * @author Ranran
 */
void halt_detail_line_t::draw(scr_coord offset)
{
	// keep previous maximum width
	int x_size = get_size().w - 51 - pos.x;
	karte_t *welt = world();
	int top = offset.y + D_MARGIN_TOP;
	offset.x += D_MARGIN_LEFT;
	//int total_height = 0;
	line_selected = 0xFFFFFFFFu;

	line_info.clear();
	if (halt.is_bound()) {

		/*
		while (!linelabels.empty()) {
			gui_label_t *l = linelabels.remove_first();
			cont.remove_component(l);
			delete l;
		}
		while (!linebuttons.empty()) {
			button_t *b = linebuttons.remove_first();
			cont.remove_component(b);
			delete b;
		}
		while (!convoylabels.empty()) {
			gui_label_t *l = convoylabels.remove_first();
			cont.remove_component(l);
			delete l;
		}
		while (!convoybuttons.empty()) {
			button_t *b = convoybuttons.remove_first();
			cont.remove_component(b);
			delete b;
		}
		*/

		display_proportional_clip(offset.x, top, translator::translate("Lines serving this stop"), ALIGN_LEFT, SYSCOL_TEXT, true);
		top += LINESPACE;
		int offset_y = top;

		if (!halt->registered_lines.empty()) {
			uint32 sel = line_selected; // dummy
			for (unsigned int i = 0; i < halt->registered_lines.get_count(); i++) {
				int offset_left = offset.x + D_BUTTON_HEIGHT;
				// Line buttons only if owner ...
				if (welt->get_active_player() == halt->registered_lines[i]->get_owner()) {
					button_t *b = new button_t();
					b->init(button_t::posbutton, NULL, scr_coord(offset.x, top));
					b->set_targetpos(koord(-1, i));
					b->add_listener(this);
					linebuttons.append(b);
					cont.add_component(b);
					// dummy
					display_img_aligned(gui_theme_t::pos_button_img[sel == 0], scr_rect(offset.x, top, D_POS_BUTTON_WIDTH, LINESPACE), ALIGN_CENTER_V | ALIGN_CENTER_H, true);
					sel--;
				}
				/*
				// Line labels with color of player
				label_names.append(strdup(halt->registered_lines[i]->get_name()));
				gui_label_t *l = new gui_label_t(label_names.back(), PLAYER_FLAG | (halt->registered_lines[i]->get_owner()->get_player_color1() + 0));
				l->set_pos(scr_coord(D_MARGIN_LEFT + D_BUTTON_HEIGHT + D_H_SPACE, top));
				linelabels.append(l);
				cont.add_component(l);
				*/

				// line name, convoy count(overcrowd->purple),
				offset_left += display_proportional_clip(offset_left, top, strdup(halt->registered_lines[i]->get_name()), ALIGN_LEFT, halt->registered_lines[i]->get_owner()->get_player_color1(), true) + D_H_SPACE*2;

				// line handling goods (symbol)
				FOR(minivec_tpl<uint8>, const catg_index, halt->registered_lines[i]->get_goods_catg_index()) {
					uint8 temp = catg_index;
					display_color_img(goods_manager_t::get_info_catg_index(temp)->get_catg_symbol(), offset_left, top, 0, false, false);
					offset_left += GOODS_SYMBOL_CELL_WIDTH;
				}

				offset_left = max(offset_left + D_H_SPACE, offset.x + D_BUTTON_WIDTH * 3);

				// convoy count(overcrowd->purple)
				offset_left += display_proportional_clip(offset_left, top, "(", ALIGN_LEFT, SYSCOL_TEXT, true);
				line_info.printf(translator::translate("%d convois"), halt->registered_lines[i]->count_convoys());
				offset_left += display_proportional_clip(offset_left, top, line_info, ALIGN_LEFT, halt->registered_lines[i]->has_overcrowded() ? COL_DARK_PURPLE : SYSCOL_TEXT, true);
				line_info.clear();
				offset_left += display_proportional_clip(offset_left, top, ", ", ALIGN_LEFT, SYSCOL_TEXT, true);

				// line service frequency. NOTE: Displayed only if pakset has a symbol due to display space
				if (skinverwaltung_t::service_frequency) {
					display_color_img(skinverwaltung_t::service_frequency->get_image_id(0), offset_left, top, 0, false, false);
					offset_left += GOODS_SYMBOL_CELL_WIDTH;
					if (halt->registered_lines[i]->get_service_frequency())
					{
						char as_clock[32];
						welt->sprintf_ticks(as_clock, sizeof(as_clock), halt->registered_lines[i]->get_service_frequency());
						line_info.append(as_clock);
						offset_left += display_proportional_clip(offset_left, top, line_info, ALIGN_LEFT, halt->registered_lines[i]->get_state() == simline_t::line_missing_scheduled_slots ? halt->registered_lines[i]->get_state_color() : SYSCOL_TEXT, true);
						line_info.clear();
					}
					else {
						offset_left += display_proportional_clip(offset_left, top, translator::translate("????"), ALIGN_LEFT, MN_GREY0, true);
					}
					offset_left += display_proportional_clip(offset_left, top, ")", ALIGN_LEFT, SYSCOL_TEXT, true) + D_H_SPACE;
				}


				top += LINESPACE;
			}
		}
		else {
			display_proportional_clip(offset.x + D_MARGIN_LEFT, top, translator::translate("keine"), ALIGN_LEFT, MN_GREY0, true);
			top += LINESPACE;
		}
		top += LINESPACE;

		// Knightly : add lineless convoys which serve this stop
		line_info.clear();
		if (!halt->registered_convoys.empty()) {
			display_proportional_clip(offset.x, top, translator::translate("Lineless convoys serving this stop"), ALIGN_LEFT, SYSCOL_TEXT, true);
			top += LINESPACE;
			int offset_left = offset.x + D_BUTTON_HEIGHT;
			uint32 sel = line_selected; // dummy
			for (uint32 i = 0; i < halt->registered_convoys.get_count(); ++i) {
				// Convoy buttons
				button_t *b = new button_t();
				b->init(button_t::posbutton, NULL, scr_coord(offset.x, top));
				b->set_targetpos(koord(-2, i));
				b->add_listener(this);
				convoybuttons.append(b);
				cont.add_component(b);

				/*
				// Line labels with color of player
				label_names.append(strdup(halt->registered_convoys[i]->get_name()));
				gui_label_t *l = new gui_label_t(label_names.back(), PLAYER_FLAG | (halt->registered_convoys[i]->get_owner()->get_player_color1()));
				l->set_pos(scr_coord(D_MARGIN_LEFT + D_BUTTON_HEIGHT + D_H_SPACE, top));
				convoylabels.append(l);
				cont.add_component(l);
				*/

				// dummy
				display_img_aligned(gui_theme_t::pos_button_img[sel == 0], scr_rect(offset.x, top, D_POS_BUTTON_WIDTH, LINESPACE), ALIGN_CENTER_V | ALIGN_CENTER_H, true);
				sel--;

				// lineless convoy name
				offset_left += display_proportional_clip(offset_left, top, strdup(halt->registered_convoys[i]->get_name()), ALIGN_LEFT, halt->registered_convoys[i]->get_owner()->get_player_color1(), true) + D_H_SPACE*2;

				// lineless conmvoy handling goods (symbol)
				FOR(minivec_tpl<uint8>, const catg_index, halt->registered_convoys[i]->get_goods_catg_index()) {
					uint8 temp = catg_index;
					display_color_img(goods_manager_t::get_info_catg_index(temp)->get_catg_symbol(), offset_left, top, 0, false, false);
					offset_left += GOODS_SYMBOL_CELL_WIDTH;
				}
				offset_left = max(offset_left + D_H_SPACE, offset.x + D_BUTTON_WIDTH * 3);

				// lineless convoy service frequency
				if (skinverwaltung_t::service_frequency) {
					offset_left += display_proportional_clip(offset_left, top, "(", ALIGN_LEFT, SYSCOL_TEXT, true);
					display_color_img(skinverwaltung_t::service_frequency->get_image_id(0), offset_left, top, 0, false, false);
					offset_left += GOODS_SYMBOL_CELL_WIDTH;
					if (halt->registered_convoys[i]->get_average_round_trip_time())
					{
						char as_clock[32];
						welt->sprintf_ticks(as_clock, sizeof(as_clock), halt->registered_convoys[i]->get_average_round_trip_time());
						line_info.append(as_clock);
						offset_left += display_proportional_clip(offset_left, top, line_info, ALIGN_LEFT, SYSCOL_TEXT, true);
						line_info.clear();
					}
					else {
						offset_left += display_proportional_clip(offset_left, top, translator::translate("????"), ALIGN_LEFT, MN_GREY0, true);
					}
					offset_left += display_proportional_clip(offset_left, top, ")", ALIGN_LEFT, SYSCOL_TEXT, true);
				}

				top += LINESPACE;
			}
		}
		top += LINESPACE*2;

		line_info.clear();
		// Direct routes from here
		display_proportional_clip(offset.x, top, translator::translate("Direkt erreichbare Haltestellen"), ALIGN_LEFT, SYSCOL_TEXT, true);
		top += LINESPACE;

		bool has_stops = false;

		const uint8 max_classes = max(goods_manager_t::passengers->get_number_of_classes(), goods_manager_t::mail->get_number_of_classes());

		for (uint i = 0; i < goods_manager_t::get_max_catg_index(); i++)
		{
			typedef quickstone_hashtable_tpl<haltestelle_t, haltestelle_t::connexion*> connexions_map_single_remote;

			// TODO: Add UI to show different connexions for multiple classes
			uint8 g_class = goods_manager_t::get_classes_catg_index(i) - 1;

			connexions_map_single_remote *connexions = halt->get_connexions(i, g_class);

			if (!connexions->empty())
			{
				top += LINESPACE/2;
				const goods_desc_t* info = goods_manager_t::get_info_catg_index(i);

				// Category symbol:
				display_color_img(info->get_catg_symbol(), offset.x, top, 0, false, false);

				line_info.append(translator::translate(info->get_catg_name()));
#if MSG_LEVEL>=4
				if (halt->is_transfer(i, g_class, max_classes)) {
					line_info.append("*");
				}
#endif
				line_info.append(":");
				display_proportional_clip(offset.x + GOODS_SYMBOL_CELL_WIDTH, top, line_info, ALIGN_LEFT, SYSCOL_TEXT, true);
				line_info.clear();
				top += LINESPACE;

				uint32 sel = line_selected; // dummy
				FOR(connexions_map_single_remote, &iter, *connexions)
				{
					halthandle_t a_halt = iter.key;
					haltestelle_t::connexion* cnx = iter.value;
					if (a_halt.is_bound())
					{
						int offset_left = offset.x + D_BUTTON_HEIGHT;

						// target button ...
						button_t *pb = new button_t();
						pb->init(button_t::posbutton, NULL, scr_coord(offset.x, top));
						pb->set_targetpos(a_halt->get_basis_pos());
						pb->add_listener(this);
						posbuttons.append(pb);
						cont.add_component(pb);

						// dummy
						display_img_aligned(gui_theme_t::pos_button_img[sel == 0], scr_rect(offset.x, top, D_POS_BUTTON_WIDTH, LINESPACE), ALIGN_CENTER_V | ALIGN_CENTER_H, true);
						sel--;

						// station name
						has_stops = true;
						offset_left += max(display_proportional_clip(offset_left, top, a_halt->get_name(), ALIGN_LEFT, a_halt->get_owner()->get_player_color1(), true), D_BUTTON_WIDTH*2);

						const uint32 tiles_to_halt = shortest_distance(halt->get_next_pos(a_halt->get_basis_pos()), a_halt->get_next_pos(halt->get_basis_pos()));
						const double &km_per_tile = welt->get_settings().get_meters_per_tile() / 1000.0;
						const double km_to_halt = (double)tiles_to_halt * km_per_tile;

						// Distance indication
						line_info.append(" (");
						if (km_to_halt < 1000.0) {
							line_info.printf("%6.2fkm,", km_to_halt);
						}
						else {
							line_info.printf("%.1fkm,", km_to_halt);
						}
						offset_left += display_proportional_clip(offset_left, top, line_info, ALIGN_LEFT, SYSCOL_TEXT, true) + D_H_SPACE;
						line_info.clear();

						// travel time
						char travelling_time_as_clock[32];
						welt->sprintf_time_tenths(travelling_time_as_clock, sizeof(travelling_time_as_clock), cnx->journey_time);
						line_info.append(travelling_time_as_clock);
						if (skinverwaltung_t::travel_time) {
							display_color_img(skinverwaltung_t::travel_time->get_image_id(0), offset_left, top, 0, false, false);
							offset_left += GOODS_SYMBOL_CELL_WIDTH;
						}
						else {
							line_info.append(translator::translate(" mins. travelling"));
						}
						line_info.append(", ");
						offset_left += display_proportional_clip(offset_left, top, line_info, ALIGN_LEFT, SYSCOL_TEXT, true);
						line_info.clear();

						// waiting time
						if (halt->is_within_walking_distance_of(a_halt) && !cnx->best_convoy.is_bound() && !cnx->best_line.is_bound())
						{
							if (skinverwaltung_t::on_foot) {
								display_color_img(skinverwaltung_t::on_foot->get_image_id(0), offset_left, top, 0, false, false);
								offset_left += GOODS_SYMBOL_CELL_WIDTH;
								line_info.append(")");
							}
							else {
								line_info.append(translator::translate("on foot)"));
							}
							offset_left += display_proportional_clip(offset_left, top, line_info, ALIGN_LEFT, SYSCOL_TEXT, true);
							line_info.clear();
						}
						else
						{
							if (skinverwaltung_t::waiting_time) {
								display_color_img(skinverwaltung_t::waiting_time->get_image_id(0), offset_left, top, 0, false, false);
								offset_left += GOODS_SYMBOL_CELL_WIDTH;
							}
							if (cnx->waiting_time > 0){
								char waiting_time_as_clock[32];
								welt->sprintf_time_tenths(waiting_time_as_clock, sizeof(waiting_time_as_clock), cnx->waiting_time);
								line_info.append(waiting_time_as_clock);
								if (skinverwaltung_t::waiting_time) {
									line_info.append(")");
								}
								else {
									line_info.append(translator::translate(" mins. waiting)"));
								}
							}
							else {
								// Test for the default waiting value
								if (skinverwaltung_t::waiting_time) {
									offset_left += display_proportional_clip(offset_left, top, translator::translate("????"), ALIGN_LEFT, MN_GREY0, true);
									line_info.append(")");
								}
								else {
									line_info.append(translator::translate("waiting time unknown)"));
								}
							}
							offset_left += display_proportional_clip(offset_left, top, line_info, ALIGN_LEFT, SYSCOL_TEXT, true);
							line_info.clear();

							//class connexions
							// FIXME: this display is still for debug
							offset_left += D_H_SPACE;
							if (g_class > 0) {
								for(int j=0; j<=g_class; ++j){
									haltestelle_t::connexion* cnx = halt->get_connexions(i, j)->get(a_halt);
									if (cnx) {
										char waiting_time_as_clock[32];
										welt->sprintf_time_tenths(waiting_time_as_clock, sizeof(waiting_time_as_clock), halt->get_average_waiting_time(a_halt, i, j));
										line_info.append(waiting_time_as_clock);
										line_info.append(", ");
										offset_left += display_proportional_clip(offset_left, top, line_info, ALIGN_LEFT, MN_GREY0, true);
										line_info.clear();
										//display_circle(offset_left + LINESPACE/2, top + LINESPACE/2, LINESPACE/2-2, SYSCOL_TEXT);
										//offset_left += LINESPACE;
									}
									else {
										offset_left += display_proportional_clip(offset_left, top, " - ,", ALIGN_LEFT, MN_GREY0, true);
										//offset_left += LINESPACE;
									}
								}
							}
						}

						top += LINESPACE;
					}
				}
			}
		}

		if (!has_stops) {
			display_proportional_clip(offset.x + D_MARGIN_LEFT, top, translator::translate("keine"), ALIGN_LEFT, MN_GREY0, true);
		}

		top += LINESPACE;

		// Symbol description
		if (skinverwaltung_t::service_frequency) {
			display_color_img(skinverwaltung_t::service_frequency->get_image_id(0), offset.x, top, 0, false, false);
			display_proportional_clip(offset.x + GOODS_SYMBOL_CELL_WIDTH, top, translator::translate("symbol_help_text_service_frequency"), ALIGN_LEFT, SYSCOL_TEXT, true);
			top += LINESPACE;
		}
		if (skinverwaltung_t::travel_time) {
			display_color_img(skinverwaltung_t::travel_time->get_image_id(0), offset.x, top, 0, false, false);
			display_proportional_clip(offset.x + GOODS_SYMBOL_CELL_WIDTH, top, translator::translate("symbol_help_text_travel_time"), ALIGN_LEFT, SYSCOL_TEXT, true);
			top += LINESPACE;
		}
		if (skinverwaltung_t::waiting_time) {
			display_color_img(skinverwaltung_t::waiting_time->get_image_id(0), offset.x, top, 0, false, false);
			display_proportional_clip(offset.x + GOODS_SYMBOL_CELL_WIDTH, top, translator::translate("symbol_help_text_waiting_time"), ALIGN_LEFT, SYSCOL_TEXT, true);
			top += LINESPACE;
		}
		if (skinverwaltung_t::on_foot) {
			display_color_img(skinverwaltung_t::on_foot->get_image_id(0), offset.x, top, 0, false, false);
			display_proportional_clip(offset.x + GOODS_SYMBOL_CELL_WIDTH, top, translator::translate("symbol_help_text_on_foot"), ALIGN_LEFT, SYSCOL_TEXT, true);
			top += LINESPACE;
		}
	}
	top += D_MARGIN_TOP;
	scr_size size(max(x_size + pos.x, get_size().w), top - offset.y);
	if (size != get_size()) {
		set_size(size);
	}
}

bool halt_detail_line_t::action_triggered(gui_action_creator_t *, value_t extra)
{
	
	if (extra.i & ~1) {
		koord k = *(const koord *)extra.p;
		if (k.x == -1) {
			// Line button pressed.
			uint16 j = k.y;
			if (j < halt->registered_lines.get_count()) {
				linehandle_t line = halt->registered_lines[j];
				player_t *player = welt->get_active_player();
				if (player == line->get_owner()) {
					// Change player => change marked lines
					player->simlinemgmt.show_lineinfo(player, line);
					welt->set_dirty();
				}
			}
		}
		else if (k.x == -2) {
			// Knightly : lineless convoy button pressed
			uint16 j = k.y;
			if (j < halt->registered_convoys.get_count()) {
				convoihandle_t convoy = halt->registered_convoys[j];
				convoy->show_info();
			}
		}
	}
	
	return false;
}



void halt_detail_t::rdwr(loadsave_t *file)
{
	koord3d halt_pos;
	scr_size size = get_windowsize();
	sint32 xoff = scrolly.get_scroll_x();
	sint32 yoff = scrolly.get_scroll_y();
	if(  file->is_saving()  ) {
		halt_pos = halt->get_basis_pos3d();
	}
	halt_pos.rdwr( file );
	size.rdwr( file );
	file->rdwr_long( xoff );
	file->rdwr_long( yoff );
	if(  file->is_loading()  ) {
		halt = welt->lookup( halt_pos )->get_halt();
		// now we can open the window ...
		scr_coord const& pos = win_get_pos(this);
		halt_detail_t *w = new halt_detail_t(halt);
		create_win(pos.x, pos.y, w, w_info, magic_halt_detail + halt.get_id());
		w->set_windowsize( size );
		w->scrolly.set_scroll_position( xoff, yoff );
		destroy_win( this );
	}
}
