/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <string.h>

#include "pier_builder.h"
#include "../descriptor/pier_desc.h"
#include "../obj/pier.h"
#include "../boden/pier_deck.h"

#include "../simtool.h"

#include "../dataobj/scenario.h"
#include "../gui/tool_selector.h"
#include "../obj/gebaeude.h"
#include "../descriptor/building_desc.h"

karte_ptr_t pier_builder_t::welt;

stringhashtable_tpl<pier_desc_t *, N_BAGS_MEDIUM> pier_builder_t::desc_table;

static bool compare_piers(const pier_desc_t* a, const pier_desc_t* b){
    return a->get_auto_group() < b->get_auto_group();
}

koord3d pier_builder_t::lookup_deck_pos(const grund_t *gr, koord3d pos){
    sint8 hdiff;
    if(gr->get_grund_hang()==0){
        hdiff=1;
    }else{
        hdiff=slope_t::max_diff(gr->get_grund_hang());
    }
    return pos+koord3d(0,0,hdiff);
}

const char * pier_builder_t::check_below_ways(player_t *player, koord3d pos, const pier_desc_t *desc, const uint8 rotation, bool halfheight){
    const grund_t *gr = welt->lookup(pos);
    if(gr){
        if(weg_t *w0 = gr->get_weg_nr(0)){
            if(desc->get_above_way_ribi() && gr->get_weg_hang()){
                return "Placing a pier here would block way(s)";
            }
            if( ( w0->get_ribi_unmasked() & desc->get_below_way_ribi(rotation) ) == w0->get_ribi_unmasked()
                    || ( (halfheight) && w0->is_low_clearence(player) ) ){
                if(weg_t *w1  = gr->get_weg_nr(1)){
                    if( ( w1->get_ribi_unmasked() & desc->get_below_way_ribi(rotation) ) != w1->get_ribi_unmasked()
                            || ( (halfheight) && w1->is_low_clearence(player) ) ){
                        return NULL;
                    }
                }else{
                    return NULL;
                }
            }
        }else{
            return NULL;
        }
    }else{
        return NULL;
    }
    return "Placing a pier here would block way(s)";
}

const char * pier_builder_t::check_for_buildings(const grund_t *gr, const pier_desc_t *desc, const uint8 rotation){
    gebaeude_t *gb = gr->get_building();
    if(gb) {
        //same general restrictions as existing elevated ways
        if(gb->is_attraction() || gb->is_townhall()){
            return "Cannot build piers on attractions or townhalls";
        }

		const building_tile_desc_t* tile = gb->get_tile();
		if (tile->get_desc()->get_level() > welt->get_settings().get_max_elevated_way_building_level())
		{
			return "Cannot build piers on large buildings";
		}

        //mask allows any buildings
        if(desc->get_sub_obj_mask(rotation)==0xFFFFFFFF){
            return NULL;
        }else if(desc->get_sub_obj_mask(rotation)==0){
            return "Cannot build this type of pier on buildings";
        }else{
            //(future work) filter buildings

            return "Cannot build this type of pier on this type of building";
        }
    }

    return NULL;
}

void pier_builder_t::register_desc(pier_desc_t *desc){
    if (const pier_desc_t *old_pier = desc_table.remove(desc->get_name())) {
        dbg->doubled("pier", desc->get_name());
        tool_t::general_tool.remove( old_pier->get_builder() );
        delete old_pier->get_builder();
        delete old_pier;
    }

    //add the tool
    tool_build_pier_t *tool = new tool_build_pier_t;
    tool->set_icon( desc->get_cursor()->get_image_id(1) );
    tool->cursor = desc->get_cursor()->get_image_id(0);
    tool->set_default_param(desc->get_name());
    tool_t::general_tool.append( tool );
    desc->set_builder( tool );
    desc_table.put(desc->get_name(), desc);
}

const pier_desc_t *pier_builder_t::get_desc(const char *name){
    return (name ? desc_table.get(name) : NULL);
}

const char *pier_builder_t::build(player_t *player, koord3d pos, const pier_desc_t *desc, const uint8 rotation){
    grund_t *gr = NULL;

    gr = welt->lookup(pos);
    if(!gr) return "Invalid Ground";

    if(desc->get_background(gr->get_grund_hang(),rotation,0) == IMG_EMPTY){
        return "Invalid ground for this type of pier";
    }

    if(welt->lookup_hgt(pos.get_2d()) < welt->get_water_hgt(pos.get_2d())){
        return "Cannot build in deep water";
    }

    if(gr->is_water() && desc->get_above_way_ribi()){
        return "Pier could block shipping lanes";
    }

    if(gr->get_typ()==grund_t::brueckenboden || gr->get_typ()==grund_t::monorailboden){
        return "Pier can not be built on bridges";
    }

    //check for ways
    const char *msg;
    msg = check_below_ways(player,pos,desc,rotation,false);
    if(msg){
        return msg;
    }
    if(welt->get_settings().get_way_height_clearance()==2){
        msg = check_below_ways(player,pos + koord3d(0,0,-1),desc,rotation,true);
        if(msg){
            return msg;
        }
        if(desc->get_above_way_ribi()){
            if(weg_t *w0 = gr->get_weg_nr(0)){
                if(!w0->is_low_clearence(player)){
                    return "Placing a pier here would block way(s)";
                }
            }
            if(weg_t *w1 = gr->get_weg_nr(1)){
                if(!w1->is_low_clearence(player)){
                    return "Placing a pier here would block way(s)";
                }
            }
        }
    }

    //check for buildings
    msg = check_for_buildings(gr,desc,rotation);
    if(msg){
        return msg;
    }

    //check lower pier has enough supporting area
    if(gr->get_typ()==grund_t::pierdeck){
        grund_t *gr2=welt->lookup(gr->get_pos() + koord3d(0,0,-1));
        if(gr2==NULL){
            gr2=welt->lookup(gr->get_pos() + koord3d(0,0,-2));
        }
        uint32 supportmask=pier_t::get_support_mask_total(gr2);
        if((supportmask & desc->get_base_mask(rotation))!=desc->get_base_mask(rotation)){
            return "Cannot build pier.  Lower pier does not provide sufficient footing";
        }
    }

    if(desc->get_middle_mask(rotation) & pier_t::get_middle_mask_total(gr)){
        return "Cannot build pier.  New pier would occupy same space as existing pier";
    }


    //make ground on top (if needed)
    koord3d gpos=lookup_deck_pos(gr, pos);
    if(!welt->lookup(gpos)){
        pier_deck_t *deck = new pier_deck_t(gpos,desc->get_above_slope(rotation),desc->get_above_slope(rotation));
        welt->access(pos.get_2d())->boden_hinzufuegen(deck);
        deck->calc_image();
    }

    //remove trees
    if(gr->ist_natur()) {
        player_t::book_construction_costs(player, -gr->remove_trees(), gr->get_pos().get_2d());
    }

    //create pier
    pier_t *p = new pier_t(pos,player,desc,rotation);
    gr->obj_add(p);

    return NULL;
}

const char *pier_builder_t::remove(player_t *player, koord3d pos){
    grund_t *gr;
    gr = welt->lookup(pos);
    if(!gr) return "Invalid Ground";

    const char *msg=0;
    uint8 pier_cnt=0;
    pier_t *p;
    for(uint8 i = 0; i < gr->get_top(); i++){
        obj_t *ob = gr->obj_bei(i);
        if(ob->get_typ()==obj_t::pier){
            pier_cnt++;
            p=(pier_t*)ob;
        }
    }

    if(pier_cnt==0){
        return "No piers here";
    }

    if(pier_cnt==1){
        koord3d gpos=lookup_deck_pos(gr,pos);
        grund_t *bd = welt->lookup(gpos);
        if(bd){
            if(bd->obj_count() || bd->get_weg_nr(0)){
                return "Cannot remove sole load bearing pier";
            }
        }
        gr->obj_remove(p);
        delete p;
        if(bd){
            welt->access(gpos.get_2d())->boden_entfernen(bd);
            delete bd;
        }
        return NULL;
    }

    for(uint8 j = 0; j < gr->obj_count(); j++){ //try every pier in tile
        obj_t *ob = gr->obj_bei(j);
        if(ob->get_typ()==obj_t::pier){
            pier_cnt++;
            p = (pier_t*)ob;
            if(p->is_deletable(player)==NULL){
                koord3d gpos=lookup_deck_pos(gr,pos);
                grund_t *bd = welt->lookup(gpos);
                //check that we are not supporting another pier
                const char *msg2=NULL;
                for(uint8 i = 0; i < bd->obj_count(); i++){
                    if(bd->obj_bei(i)->get_typ()==obj_t::pier){
                        if(((pier_t*)bd->obj_bei(i))->get_base_mask() & p->get_support_mask()){
                            msg2= "Cannot remove load bearing pier";
                            break;
                        }
                    }
                }
                if(msg2) {
                    if(msg){
                        msg="No piers can be safely removed";
                    }else{
                        msg=msg2;
                    }
                    continue;
                }

                gr->obj_remove(p);
                delete p;

                return NULL;

            }
        }
    }

    return msg;
}

void pier_builder_t::fill_menu(tool_selector_t *tool_selector){
    if(!welt->get_scenario()->is_tool_allowed(welt->get_active_player(), TOOL_BUILD_PIER | GENERAL_TOOL)){
        return;
    }

    const uint16 time = welt->get_timeline_year_month();
    vector_tpl<const pier_desc_t*> matching(desc_table.get_count());

    for(auto const & i :desc_table){
        pier_desc_t const * const p = i.value;
        if(p->is_available(time)){
            matching.insert_ordered(p, compare_piers);
        }
    }

    FOR(vector_tpl<pier_desc_t const*>, const i, matching){
        tool_selector->add_tool_selector(i->get_builder());
    }

}
