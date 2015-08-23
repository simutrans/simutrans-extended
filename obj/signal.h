/*
 * Copyright (c) 1997 - 2002 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#ifndef obj_signal_h
#define obj_signal_h

#include "roadsign.h"

#include "../simobj.h"


/**
 * Signale f�r die Bahnlinien.
 *
 * @see blockstrecke_t
 * @see blockmanager
 * @author Hj. Malthaner
 */
class signal_t : public roadsign_t
{
private:
	koord3d signalbox;

public:
	signal_t(loadsave_t *file);
	signal_t(player_t *player, koord3d pos, ribi_t::ribi dir,const roadsign_besch_t *besch, /*koord3d sb,*/ bool preview = false);

	void save_signalbox_location(loadsave_t *file);

	void rotate90();

	/**
	* @return Einen Beschreibungsstring f�r das Objekt, der z.B. in einem
	* Beobachtungsfenster angezeigt wird.
	* @author Hj. Malthaner
	*/
	virtual void info(cbuffer_t & buf, bool dummy = false) const;

#ifdef INLINE_OBJ_TYPE
#else
	typ get_typ() const { return obj_t::signal; }
#endif
	const char *get_name() const { return besch->get_name(); }

	/**
	* berechnet aktuelles image
	*/
	void calc_image();

	void set_signalbox(koord3d k) { signalbox = k; }
	koord3d get_signalbox() const { return signalbox; }
};

#endif
