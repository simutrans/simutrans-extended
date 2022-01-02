/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef OBJ_BAUM_H
#define OBJ_BAUM_H


#include "simobj.h"

#include "../tpl/stringhashtable_tpl.h"
#include "../tpl/vector_tpl.h"
#include "../tpl/weighted_vector_tpl.h"
#include "../descriptor/tree_desc.h"
#include "../simcolor.h"
#include "../dataobj/environment.h"

#include <string>

#define TREE_MAX_RANDOM_AGE (703)
#define TREE_MIN_PROBABILITY (38)

/**
 * Simulated trees for Simutrans.
 */
class baum_t : public obj_t
{
private:
	static FLAGGED_PIXVAL outline_color;

	/** month of birth */
	uint16 purchase_time;

	/** type of tree (was 9 but for more compact saves now only 254 different tree types are allowed) */
	uint8 tree_id;

	uint8 season:3;

	/** z-offset, max TILE_HEIGHT_STEP ie 4 bits */
	uint8 zoff:4;

	// one bit free ;)

	// static for administration
	static stringhashtable_tpl<const tree_desc_t *, N_BAGS_SMALL> desc_names;
	static vector_tpl<const tree_desc_t *> tree_list;
	static weighted_vector_tpl<uint32>* tree_list_per_climate;

	bool plant_tree();

	/**
	 * calculate offsets for new trees
	 */
	void calc_off(uint8 slope, sint8 x=-128, sint8 y=-128);

	static const uint8 invalid_tree_id = 0xFF;

	static uint8 random_tree_for_climate_intern(climate cl);

	static uint8 plant_tree_on_coordinate(koord pos, const uint8 maximum_count, const uint8 count);

public:
	/**
	 * Only the load save constructor should be called outside
	 * otherwise I suggest use the plant tree function (see below)
	 */
	baum_t(loadsave_t *file);
	baum_t(koord3d pos);
	baum_t(koord3d pos, uint8 type, sint32 age, uint8 slope );
	baum_t(koord3d pos, const tree_desc_t *desc);

	void rdwr(loadsave_t *file) OVERRIDE;

	void finish_rd() OVERRIDE;

	image_id get_image() const OVERRIDE;

	/**
	 * hide trees eventually with transparency
	 */
	FLAGGED_PIXVAL get_outline_colour() const OVERRIDE { return outline_color; }
	image_id get_outline_image() const OVERRIDE;

	static void recalc_outline_color() { outline_color = (env_t::hide_trees  &&  env_t::hide_with_transparency) ? (TRANSPARENT25_FLAG | OUTLINE_FLAG | color_idx_to_rgb(COL_BLACK)) : 0; }

	/**
	 * Calculates tree image dependent on tree age
	 */
	void calc_image() OVERRIDE;

	/**
	 * Called whenever the season or snowline height changes
	 * return false and the obj_t will be deleted
	 */
	bool check_season(const bool) OVERRIDE;

	void rotate90() OVERRIDE;

	/**
	 * re-calculate z-offset if slope of the tile has changed
	 */
	void recalc_off();

	const char *get_name() const OVERRIDE {return "Baum";}
#ifdef INLINE_OBJ_TYPE
#else
	typ get_typ() const OVERRIDE { return baum; }
#endif

	void show_info() OVERRIDE;

	void cleanup(player_t *player) OVERRIDE;

	void * operator new(size_t s);
	void operator delete(void *p);

	const tree_desc_t* get_desc() const { return tree_list[tree_id]; }
	void set_desc( const tree_desc_t *b ) { tree_id = tree_list.index_of(b); }
	uint16 get_desc_id() const { return tree_id; }

	uint32 get_age() const;

	// static functions to handle trees

	// distributes trees on a map
	static void distribute_trees(int density);

	static bool plant_tree_on_coordinate(koord pos, const tree_desc_t *desc, const bool check_climate, const bool random_age );

	static bool register_desc(tree_desc_t *desc);
	static bool successfully_loaded();

	static uint32 create_forest(koord center, koord size );
	static void fill_trees(int density);

	// return list to descriptors
	static vector_tpl<tree_desc_t const*> const& get_all_desc() { return tree_list; }

	static const tree_desc_t *random_tree_for_climate(climate cl) { uint8 b = random_tree_for_climate_intern(cl);  return b!=invalid_tree_id ? tree_list[b] : NULL; }

	static const tree_desc_t *find_tree( const char *tree_name ) { return tree_list.empty() ? NULL : desc_names.get(tree_name); }

	static int get_count() { return tree_list.get_count()-1; }
	static int get_count(climate cl);

};

#endif
