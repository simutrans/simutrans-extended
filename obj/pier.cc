/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "pier.h"
#include "../bauer/pier_builder.h"

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
		back_image=desc->get_background(0,rotation,snow);
		front_image=desc->get_foreground(0,rotation, snow);
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

		//TODO what if load fails

		free(const_cast<char *>(s));
	}
}

void pier_t::finish_rd(){
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
	if (get_player_nr()==welt->get_public_player()->get_player_nr()) {
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

ribi_t::ribi pier_t::get_above_ribi_total(const grund_t *gr){
	ribi_t::ribi ret=ribi_t::none;
	for(uint8 i = 0; i < gr->get_top(); i++){
		obj_t *ob = gr->obj_bei(i);
		if(ob->get_typ()==obj_t::pier){
			ret |= ((pier_t*)ob)->get_above_ribi();
		}
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

uint32 pier_t::get_base_mask_total(const grund_t *gr){
	uint32 ret=0;
	for(uint i = 0; i < gr->get_top(); i++){
		obj_t *ob = gr->obj_bei(i);
		if(ob->get_typ()==obj_t::pier){
			ret |= ((pier_t*)ob)->get_base_mask();
		}
	}
	return ret;
}

uint32 pier_t::get_middle_mask_total(const grund_t *gr){
	uint32 ret=0;
	for(uint i = 0; i < gr->get_top(); i++){
		obj_t *ob = gr->obj_bei(i);
		if(ob->get_typ()==obj_t::pier){
			ret |= ((pier_t*)ob)->get_middle_mask();
		}
	}
	return ret;
}

uint32 pier_t::get_support_mask_total(const grund_t *gr){
	uint32 ret=0;
	for(uint i = 0; i < gr->get_top(); i++){
		obj_t *ob = gr->obj_bei(i);
		if(ob->get_typ()==obj_t::pier){
			ret |= ((pier_t*)ob)->get_support_mask();
		}
	}
	return ret;
}
