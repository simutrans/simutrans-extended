/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "pier.h"
#include "../bauer/pier_builder.h"
#include "../descriptor/building_desc.h"
#include "../player/simplay.h"

pier_t::pier_t(loadsave_t* const file) :
#ifdef INLINE_OBJ_TYPE
	obj_no_info_t(obj_t::pier)
#else
	obj_no_info_t()
#endif
{
	rdwr(file);
}

pier_t::pier_t(koord3d pos, player_t *player, const pier_desc_t *desc, uint8 rot) :
#ifdef INLINE_OBJ_TYPE
	obj_no_info_t(obj_t::pier, pos)
#else
	obj_no_info_t(pos)
#endif
{
	this->desc = desc;
	this->rotation = rot;
	this->bad_load = false;
	set_owner(player);
	player_t::book_construction_costs( get_owner(), -desc->get_value(), get_pos().get_2d(), desc->get_waytype());
	calc_image();
}

void pier_t::calc_image(){
	grund_t *gr=welt->lookup(get_pos());
	uint8 snow=get_pos().z >= welt->get_snowline()  ||  welt->get_climate( get_pos().get_2d() ) == arctic_climate ? 1 : 0;
	if(gr && gr->get_typ()!=grund_t::pierdeck){
		back_image=desc->get_background(gr->get_grund_hang(),rotation,snow );
		front_image=desc->get_foreground(gr->get_grund_hang(), rotation, snow);
	}else{ //assuming flat
		if(get_low_waydeck() && gr->get_weg_nr(0)){
			back_image=desc->get_background(pier_desc_t::low_waydeck_image,rotation, snow);
			front_image=desc->get_foreground(pier_desc_t::low_waydeck_image,rotation, snow);
		}else{
			back_image=desc->get_background(0,rotation,snow);
			front_image=desc->get_foreground(0,rotation, snow);
		}
	}
	set_flag(obj_t::dirty);
}

image_id pier_t::get_image() const{
	return back_image;
}

image_id pier_t::get_front_image() const{
	return front_image;
}

void pier_t::rdwr(loadsave_t *file){
	xml_tag_t d(file, "peir_t");

	obj_t::rdwr(file);

	const char *s = NULL;

	if(file->is_saving()){
		s = desc->get_name();
	}
	file->rdwr_str(s);
	file->rdwr_byte(rotation);

	if(file->is_loading()){
		desc = pier_builder_t::get_desc(s);

		//cannot determine what to replace this with here, delay until rest of game loaded
		bad_load=(desc==NULL);

		free(const_cast<char *>(s));
	}
}

void pier_t::finish_rd(){
	if(desc==NULL){
		desc = pier_builder_t::get_desc_bad_load(this->get_pos(),this->get_owner(),rotation);
	}

	player_t *player=get_owner();
	player_t::add_maintenance( player,  desc->get_maintenance(), waytype_t::any_wt);
}

void pier_t::cleanup(player_t *player2){
	player_t *player = get_owner();

	player_t::add_maintenance( player,  -desc->get_maintenance(), waytype_t::any_wt );
	player_t::book_construction_costs( player2, -desc->get_value(), get_pos().get_2d(), waytype_t::any_wt );
}

void pier_t::rotate90(){
	set_yoff(0);
	obj_t::rotate90();
	rotation = (rotation + 1) & 3; //rotation modulo 4
}

const char *pier_t::is_deletable(const player_t *player){
	if (get_owner_nr()==welt->get_public_player()->get_player_nr()) {
		return NULL;
	}
	if(grund_t *gr = welt->lookup(get_pos())){
		if(gr->get_weg_nr(1) && !gr->get_weg_nr(1)->is_deletable(player)){
			return NULL;
		}
		if(gr->get_weg_nr(0) && !gr->get_weg_nr(0)->is_deletable(player)){
			return NULL;
		}
	}
	return obj_t::is_deletable(player);
}

const grund_t* pier_t::ground_below(const grund_t *gr){
	return ground_below(gr->get_pos());
}

const grund_t* pier_t::ground_below(koord3d pos){
	const grund_t* gr = welt->lookup(pos + koord3d(0,0,-1));
	if(!gr){
		gr = welt->lookup(pos + koord3d(0,0,-2));
		if(gr && slope_t::max_diff(gr->get_grund_hang()) != 2){
			return NULL;
		}
	}
	return gr;
}


ribi_t::ribi pier_t::get_above_ribi_total(const grund_t *gr, bool gr_is_base){
	ribi_t::ribi ret=ribi_t::none;
	const grund_t *gr2;
	if(!gr_is_base){
		gr2=gr;
		gr=ground_below(gr);
	}else{
		gr2=welt->lookup(pier_builder_t::lookup_deck_pos(gr));
	}
	//first check for low deck on upper pier
	if(gr2){
		for(uint8 i = 0; i < gr2->get_top(); i++){
			obj_t *ob = gr2->obj_bei(i);
			if(ob->get_typ()==obj_t::pier && ((pier_t*)ob)->get_low_waydeck()){
				ret |= ((pier_t*)ob)->get_below_ribi();
			}
		}
	}
	if(ret){
		return ret;
	}
	//check for normal deck
	ribi_t::ribi supplement=ribi_t::none;
	if(gr){
		for(uint8 i = 0; i < gr->get_top(); i++){
			obj_t *ob = gr->obj_bei(i);
			if(ob->get_typ()==obj_t::pier){
				if(((pier_t*)ob)->get_above_way_supplement()){
					supplement |= ((pier_t*)ob)->get_above_ribi();
				}else{
					ret |= ((pier_t*)ob)->get_above_ribi();
				}
			}
		}
	}
	if(ret){
		ret |= supplement;
	}
	return ret;
}

ribi_t::ribi pier_t::get_below_ribi_total(const grund_t *gr){
	ribi_t::ribi ret=ribi_t::all;
	for(uint8 i = 0; i < gr->get_top(); i++){
		obj_t *ob = gr->obj_bei(i);
		if(ob->get_typ()==obj_t::pier){
			ret &= ((pier_t*)ob)->get_below_ribi();
		}
	}
	return ret;
}

uint64 pier_t::get_base_mask_total(const grund_t *gr){
	uint64 ret=0;
	for(uint8 i = 0; i < gr->get_top(); i++){
		obj_t *ob = gr->obj_bei(i);
		if(ob->get_typ()==obj_t::pier){
			ret |= ((pier_t*)ob)->get_base_mask();
		}
	}
	return ret;
}

uint64 pier_t::get_middle_mask_total(const grund_t *gr){
	uint64 ret=0;
	for(uint8 i = 0; i < gr->get_top(); i++){
		obj_t *ob = gr->obj_bei(i);
		if(ob->get_typ()==obj_t::pier){
			ret |= ((pier_t*)ob)->get_middle_mask();
		}
	}
	return ret;
}

uint64 pier_t::get_support_mask_total(const grund_t *gr){
	uint64 ret=0;
	for(uint8 i = 0; i < gr->get_top(); i++){
		obj_t *ob = gr->obj_bei(i);
		if(ob->get_typ()==obj_t::pier){
			ret |= ((pier_t*)ob)->get_support_mask();
		}
	}
	return ret;
}

uint16 pier_t::get_speed_limit_deck_total(const grund_t *gr, uint16 maxspeed){
	if(gr){
		for(uint8 i = 0; i < gr->get_top(); i++){
			obj_t *ob = gr->obj_bei(i);
			if(ob->get_typ()==obj_t::pier){
				if(((pier_t*)ob)->get_low_waydeck() && maxspeed > ((pier_t*)ob)->get_maxspeed()){
					maxspeed = ((pier_t*)ob)->get_maxspeed();
				}
			}
		}
	}
	gr=ground_below(gr);
	if(gr){
		for(uint8 i = 0; i < gr->get_top(); i++){
			obj_t *ob = gr->obj_bei(i);
			if(ob->get_typ()==obj_t::pier){
				if(maxspeed > ((pier_t*)ob)->get_maxspeed()){
					maxspeed = ((pier_t*)ob)->get_maxspeed();
				}
			}
		}
	}
	return maxspeed;
}

uint32 pier_t::get_deck_obj_mask_total(const grund_t *gr){
	gr=ground_below(gr);
	uint32 ret=0x0;
	if(gr){
		for(uint8 i = 0; i < gr->get_top(); i++){
			obj_t *ob = gr->obj_bei(i);
			if(ob->get_typ()==obj_t::pier){
				ret |= ((pier_t*)ob)->get_deck_obj_mask();
			}
		}
	}
	return ret;
}

uint16 pier_t::get_max_axle_load_deck_total(const grund_t *gr, uint16 maxload){
	if(gr){
		for(uint8 i = 0; i < gr->get_top(); i++){
			obj_t *ob = gr->obj_bei(i);
			if(ob->get_typ()==obj_t::pier){
				if(((pier_t*)ob)->get_low_waydeck() && maxload > ((pier_t*)ob)->get_axle_load()){
					maxload = ((pier_t*)ob)->get_axle_load();
				}
			}
		}
	}
	gr=ground_below(gr);
	if(gr){
		for(uint8 i = 0; i < gr->get_top(); i++){
			obj_t *ob = gr->obj_bei(i);
			if(ob->get_typ()==obj_t::pier){
				if(maxload > ((pier_t*)ob)->get_axle_load()){
					maxload = ((pier_t*)ob)->get_axle_load();
				}
			}
		}
	}
	return maxload;
}

uint32 pier_t::get_sub_mask_total(const grund_t *gr){
	uint32 ret=0;
	if(gr){
		for(uint8 i = 0; i < gr->get_top(); i++){
			obj_t *ob = gr->obj_bei(i);
			if(ob->get_typ()==obj_t::pier){
				ret |= ((pier_t*)ob)->get_sub_mask();
			}
		}
	}
	return ret;
}

bool pier_t::check_sub_masks(uint32 pier1, uint32 building1, uint32 pier2, uint32 building2){
	return ((pier1&building1)==pier1) && ((pier2&building2)==pier2);
}

const char* pier_t::check_building(const building_desc_t *desc, koord3d pos){
	uint32 pier_sub_1_mask=pier_t::get_sub_mask_total(welt->lookup(pos));
	uint32 pier_sub_2_mask=pier_t::get_sub_mask_total(welt->lookup(pos + koord3d(0,0,1)));
	if(desc->get_pier_needed()&&pier_sub_1_mask){
		return "This building needs the proper type of pier";
	}else if(!pier_t::check_sub_masks(pier_sub_1_mask,desc->get_pier_mask(0),pier_sub_2_mask,desc->get_pier_mask(1))){
		return "This building cannot be placed under this type of pier";
	}
	return NULL;
}
