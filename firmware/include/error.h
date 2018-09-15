/**
 * Simple chained error functionality
 *
 * Kevin Cuzner
 */
#ifndef _ERROR_H_
#define _ERROR_H_

#include <stdint.h>
#include <stddef.h>

/**
 * Error instance so we have somewhere in memory to store it
 */
typedef int32_t ErrorInst;

/**
 * Error type that is passed and chained
 */
typedef int32_t* Error;

#define ERROR_OK (0)
#define ERROR_INST(name) ErrorInst name = ERROR_OK
#define ERROR_FROM_INST(ei) (&(ei))
#define ERROR_IS_FATAL(e) (*e < ERROR_OK)
#define ERROR_IS_WARN(e) (*e > ERROR_OK)
#define ERROR_CLEAR(e) { *e = ERROR_OK }
#define ERROR_SET(e, code) { *e = (code); }

#endif //_ERROR_H_

