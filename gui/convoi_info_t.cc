/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <stdio.h>

#include "convoi_info_t.h"
#include "replace_frame.h"

#include "../vehicle/air_vehicle.h"
#include "../vehicle/rail_vehicle.h"
#include "../simcolor.h"
#include "../display/viewport.h"
#include "../simworld.h"
#include "../simmenu.h"
#include "../simhalt.h"
#include "simwin.h"
#include "../convoy.h"

#include "../dataobj/schedule.h"
#include "../dataobj/translator.h"
#include "../dataobj/environment.h"
#include "../dataobj/loadsave.h"
#include "../simconvoi.h"
#include "../simline.h"

#include "../player/simplay.h"

#include "../utils/simstring.h"
#include "convoi_detail_t.h"

#include "../obj/roadsign.h"

#define CHART_HEIGHT (100)

sint16 convoi_info_t::tabstate = -1;

static const char cost_type[BUTTON_COUNT][64] =
{
	"Free Capacity",
	"Transported",
	"Distance",
	"Average speed",
	//"Maxspeed",
	"Comfort",
	"Revenue",
	"Operation",
	"Refunds",
	"Road toll",
	"Profit"
};

static const uint8 cost_type_color[BUTTON_COUNT] =
{
	COL_FREE_CAPACITY,
	COL_TRANSPORTED,
	COL_DISTANCE,
	COL_AVERAGE_SPEED,
//	COL_MAXSPEED,
	COL_COMFORT,
	COL_REVENUE,
	COL_OPERATION,
	COL_CAR_OWNERSHIP,
	COL_TOLL,
	COL_PROFIT
};

static const bool cost_type_money[BUTTON_COUNT] =
{
	false,
	false,
	false,
	false,
	//false,
	false,
	true,
	true,
	true,
	true,
	true
};

static uint8 statistic[convoi_t::MAX_CONVOI_COST] = {
	convoi_t::CONVOI_CAPACITY, convoi_t::CONVOI_TRANSPORTED_GOODS, convoi_t::CONVOI_DISTANCE, convoi_t::CONVOI_AVERAGE_SPEED, convoi_t::CONVOI_COMFORT,
	convoi_t::CONVOI_REVENUE, convoi_t::CONVOI_OPERATIONS, convoi_t::CONVOI_REFUNDS, convoi_t::CONVOI_WAYTOLL, convoi_t::CONVOI_PROFIT
};

//bool convoi_info_t::route_search_in_progress=false;

/**
 * This variable defines by which column the table is sorted
 * Values: 0 = destination
 *                 1 = via
 *                 2 = via_amount
 *                 3 = amount
 */
const char *convoi_info_t::sort_text[SORT_MODES] =
{
	"Zielort",
	"via",
	"via Menge",
	"Menge",
	"origin (detail)",
	"origin (amount)",
	"destination (detail)",
	"wealth (detail)",
	"wealth (via)",
	"accommodation (detail)",
	"accommodation (via)"
};



convoi_info_t::convoi_info_t(convoihandle_t cnv) :
	gui_frame_t(""),
	text(&freight_info),
	view(scr_size(max(64, get_base_tile_raster_width()), max(56, (get_base_tile_raster_width() * 7) / 8))),
	loading_bar(cnv),
	next_halt_number(-1),
	cont_times_history(linehandle_t(), cnv),
	scroll_freight(&container_freight, true, true),
	scroll_times_history(&cont_times_history, true)
{
	if (cnv.is_bound()) {
		init(cnv);
	}
}

void convoi_info_t::init(convoihandle_t cnv)
{
	this->cnv = cnv;
	this->mean_convoi_speed = speed_to_kmh(cnv->get_akt_speed()*4);
	this->max_convoi_speed = speed_to_kmh(cnv->get_min_top_speed()*4);
	gui_frame_t::set_name(cnv->get_name());
	gui_frame_t::set_owner(cnv->get_owner());
	cont_times_history.set_convoy(cnv);

	set_table_layout(1,0);

	// top part: speedbars, view, buttons
	add_table(2, 0)->set_alignment(ALIGN_TOP);
	{
		container_top = add_table(1,0);
		{
			input.add_listener(this);
			input.set_size(input.get_min_size());
			reset_cnv_name();
			add_component(&input);

			add_table(2,0);
			{
				add_component(&speed_label);
				speed_bar.set_rigid(true);
				add_component(&speed_bar);

				add_component(&weight_label);
				add_component(&loading_bar);

				add_table(3, 1);
				{
					new_component<gui_label_t>("Gewinn");
					profit_label.set_align(gui_label_t::left);
					add_component(&profit_label);
					add_component(&running_cost_label);
				}
				end_table();
				new_component<gui_margin_t>(100);

				add_component(&container_line,2);
				container_line.set_table_layout(4,1);
				container_line.add_component(&line_button);
				container_line.new_component<gui_label_t>("Serves Line:");
				container_line.add_component(&line_label);
				if (skinverwaltung_t::reverse_arrows) {
					img_reverse_route.set_image(cnv->get_schedule()->is_mirrored() ? skinverwaltung_t::reverse_arrows->get_image_id(0) : skinverwaltung_t::reverse_arrows->get_image_id(1), true);
					img_reverse_route.set_visible(cnv->get_reverse_schedule());
					img_reverse_route.set_rigid(true);
					container_line.add_component(&img_reverse_route);
				}
				else {
					container_line.new_component<gui_empty_t>();
				}
				// goto line button
				line_button.init( button_t::arrowright, NULL );
				line_button.set_targetpos3d( koord3d::invalid );
				line_button.add_listener( this );
				line_bound = false;

				add_component(&next_halt_cells,2);
				next_halt_cells.set_table_layout(3,1);
				next_halt_cells.new_component<gui_label_t>("Fahrtziel"); // "Destination"
				next_halt_cells.add_component(&next_halt_number);
				next_halt_cells.add_component(&target_label);

				distance_label.set_align(gui_label_t::right);
				add_component(&distance_label);
				add_component(&route_bar);

				lb_working_method.set_align(gui_label_t::right);
				add_component(&lb_working_method, 2);
			}
			end_table();

		}
		end_table();

		add_table(1, 0)->set_spacing(scr_size(0,0));
		{
			add_component(&view);
			view.set_obj(cnv->front());

			follow_button.init(button_t::roundbox_state, "follow me", scr_coord(0,0), scr_size(view.get_size().w, D_BUTTON_HEIGHT));
			follow_button.set_tooltip("Follow the convoi on the map.");
			follow_button.add_listener(this);
			add_component(&follow_button);
			new_component<gui_empty_t>();
		}
		end_table();
	}
	end_table();

	add_table(4,2)->set_force_equal_columns(true);
	{
		// this convoi doesn't belong to an AI
		button.init(button_t::roundbox | button_t::flexible, "Fahrplan");
		button.set_tooltip("Alters a schedule.");
		button.add_listener(this);
		add_component(&button);

		go_home_button.init(button_t::roundbox | button_t::flexible, "go home");
		go_home_button.set_tooltip("Sends the convoi to the last depot it departed from!");
		go_home_button.add_listener(this);
		add_component(&go_home_button);

		replace_button.init(button_t::roundbox | button_t::flexible, "Replace");
		replace_button.set_tooltip("Automatically replace this convoy.");
		add_component(&replace_button);
		replace_button.add_listener(this);

		details_button.init(button_t::roundbox | button_t::flexible, "Details");
		details_button.set_tooltip("Vehicle details");
		details_button.add_listener(this);
		add_component(&details_button);

		reverse_button.init(button_t::square_state, "reverse route");
		reverse_button.add_listener(this);
		reverse_button.set_tooltip("When this is set, the vehicle will visit stops in reverse order.");
		reverse_button.pressed = cnv->get_reverse_schedule();
		add_component(&reverse_button);

		no_load_button.init(button_t::square_state, "no load");
		no_load_button.set_tooltip("No goods are loaded onto this convoi.");
		no_load_button.add_listener(this);
		add_component(&no_load_button);

		new_component_span<gui_empty_t>(2);
	}
	end_table();

	// tab panel: connections, chart panels
	switch_mode.add_listener(this);
	add_component(&switch_mode);
	switch_mode.add_tab(&scroll_freight, translator::translate("cd_payload_tab"));

	container_freight.set_table_layout(1,0);
	container_freight.add_table(2,1);
	{
		container_freight.new_component<gui_label_t>("loaded passenger/freight");
		freight_sort_selector.clear_elements();
		for (int i = 0; i < SORT_MODES; i++)
		{
			freight_sort_selector.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate(sort_text[i]), SYSCOL_TEXT);
		}
		freight_sort_selector.set_focusable(true);
		freight_sort_selector.set_selection(env_t::default_sortmode);
		freight_sort_selector.set_width_fixed(true);
		freight_sort_selector.set_size(scr_size(D_BUTTON_WIDTH*2, D_EDIT_HEIGHT));
		freight_sort_selector.add_listener(this);
		container_freight.add_component(&freight_sort_selector);
	}
	container_freight.end_table();
	container_freight.add_component(&text);

	switch_mode.add_tab(&container_stats, translator::translate("Chart"));

	container_stats.set_table_layout(1,0);

	chart.set_dimension(12, 10000);
	chart.set_background(SYSCOL_CHART_BACKGROUND);
	chart.set_min_size(scr_size(0, CHART_HEIGHT));
	container_stats.add_component(&chart);

	container_stats.add_table(4, int((convoi_t::MAX_CONVOI_COST+3) / 4))->set_force_equal_columns(true);

	for (int cost = 0; cost<convoi_t::MAX_CONVOI_COST; cost++) {
		uint16 curve = chart.add_curve( color_idx_to_rgb(cost_type_color[cost]), cnv->get_finance_history(), convoi_t::MAX_CONVOI_COST, statistic[cost], MAX_MONTHS, cost_type_money[cost], false, true, cost_type_money[cost]*2 );

		button_t *b = container_stats.new_component<button_t>();
		b->init(button_t::box_state_automatic  | button_t::flexible, cost_type[cost]);
		b->background_color = color_idx_to_rgb(cost_type_color[cost]);
		b->pressed = false;

		button_to_chart.append(b, &chart, curve);
	}
	container_stats.end_table();

	switch_mode.add_tab(&scroll_times_history, translator::translate("times_history"));

	cnv->set_sortby( env_t::default_sortmode );

	speed_bar.set_base(max_convoi_speed);
	speed_bar.set_vertical(false);
	speed_bar.add_color_value(&mean_convoi_speed, color_idx_to_rgb(COL_GREEN));

	// we update this ourself!
	route_bar.init(&cnv_route_index, 0);
	if( cnv->get_vehicle_count()>0  &&  dynamic_cast<rail_vehicle_t *>(cnv->front()) ) {
		// only for trains etc.
		route_bar.set_reservation( &next_reservation_index );
	}
	route_bar.set_height(9);

	update_labels();

	reset_min_windowsize();
	set_windowsize(get_min_windowsize());

	set_resizemode(diagonal_resize);
}


// only handle a pending renaming ...
convoi_info_t::~convoi_info_t()
{
	button_to_chart.clear();
	// rename if necessary
	rename_cnv();
}


void convoi_info_t::update_labels()
{
	air_vehicle_t* air_vehicle = NULL;
	if (cnv->front()->get_waytype() == air_wt)
	{
		air_vehicle = (air_vehicle_t*)cnv->front();
	}
	bool runway_too_short = air_vehicle == NULL ? false : air_vehicle->is_runway_too_short();

	speed_bar.set_visible(false);
	route_bar.set_state(1);
	switch (cnv->get_state())
	{
		case convoi_t::WAITING_FOR_CLEARANCE_ONE_MONTH:
		case convoi_t::WAITING_FOR_CLEARANCE:

			if (runway_too_short)
			{
				speed_label.buf().printf("%s (%s) %i%s", translator::translate("Runway too short"), translator::translate("requires"), cnv->front()->get_desc()->get_minimum_runway_length(), translator::translate("m"));
				speed_label.set_color(COL_CAUTION);
				route_bar.set_state(3);
			}
			else
			{
				speed_label.buf().append(translator::translate("Waiting for clearance!"));
				speed_label.set_color(COL_CAUTION);
				route_bar.set_state(1);
			}
			break;

		case convoi_t::CAN_START:
		case convoi_t::CAN_START_ONE_MONTH:

			speed_label.buf().append(translator::translate("Waiting for clearance!"));
			speed_label.set_color(SYSCOL_TEXT);
			route_bar.set_state(1);
			break;

		case convoi_t::EMERGENCY_STOP:

			char emergency_stop_time[64];
			cnv->snprintf_remaining_emergency_stop_time(emergency_stop_time, sizeof(emergency_stop_time));

			speed_label.buf().printf(translator::translate("emergency_stop %s left"), emergency_stop_time);
			speed_label.set_color(COL_DANGER);
			route_bar.set_state(3);
			break;

		case convoi_t::LOADING:

			char waiting_time[64];
			cnv->snprintf_remaining_loading_time(waiting_time, sizeof(waiting_time));
			if (cnv->get_schedule()->get_current_entry().wait_for_time)
			{
				speed_label.buf().printf(translator::translate("Waiting for schedule. %s left"), waiting_time);
				speed_label.set_color(COL_CAUTION);
			}
			else if (cnv->get_loading_limit())
			{
				if (!cnv->is_wait_infinite() && strcmp(waiting_time, "0:00"))
				{
					speed_label.buf().printf(translator::translate("Loading (%i->%i%%), %s left!"), cnv->get_loading_level(), cnv->get_loading_limit(), waiting_time);
					speed_label.set_color(COL_CAUTION);
				}
				else
				{
					speed_label.buf().printf(translator::translate("Loading (%i->%i%%)!"), cnv->get_loading_level(), cnv->get_loading_limit());
					speed_label.set_color(COL_CAUTION);
				}
			}
			else
			{
				speed_label.buf().printf(translator::translate("Loading. %s left!"), waiting_time);
				speed_label.set_color(SYSCOL_TEXT);
			}
			route_bar.set_state(1);

			break;

		case convoi_t::WAITING_FOR_LOADING_THREE_MONTHS:
		case convoi_t::WAITING_FOR_LOADING_FOUR_MONTHS:

			speed_label.buf().printf(translator::translate("Loading (%i->%i%%) Long Time"), cnv->get_loading_level(), cnv->get_loading_limit());
			route_bar.set_state(2);
			break;

		case convoi_t::REVERSING:

			char reversing_time[64];
			cnv->snprintf_remaining_reversing_time(reversing_time, sizeof(reversing_time));
			switch (cnv->get_terminal_shunt_mode()) {
			case convoi_t::rearrange:
			case convoi_t::shunting_loco:
				speed_label.buf().printf(translator::translate("Shunting. %s left"), reversing_time);
				break;
			case convoi_t::change_direction:
				speed_label.buf().printf(translator::translate("Changing direction. %s left"), reversing_time);
				break;
			default:
				speed_label.buf().printf(translator::translate("Reversing. %s left"), reversing_time);
				break;
			}
			speed_label.set_color(SYSCOL_TEXT);
			route_bar.set_state(1);
			break;

		case convoi_t::CAN_START_TWO_MONTHS:
		case convoi_t::WAITING_FOR_CLEARANCE_TWO_MONTHS:

			if (runway_too_short)
			{
				speed_label.buf().printf("%s (%s %i%s)", translator::translate("Runway too short"), translator::translate("requires"), cnv->front()->get_desc()->get_minimum_runway_length(), translator::translate("m"));
				speed_label.set_color(COL_DANGER);
				route_bar.set_state(3);
			}
			else
			{
				speed_label.buf().append(translator::translate("clf_chk_stucked"));
				speed_label.set_color(COL_WARNING);
				route_bar.set_state(2);
			}
			break;

		case convoi_t::NO_ROUTE:

			if (runway_too_short)
			{
				speed_label.buf().printf("%s (%s %i%s)", translator::translate("Runway too short"), translator::translate("requires"), cnv->front()->get_desc()->get_minimum_runway_length(), translator::translate("m"));
			}
			else
			{
				speed_label.buf().append(translator::translate("clf_chk_noroute"));
			}
			speed_label.set_color(COL_DANGER);
			route_bar.set_state(3);
			break;

		case convoi_t::NO_ROUTE_TOO_COMPLEX:
			//speed_label.buf().append(translator::translate("no_route_too_complex_message"));
			speed_label.buf().append(translator::translate("clf_chk_noroute"));
			speed_label.set_color(COL_DANGER);
			route_bar.set_state(3);
			break;

		case convoi_t::OUT_OF_RANGE:

			//speed_label.buf().printf(translator::translate("out_of_range (max %i km)"), cnv->front()->get_desc()->get_range());
			speed_label.buf().printf("%s (%s %i%s)", translator::translate("out of range"), translator::translate("max"), cnv->front()->get_desc()->get_range(), translator::translate("km"));
			speed_label.set_color(COL_DANGER);
			route_bar.set_state(3);
			break;

		case convoi_t::DRIVING:
			route_bar.set_state(0);

		default:
			if (runway_too_short)
			{
				speed_label.buf().printf("%s (%s %i%s)", translator::translate("Runway too short"), translator::translate("requires"), cnv->front()->get_desc()->get_minimum_runway_length(), translator::translate("m"));
				speed_label.set_color(COL_DANGER);
				route_bar.set_state(3);
			}
			else
			{
				uint32 empty_weight = cnv->get_vehicle_summary().weight;

				speed_bar.set_visible(true);
				//use median speed to avoid flickering
				mean_convoi_speed += speed_to_kmh(cnv->get_akt_speed() * 4);
				mean_convoi_speed /= 2;
				const sint32 min_speed = cnv->calc_max_speed(cnv->get_weight_summary());
				const sint32 max_speed = cnv->calc_max_speed(weight_summary_t(empty_weight, cnv->get_current_friction()));
				speed_label.buf().printf(translator::translate(min_speed == max_speed ? "%i km/h (max. %ikm/h)" : "%i km/h (max. %i %s %ikm/h)"),
					(mean_convoi_speed + 3) / 4, min_speed, translator::translate("..."), max_speed);
				speed_label.set_color(SYSCOL_TEXT);
			}
			break;
	}
	profit_label.append_money(cnv->get_jahresgewinn()/100.0);
	profit_label.update();

	running_cost_label.buf().printf("(%1.2f$/km, %1.2f$/month", cnv->get_per_kilometre_running_cost() / 100.0, welt->calc_adjusted_monthly_figure(cnv->get_fixed_cost()) / 100.0);
	running_cost_label.update();

	weight_label.buf().printf("%s %.1ft (%.1ft)",
		translator::translate("Weight:"),
		cnv->get_weight_summary().weight / 1000.0,
		(cnv->get_weight_summary().weight-cnv->get_sum_weight()) / 1000.0);
	weight_label.update();

	// next stop
	const schedule_t *schedule = cnv->get_schedule();
	if (go_home_button.pressed) {
		target_label.buf().append(translator::translate("go home"));
	}
	else {
		schedule_t::gimme_short_stop_name(target_label.buf(), welt, cnv->get_owner(), schedule, schedule->get_current_stop(), 50);
	}
	target_label.update();
	uint8 halt_col_idx = COL_INACTIVE;
	uint8 halt_symbol_style=0;
	const koord3d next_pos = schedule->get_current_entry().pos;
	const halthandle_t next_halt = haltestelle_t::get_halt(next_pos, cnv->get_owner());
	if (next_halt.is_bound()) {
		halt_col_idx= next_halt->get_owner()->get_player_color1();
		if ((next_halt->registered_lines.get_count() + next_halt->registered_convoys.get_count()) > 1) {
			halt_symbol_style = gui_schedule_entry_number_t::number_style::interchange;
		}
	}
	else if (welt->lookup(next_pos) && welt->lookup(next_pos)->get_depot() != NULL) {
		halt_symbol_style=gui_schedule_entry_number_t::number_style::depot;
	}
	next_halt_number.init(schedule->get_current_stop(), halt_col_idx, halt_symbol_style, next_pos);

	// distance
	sint32 cnv_route_index_left = cnv->get_route()->get_count() - 1 - cnv_route_index;
	double distance;
	char distance_display[13];
	distance = (double)(cnv_route_index_left * welt->get_settings().get_meters_per_tile()) / 1000.0;

	if (distance <= 0)
	{
		sprintf(distance_display, "0km");
	}
	else if (distance < 1)
	{
		sprintf(distance_display, "%.0fm", distance * 1000);
	}
	else
	{
		uint n_actual = distance < 5 ? 1 : 0;
		char tmp[10];
		number_to_string(tmp, distance, n_actual);
		sprintf(distance_display, "%skm", tmp);
	}
	distance_label.buf().printf( translator::translate("%s left"), distance_display);

	// only show assigned line, if there is one!
	if(  cnv->get_line().is_bound()  ) {
		line_label.buf().append(cnv->get_line()->get_name());
		line_label.set_color(cnv->get_line()->get_state_color());
	}
	line_label.update();


	vehicle_t* v1 = cnv->get_vehicle(0);
	if (v1->get_waytype() == track_wt || v1->get_waytype() == maglev_wt || v1->get_waytype() == tram_wt || v1->get_waytype() == narrowgauge_wt || v1->get_waytype() == monorail_wt) {
		if (cnv->in_depot()) {
			lb_working_method.buf().append("");
		}
		else {
			// Current working method
			rail_vehicle_t* rv1 = (rail_vehicle_t*)v1;
			rail_vehicle_t* rv2 = (rail_vehicle_t*)cnv->get_vehicle(cnv->get_vehicle_count() - 1);
			lb_working_method.buf().printf("%s: %s", translator::translate("Current working method"), translator::translate(rv1->is_leading() ? roadsign_t::get_working_method_name(rv1->get_working_method()) : roadsign_t::get_working_method_name(rv2->get_working_method())));
		}
	}
	else if (uint16 minimum_runway_length = cnv->get_vehicle(0)->get_desc()->get_minimum_runway_length()) {
		// for air vehicle
		lb_working_method.buf().printf("%s: %i m \n", translator::translate("Minimum runway length"), minimum_runway_length);
	}
	lb_working_method.update();

	// buffer update now only when needed by convoi itself => dedicated buffer for this
	const int old_len=freight_info.len();
	cnv->get_freight_info(freight_info);
	if(  old_len!=freight_info.len()  ) {
		text.recalc_size();
		scroll_freight.set_size( scroll_freight.get_size() );
	}

	if (skinverwaltung_t::reverse_arrows) {
		img_reverse_route.set_image(cnv->get_schedule()->is_mirrored() ? skinverwaltung_t::reverse_arrows->get_image_id(0) : skinverwaltung_t::reverse_arrows->get_image_id(1), true);
		img_reverse_route.set_visible(cnv->get_reverse_schedule());
	}

	// realign container - necessary if strings changed length
	container_top->set_size( container_top->get_size() );
	set_min_windowsize(scr_size(max(D_DEFAULT_WIDTH, get_min_windowsize().w), D_TITLEBAR_HEIGHT + switch_mode.get_pos().y + D_TAB_HEADER_HEIGHT));
}


/**
 * Draw new component. The values to be passed refer to the window
 * i.e. It's the screen coordinates of the window where the
 * component is displayed.
 */
void convoi_info_t::draw(scr_coord pos, scr_size size)
{
	if (!cnv.is_bound() || cnv->in_depot() || cnv->get_vehicle_count() == 0)
	{
		destroy_win(this);
	}
	next_reservation_index = cnv->get_next_reservation_index();

	// make titlebar dirty to display the correct coordinates
	if(cnv->get_owner()==welt->get_active_player()  &&  !welt->get_active_player()->is_locked()) {

		if (line_bound != cnv->get_line().is_bound()  ) {
			line_bound = cnv->get_line().is_bound();
			container_line.set_visible(line_bound);
		}
		button.enable();
		line_button.enable();
		details_button.pressed = win_get_magic(magic_convoi_detail + cnv.get_id());

		if (!cnv->get_schedule()->empty()) {
			const grund_t* g = welt->lookup(cnv->get_schedule()->get_current_entry().pos);
			if (g != NULL && g->get_depot()) {
				go_home_button.disable();
			}
			else {
				goto enable_home;
			}
		}
		else
		{
		enable_home:
			go_home_button.enable();
		}

		// Bernd Gabriel, 01.07.2009: show some colored texts and indicator
		//input.set_color(cnv->has_obsolete_vehicles() ? color_idx_to_rgb(COL_OBSOLETE) : SYSCOL_TEXT);

		if(  grund_t* gr=welt->lookup(cnv->get_schedule()->get_current_entry().pos)  ) {
			go_home_button.pressed = gr->get_depot() != NULL;
		}
		details_button.pressed = win_get_magic(magic_convoi_detail + cnv.get_id());
		no_load_button.pressed = cnv->get_no_load();
		no_load_button.enable();
		replace_button.pressed = cnv->get_replace();
		replace_button.set_text(cnv->get_replace() ? "Replacing" : "Replace");
		replace_button.enable();
		reverse_button.pressed = cnv->get_reverse_schedule();
		reverse_button.set_text(cnv->get_schedule()->is_mirrored() ? "Return trip" : "reverse route");
		reverse_button.set_tooltip(cnv->get_schedule()->is_mirrored() ? "during the return trip of the mirror schedule" : "When this is set, the vehicle will visit stops in reverse order.");
		reverse_button.enable();
	}
	else {
		if (line_bound) {
			// do not jump to other player line window
			remove_component(&line_button);
			line_bound = false;
		}
		button.disable();
		go_home_button.disable();
		no_load_button.disable();
		replace_button.disable();
		reverse_button.disable();
	}

/*
#ifdef DEBUG_CONVOY_STATES
		{
			// Debug: show covnoy states
			int debug_row = 9;
			{
				const int pos_y = pos_y0 + debug_row * LINESPACE;

				char state_text[32];
				switch (cnv->get_state())
				{
				case convoi_t::INITIAL:			sprintf(state_text, "INITIAL");	break;
				case convoi_t::EDIT_SCHEDULE:	sprintf(state_text, "EDIT_SCHEDULE");	break;
				case convoi_t::ROUTING_1:		sprintf(state_text, "ROUTING_1");	break;
				case convoi_t::ROUTING_2:		sprintf(state_text, "ROUTING_2");	break;
				case convoi_t::DUMMY5:			sprintf(state_text, "DUMMY5");	break;
				case convoi_t::NO_ROUTE:		sprintf(state_text, "NO_ROUTE");	break;
				case convoi_t::NO_ROUTE_TOO_COMPLEX:		sprintf(state_text, "NO_ROUTE_TOO_COMPLEX");	break;
				case convoi_t::DRIVING:			sprintf(state_text, "DRIVING");	break;
				case convoi_t::LOADING:			sprintf(state_text, "LOADING");	break;
				case convoi_t::WAITING_FOR_CLEARANCE:			sprintf(state_text, "WAITING_FOR_CLEARANCE");	break;
				case convoi_t::WAITING_FOR_CLEARANCE_ONE_MONTH:	sprintf(state_text, "WAITING_FOR_CLEARANCE_ONE_MONTH");	break;
				case convoi_t::CAN_START:			sprintf(state_text, "CAN_START");	break;
				case convoi_t::CAN_START_ONE_MONTH:	sprintf(state_text, "CAN_START_ONE_MONTH");	break;
				case convoi_t::SELF_DESTRUCT:		sprintf(state_text, "SELF_DESTRUCT");	break;
				case convoi_t::WAITING_FOR_CLEARANCE_TWO_MONTHS:	sprintf(state_text, "WAITING_FOR_CLEARANCE_TWO_MONTHS");	break;
				case convoi_t::CAN_START_TWO_MONTHS:				sprintf(state_text, "CAN_START_TWO_MONTHS");	break;
				case convoi_t::LEAVING_DEPOT:	sprintf(state_text, "LEAVING_DEPOT");	break;
				case convoi_t::ENTERING_DEPOT:	sprintf(state_text, "ENTERING_DEPOT");	break;
				case convoi_t::REVERSING:		sprintf(state_text, "REVERSING");	break;
				case convoi_t::OUT_OF_RANGE:	sprintf(state_text, "OUT_OF_RANGE");	break;
				case convoi_t::EMERGENCY_STOP:	sprintf(state_text, "EMERGENCY_STOP");	break;
				case convoi_t::ROUTE_JUST_FOUND:	sprintf(state_text, "ROUTE_JUST_FOUND");	break;
				case convoi_t::WAITING_FOR_LOADING_THREE_MONTHS:	sprintf(state_text, "WAITING_FOR_LOADING_THREE_MONTHS");	break;
				case convoi_t::WAITING_FOR_LOADING_FOUR_MONTHS:	sprintf(state_text, "WAITING_FOR_LOADING_FOUR_MONTHS");	break;
				default:	sprintf(state_text, "default");	break;

				}

				display_proportional_rgb(pos_x, pos_y, state_text, ALIGN_LEFT, SYSCOL_TEXT, true);
				debug_row++;
			}
			if (runway_too_short)
			{
				const int pos_y = pos_y0 + debug_row * LINESPACE;
				char runway_too_short[32];
				sprintf(runway_too_short, "air->runway_too_short");
				display_proportional_rgb(pos_x, pos_y, runway_too_short, ALIGN_LEFT, SYSCOL_TEXT, true);
				debug_row++;
			}
			if (cnv->front()->get_is_overweight() == true) // This doesnt flag!
			{
				const int pos_y = pos_y0 + debug_row * LINESPACE;
				char too_heavy[32];
				sprintf(too_heavy, "is_overweight");
				display_proportional_rgb(pos_x, pos_y, too_heavy, ALIGN_LEFT, SYSCOL_TEXT, true);
				debug_row++;
			}
		}
#endif
*/
/*
#ifdef DEBUG_PHYSICS
		// Show braking distance
		{
			const int pos_y = pos_y0 + 6 * LINESPACE; // line 7
			const sint32 brk_meters = convoy.calc_min_braking_distance(convoy.get_weight_summary(), speed_to_v(cnv->get_akt_speed()));
			char tmp[256];
			sprintf(tmp, translator::translate("minimum brake distance"));
			const int len = display_proportional_rgb(pos_x, pos_y, tmp, ALIGN_LEFT, SYSCOL_TEXT, true);
			sprintf(tmp, translator::translate(": %im"), brk_meters);
			display_proportional_rgb(pos_x + len, pos_y, tmp, ALIGN_LEFT, cnv->get_akt_speed() <= cnv->get_akt_speed_soll() ? SYSCOL_TEXT : SYSCOL_TEXT_STRONG, true);
		}
		{
			const int pos_y = pos_y0 + 7 * LINESPACE; // line 8
			char tmp[256];
			const settings_t &settings = welt->get_settings();
			const sint32 kmh = speed_to_kmh(cnv->next_speed_limit);
			const sint32 m_til_limit = settings.steps_to_meters(cnv->steps_til_limit).to_sint32();
			const sint32 m_til_brake = settings.steps_to_meters(cnv->steps_til_brake).to_sint32();
			if (kmh)
				sprintf(tmp, translator::translate("max %ikm/h in %im, brake in %im "), kmh, m_til_limit, m_til_brake);
			else
				sprintf(tmp, translator::translate("stop in %im, brake in %im "), m_til_limit, m_til_brake);
			const int len = display_proportional_rgb(pos_x, pos_y, tmp, ALIGN_LEFT, SYSCOL_TEXT, true);
		}
		{
			const int pos_y = pos_y0 + 8 * LINESPACE; // line 9
			const sint32 current_friction = cnv->front()->get_frictionfactor();
			char tmp[256];
			sprintf(tmp, translator::translate("current friction factor"));
			const int len = display_proportional_rgb(pos_x, pos_y, tmp, ALIGN_LEFT, SYSCOL_TEXT, true);
			sprintf(tmp, translator::translate(": %i"), current_friction);
			display_proportional_rgb(pos_x + len, pos_y, tmp, ALIGN_LEFT, current_friction <= 20 ? SYSCOL_TEXT : SYSCOL_TEXT_STRONG, true);
		}
#endif
*/

	// update button & labels
	follow_button.pressed = (welt->get_viewport()->get_follow_convoi()==cnv);
	update_labels();

	route_bar.set_base(cnv->get_route()->get_count()-1);
	cnv_route_index = cnv->front()->get_route_index() - 1;

	// all gui stuff set => display it
	gui_frame_t::draw(pos, size);
}


bool convoi_info_t::is_weltpos()
{
	return (welt->get_viewport()->get_follow_convoi()==cnv);
}


koord3d convoi_info_t::get_weltpos( bool set )
{
	if(  set  ) {
		if(  !is_weltpos()  )  {
			welt->get_viewport()->set_follow_convoi( cnv );
		}
		else {
			welt->get_viewport()->set_follow_convoi( convoihandle_t() );
		}
		return koord3d::invalid;
	}
	else {
		return cnv->get_pos();
	}
}

void convoi_info_t::set_tab_opened()
{
	tabstate = switch_mode.get_active_tab_index();

	const scr_coord_val margin_above_tab = switch_mode.get_pos().y + D_TAB_HEADER_HEIGHT + D_TITLEBAR_HEIGHT;

	switch (tabstate)
	{
		case 0: // loaded detail
		default:
			set_windowsize(scr_size(get_windowsize().w, min(display_get_height() - margin_above_tab, margin_above_tab + scroll_freight.get_size().h)));
			break;
		case 1: // chart
			set_windowsize(scr_size(get_windowsize().w, min(display_get_height() - margin_above_tab, margin_above_tab + container_stats.get_size().h)));
			break;
		case 2: // times history
			set_windowsize(scr_size(get_windowsize().w, min(display_get_height() - margin_above_tab, margin_above_tab + cont_times_history.get_size().h)));
			break;

	}
}


/**
 * This method is called if an action is triggered
 */
bool convoi_info_t::action_triggered( gui_action_creator_t *comp,value_t /* */)
{
	if(  comp == &switch_mode  &&  get_windowsize().h == get_min_windowsize().h  ) {
		set_tab_opened();
		return true;
	}

	// follow convoi on map?
	if(  comp == &follow_button  ) {
		if(welt->get_viewport()->get_follow_convoi()==cnv) {
			// stop following
			welt->get_viewport()->set_follow_convoi( convoihandle_t() );
		}
		else {
			welt->get_viewport()->set_follow_convoi(cnv);
		}
		return true;
	}

	// details?
	if(  comp == &details_button  ) {
		create_win(20, 20, new convoi_detail_t(cnv), w_info, magic_convoi_detail+cnv.get_id() );
		return true;
	}

	if(  comp == &line_button  ) {
		cnv->get_owner()->simlinemgmt.show_lineinfo( cnv->get_owner(), cnv->get_line() );
		welt->set_dirty();
	}
	else if(  comp == &input  ) {
		// rename if necessary
		rename_cnv();
	}
	// sort by what
	else if(  comp == &freight_sort_selector  ) {
		sint32 sort_mode = freight_sort_selector.get_selection();
		if (sort_mode < 0)
		{
			freight_sort_selector.set_selection(0);
			sort_mode = 0;
		}
		env_t::default_sortmode = (sort_mode_t)((int)(sort_mode)%(int)SORT_MODES);
		cnv->set_sortby( env_t::default_sortmode );
	}

	// some actions only allowed, when I am the player
	if(cnv->get_owner()==welt->get_active_player()  &&  !welt->get_active_player()->is_locked()) {

		if(  comp == &button  ) {
			cnv->call_convoi_tool( 'f', NULL );
			return true;
		}

		//if(comp == &no_load_button    &&    !route_search_in_progress) {
		if(  comp == &no_load_button  ) {
			cnv->call_convoi_tool( 'n', NULL );
			return true;
		}

		if(  comp == &replace_button  )	{
			create_win(20, 20, new replace_frame_t(cnv, get_name()), w_info, magic_replace + cnv.get_id() );
			return true;
		}

		if(  comp == &go_home_button  ) {
			// limit update to certain states that are considered to be safe for schedule updates
			if(cnv->is_locked())
			{
				DBG_MESSAGE("convoi_info_t::action_triggered()","convoi state %i => cannot change schedule ... ", cnv->get_state() );
				return false;
			}
			go_home_button.pressed = true;
			cnv->call_convoi_tool('P', NULL);
			go_home_button.pressed = false;
			return true;
		} // end go home button

		if(comp == &reverse_button)
		{
			cnv->call_convoi_tool('V', NULL);
			reverse_button.pressed = !reverse_button.pressed;
		}
	}

	return false;
}


void convoi_info_t::reset_cnv_name()
{
	// change text input of selected line
	if (cnv.is_bound()) {
		tstrncpy(old_cnv_name, cnv->get_name(), sizeof(old_cnv_name));
		tstrncpy(cnv_name, cnv->get_name(), sizeof(cnv_name));
		input.set_text(cnv_name, sizeof(cnv_name));
	}
}


void convoi_info_t::rename_cnv()
{
	if (cnv.is_bound()) {
		const char *t = input.get_text();
		// only change if old name and current name are the same
		// otherwise some unintended undo if renaming would occur
		if(  t  &&  t[0]  &&  strcmp(t, cnv->get_name())  &&  strcmp(old_cnv_name, cnv->get_name())==0) {
			// text changed => call tool
			cbuffer_t buf;
			buf.printf( "c%u,%s", cnv.get_id(), t );
			tool_t *tool = create_tool( TOOL_RENAME | SIMPLE_TOOL );
			tool->set_default_param( buf );
			welt->set_tool( tool, cnv->get_owner());
			// since init always returns false, it is safe to delete immediately
			delete tool;
			// do not trigger this command again
			tstrncpy(old_cnv_name, t, sizeof(old_cnv_name));
		}
	}
}


void convoi_info_t::rdwr(loadsave_t *file)
{
	// handle
	convoi_t::rdwr_convoihandle_t(file, cnv);

	file->rdwr_byte( env_t::default_sortmode );

	// window size
	scr_size size = get_windowsize();
	size.rdwr( file );

	// init window
	if(  file->is_loading()  &&  cnv.is_bound())
	{
		init(cnv);

		reset_min_windowsize();
		set_windowsize(size);
	}

	// after initialization
	// components
	scroll_freight.rdwr(file);
	switch_mode.rdwr(file);
	// button-to-chart array
	button_to_chart.rdwr(file);

	// convoy vanished
	if(  !cnv.is_bound()  ) {
		dbg->error( "convoi_info_t::rdwr()", "Could not restore convoi info window of (%d)", cnv.get_id() );
		destroy_win( this );
		return;
	}
}
