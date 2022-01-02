/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "halt_detail.h"
#include "halt_info.h"
#include "components/gui_button_to_chart.h"
#include "components/gui_divider.h"

#include "../simworld.h"
#include "../simware.h"
#include "../simcolor.h"
#include "../simconvoi.h"
#include "../simintr.h"
#include "../display/simgraph.h"
#include "../display/viewport.h"
#include "../simmenu.h"
#include "../simskin.h"
#include "../simline.h"

#include "../freight_list_sorter.h"

#include "../dataobj/schedule.h"
#include "../dataobj/environment.h"
#include "../dataobj/translator.h"
#include "../dataobj/loadsave.h"

#include "../vehicle/vehicle.h"

#include "../utils/simstring.h"

#include "../descriptor/skin_desc.h"

#include "../player/simplay.h"

#define CHART_HEIGHT (100)
#define PAX_EVALUATIONS 5

#define L_BUTTON_WIDTH button_size.w
#define L_CHART_INDENT (66)

#define L_WAITING_CELL_WIDTH (proportional_string_width(" 0000000"))
#define L_CAPACITY_CELL_WIDTH (proportional_string_width("000000"))

static const char *sort_text[halt_info_t::SORT_MODES] = {
	"Zielort",
	"via",
	"via Menge",
	"Menge",
	"origin (detail)",
	"origin (amount)",
	"destination (detail)",
	"wealth (detail)",
	"wealth (via)",
	"Line",
	"Line to"/*,
	"transferring time"*/
};

static const char cost_type[MAX_HALT_COST][64] =
{
	"Happy",
	"Turned away",
	"Gave up waiting",
	"Too slow",
	"No route (pass.)",
	"Mail delivered",
	"No route (mail)",
	"hl_btn_sort_waiting",
	"Arrived",
	"Departed",
	"Convoys"
};

static const char cost_tooltip[MAX_HALT_COST][128] =
{
	"The number of passengers who have travelled successfully from this stop",
	"The number of passengers who have been turned away from this stop because it is overcrowded",
	"The number of passengers who had to wait so long that they gave up",
	"The number of passengers who decline to travel because the journey would take too long",
	"The number of passengers who could not find a route to their destination",
	"The amount of mail successfully delivered from this stop",
	"The amount of mail which could not find a route to its destination",
	"The number of passengers/units of mail/goods waiting at this stop",
	"The number of passengers/units of mail/goods that have arrived at this stop",
	"The number of passengers/units of mail/goods that have departed from this stop",
	"The number of convoys that have serviced this stop"
};

const uint8 index_of_haltinfo[MAX_HALT_COST] = {
	HALT_HAPPY,
	HALT_UNHAPPY,
	HALT_TOO_WAITING,
	HALT_TOO_SLOW,
	HALT_NOROUTE,
	HALT_MAIL_DELIVERED,
	HALT_MAIL_NOROUTE,
	HALT_WAITING,
	HALT_ARRIVED,
	HALT_DEPARTED,
	HALT_CONVOIS_ARRIVED
};

#define COL_WAITING COL_DARK_TURQUOISE
#define COL_ARRIVED COL_LIGHT_BLUE

const uint8 cost_type_color[MAX_HALT_COST] =
{
	COL_HAPPY,
	COL_OVERCROWD,
	COL_TOO_WAITNG,
	COL_TOO_SLOW,
	COL_NO_ROUTE,
	COL_MAIL_DELIVERED,
	COL_MAIL_NOROUTE,
	COL_WAITING,
	COL_ARRIVED,
	COL_PASSENGERS,
	COL_TURQUOISE
};

struct type_symbol_t {
	haltestelle_t::stationtyp type;
	const skin_desc_t **desc;
};

const type_symbol_t symbols[] = {
	{ haltestelle_t::railstation, &skinverwaltung_t::zughaltsymbol },
	{ haltestelle_t::loadingbay, &skinverwaltung_t::autohaltsymbol },
	{ haltestelle_t::busstop, &skinverwaltung_t::bushaltsymbol },
	{ haltestelle_t::dock, &skinverwaltung_t::schiffshaltsymbol },
	{ haltestelle_t::airstop, &skinverwaltung_t::airhaltsymbol },
	{ haltestelle_t::monorailstop, &skinverwaltung_t::monorailhaltsymbol },
	{ haltestelle_t::tramstop, &skinverwaltung_t::tramhaltsymbol },
	{ haltestelle_t::maglevstop, &skinverwaltung_t::maglevhaltsymbol },
	{ haltestelle_t::narrowgaugestop, &skinverwaltung_t::narrowgaugehaltsymbol }
};


// helper class
gui_halt_type_images_t::gui_halt_type_images_t(halthandle_t h)
{
	halt = h;
	set_table_layout(lengthof(symbols), 1);
	set_alignment(ALIGN_LEFT | ALIGN_CENTER_V);
	assert( lengthof(img_transport) == lengthof(symbols) );
	// indicator for supplied transport modes
	haltestelle_t::stationtyp const halttype = halt->get_station_type();
	for(uint i=0; i < lengthof(symbols); i++) {
		if ( *symbols[i].desc ) {
			add_component(img_transport + i);
			img_transport[i].set_image( (*symbols[i].desc)->get_image_id(0));
			img_transport[i].enable_offset_removal(true);
			img_transport[i].set_visible( (halttype & symbols[i].type) != 0);
		}
	}
}

void gui_halt_type_images_t::draw(scr_coord offset)
{
	haltestelle_t::stationtyp const halttype = halt->get_station_type();
	for(uint i=0; i < lengthof(symbols); i++) {
		img_transport[i].set_visible( (halttype & symbols[i].type) != 0);
	}
	gui_aligned_container_t::draw(offset);
}

gui_halt_handled_goods_images_t::gui_halt_handled_goods_images_t(halthandle_t h)
{
	halt = h;
}

void gui_halt_handled_goods_images_t::draw(scr_coord offset)
{
	scr_coord_val xoff = D_H_SPACE;
	for (uint8 i = 0; i<goods_manager_t::get_max_catg_index(); i++) {
		uint8 g_class = goods_manager_t::get_classes_catg_index(i) - 1;
		haltestelle_t::connexions_map *connexions = halt->get_connexions(i, g_class);

		if (!connexions->empty())
		{
			display_color_img_with_tooltip(goods_manager_t::get_info_catg_index(i)->get_catg_symbol(), offset.x + xoff, offset.y + D_GET_CENTER_ALIGN_OFFSET(10, D_LABEL_HEIGHT), 0, false, false, translator::translate(goods_manager_t::get_info_catg_index(i)->get_catg_name()));
			xoff += 14;
		}
	}
	set_size(scr_size(xoff + D_H_SPACE*2, D_LABEL_HEIGHT));
	gui_container_t::draw(offset);
}


gui_halt_goods_demand_t::gui_halt_goods_demand_t(halthandle_t h, bool show_products)
{
	halt = h;
	this->show_products = show_products;
	build_goods_list();
	set_size(D_LABEL_SIZE);
}

void gui_halt_goods_demand_t::build_goods_list()
{
	goods_list.clear();
	if (old_fab_count = halt->get_fab_list().get_count()) {
		FOR(const slist_tpl<fabrik_t*>, const fab, halt->get_fab_list()) {
			FOR(array_tpl<ware_production_t>, const& i, show_products ? fab->get_output() : fab->get_input()) {
				goods_desc_t const* const ware = i.get_typ();
				goods_list.append_unique(ware);
			}
		}
	}
}

void gui_halt_goods_demand_t::draw(scr_coord offset)
{
	offset += pos;
	scr_coord_val xoff = D_H_SPACE;
	if (halt->get_fab_list().get_count() != old_fab_count){
		build_goods_list();
	}
	if (goods_list.get_count() && skinverwaltung_t::input_output) {
		// show symbol
		display_color_img(skinverwaltung_t::input_output->get_image_id(show_products ? 1:0), offset.x, offset.y + FIXED_SYMBOL_YOFF, 0, false, false);
		xoff += 12;
	}
	FOR(slist_tpl<goods_desc_t const*>, const good, goods_list) {
		display_colorbox_with_tooltip(offset.x + xoff, offset.y + GOODS_COLOR_BOX_YOFF, GOODS_COLOR_BOX_HEIGHT, GOODS_COLOR_BOX_HEIGHT, good->get_color(), NULL);
		xoff += GOODS_COLOR_BOX_HEIGHT+2;
		xoff += display_proportional_clip_rgb(offset.x + xoff, offset.y, translator::translate(good->get_name()), ALIGN_LEFT, halt->gibt_ab(good) ? SYSCOL_TEXT : SYSCOL_TEXT_WEAK, true);
		xoff += D_H_SPACE;
	}
	set_size(scr_size(max(D_LABEL_WIDTH, xoff + D_H_SPACE * 2), D_LABEL_HEIGHT));
}


gui_halt_waiting_catg_t::gui_halt_waiting_catg_t(halthandle_t h, uint8 catg)
{
	halt = h;
	catg_index = catg;
	set_size(scr_size(D_H_SPACE, D_LABEL_HEIGHT));
}

void gui_halt_waiting_catg_t::draw(scr_coord offset)
{
	offset += pos;
	scr_coord_val xoff = D_H_SPACE;
	uint8 g_class = goods_manager_t::get_classes_catg_index(catg_index) - 1;
	haltestelle_t::connexions_map *connexions = halt->get_connexions(catg_index, g_class);
	if (connexions->empty()) {
		// no connection
		xoff += display_proportional_clip_rgb(offset.x + xoff, offset.y, "-", ALIGN_LEFT, SYSCOL_TEXT_WEAK, true);
	}
	else {
		bool got_one = false;
		bool overcrowded = (halt->get_status_color(catg_index==goods_manager_t::INDEX_PAS ? 0 : catg_index == goods_manager_t::INDEX_MAIL ? 1 : 2)==color_idx_to_rgb(COL_OVERCROWD));

		for (uint8 j = 0; j < goods_manager_t::get_count(); j++) {
			const goods_desc_t *wtyp = goods_manager_t::get_info(j);
			if (wtyp->get_catg_index() != catg_index) {
				continue;
			}
			const uint32 sum = halt->get_ware_summe(wtyp);
			if (sum > 0) {
				buf.clear();
				display_colorbox_with_tooltip(offset.x + xoff, offset.y, GOODS_COLOR_BOX_HEIGHT, GOODS_COLOR_BOX_HEIGHT, wtyp->get_color(), NULL);
				xoff += GOODS_COLOR_BOX_HEIGHT+2;

				buf.printf("%s ", translator::translate(wtyp->get_name()));
				xoff += display_proportional_clip_rgb(offset.x + xoff, offset.y, buf, ALIGN_LEFT, SYSCOL_TEXT, true);
				buf.clear();
				buf.printf("%d ", sum);
				xoff += display_proportional_clip_rgb(offset.x + xoff, offset.y, buf, ALIGN_LEFT, overcrowded ? color_idx_to_rgb(COL_OVERCROWD) : SYSCOL_TEXT, true);
				xoff += D_H_SPACE;
				got_one = true;
			}
		}
		if (!got_one) {
			xoff += display_proportional_clip_rgb(offset.x + xoff, offset.y, "0", ALIGN_LEFT, SYSCOL_TEXT_WEAK, true);
		}
	}

	set_size( scr_size(xoff+D_H_SPACE*2, D_LABEL_HEIGHT) );
	gui_container_t::draw(offset);
}


gui_halt_waiting_summary_t::gui_halt_waiting_summary_t(halthandle_t h)
{
	halt = h;
}

void gui_halt_waiting_summary_t::draw(scr_coord offset)
{
	scr_coord_val xoff = D_H_SPACE;
	bool got_one = false;
	for (uint8 i = 0; i < goods_manager_t::get_max_catg_index(); i++) {
		bool already_show_icon=false;
		for (uint8 j = 0; j < goods_manager_t::get_count(); j++) {
			const goods_desc_t *wtyp = goods_manager_t::get_info(j);
			if (wtyp->get_catg_index()!=i) {
				continue;
			}
			if (halt->gibt_ab(wtyp)) {
				// ignore goods with sum=zero
				const uint32 sum = halt->get_ware_summe(wtyp); // UI TODO: Consider the display for each class
				if (sum > 0) {
					buf.clear();
					if (!already_show_icon) {
						if (got_one) {
							xoff += D_H_SPACE+2;
						}
						display_color_img_with_tooltip(goods_manager_t::get_info_catg_index(i)->get_catg_symbol(), offset.x + xoff, offset.y + D_GET_CENTER_ALIGN_OFFSET(10, D_LABEL_HEIGHT), 0, false, false, translator::translate(goods_manager_t::get_info_catg_index(i)->get_catg_name()));
						xoff += 12;
						got_one = true;
					}
					else {
						buf.append(", ");
					}
					buf.printf("%d%s %s", sum, translator::translate(wtyp->get_mass()), translator::translate(wtyp->get_name()));
					xoff += display_proportional_clip_rgb(offset.x + xoff, offset.y, buf, ALIGN_LEFT, SYSCOL_TEXT, true);

					already_show_icon = true;
				}
			}
		}
	}
	if (!got_one) {
		xoff += display_proportional_clip_rgb(offset.x + xoff, offset.y, translator::translate("no goods waiting"), ALIGN_LEFT, SYSCOL_TEXT_WEAK, true);
	}

	set_size(scr_size(xoff + D_H_SPACE * 2, D_LABEL_HEIGHT));
	gui_container_t::draw(offset);
}


gui_halt_capacity_bar_t::gui_halt_capacity_bar_t(halthandle_t h, uint8 ft)
{
	if (ft > 2) { return; }
	freight_type = ft;
	halt = h;
}

void gui_halt_capacity_bar_t::draw(scr_coord offset)
{
	uint32 wainting_sum = 0;
	uint32 transship_in_sum = 0;
	uint32 leaving_sum = 0;
	bool overcrowded = false;
	const uint32 capacity= halt->get_capacity(freight_type);
	if (!capacity) { return; }
	switch (freight_type) {
		case 0:
			wainting_sum = halt->get_ware_summe(goods_manager_t::get_info(goods_manager_t::INDEX_PAS));
			overcrowded = halt->is_overcrowded(goods_manager_t::INDEX_PAS);
			transship_in_sum = halt->get_transferring_goods_sum(goods_manager_t::get_info(goods_manager_t::INDEX_PAS)) - halt->get_leaving_goods_sum(goods_manager_t::get_info(goods_manager_t::INDEX_PAS));
			break;
		case 1:
			wainting_sum = halt->get_ware_summe(goods_manager_t::get_info(goods_manager_t::INDEX_MAIL));
			overcrowded = halt->is_overcrowded(goods_manager_t::INDEX_MAIL);
			transship_in_sum = halt->get_transferring_goods_sum(goods_manager_t::get_info(goods_manager_t::INDEX_MAIL)) - halt->get_leaving_goods_sum(goods_manager_t::get_info(goods_manager_t::INDEX_MAIL));
			break;
		case 2:
			for (uint8 g1 = 0; g1 < goods_manager_t::get_max_catg_index(); g1++) {
				if (g1 == goods_manager_t::INDEX_PAS || g1 == goods_manager_t::INDEX_MAIL)
				{
					continue;
				}
				const goods_desc_t *wtyp = goods_manager_t::get_info(g1);
				switch (g1) {
				case 0:
					wainting_sum += halt->get_ware_summe(wtyp);
					break;
				default:
					const uint8 count = goods_manager_t::get_count();
					for (uint32 g2 = 3; g2 < count; g2++) {
						goods_desc_t const* const wtyp2 = goods_manager_t::get_info(g2);
						if (wtyp2->get_catg_index() != g1) {
							continue;
						}
						wainting_sum += halt->get_ware_summe(wtyp2);
						transship_in_sum += halt->get_transferring_goods_sum(wtyp2, 0);
						leaving_sum += halt->get_leaving_goods_sum(wtyp2, 0);
					}
					break;
				}
			}
			overcrowded = ((wainting_sum + transship_in_sum) > capacity);
			transship_in_sum -= leaving_sum;
			break;
		default:
			return;
	}
	display_ddd_box_clip_rgb(pos.x + offset.x, pos.y + offset.y, HALT_CAPACITY_BAR_WIDTH + 2, GOODS_COLOR_BOX_HEIGHT, color_idx_to_rgb(MN_GREY0), color_idx_to_rgb(MN_GREY4));
	display_fillbox_wh_clip_rgb(pos.x+offset.x + 1, pos.y+offset.y + 1, HALT_CAPACITY_BAR_WIDTH, GOODS_COLOR_BOX_HEIGHT - 2, color_idx_to_rgb(MN_GREY2), true);
	// transferring (to this station) bar
	display_fillbox_wh_clip_rgb(pos.x+offset.x + 1, pos.y+offset.y + 1, min(100, (transship_in_sum + wainting_sum) * 100 / capacity), 6, color_idx_to_rgb(MN_GREY1), true);

	const PIXVAL col = overcrowded ? color_idx_to_rgb(COL_OVERCROWD) : COL_CLEAR;
	uint8 waiting_factor = min(100, wainting_sum * 100 / capacity);

	display_cylinderbar_wh_clip_rgb(pos.x+offset.x + 1, pos.y+offset.y + 1, HALT_CAPACITY_BAR_WIDTH * waiting_factor / 100, 6, col, true);

	set_size(scr_size(HALT_CAPACITY_BAR_WIDTH+2, GOODS_COLOR_BOX_HEIGHT));
	gui_container_t::draw(offset);
}

gui_halt_waiting_indicator_t::gui_halt_waiting_indicator_t(halthandle_t h, bool yesno)
{
	halt = h;
	show_transfer_time = yesno;
	init();
}

void gui_halt_waiting_indicator_t::init()
{
	remove_all();
	if(!halt.is_bound()) {
		return;
	}
	set_table_layout(7+show_transfer_time*3, 0);
	set_alignment(ALIGN_LEFT | ALIGN_CENTER_V);
	set_margin(scr_size(D_H_SPACE, 0), scr_size(D_H_SPACE, 0));
	set_spacing(scr_size(D_H_SPACE, 1));

	bool served = false;
	for (uint8 i = 0; i < 3; i++) {
		switch (i) {
			case 0:
				served = halt->get_pax_enabled();  break;
			case 1:
				served = halt->get_mail_enabled(); break;
			case 2:
				served = halt->get_ware_enabled(); break;
			default:
				served = false; break;
		}

		if (served) {
			switch (i) {
				case 0:
					new_component<gui_image_t>()->set_image(skinverwaltung_t::passengers->get_image_id(0), true);
					break;
				case 1:
					new_component<gui_image_t>()->set_image(skinverwaltung_t::mail->get_image_id(0), true);
					break;
				case 2:
					new_component<gui_image_t>()->set_image(skinverwaltung_t::goods->get_image_id(0), true);
					break;
				default:
					continue;
			}

			// waiting bar
			capacity_bar[i] = new_component<gui_halt_capacity_bar_t>(halt, i);

			// capacity (text)
			lb_waiting[i].set_align(gui_label_t::right);
			lb_waiting[i].set_fixed_width(L_WAITING_CELL_WIDTH);
			add_component(&lb_waiting[i]);
			new_component<gui_label_t>("/");
			lb_capacity[i].set_align(gui_label_t::right);
			lb_capacity[i].set_fixed_width(L_CAPACITY_CELL_WIDTH);
			add_component(&lb_capacity[i]);

			if (show_transfer_time) {
				new_component<gui_margin_t>(D_H_SPACE);

				new_component<gui_label_t>(i==2 ? "Transfer time: " : "Transshipment time: ");
				lb_transfer_time[i].set_fixed_width(L_WAITING_CELL_WIDTH);
				add_component(&lb_transfer_time[i]);
			}
			img_alert.set_image(skinverwaltung_t::alerts ? skinverwaltung_t::alerts->get_image_id(2) : IMG_EMPTY, true);
			img_alert.set_rigid(true);
			img_alert.set_tooltip("No service");
			img_alert.set_visible(false);
			add_component(&img_alert);

			new_component<gui_fill_t>();
		}
	}
}

void gui_halt_waiting_indicator_t::update()
{
	init();
}

void gui_halt_waiting_indicator_t::draw(scr_coord offset)
{
	uint32 wainting_sum;
	bool is_operating;
	bool served;
	//PIXVAL goods_colval;
	char transfer_time_as_clock[32];
	for (uint8 i = 0; i < 3; i++) {
		switch (i) {
			case 0:
				served = halt->get_pax_enabled();  break;
			case 1:
				served = halt->get_mail_enabled(); break;
			case 2:
				served = halt->get_ware_enabled(); break;
			default:
				served = false; break;
		}
		if (served) {
			is_operating = false;
			wainting_sum = 0;
			switch (i) {
				case 0:
					wainting_sum = halt->get_ware_summe(goods_manager_t::get_info(goods_manager_t::INDEX_PAS));
					if (!halt->registered_lines.get_count() && !halt->registered_convoys.get_count()) {
						is_operating = false;
					}
					else {
						is_operating = halt->gibt_ab(goods_manager_t::get_info(goods_manager_t::INDEX_PAS));
					}
					break;
				case 1:
					wainting_sum = halt->get_ware_summe(goods_manager_t::get_info(goods_manager_t::INDEX_MAIL));
					is_operating = halt->gibt_ab(goods_manager_t::get_info(goods_manager_t::INDEX_MAIL));
					break;
				case 2:
					for (uint8 g1 = 0; g1 < goods_manager_t::get_max_catg_index(); g1++) {
						if (g1 == goods_manager_t::INDEX_PAS || g1 == goods_manager_t::INDEX_MAIL)
						{
							continue;
						}
						const goods_desc_t *wtyp = goods_manager_t::get_info(g1);
						if (!is_operating)
						{
							is_operating = halt->gibt_ab(wtyp);
						}
						switch (g1) {
						case 0:
							wainting_sum += halt->get_ware_summe(wtyp);
							break;
						default:
							const uint8 count = goods_manager_t::get_count();
							for (uint32 g2 = 3; g2 < count; g2++) {
								goods_desc_t const* const wtyp2 = goods_manager_t::get_info(g2);
								if (wtyp2->get_catg_index() != g1) {
									continue;
								}
								wainting_sum += halt->get_ware_summe(wtyp2);
							}
							break;
						}
					}
					break;
				default:
					continue;
			}
			lb_waiting[i].buf().append(wainting_sum);
			lb_waiting[i].update();
			lb_capacity[i].buf().append(halt->get_capacity(i));
			lb_capacity[i].update();

			if (i == 2) {
				world()->sprintf_time_tenths(transfer_time_as_clock, sizeof(transfer_time_as_clock), (halt->get_transshipment_time()));
			}
			else {
				world()->sprintf_time_tenths(transfer_time_as_clock, sizeof(transfer_time_as_clock), (halt->get_transfer_time()));
			}
			lb_transfer_time[i].buf().append(transfer_time_as_clock);
			lb_transfer_time[i].update();
			lb_transfer_time[i].set_color(is_operating ? SYSCOL_TEXT : SYSCOL_TEXT_INACTIVE);

			if (!is_operating && skinverwaltung_t::alerts) {
				img_alert.set_visible(true);
			}
		}
	}
	set_size(get_size());
	gui_aligned_container_t::draw(offset);
}

// main class
halt_info_t::halt_info_t(halthandle_t halt) :
		gui_frame_t("", NULL),
		lb_evaluation("Evaluation:"),
		scrolly_departure_board(&cont_departure, true, true),
		text_freight(&freight_info),
		scrolly_freight(&container_freight, true, true),
		waiting_bar(halt, false),
		view(koord3d::invalid, scr_size(max(64, get_base_tile_raster_width()), max(56, get_base_tile_raster_width() * 7 / 8)))
{
	if (halt.is_bound()) {
		init(halt);
	}
}

void halt_info_t::init(halthandle_t halt)
{
	this->halt = halt;
	old_pax_ev_sum = -1;
	old_mail_ev_sum = -1;
	if(halt->get_station_type() & haltestelle_t::airstop && halt->has_no_control_tower())
	{
		sprintf(edit_name, "%s [%s]", halt->get_name(), translator::translate("NO CONTROL TOWER"));
		set_name(edit_name);
	}
	else {
		set_name(halt->get_name());
	}
	set_owner(halt->get_owner());

	halt->set_sortby( env_t::default_sortmode );

	set_table_layout(1,0);

	// top part
	add_table(2,1)->set_alignment(ALIGN_TOP);
	{
		// top left
		container_top = add_table(1,0);
		{
			// 1st row: input name
			tstrncpy(edit_name, halt->get_name(), lengthof(edit_name));
			input.set_text(edit_name, lengthof(edit_name));
			input.add_listener(this);
			add_component(&input);

			// 2nd row: status images
			add_table(3, 1);
			{
				indicator_color.set_size(scr_size(LINEASCENT*2, D_INDICATOR_BOX_HEIGHT));
				indicator_color.set_size_fixed(true);
				add_component(&indicator_color);

				// company name
				new_component<gui_label_t>(halt->get_owner()->get_name(), color_idx_to_rgb(halt->get_owner()->get_player_color1()+env_t::gui_player_color_bright), gui_label_t::left)->set_shadow(SYSCOL_TEXT_SHADOW, true);

				img_types = new_component<gui_halt_type_images_t>(halt);
			}
			end_table();

			new_component<gui_margin_t>(0, D_V_SPACE);

			// capacities
			add_component(&waiting_bar);
			new_component<gui_margin_t>(0, LINESPACE/4);

			// evaluations
			add_component(&lb_evaluation);
			add_table(4,0)->set_spacing(scr_size(D_H_SPACE,1));
			{
				// indicator for enabled freight type
				img_enable[0].set_image(skinverwaltung_t::passengers->get_image_id(0), true);
				img_enable[1].set_image(skinverwaltung_t::mail->get_image_id(0), true);
				img_enable[0].set_rigid(true);
				img_enable[1].set_rigid(true);
				evaluation_pax.set_width(0);
				evaluation_mail.set_width(0);
				evaluation_pax.set_rigid(true);
				evaluation_mail.set_rigid(true);
				pax_ev_num[0] = halt->haltestelle_t::get_pax_happy();
				pax_ev_num[1] = halt->haltestelle_t::get_pax_unhappy();
				pax_ev_num[2] = halt->haltestelle_t::get_pax_too_waiting();
				pax_ev_num[3] = halt->haltestelle_t::get_pax_too_slow();
				pax_ev_num[4] = halt->haltestelle_t::get_pax_no_route();
				mail_ev_num[0] = halt->haltestelle_t::get_mail_delivered();
				mail_ev_num[1] = halt->haltestelle_t::get_mail_no_route();

				// passenger
				new_component<gui_margin_t>( LINESPACE/3 );
				add_component(&img_enable[0]);

				// passenger evaluation icons ok?
				if (skinverwaltung_t::pax_evaluation_icons) {
					add_table(2,1);
					{
						add_component(&lb_pax_storage);
						cont_pax_ev_detail.set_table_layout(17,1);
						add_component(&cont_pax_ev_detail);
					}
					end_table();
				}
				else {
					add_component(&lb_pax_storage);

				}
				new_component<gui_fill_t>();

				new_component<gui_margin_t>( LINESPACE/2, D_INDICATOR_HEIGHT+2 );
				new_component<gui_empty_t>();
				for (uint8 i = 0; i < 5; i++) {
					evaluation_pax.add_color_value(&pax_ev_num[i], color_idx_to_rgb(cost_type_color[i]));
				}
				add_component(&evaluation_pax);
				new_component<gui_empty_t>();

				new_component_span<gui_margin_t>(0, D_V_SPACE/2, 4);

				// mail
				new_component<gui_margin_t>( LINESPACE/3 );
				add_component(&img_enable[1]);

				if (skinverwaltung_t::mail_evaluation_icons) {
					add_table(2,1);
					{
						add_component(&lb_mail_storage);
						cont_mail_ev_detail.set_table_layout(8,1);
						add_component(&cont_mail_ev_detail);
					}
					end_table();
				}
				else {
					add_component(&lb_mail_storage);
				}
				new_component<gui_fill_t>();

				new_component<gui_margin_t>( LINESPACE/2, D_INDICATOR_HEIGHT+2 );
				new_component<gui_empty_t>();

				evaluation_mail.add_color_value(&mail_ev_num[0], color_idx_to_rgb(COL_MAIL_DELIVERED));
				evaluation_mail.add_color_value(&mail_ev_num[1], color_idx_to_rgb(COL_MAIL_NOROUTE));
				add_component(&evaluation_mail);
				new_component<gui_empty_t>();

				new_component_span<gui_margin_t>(0,D_V_SPACE, 4);
			}
			end_table();
		}
		end_table();

		// top right
		add_table(1,0)->set_spacing(scr_size(0,0));
		{
			add_component(&view);
			view.set_location(halt->get_basis_pos3d());

			detail_button.init(button_t::roundbox, "Details");
			detail_button.set_width(view.get_size().w);
			detail_button.set_tooltip("Open station/stop details");
			detail_button.add_listener(this);
			add_component(&detail_button);
		}
		end_table();

		//new_component<gui_empty_t>();
	}
	end_table();


	// tabs: waiting, departure, chart

	add_component(&switch_mode);
	switch_mode.add_listener(this);
	switch_mode.add_tab(&scrolly_freight, translator::translate("Hier warten/lagern:"));

	// list of waiting cargo
	// sort mode
	container_freight.set_table_layout(1,0);
	container_freight.add_table(2,1);
	container_freight.new_component<gui_label_t>("Sort waiting list by");

	freight_sort_selector.clear_elements();
	for (int i = 0; i < SORT_MODES; i++)
	{
		freight_sort_selector.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate(sort_text[i]), SYSCOL_TEXT);
	}
	uint8 sort_mode = env_t::default_sortmode;
	// 9 and 10 are sort modes for convoi, but 11 is for halt
	halt->set_sortby(sort_mode >= by_line ? env_t::default_sortmode + 2 : env_t::default_sortmode);
	freight_sort_selector.set_selection(sort_mode);
	freight_sort_selector.set_focusable(true);
	freight_sort_selector.set_width_fixed(true);
	freight_sort_selector.set_highlight_color(1);
	freight_sort_selector.set_size(scr_size(D_BUTTON_WIDTH * 2, D_EDIT_HEIGHT));
	freight_sort_selector.add_listener(this);
	container_freight.add_component(&freight_sort_selector);
	container_freight.end_table();

	container_freight.add_component(&text_freight);

	// departure board
	cont_tab_departure.set_table_layout(1,0);
	cont_tab_departure.set_margin(scr_size(0,D_V_SPACE), scr_size(0,0));
	cont_tab_departure.set_spacing(scr_size(0,0));
	cont_departure.set_table_layout(2,0);
	cont_departure.set_margin(scr_size(D_H_SPACE, 0), scr_size(D_H_SPACE, 0));
	cont_tab_departure.add_table(10,1)->set_spacing(scr_size(0,0));
	{
		cont_tab_departure.new_component<gui_margin_t>(D_MARGIN_LEFT);
		bt_arrivals.init(button_t::roundbox_left_state, "Arrivals from\n");
		bt_arrivals.set_size(D_BUTTON_SIZE);
		bt_arrivals.add_listener(this);
		cont_tab_departure.add_component(&bt_arrivals);
		bt_departures.init(button_t::roundbox_right_state,"Departures to\n");
		bt_departures.set_size(D_BUTTON_SIZE);
		bt_departures.add_listener(this);
		cont_tab_departure.add_component(&bt_departures);
		bt_arrivals.pressed   = !(display_mode_bits&SHOW_DEPARTURES);
		bt_departures.pressed = display_mode_bits & SHOW_DEPARTURES;

		cont_tab_departure.new_component<gui_margin_t>(D_H_SPACE*3);

		db_mode_selector.add_listener(this);
		db_mode_selector.set_size(D_LABEL_SIZE);
		db_mode_selector.clear_elements();
		db_mode_selector.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate("Line"), SYSCOL_TEXT);
		db_mode_selector.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate("Convoy"), SYSCOL_TEXT);
		db_mode_selector.set_width_fixed(true);
		db_mode_selector.set_selection( display_mode_bits&SHOW_LINE_NAME ? 0:1 );

		cont_tab_departure.add_component(&db_mode_selector);

		cont_tab_departure.new_component<gui_margin_t>(D_H_SPACE*3);

		bt_db_filter[0].init( button_t::roundbox_left_state,   NULL );
		bt_db_filter[1].init( button_t::roundbox_middle_state, NULL );
		bt_db_filter[2].init( button_t::roundbox_right_state,  NULL );
		bt_db_filter[0].set_image(skinverwaltung_t::passengers->get_image_id(0));
		bt_db_filter[1].set_image(skinverwaltung_t::mail->get_image_id(0));
		bt_db_filter[2].set_image(skinverwaltung_t::goods->get_image_id(0));
		for( uint8 i=0; i<3; i++ ) {
			bt_db_filter[i].add_listener(this);
			bt_db_filter[i].pressed = db_filter_bits & (1<<i);
			cont_tab_departure.add_component(&bt_db_filter[i]);
		}

		cont_tab_departure.new_component<gui_fill_t>();
	}
	cont_tab_departure.end_table();
	cont_tab_departure.add_component(&scrolly_departure_board);
	switch_mode.add_tab(&cont_tab_departure, translator::translate("Departure board"));

	// chart
	switch_mode.add_tab(&container_chart, translator::translate("Chart"));
	container_chart.set_table_layout(1, 0);

	chart.set_min_size(scr_size(0, CHART_HEIGHT));
	chart.set_dimension(12, 10000);
	chart.set_background(SYSCOL_CHART_BACKGROUND);
	container_chart.add_component(&chart);

	container_chart.add_table(4, int((MAX_HALT_COST + 3) / 4))->set_force_equal_columns(true);
	for (int cost = 0; cost < MAX_HALT_COST; cost++) {
		uint16 curve = chart.add_curve(color_idx_to_rgb(cost_type_color[cost]), halt->get_finance_history(), MAX_HALT_COST, index_of_haltinfo[cost], MAX_MONTHS, 0, false, true, 0);

		button_t *b = container_chart.new_component<button_t>();
		b->init(button_t::box_state_automatic | button_t::flexible, cost_type[cost]);
		b->background_color = color_idx_to_rgb(cost_type_color[cost]);
		b->pressed = false;

		button_to_chart.append(b, &chart, curve);
	}
	container_chart.end_table();

	update_components();
	set_resizemode(diagonal_resize);
	reset_min_windowsize();
	set_windowsize(get_min_windowsize());
}


halt_info_t::~halt_info_t()
{
	if(  halt.is_bound()  &&  strcmp(halt->get_name(),edit_name)  &&  edit_name[0]  ) {
		// text changed => call tool
		cbuffer_t buf;
		buf.printf( "h%u,%s", halt.get_id(), edit_name );
		tool_t *tool = create_tool( TOOL_RENAME | SIMPLE_TOOL );
		tool->set_default_param( buf );
		welt->set_tool( tool, halt->get_owner() );
		// since init always returns false, it is safe to delete immediately
		delete tool;
	}
}


koord3d halt_info_t::get_weltpos(bool)
{
	return halt->get_basis_pos3d();
}


bool halt_info_t::is_weltpos()
{
	return ( welt->get_viewport()->is_on_center(get_weltpos(false)));
}


void halt_info_t::update_components()
{
	indicator_color.set_color(halt->get_status_farbe());

	// update evaluation
	if (halt->get_pax_enabled() || halt->get_mail_enabled()) {
		const bool japanese_order = !(env_t::show_month == env_t::DATE_FMT_JAPANESE || env_t::show_month==env_t::DATE_FMT_JAPANESE_NO_SEASON || env_t::show_month == env_t::DATE_FMT_JAPANESE_INTERNAL_MINUTE);
		// passengers evaluation
		if (halt->get_pax_enabled()) {
			pax_ev_num[0] = halt->haltestelle_t::get_pax_happy();
			pax_ev_num[1] = halt->haltestelle_t::get_pax_unhappy();
			pax_ev_num[2] = halt->haltestelle_t::get_pax_too_waiting();
			pax_ev_num[3] = halt->haltestelle_t::get_pax_too_slow();
			pax_ev_num[4] = halt->haltestelle_t::get_pax_no_route();
			int pax_sum = 0;
			for (uint8 i = 0; i < PAX_EVALUATIONS; i++) {
				pax_sum += pax_ev_num[i];
			}

			if (!halt->get_ware_summe(goods_manager_t::passengers) && !halt->has_pax_user(2, true)) {
				if (halt->registered_lines.empty() && halt->registered_convoys.empty()) {
					// no convoys
					lb_pax_storage.buf().printf(" %s", translator::translate("No passenger service"));
				}
				else if (!halt->get_connexions(goods_manager_t::INDEX_PAS, goods_manager_t::passengers->get_number_of_classes()-1)->empty()) {
					// There is no evaluation because there is no trip starting from this stop
					lb_pax_storage.buf().printf(" %s", translator::translate("This stop has no user"));
				}
				else {
					lb_pax_storage.buf().printf(" %s", translator::translate("No passenger service"));
				}
				lb_pax_storage.set_color(SYSCOL_TEXT_INACTIVE);
				lb_pax_storage.update();
				lb_pax_storage.set_fixed_width(lb_pax_storage.get_min_size().w);
			}
			else {
				// There are users
				if (old_pax_ev_sum != pax_sum) {
					old_pax_ev_sum = pax_sum;
					uint8 indicator_height = D_INDICATOR_HEIGHT-1;
					if (pax_sum > 255) { indicator_height++; }
					if (pax_sum > 999) { indicator_height++; }
					if (pax_sum > 9999) { indicator_height++; }
					evaluation_pax.set_size(scr_size(min(pax_sum, 255), indicator_height));

					if (!skinverwaltung_t::pax_evaluation_icons) {
						if (has_character(0x263A)) {
							utf8 happy[4], unhappy[4];
							happy[utf16_to_utf8(0x263A, happy)] = 0;
							unhappy[utf16_to_utf8(0x2639, unhappy)] = 0;
							lb_pax_storage.buf().printf(translator::translate("Passengers %d %s, %d %s, %d no route, %d too slow"), halt->get_pax_happy(), happy, halt->get_pax_unhappy(), unhappy, halt->get_pax_no_route(), halt->haltestelle_t::get_pax_too_slow());
						}
						else {
							lb_pax_storage.buf().printf(translator::translate("Passengers %d %c, %d %c, %d no route, %d too slow"), halt->get_pax_happy(), 30, halt->get_pax_unhappy(), 31, halt->get_pax_no_route(), halt->haltestelle_t::get_pax_too_slow());
						}
						lb_pax_storage.update();
						lb_pax_storage.set_fixed_width(lb_pax_storage.get_min_size().w);
					}
					else {
						lb_pax_storage.buf().printf(":%5i", pax_sum);

						cont_pax_ev_detail.remove_all();
						gui_label_buf_t *lb = cont_pax_ev_detail.new_component<gui_label_buf_t>();
						lb->buf().printf("(", halt->haltestelle_t::get_mail_delivered());
						lb->update();
						lb->set_fixed_width(lb->get_min_size().w);
						for (int i = 0; i < PAX_EVALUATIONS; i++) {
							if (japanese_order) cont_pax_ev_detail.new_component<gui_image_t>(skinverwaltung_t::pax_evaluation_icons->get_image_id(i), 0, ALIGN_NONE, true)->set_tooltip(translator::translate(cost_tooltip[i]));
							lb = cont_pax_ev_detail.new_component<gui_label_buf_t>();
							lb->buf().printf("%d", pax_ev_num[i]);
							lb->update();
							lb->set_fixed_width(lb->get_min_size().w);
							if (!japanese_order) cont_pax_ev_detail.new_component<gui_image_t>(skinverwaltung_t::pax_evaluation_icons->get_image_id(i), 0, ALIGN_NONE, true)->set_tooltip(translator::translate(cost_tooltip[i]));
							if (i<PAX_EVALUATIONS-1) {
								cont_pax_ev_detail.new_component<gui_label_t>(", ");
							}
						}
						cont_pax_ev_detail.new_component<gui_label_t>(")");
						cont_pax_ev_detail.new_component<gui_fill_t>();
						lb_pax_storage.update();
						lb_pax_storage.set_fixed_width(L_WAITING_CELL_WIDTH);
					}
					lb_pax_storage.set_color(SYSCOL_TEXT);
				}
			}
		}

		// mail evaluation
		if (halt->get_mail_enabled()) {
			mail_ev_num[0] = halt->haltestelle_t::get_mail_delivered();
			mail_ev_num[1] = halt->haltestelle_t::get_mail_no_route();
			int mail_sum = mail_ev_num[0] + mail_ev_num[1];
			if (!halt->get_ware_summe(goods_manager_t::mail) && !halt->has_mail_user(2, true)) {
				if (halt->registered_lines.empty() && halt->registered_convoys.empty()) {
					// no convoys
					lb_mail_storage.buf().printf(" %s", translator::translate("No mail service"));
				}
				else if (!halt->get_connexions(goods_manager_t::INDEX_MAIL, goods_manager_t::mail->get_number_of_classes()-1)->empty()) {
					// There is no evaluation because there is no trip starting from this stop
					lb_mail_storage.buf().printf(" %s", translator::translate("This stop does not have mail customers"));
				}
				else {
					// This station seems not to be used by mail vehicles
					lb_mail_storage.buf().printf(" %s", translator::translate("No mail service"));
				}
				lb_mail_storage.set_color(SYSCOL_TEXT_INACTIVE);
				lb_mail_storage.update();
				lb_mail_storage.set_fixed_width(lb_mail_storage.get_min_size().w);
			}
			else {
				// There are users
				if (old_mail_ev_sum != mail_sum) {
					old_mail_ev_sum = mail_sum;
					uint8 indicator_height = D_INDICATOR_HEIGHT-1;
					if (mail_sum > 255) { indicator_height++; }
					if (mail_sum > 999) { indicator_height++; }
					if (mail_sum > 9999) { indicator_height++; }
					evaluation_mail.set_size(scr_size(min(mail_sum, 255), indicator_height));

					if (!skinverwaltung_t::mail_evaluation_icons) {
						lb_mail_storage.buf().printf(translator::translate("%d delivered, %d no route"), halt->haltestelle_t::get_mail_delivered(), halt->haltestelle_t::get_mail_no_route());
					}
					else {
						lb_mail_storage.buf().printf(":%5i", mail_sum);
						cont_mail_ev_detail.remove_all();
						cont_mail_ev_detail.new_component<gui_label_t>("(");
						if (japanese_order) cont_mail_ev_detail.new_component<gui_image_t>(skinverwaltung_t::mail_evaluation_icons->get_image_id(0), 0, ALIGN_NONE, true)->set_tooltip(translator::translate(cost_tooltip[5]));
						gui_label_buf_t *lb = cont_mail_ev_detail.new_component<gui_label_buf_t>();
						lb->buf().printf("%d",halt->haltestelle_t::get_mail_delivered());
						lb->update();
						lb->set_fixed_width(lb->get_min_size().w);
						if (!japanese_order) cont_mail_ev_detail.new_component<gui_image_t>(skinverwaltung_t::mail_evaluation_icons->get_image_id(0), 0, ALIGN_NONE, true)->set_tooltip(translator::translate(cost_tooltip[5]));
						cont_mail_ev_detail.new_component<gui_label_t>(", ");

						if (japanese_order) cont_mail_ev_detail.new_component<gui_image_t>(skinverwaltung_t::mail_evaluation_icons->get_image_id(1), 0, ALIGN_NONE, true)->set_tooltip(translator::translate(cost_tooltip[6]));
						lb = cont_mail_ev_detail.new_component<gui_label_buf_t>();
						lb->buf().printf("%d", halt->haltestelle_t::get_mail_no_route());
						lb->update();
						lb->set_fixed_width(lb->get_min_size().w);
						if (!japanese_order) cont_mail_ev_detail.new_component<gui_image_t>(skinverwaltung_t::mail_evaluation_icons->get_image_id(1), 0, ALIGN_NONE, true)->set_tooltip(translator::translate(cost_tooltip[6]));

						cont_mail_ev_detail.new_component<gui_label_t>(")");
						cont_mail_ev_detail.new_component<gui_fill_t>();
					}
					lb_mail_storage.set_color(SYSCOL_TEXT);
				}
				lb_mail_storage.set_fixed_width(L_WAITING_CELL_WIDTH);
				lb_mail_storage.update();
			}
		}
	}

	lb_evaluation.set_visible(halt->get_pax_enabled() || halt->get_mail_enabled());
	img_enable[0].set_visible(halt->get_pax_enabled());
	img_enable[1].set_visible(halt->get_mail_enabled());
	lb_pax_storage.set_visible(halt->get_pax_enabled());
	lb_mail_storage.set_visible(halt->get_mail_enabled());
	// bandgraph
	evaluation_pax.set_visible(halt->get_pax_enabled());
	evaluation_mail.set_visible(halt->get_mail_enabled());

	container_top->set_size(container_top->get_size());

	// buffer update now only when needed by halt itself => dedicated buffer for this
	int old_len = freight_info.len();
	halt->get_freight_info(freight_info);
	if (old_len != freight_info.len()) {
		text_freight.recalc_size();
		container_freight.set_size(container_freight.get_min_size());
	}

	if (switch_mode.get_aktives_tab() == &cont_tab_departure){
		update_cont_departure();
	}

	set_dirty();

	set_min_windowsize(scr_size(max(D_DEFAULT_WIDTH, get_min_windowsize().w), D_TITLEBAR_HEIGHT + switch_mode.get_pos().y + D_TAB_HEADER_HEIGHT));
	resize(scr_coord(0,0));

	switch_mode.set_size(scr_size(switch_mode.get_size().w, max(0, get_windowsize().h - switch_mode.get_pos().y - D_TITLEBAR_HEIGHT)));

	if (switch_mode.get_aktives_tab() == &cont_tab_departure) {
		scrolly_departure_board.set_size(scr_size(switch_mode.get_size().w, max(0, cont_tab_departure.get_size().h - scrolly_departure_board.get_pos().y)));
		cont_tab_departure.set_size(scr_size(switch_mode.get_size().w, max(0,switch_mode.get_size().h-D_TAB_HEADER_HEIGHT)));
	}
}


void halt_info_t::set_tab_opened()
{
	resize(scr_coord(0, 0));
	scr_coord_val margin_above_tab = switch_mode.get_pos().y + D_TAB_HEADER_HEIGHT + D_MARGINS_Y + D_V_SPACE;
	switch (switch_mode.get_active_tab_index())
	{
		case 0:
		default:
			set_windowsize(scr_size(get_windowsize().w, min(display_get_height() - margin_above_tab, margin_above_tab + container_freight.get_size().h)));
			break;
		case 1: // departure board
			set_windowsize(scr_size(get_windowsize().w, min(display_get_height() - margin_above_tab, margin_above_tab + cont_departure.get_size().h + scrolly_departure_board.get_pos().y - D_V_SPACE)));
			break;
		case 2: // chart
			set_windowsize(scr_size(get_windowsize().w, min(display_get_height() - margin_above_tab, margin_above_tab + container_chart.get_size().h)));
			break;
	}
}


void halt_info_t::update_cont_departure()
{
	cont_departure.remove_all();
	if (!halt.is_bound()) {
		return;
	}

	db_halts.clear();

	const sint64 cur_ticks = world()->get_ticks();

	typedef inthashtable_tpl<uint16, sint64, N_BAGS_SMALL> const arrival_times_map; // Not clear why this has to be redefined here.

	convoihandle_t cnv;
	sint32 delta_t;
	const uint32 max_listings = 15;

	FOR(arrival_times_map, const& iter, display_mode_bits&SHOW_DEPARTURES ? halt->get_estimated_convoy_departure_times() : halt->get_estimated_convoy_arrival_times())
	{
		cnv.set_id(iter.key);
		if(!cnv.is_bound())
		{
			continue;
		}

		if(!cnv->get_schedule())
		{
			//XXX vehicle in depot.
			continue;
		}

		// goods filtering
		if (db_filter_bits != DB_SHOW_ALL) {
			bool found = false;
			if ( db_filter_bits&DB_SHOW_PAX && cnv->get_goods_catg_index().is_contained(goods_manager_t::INDEX_PAS)) {
				found=true;
			}
			if (!found && db_filter_bits&DB_SHOW_MAIL && cnv->get_goods_catg_index().is_contained(goods_manager_t::INDEX_MAIL)) {
				found = true;
			}
			if (!found && db_filter_bits&DB_SHOW_GOODS) {
				for (uint8 catg_index=goods_manager_t::INDEX_NONE+1; catg_index < goods_manager_t::get_max_catg_index(); catg_index++) {
					if( cnv->get_goods_catg_index().is_contained(catg_index) ) {
						found = true;
						break;
					}
				}
			}
			if (!found) {
				continue;
			}
		}

		halthandle_t target_halt = get_convoy_target_halt(cnv);

		if( cnv->is_wait_infinite() ) {
			delta_t = SINT32_MAX_VALUE;
		}
		else {
			delta_t = iter.value - cur_ticks;
		}

		halt_info_t::dest_info_t dest(target_halt, max(delta_t, 0l), cnv);

		db_halts.insert_ordered( dest, compare_hi );
		if (db_halts.get_count() > max_listings) {
			db_halts.remove_at(db_halts.get_count()-1);
		}
	}

	// now we build the table ...
	cont_departure.new_component_span<gui_margin_t>(0,D_V_SPACE, 2);
	if (db_halts.get_count() > 0) {
		cont_departure.add_table(4,0);
		{
			// header
			cont_departure.new_component<gui_empty_t>();
			cont_departure.new_component<gui_empty_t>();
			cont_departure.new_component<gui_label_t>(display_mode_bits&SHOW_LINE_NAME ? "Line" : "Convoy");
			cont_departure.new_component<gui_label_t>(display_mode_bits&SHOW_DEPARTURES ? "db_convoy_to" : "db_convoy_from");

			cont_departure.new_component<gui_divider_t>()->init(scr_coord(0, 0), proportional_string_width("--:--:--"));
			cont_departure.new_component<gui_divider_t>();
			cont_departure.new_component<gui_divider_t>();
			cont_departure.new_component<gui_divider_t>();

			FOR(vector_tpl<halt_info_t::dest_info_t>, hi, db_halts) {
				gui_label_buf_t *lb = cont_departure.new_component<gui_label_buf_t>(SYSCOL_TEXT, gui_label_t::right);
				if (hi.delta_ticks == SINT32_MAX_VALUE) {
					lb->buf().append(translator::translate("Unknown"));
				}
				else {
					char timebuf[32];
					world()->sprintf_ticks(timebuf, sizeof(timebuf), hi.delta_ticks);
					lb->buf().append(timebuf);
				}
				lb->set_fixed_width(proportional_string_width("--:--:--"));
				lb->update();

				const bool is_bus = (hi.cnv->front()->get_waytype() == road_wt && hi.cnv->get_goods_catg_index().is_contained(goods_manager_t::INDEX_PAS));
				cont_departure.add_table(2,1);
				{
					cont_departure.new_component<gui_image_t>(is_bus ? skinverwaltung_t::bushaltsymbol->get_image_id(0) : hi.cnv->get_schedule()->get_schedule_type_symbol(), 0, ALIGN_NONE, true);
					// convoy ID
					char buf[128];
					sprintf(buf, "%u", hi.cnv->self.get_id());
					cont_departure.new_component<gui_vehicle_number_t>(buf, color_idx_to_rgb(hi.cnv->get_owner()->get_player_color1()+3) );
				}
				cont_departure.end_table();

				if (display_mode_bits&SHOW_LINE_NAME) {
						cont_departure.new_component<gui_label_t>(hi.cnv->get_line().is_bound() ? hi.cnv->get_line()->get_name() : "-", color_idx_to_rgb(hi.cnv->get_owner()->get_player_color1() + env_t::gui_player_color_dark), gui_label_t::left);
				}
				else {
					const PIXVAL textcol = hi.cnv->get_no_load() ? SYSCOL_TEXT_INACTIVE : hi.cnv->has_obsolete_vehicles() ? COL_OBSOLETE : hi.cnv->get_overcrowded() ? color_idx_to_rgb(COL_OVERCROWD) : SYSCOL_TEXT;
					cont_departure.new_component<gui_label_t>(hi.cnv->get_internal_name(), textcol);
				}

				cont_departure.new_component<gui_label_t>(hi.halt.is_bound() ? hi.halt->get_name() : "Unknown");
			}
		}
		cont_departure.end_table();
		cont_departure.new_component<gui_fill_t>();
	}
	else {
		cont_departure.add_table(2,1);
		{
			cont_departure.new_component<gui_margin_t>(D_MARGINS_X);
			cont_departure.new_component<gui_label_t>(db_filter_bits == 0 ? "Invalid filter" : "no convois", SYSCOL_TEXT_INACTIVE);
		}
		cont_departure.end_table();
		cont_departure.new_component<gui_fill_t>();
	}
	cont_departure.new_component_span<gui_margin_t>(0, D_MARGIN_BOTTOM,2);

	cont_departure.set_size(cont_departure.get_size());
}


halthandle_t halt_info_t::get_convoy_target_halt(convoihandle_t cnv)
{
	halthandle_t temp_halt;
	if (!cnv.is_bound()) {
		dbg->warning("halt_info_t::get_convoy_target_halt", "convoy not found");
		return temp_halt;
	}
	const schedule_t *schedule = cnv->get_schedule();

	uint8 entry = cnv->is_reversed() ? schedule->get_count()-1 : 0;
	halthandle_t origin_halt;
	bool found_in_forward=false;
	while (entry>=0 && entry<schedule->get_count()) {
		const halthandle_t entry_halt = haltestelle_t::get_halt(schedule->entries[entry].pos, cnv->get_owner());
		if (entry_halt.is_bound()) {
			if (entry_halt == halt) {
				if ((cnv->is_reversed() && entry < schedule->get_current_stop()) || (!cnv->is_reversed() && entry > schedule->get_current_stop())) {
					if (found_in_forward && temp_halt.is_bound()) {
						return temp_halt;
					}
					found_in_forward = true; // OK, next terminal is the destination
				}
			}
			else {
				if (!origin_halt.is_bound()) origin_halt = entry_halt;

				if (schedule->entries[entry].reverse) {
					if (found_in_forward) {
						if (display_mode_bits&SHOW_DEPARTURES) {
							// destination
							return entry_halt;
						}
						else if (temp_halt.is_bound()) {
							// origin (nearest terminal)
							return temp_halt;
						}
						else {
							// origin
							return origin_halt;
						}
					}
					else {
						temp_halt = entry_halt; // still candidate
					}
				}
				else if (found_in_forward) {
					temp_halt = entry_halt; // For the case of going back and forth to this stop many times
				}
			}
		}
		cnv->is_reversed() ? entry-- : entry++;
	}

	if (schedule->is_mirrored()) {
		return display_mode_bits & SHOW_DEPARTURES ? temp_halt : origin_halt;
	}
	else {
		return temp_halt.is_bound() ? temp_halt : origin_halt;
	}

	return temp_halt;
}

void halt_info_t::draw(scr_coord pos, scr_size size)
{
	if (!halt.is_bound()) {
		destroy_win(this);
	}
	assert(halt.is_bound());

	update_components();

	gui_frame_t::draw(pos, size);
}



/**
 * This method is called if an action is triggered
 */
bool halt_info_t::action_triggered( gui_action_creator_t *comp,value_t /* */)
{
	if (comp == &switch_mode  &&  get_windowsize().h == get_min_windowsize().h) {
		set_tab_opened();
		return true;
	}

	if (comp == &detail_button) {
		create_win( new halt_detail_t(halt), w_info, magic_halt_detail + halt.get_id() );
	}
	else if (comp == &freight_sort_selector) {
		sint32 sort_mode = freight_sort_selector.get_selection();
		if (sort_mode < 0 || sort_mode > SORT_MODES) {
			freight_sort_selector.set_selection(0);
			sort_mode = 0;
		}
		env_t::default_sortmode = (sort_mode_t)((int)(sort_mode) % (int)SORT_MODES);
		halt->set_sortby(sort_mode >= by_line ? env_t::default_sortmode+2 : env_t::default_sortmode);
	}
	else if(  comp == &input  ) {
		if(  strcmp(halt->get_name(),edit_name)  ) {
			// text changed => call tool
			cbuffer_t buf;
			buf.printf( "h%u,%s", halt.get_id(), edit_name );
			tool_t *tool = create_tool( TOOL_RENAME | SIMPLE_TOOL );
			tool->set_default_param( buf );
			welt->set_tool( tool, halt->get_owner() );
			// since init always returns false, it is safe to delete immediately
			delete tool;
		}
	}
	else if (comp == &bt_arrivals) {
		display_mode_bits &= ~SHOW_DEPARTURES;
		bt_arrivals.pressed   = !(display_mode_bits&SHOW_DEPARTURES);
		bt_departures.pressed = display_mode_bits & SHOW_DEPARTURES;
	}
	else if (comp == &bt_departures) {
		display_mode_bits |= SHOW_DEPARTURES;
		bt_arrivals.pressed   = !(display_mode_bits&SHOW_DEPARTURES);
		bt_departures.pressed = display_mode_bits&SHOW_DEPARTURES;
	}
	else if (comp == &db_mode_selector) {
		display_mode_bits ^= SHOW_LINE_NAME;
	}
	else if (comp == &bt_db_filter[0]) {
		db_filter_bits ^= DB_SHOW_PAX;
		bt_db_filter[0].pressed = db_filter_bits & DB_SHOW_PAX;
	}
	else if (comp == &bt_db_filter[1]) {
		db_filter_bits ^= DB_SHOW_MAIL;
		bt_db_filter[1].pressed = db_filter_bits & DB_SHOW_MAIL;
	}
	else if (comp == &bt_db_filter[2]) {
		db_filter_bits ^= DB_SHOW_GOODS;
		bt_db_filter[2].pressed = db_filter_bits & DB_SHOW_GOODS;
	}

	return true;
}


void halt_info_t::map_rotate90( sint16 new_ysize )
{
	view.map_rotate90(new_ysize);
}


void halt_info_t::rdwr(loadsave_t *file)
{
	// window size
	scr_size size = get_windowsize();
	size.rdwr(file);
	// halt
	koord3d halt_pos;
	if(  file->is_saving()  ) {
		halt_pos = halt->get_basis_pos3d();
	}
	halt_pos.rdwr( file );
	if(  file->is_loading()  ) {
		halt = world()->lookup( halt_pos )->get_halt();
		if (halt.is_bound()) {
			init(halt);
			reset_min_windowsize();
			set_windowsize(size);
			waiting_bar.set_halt(halt);
		}
	}
	// sort
	file->rdwr_byte( env_t::default_sortmode );

	scrolly_freight.rdwr(file);
	scrolly_departure_board.rdwr(file);
	switch_mode.rdwr(file);

	// button-to-chart array
	button_to_chart.rdwr(file);

	file->rdwr_byte(display_mode_bits);

	if (!halt.is_bound()) {
		destroy_win( this );
	}
}
