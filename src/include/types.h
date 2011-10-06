/* ex: set ts=4 noet: */
/* $Id$ */
/**
 * set up guarenteed-sized types for use (especially) in fixed-width protocols
 */

#ifndef TYPES_H
#define TYPES_H

#define LITTLE_ENDIAN	0
#define BIG_ENDIAN		1
#define __ENDIAN		LITTLE_ENDIAN

#ifndef __ENDIAN
#	error "__ENDIAN MUST BE "
#endif

#define __BITS				32

typedef char				s8;
typedef unsigned char		u8;
typedef short				s16;
typedef unsigned short		u16;
typedef unsigned int		u32;

#if __BITS == 32
typedef long long			s64;
typedef unsigned long long	u64;
#elif __BITS == 64
typedef long 				s64;
typedef unsigned long		u64;
#else
#	error "__BITS unsupported value!"
#endif


#endif

