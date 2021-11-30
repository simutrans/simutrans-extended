/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef SIMTYPES_H
#define SIMTYPES_H


#include <climits>
#include <stdlib.h>

#include "utils/for.h"
#include <limits.h>

#ifndef PATH_MAX
#ifdef MAX_PATH
#define PATH_MAX (MAX_PATH)
#else
#define PATH_MAX 1024
#endif
#endif

#if defined _MSC_VER
#	if _MSC_VER <= 1200
#		error "Simutrans cannot be compiled with Visual C++ 6.0 or earlier."
#	endif
#
#	include <malloc.h>
#	define ALLOCA(type, name, count) type* name = static_cast<type*>(alloca(sizeof(type) * (count)))
#elif defined __clang__
#	include <stdlib.h>
#	define ALLOCA(type, name, count) type* name = static_cast<type*>(alloca(sizeof(type) * (count)))
#else
#	define ALLOCA(type, name, count) type name[count]
#endif

#if defined DEBUG
#	define NOT_REACHED abort();
#else
#	define NOT_REACHED
#endif

#define DELETED = delete
#define OVERRIDE override

#include <stdint.h> // intptr_t (standard)
#include <stddef.h> // ptrdiff_t, intptr_t (Microsoft)

#define ENUM_BITSET(T) \
	static inline T operator ~  (T  a)      { return     (T)~(unsigned)a;                } \
	static inline T operator &  (T  a, T b) { return     (T)((unsigned)a & (unsigned)b); } \
	static inline T operator &= (T& a, T b) { return a = (T)((unsigned)a & (unsigned)b); } \
	static inline T operator |  (T  a, T b) { return     (T)((unsigned)a | (unsigned)b); } \
	static inline T operator |= (T& a, T b) { return a = (T)((unsigned)a | (unsigned)b); }

/* divers enums:
 * better defined here than scattered in thousand files ...
 */
enum climate
{
	water_climate = 0,
	desert_climate,
	tropic_climate,
	mediterran_climate,
	temperate_climate,
	tundra_climate,
	rocky_climate,
	arctic_climate,
	MAX_CLIMATES
};

enum climate_bits
{
	water_climate_bit      = 1 << water_climate,
	desert_climate_bit     = 1 << desert_climate,
	tropic_climate_bit     = 1 << tropic_climate,
	mediterran_climate_bit = 1 << mediterran_climate,
	temperate_climate_bit  = 1 << temperate_climate,
	tundra_climate_bit     = 1 << tundra_climate,
	rocky_climatebit       = 1 << rocky_climate,
	arctic_climate_bit     = 1 << arctic_climate,
	ALL_CLIMATES           = (1 << MAX_CLIMATES) - 1,
	all_but_water_climate  = ALL_CLIMATES & ~water_climate_bit,
	all_but_arctic_climate = ALL_CLIMATES & ~arctic_climate_bit
};

enum sound_type_t {
	TOOL_SOUND     = 0,
	TRAFFIC_SOUND  = 1,
	AMBIENT_SOUND  = 2,
	FACTORY_SOUND  = 3,
	CROSSING_SOUND = 4,
	CASH_SOUND     = 5,
	MAX_SOUND_TYPES,

	ALL_SOUND = 0xFF
};

/**
 * Vordefinierte Wegtypen.
 */
enum waytype_t {
	invalid_wt       =  -1,
	ignore_wt        =   0,
	road_wt          =   1,
	track_wt         =   2,
	water_wt         =   3,
	overheadlines_wt =   4,
	monorail_wt      =   5,
	maglev_wt        =   6,
	tram_wt          =   7,
	narrowgauge_wt   =   8,
	air_wt           =  16,
	noise_barrier_wt =  17,
	powerline_wt     = 128,
	any_wt           = 255
};

/**
 * System types for ways
 */
enum systemtype_t {
	type_flat     = 0,   ///< flat track
	type_elevated = 1,   ///< flag for elevated ways
	type_runway   = 1,   ///< flag for runway (only aircrafts)
	type_tram     = 7,   ///< tram track (waytype = track_wt)
	type_river    = 255, ///< flag for river
	type_all      = 255  ///< special ?
};

/**
 * conditions for overtaking on roads
 */
 enum overtaking_mode_t {
	 halt_mode         = -1, // vehicles can stop on passing lane
	 oneway_mode       = 0,  // condition for one-way road
	 twoway_mode       = 1,  // condition for two-way road
	 //loading_only_mode = 2,  // overtake a loading convoy only
	 prohibited_mode   = 3,  // overtaking is completely forbidden
	 invalid_mode      = 63
 };

enum working_method_t { drive_by_sight, time_interval, absolute_block, token_block, track_circuit_block, cab_signalling, moving_block, one_train_staff, time_interval_with_telegraph };

// define machine independent types
typedef unsigned int        uint;
typedef   signed char       sint8;
typedef unsigned char       uint8;
typedef   signed short      sint16;
typedef unsigned short      uint16;
#ifndef __BEOS__
typedef   signed int        sint32;
#define SINT32_MAX_VALUE	INT_MAX
#ifndef NO_UINT32_TYPES
typedef unsigned int        uint32;
#define UINT32_MAX_VALUE	UINT_MAX
#define SINT64_MAX_VALUE	LLONG_MAX
#endif
#else
// BeOS: int!=long (even though both 32 bit)
typedef   signed long       sint32;
#define SINT32_MAX_VALUE	LONG_MAX
#ifndef NO_UINT32_TYPES
typedef unsigned long       uint32;
#define UINT32_MAX_VALUE	ULONG_MAX
#define SINT64_MAX_VALUE	LLONG_MAX
#endif
#endif
typedef   signed long long  sint64;
#ifndef NO_UINT64_TYPES
typedef unsigned long long  uint64;
#endif
#ifdef _MSC_VER
#	define GCC_PACKED
#	define GCC_ALIGN(a)
#	define MSVC_ALIGN(a) __declspec(align(a))
#	define NORETURN __declspec(noreturn)
#	pragma warning(disable: 4200 4311 4800 4996)
#else
#	define GCC_PACKED    __attribute__ ((__packed__))
#	define GCC_ALIGN(a)  __attribute__ ((aligned (a)))
#	define MSVC_ALIGN(a)
#	define NORETURN      __attribute__ ((noreturn))
#endif
#define UINT64_MAX_VALUE	ULLONG_MAX
#ifndef  MULTI_THREAD
#if defined _MSC_VER
#include <xkeycheck.h>
#define thread_local
#endif
#endif // ! MULTI_THREAD

static inline uint32 hammingWeight(uint8 x){
#ifdef USE_GCC_POPCOUNT
	return (__builtin_popcount(x));
#else
	x-=((x>>1) & 0x55);
	x=(x & 0x33) + ((x>>2) & 0x33);
	x+=(x>>4);
	return x & 0xF;
#endif
}

static inline uint32 hammingWeight(uint16 x){
#ifdef USE_GCC_POPCOUNT
	return (__builtin_popcount(x));
#else
	x-=((x>>1) & 0x5555);
	x=(x & 0x3333) + ((x>>2) & 0x3333);
	x=(x + (x>>4)) & 0x0F0F;
	x+=(x>>8);
	return x & 0x1F;
#endif
}

static inline uint32 hammingWeight(uint32 x){
#ifdef USE_GCC_POPCOUNT
	return (__builtin_popcount(x));
#else
	x-=((x>>1) & 0x55555555);
	x=(x & 0x33333333) + ((x>>2) & 0x33333333);
	x=(x + (x>>4)) & 0x0F0F0F0F;
	x+=(x>>8);
	x+=(x>>16);
	return x & 0x3F;
#endif
}

static inline uint32 hammingWeight(uint64 x){
#ifdef USE_GCC_POPCOUNT
	return (__builtin_popcountl(x));
#else
	x-=((x>>1) & 0x5555555555555555);
	x=(x & 0x3333333333333333) + ((x>>2) & 0x3333333333333333);
	x=(x + (x>>4)) & 0x0F0F0F0F0F0F0F0F;
	x+=(x>>8);
	x+=(x>>16);
	x+=(x>>32);
	return x & 0x7F;
#endif
}


template<typename T> static inline int sgn(T x)
{
		if (x < 0) return -1;
		if (x > 0) return  1;
		return 0;
}

#ifndef min
static inline int min(const int a, const int b)
{
	return a < b ? a : b;
}
#endif

#ifndef max
static inline int max(const int a, const int b)
{
	return a > b ? a : b;
}
#endif

template<typename T> static inline void set_bits_to(T& bits, const T mask, const T values){
	bits = (bits & ~mask) | values;
}

// @author: jamespetts, April 2011
template<class T> static T set_scale_generic(T value, uint16 scale_factor) { return (value * (T)scale_factor) / (T)1000; }

template<class T> class average_tpl
{
public:
	T total;
	T count;

	average_tpl()
	{
		reset();
	}

	inline void add(T value)
	{
		total += value;
		count ++;
	}

	inline void add_check_overflow_16(T value)
	{
		sint64 new_total = (sint64)total + (sint64)value;
		count++;
		while(new_total > 65535ll)
		{
			new_total /= 2;
			count /= 2;
		}
		total = (uint16)new_total;
	}

	inline void add_autoreduce(T value, T reduce_at)
	{
		if((reduce_at % 2) != 0)
		{
			// This *must* be an even number, or else the
			// average will drift too high as "count"
			// is truncated at each reduction.
			reduce_at++;
		}

		sint64 new_total = (sint64)total + (sint64)value;
		count++;

		if(count >= reduce_at)
		{
			new_total /= 2;
			count /= 2;
		}

		while(new_total > 65535ll)
		{
			new_total /= 2;
			count /= 2;
		}
		total = (uint16)new_total;
	}

	inline T get_average() const
	{
		if(count == 0)
		{
			return 0;
		}
		if(count == 1)
		{
			return total;
		}
		return total / count;
	}

	inline void reset()
	{
		total = 0;
		count = 0;
	}

	/**
	* This compresses all values to give new numbers added
	* after this method is called greater weight in the
	* calculations than existing values, without erasing the
	* current average. Returns the current average.
	*/
	inline T reduce()
	{
		total = get_average();
		count = 1;
		return total;
	}
};

struct id_pair
{
	uint16 x;
	uint16 y;

	id_pair(uint16 a, uint16 b)
	{
		x = a;
		y = b;
	}

	id_pair()
	{
		x = 0;
		y = 0;
	}
};

static inline bool operator == (const id_pair &a, const id_pair &b)
{
	// only this works with O3 optimisation!
	return ((a.x-b.x)|(a.y-b.y))==0;
}

static inline bool operator != (const id_pair &a, const id_pair &b)
{
	// only this works with O3 optimisation!
	return ((a.x-b.x)|(a.y-b.y))!=0;
}

static inline bool operator == (const id_pair& a, int b)
{
	// For hashtable use.
	return b == 0 && a == id_pair(0, 0);
}

// endian conversion routines

static inline uint16 endian(uint16 v)
{
#ifdef SIM_BIG_ENDIAN
	v = (v << 8) | (v >> 8); // 0x0011
#endif
	return v;
}

static inline uint32 endian(uint32 v)
{
#ifdef SIM_BIG_ENDIAN
	v =   (v << 16)                |   (v >> 16);                // 0x22330011
	v = ( (v <<  8) & 0xFF00FF00 ) | ( (v >>  8) & 0x00FF00FF ); // 0x33221100
#endif
	return v;
}

static inline uint64 endian(uint64 v)
{
#ifdef SIM_BIG_ENDIAN
	v =   (v << 32)                           |   (v >> 32);                           // 0x4455667700112233
	v = ( (v << 16) & 0xFFFF0000FFFF0000ULL ) | ( (v >> 16) & 0x0000FFFF0000FFFFULL ); // 0x6677445522330011
	v = ( (v <<  8) & 0xFF00FF00FF00FF00ULL ) | ( (v >>  8) & 0x00FF00FF00FF00FFULL ); // 0x7766554433221100
#endif
	return v;
}

static inline sint16 endian(sint16 const v) { return sint16(endian(uint16(v))); }
static inline sint32 endian(sint32 const v) { return sint32(endian(uint32(v))); }
static inline sint64 endian(sint64 const v) { return sint64(endian(uint64(v))); }


/**
 * Sometimes we need to pass pointers as well as integers through a
 * standardized interface (i.e. via a function pointer). This union is used as
 * a helper type to avoid cast operations.  This isn't very clean, but if used
 * with care it seems better than using "long" and casting to a pointer type.
 * In all cases it ensures that no bits are lost.
 */
union value_t
{
	value_t()                : p(0)   {}
	value_t(long itg)        : i(itg) {}
	value_t(const void* ptr) : p(ptr) {}

	const void* p;
	long i;
};

#define IGNORE_ZERO_WEIGHT

#endif
