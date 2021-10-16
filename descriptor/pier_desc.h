/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DESCRIPTOR_PIER_DESC_H
#define DESCRIPTOR_PIER_DESC_H


#include "way_desc.h"
#include "../dataobj/ribi.h"
#include "../dataobj/way_constraints.h"

class grund_t;

//pier and (sometimes) deck for viaduct
class pier_desc_t : public obj_desc_transport_infrastructure_t {
    friend class pier_reader_t;

private:
    ribi_t::ribi above_way_ribi :4;      //which directions ways can go on deck (0 if no deck)
    ribi_t::ribi below_way_ribi :4;      //which directions ways can go beneath (0 for no ways)


    slope_t::type above_slope;        //(future use) slope of ramp

    uint8 auto_group;                 //(future use) group for automatic placement (0 is nor group)
    uint8 auto_height;                //(future use) bit feild for automatic placement height
                                      //(future use) MSB to LSB: Reserved; Reserved; Only 1 pier; Top Peir; Upper third; Middle; lower Third; Bottom;

    //masks are rotated by 8 bits during rotation;
    uint32 base_mask;           //bit field for where foundation is needed (up to pakset designer)
    uint32 middle_mask;         //bit field to prevend piers occupying the same physical space (up to pakset designer)
    uint32 support_mask;        //bit field for locations on deck (or lack thereof) where columns can be supported (up to packset designer)
    uint32 sub_obj_mask;        //(future use) restrict buildings and waytypes coninsiding with
    uint32 deck_obj_mask;       //(future use) restrict objects and buildings on deck

    //other items
    uint32 max_weight; //(future use)
    uint32 pier_weight; //(future use)
    uint8 number_of_seasons : 2;
    uint8 rotational_symmetry : 3;

    uint8 drag_ribi : 4; //direction for dragging
    uint8 above_way_supplement : 1; //(future use) only allow above_way_ribi if not only pier

    static uint32 rotate_mask(uint32 m,uint8 r){
        return (m << (8*r)) | (m >> (32 - 8*r));
    }

    //TODO maybe move this into ribi.h
    static ribi_t::ribi rotate_ribi(ribi_t::ribi x, uint8 r){
        return ((x | x<<4) >> (r) ) & 0xf;
    }

    uint8 img_rotation(uint8 rot) const {
        switch (rotational_symmetry) {
        case 4: return rot;
        case 2: return rot&1;
        case 1: return 0;
        }
        return 0;
    }

public:
	const char *get_name() const { return get_cursor()->get_name(); }
	const char *get_copyright() const { return get_cursor()->get_copyright(); }

	skin_desc_t const* get_cursor() const { return get_child<skin_desc_t>(4); }

	//accessors
	ribi_t::ribi get_above_way_ribi(uint8 rotation=0) const {return rotate_ribi(above_way_ribi,rotation);}
	ribi_t::ribi get_below_way_ribi(uint8 rotation=0) const {return rotate_ribi(below_way_ribi, rotation);}
	slope_t::type get_above_slope(uint8 rotation=0) const;
	uint8 get_auto_group() const {return auto_group;}
	uint32 get_base_mask(uint8 rotation=0) const {return rotate_mask(base_mask,rotation);}
	uint32 get_middle_mask(uint8 rotation=0) const {return rotate_mask(middle_mask,rotation);}
	uint32 get_support_mask(uint8 rotation=0) const {return rotate_mask(support_mask, rotation);}
	uint32 get_sub_obj_mask(uint8 rotation=0) const {return rotate_mask(sub_obj_mask, rotation);}
	ribi_t::ribi get_drag_ribi(uint8 rotation=0) const {return rotate_ribi(drag_ribi, rotation);}


	image_id get_background(slope_t::type slope, uint8 rotation, uint8 season) const;

	image_id get_foreground(slope_t::type slope, uint8 rotation, uint8 season) const;

	void calc_checksum(checksum_t *chk) const;
};

#endif // PIER_DESC_H
