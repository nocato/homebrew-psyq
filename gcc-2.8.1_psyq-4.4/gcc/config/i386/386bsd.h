/* Configuration for an i386 running 386BSD as the target machine.  */

/* This is tested by i386gas.h.  */
#define YES_UNDERSCORES

#include "i386/gstabs.h"

#undef CPP_PREDEFINES
#define CPP_PREDEFINES "-Dunix -Di386 -D____386BSD____ -D__386BSD__ -DBSD_NET2"

/* Like the default, except no -lg.  */
#define LIB_SPEC "%{!p:%{!pg:-lc}}%{p:-lc_p}%{pg:-lc_p}"

#undef SIZE_TYPE
#define SIZE_TYPE "unsigned int"

#undef PTRDIFF_TYPE
#define PTRDIFF_TYPE "int"

#undef WCHAR_TYPE
#define WCHAR_TYPE "short unsigned int"

#define WCHAR_UNSIGNED 1

#undef WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 16

/* 386BSD does have atexit.  */

#define HAVE_ATEXIT

/* Redefine this to use %eax instead of %edx.  */
#undef FUNCTION_PROFILER
#define FUNCTION_PROFILER(FILE, LABELNO)  \
{									\
  if (flag_pic)								\
    {									\
      fprintf (FILE, "\tleal %sP%d@GOTOFF(%%ebx),%%eax\n",		\
	       LPREFIX, (LABELNO));					\
      fprintf (FILE, "\tcall *mcount@GOT(%%ebx)\n");			\
    }									\
  else									\
    {									\
      fprintf (FILE, "\tmovl $%sP%d,%%eax\n", LPREFIX, (LABELNO));	\
      fprintf (FILE, "\tcall mcount\n");				\
    }									\
}

/* There are conflicting reports about whether this system uses
   a different assembler syntax.  wilson@cygnus.com says # is right.  */
#undef COMMENT_BEGIN
#define COMMENT_BEGIN "#"

#undef ASM_APP_ON
#define ASM_APP_ON "#APP\n"

#undef ASM_APP_OFF
#define ASM_APP_OFF "#NO_APP\n"

/* Defines to be able to build libgcc.a with GCC.
   These are the same as in i386mach.h.  */

/* It might seem that these are not important, since gcc 2 will never
   call libgcc for these functions.  But programs might be linked with
   code compiled by gcc 1, and then these will be used.  */

/* The arg names used to be a and b, but `a' appears inside strings
   and that confuses non-ANSI cpp.  */

#define perform_udivsi3(arg0,arg1)					\
{									\
  register int dx asm("dx");						\
  register int ax asm("ax");						\
									\
  dx = 0;								\
  ax = arg0;								\
  asm ("divl %3" : "=a" (ax), "=d" (dx) : "a" (ax), "g" (arg1), "d" (dx)); \
  return ax;								\
}

#define perform_divsi3(arg0,arg1)					\
{									\
  register int dx asm("dx");						\
  register int ax asm("ax");						\
									\
  ax = arg0;								\
  asm ("cltd\n\tidivl %3" : "=a" (ax), "=d" (dx) : "a" (ax), "g" (arg1)); \
  return ax;								\
}

#define perform_umodsi3(arg0,arg1)					\
{									\
  register int dx asm("dx");						\
  register int ax asm("ax");						\
									\
  dx = 0;								\
  ax = arg0;								\
  asm ("divl %3" : "=a" (ax), "=d" (dx) : "a" (ax), "g" (arg1), "d" (dx)); \
  return dx;								\
}

#define perform_modsi3(arg0,arg1)					\
{									\
  register int dx asm("dx");						\
  register int ax asm("ax");						\
									\
  ax = arg0;								\
  asm ("cltd\n\tidivl %3" : "=a" (ax), "=d" (dx) : "a" (ax), "g" (arg1)); \
  return dx;								\
}


#define perform_fixdfsi(arg0)						\
{									\
  auto unsigned short ostatus;						\
  auto unsigned short nstatus;						\
  auto int ret;								\
  auto double tmp;							\
									\
  &ostatus;			/* guarantee these land in memory */	\
  &nstatus;								\
  &ret;									\
  &tmp;									\
									\
  asm volatile ("fnstcw %0" : "=m" (ostatus));				\
  nstatus = ostatus | 0x0c00;						\
  asm volatile ("fldcw %0" : /* no outputs */ : "m" (nstatus));		\
  tmp = arg0;								\
  asm volatile ("fldl %0" : /* no outputs */ : "m" (tmp));		\
  asm volatile ("fistpl %0" : "=m" (ret));				\
  asm volatile ("fldcw %0" : /* no outputs */ : "m" (ostatus));		\
									\
  return ret;								\
}

/* The following macros are stolen from i386v4.h */
/* These have to be defined to get PIC code correct */

/* This is how to output an element of a case-vector that is relative.
   This is only used for PIC code.  See comments by the `casesi' insn in
   i386.md for an explanation of the expression this outputs. */

#undef ASM_OUTPUT_ADDR_DIFF_ELT
#define ASM_OUTPUT_ADDR_DIFF_ELT(FILE, VALUE, REL) \
  fprintf (FILE, "\t.long _GLOBAL_OFFSET_TABLE_+[.-%s%d]\n", LPREFIX, VALUE)

/* Indicate that jump tables go in the text section.  This is
   necessary when compiling PIC code.  */

#define JUMP_TABLES_IN_TEXT_SECTION
