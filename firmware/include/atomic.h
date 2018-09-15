/** @(#)atomic.h     <18-Aug-2014 13:31:09 bob p>
 *  Last Time-stamp: <19-Jan-2016 09:02:58 bob p>
 *
 *  \file atomic.h
 *  \brief  Port of Dean Camera's ATOMIC_BLOCK macros for AVR to ARM Cortex M3.
 *
 */

/*lint -save -e755 -e756 Disable warning(s), this file only, global macro/typedef 'Symbol' (Location) not referenced */

#ifndef _ATOMIC_H_
#define _ATOMIC_H_ (1)

#ifdef  DEFINE_SPACE_ATOMIC_H
#define EXTERN_ATOMIC
#else
#define EXTERN_ATOMIC extern
#endif

#if defined(__cplusplus) && __cplusplus
 extern "C" {
#endif

/*
* This is port of Dean Camera's ATOMIC_BLOCK macros for AVR to ARM Cortex M3
* v1.0
* Mark Pendrith, Nov 27, 2012.
*
* From Mark:
* >When I ported the macros I emailed Dean to ask what attribution would be
* >appropriate, and here is his response:
* >
* >>Mark,
* >>I think it's great that you've ported the macros; consider them
* >>public domain, to do with whatever you wish. I hope you find them useful .
* >>
* >>Cheers!
* >>- Dean
*/

#ifdef __arm__
#ifndef _CORTEX_M3_ATOMIC_H_
#define _CORTEX_M3_ATOMIC_H_

static __inline__ uint32_t __get_primask(void) \
{ uint32_t primask = 0; \
  __asm__ volatile ("MRS %[result], PRIMASK\n\t":[result]"=r"(primask)::); \
  return primask; } // returns 0 if interrupts enabled, 1 if disabled

static __inline__ void __set_primask(uint32_t setval) \
{ __asm__ volatile ("MSR PRIMASK, %[value]\n\t""dmb\n\t""dsb\n\t""isb\n\t"::[value]"r"(setval):);
  __asm__ volatile ("" ::: "memory");}

static __inline__ uint32_t __iSeiRetVal(void) \
{ __asm__ volatile ("CPSIE i\n\t""dmb\n\t""dsb\n\t""isb\n\t"); \
  __asm__ volatile ("" ::: "memory"); return 1; }

static __inline__ uint32_t __iCliRetVal(void) \
{ __asm__ volatile ("CPSID i\n\t""dmb\n\t""dsb\n\t""isb\n\t"); \
  __asm__ volatile ("" ::: "memory"); return 1; }

static __inline__ void    __iSeiParam(const uint32_t *__s) \
{ __asm__ volatile ("CPSIE i\n\t""dmb\n\t""dsb\n\t""isb\n\t"); \
  __asm__ volatile ("" ::: "memory"); (void)__s; }

static __inline__ void    __iCliParam(const uint32_t *__s) \
{ __asm__ volatile ("CPSID i\n\t""dmb\n\t""dsb\n\t""isb\n\t"); \
  __asm__ volatile ("" ::: "memory"); (void)__s; }

static __inline__ void    __iRestore(const  uint32_t *__s) \
{ __set_primask(*__s); __asm__ volatile ("dmb\n\t""dsb\n\t""isb\n\t"); \
  __asm__ volatile ("" ::: "memory"); }


#define ATOMIC_BLOCK(type) \
for ( type, __ToDo = __iCliRetVal(); __ToDo ; __ToDo = 0 )

#define ATOMIC_RESTORESTATE \
uint32_t primask_save __attribute__((__cleanup__(__iRestore)))  = __get_primask()

#define ATOMIC_FORCEON \
uint32_t primask_save __attribute__((__cleanup__(__iSeiParam))) = 0

#define NONATOMIC_BLOCK(type) \
for ( type, __ToDo = __iSeiRetVal(); __ToDo ;  __ToDo = 0 )

#define NONATOMIC_RESTORESTATE \
uint32_t primask_save __attribute__((__cleanup__(__iRestore))) = __get_primask()

#define NONATOMIC_FORCEOFF \
uint32_t primask_save __attribute__((__cleanup__(__iCliParam))) = 0

#endif
#endif

#if defined(__cplusplus) && __cplusplus
 }
#endif

#endif /* _ATOMIC_H_ */

/*lint -restore */
