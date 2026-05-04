#ifndef _STD_TYPES_H
#define _STD_TYPES_H

/* If <stdint.h> has already been included (or gets included later in the
 * same TU), defer to its definitions to avoid conflicting typedefs.
 * GCC/Clang define __int8_t_defined / __uint32_t_defined when stdint
 * types are available. */
#if defined(__GNUC__) || defined(__clang__)
  #include <stdint.h>
  /* stdint.h already provides uint8_t, uint16_t, uint32_t, uint64_t,
     int8_t, int16_t, int32_t, int64_t — do not redefine them. */
#else
  typedef unsigned char       uint8_t;
  typedef unsigned short      uint16_t;
  typedef unsigned int        uint32_t;
  typedef unsigned long long  uint64_t;
#endif

/* Project-specific signed aliases (sint*_t) — these don't collide with stdint */
typedef signed char           sint8_t;
typedef signed short          sint16_t;
typedef signed int            sint32_t;
typedef signed long long      sint64_t;

typedef float                 float32_t;
typedef double                float64_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

#endif /* _STD_TYPES_H */
