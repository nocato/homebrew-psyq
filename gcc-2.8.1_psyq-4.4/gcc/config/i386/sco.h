/* Definitions for Intel 386 running SCO Unix System V.  */


/* Mostly it's like AT&T Unix System V. */

#include "i386v.h"

/* Use crt1.o as a startup file and crtn.o as a closing file.  */

#undef STARTFILE_SPEC
#define STARTFILE_SPEC  "%{pg:gcrt1.o%s}%{!pg:%{p:mcrt1.o%s}%{!p:crt1.o%s}} crtbegin.o%s"

#define ENDFILE_SPEC "crtend.o%s crtn.o%s"

/* Library spec, including SCO international language support. */

#undef LIB_SPEC
#define LIB_SPEC \
 "%{p:-L/usr/lib/libp}%{pg:-L/usr/lib/libp} %{scointl:libintl.a%s} -lc"

/* Specify predefined symbols in preprocessor.  */

#undef CPP_PREDEFINES
#define CPP_PREDEFINES "-Dunix -Di386 -DM_UNIX -DM_I386 -DM_COFF -DM_WORDSWAP"

#undef CPP_SPEC
#define CPP_SPEC "%{scointl:-DM_INTERNAT}"

/* Use atexit for static destructors, instead of defining
   our own exit function.  */
#define HAVE_ATEXIT

/* Specify the size_t type.  */
#define SIZE_TYPE "unsigned int"

#if 0 /* Not yet certain whether this is needed.  */
/* If no 387, use the general regs to return floating values,
   since this system does not emulate the 80387.  */

#define VALUE_REGNO(MODE) \
  ((TARGET_80387 && ((MODE) == SFmode || (MODE) == DFmode))
   ? FIRST_FLOAT_REG : 0)

#define HARD_REGNO_MODE_OK(REGNO, MODE) \
  ((REGNO) < 2 ? 1							\
   : (REGNO) < 4 ? 1							\
   : (REGNO) >= 8 ? ((GET_MODE_CLASS (MODE) == MODE_FLOAT		\
		      || GET_MODE_CLASS (MODE) == MODE_COMPLEX_FLOAT)	\
		     && TARGET_80387					\
		     && GET_MODE_UNIT_SIZE (MODE) <= 8)			\
   : (MODE) != QImode)
#endif

/* caller has to pop the extra argument passed to functions that return
   structures. */

#undef RETURN_POPS_ARGS
#define RETURN_POPS_ARGS(FUNTYPE,SIZE)   \
  (TREE_CODE (FUNTYPE) == IDENTIFIER_NODE ? 0			\
   : (TARGET_RTD						\
      && (TYPE_ARG_TYPES (FUNTYPE) == 0				\
	  || (TREE_VALUE (tree_last (TYPE_ARG_TYPES (FUNTYPE)))	\
	      == void_type_node))) ? (SIZE)			\
   : 0)
/* On other 386 systems, the last line looks like this:
   : (aggregate_value_p (FUNTYPE)) ? GET_MODE_SIZE (Pmode) : 0)  */
