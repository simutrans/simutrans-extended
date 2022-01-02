/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef BODEN_WEGE_SCHIENE_H
#define BODEN_WEGE_SCHIENE_H


#include "weg.h"
#include "../../convoihandle_t.h"

class vehicle_t;

/**
 * Class for Rails in Simutrans.
 * Trains can run over rails.
 * Every rail belongs to a section block
 */
class schiene_t : public weg_t
{
public:
	enum reservation_type : uint8 	{ block, directional, priority, stale_block, end };

protected:
	/**
	* Bound when this block was successfully reserved by the convoi
	*/
	convoihandle_t reserved;

	// The type of reservation
	reservation_type type;

	// Additional data for reservations, such as the priority level or direction.
	ribi_t::ribi direction;

	schiene_t(waytype_t waytype);

	mutable uint8 textlines_in_info_window;

	bool is_type_rail_type(waytype_t wt) { return wt == track_wt || wt == monorail_wt || wt == maglev_wt || wt == tram_wt || wt == narrowgauge_wt; }

public:
	static const way_desc_t *default_schiene;

	static bool show_reservations;

	/**
	* File loading constructor.
	*/
	schiene_t(loadsave_t *file);

	schiene_t();

	/**
	* true, if this rail can be reserved
	*/
	bool can_reserve(convoihandle_t c, ribi_t::ribi dir, reservation_type rt = block, bool check_directions_at_junctions = false) const
	{
		switch (rt) {
			case block:
				return !reserved.is_bound()
				|| c == reserved
				|| (type == directional && (dir == direction || dir == ribi_t::all || ((is_diagonal() || ribi_t::is_bend(direction)) && (dir & direction)) || (is_junction() && (dir & direction))))
				|| (type == priority && true /*Insert real logic here*/);

			case directional:
				return !reserved.is_bound()
				|| c == reserved
				|| type == priority
				|| (dir == direction || dir == ribi_t::all)
				|| (!check_directions_at_junctions && is_junction());

			case priority:
				return !reserved.is_bound()
				|| c == reserved; // TODO: Obtain the priority data from the convoy here and comapre it.

			default:
				// Fail with non-standard reservation type
				return false;
		}
	}

	/**
	* true, if this rail can be reserved
	*/
	bool is_reserved(reservation_type t = block) const { return reserved.is_bound() && (t == type || (t == block && type == stale_block) || type >= end); }
	bool is_reserved_directional(reservation_type t = directional) const { return reserved.is_bound() && t == type; }
	bool is_reserved_priority(reservation_type t = priority) const { return reserved.is_bound() && t == type; }

	void set_stale() { if (type == block) { type = stale_block; } }
	bool is_stale() { return type == stale_block; }

	reservation_type get_reservation_type() const { return type != stale_block ? type : block; }

	ribi_t::ribi get_reserved_direction() const { return direction; }

	/**
	* true, then this rail was reserved
	*/
	bool reserve(convoihandle_t c, ribi_t::ribi dir, reservation_type t = block, bool check_directions_at_junctions = false);

	/**
	* releases previous reservation
	*/
	bool unreserve(convoihandle_t c);

	/**
	* releases previous reservation
	*/
	bool unreserve(vehicle_t *);

	/* called before deletion;
	 * last chance to unreserve tiles ...
	 */
	virtual void cleanup(player_t *player) OVERRIDE;

	/**
	* gets the related convoi
	*/
	convoihandle_t get_reserved_convoi() const {return reserved;}

	void rdwr(loadsave_t *file) OVERRIDE;

	void rotate90() OVERRIDE;

	/**
	 * if a function return here a value with TRANSPARENT_FLAGS set
	 * then a transparent outline with the color form the lower 8 Bit is drawn
	 */
	virtual FLAGGED_PIXVAL get_outline_colour() const OVERRIDE;

	/*
	 * to show reservations if needed
	 */
	virtual image_id get_outline_image() const OVERRIDE { return weg_t::get_image(); }

	static const char* get_reservation_type_name(reservation_type rt)
	{
		switch (rt)
		{
		case block:
		default:
			return "block_reservation";
		case directional:
			return "directional_reservation";
		case priority:
			return "priority_reservation";
		};
	}
};


template<> inline schiene_t* obj_cast<schiene_t>(obj_t* const d)
{
	return dynamic_cast<schiene_t*>(d);
}

#endif
