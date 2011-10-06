/* ex: set ts=4 noet: */
/* $Id$ */
/**
 * deal with endiannessosity!
 */

#include "types.h"

#define U16_REVERSEBYTES(ptr)						\
	do {											\
		u16 *p = ptr;								\
		u16 u = *p;									\
		u = ((u & 0xFF00) >> 8) | (u << 8);			\
		*p = u;										\
	} while (0)

#define U32_REVERSEBYTES(ptr)						\
	do {											\
		u32 *p = ptr;								\
		register u32 u = *p;						\
		u = ((u & 0xFF000000UL) >> 24)				\
			| ((u & 0xFF0000UL) >> 8)				\
			| ((u & 0xFF00) << 8)					\
			| (u << 24);							\
		*p = u;										\
	} while (0)

#define U64_REVERSEBYTES(ptr)						\
	do {											\
		u64 *p = ptr;								\
		register u64 u = *p;						\
		u = ((u & 0xFF00000000000000ULL) >> 56)		\
			| ((u & 0xFF000000000000ULL) >> 40)		\
			| ((u & 0xFF0000000000ULL) >> 24)		\
			| ((u & 0xFF00000000ULL) >> 8)			\
			| ((u & 0xFF000000UL) << 8) 			\
			| ((u & 0xFF0000UL) << 24) 				\
			| ((u & 0xFF00) << 40) 					\
			| (u << 56);							\
		*p = u;										\
	} while (0)

/* because we anticipate mostly using little endian machines (Intel and AMD IA-32 and AMD-64)
 * we send all fields as little-endian, so no extra translation is required */
#if __ENDIAN == BIG_ENDIAN
#	define U16_NTH(ptr)		U16_REVERSEBYTES(ptr)
#	define U16_HTN(ptr)		U16_REVERSEBYTES(ptr)
#	define U32_NTH(ptr)		U32_REVERSEBYTES(ptr)
#	define U32_HTN(ptr)		U32_REVERSEBYTES(ptr)
#else /*  */
#	define U16_NTH(ptr)
#	define U16_HTN(ptr)
#	define U32_NTH(ptr)
#	define U32_HTN(ptr)
#endif

