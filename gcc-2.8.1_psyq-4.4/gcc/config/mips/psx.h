/* Definitions of target machine for GNU compiler.  Iris version.
   Copyright (C) 1991, 1993, 1995, 1996 Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */


/* Definitions by GIL for PSX */

#define TARGET_DEFAULT (MASK_GAS+MASK_SOFT_FLOAT+MASK_SPLIT_ADDR+MASK_GPOPT)

#define CPP_PREDEFINES	"-Dmips -DMIPSEL"

#define STARTFILE_SPEC  "%{pg:gcrt1.o%s}%{!pg:%{p:mcrt1.o%s}%{!p:crt1.o%s}}"

#define CPP_SPEC "\
%{!ansi:-D__EXTENSIONS__} -D_MIPSEL \
%{.S:	-D_LANGUAGE_ASSEMBLY %{!ansi:-DLANGUAGE_ASSEMBLY}} \
%{.s:	-D_LANGUAGE_ASSEMBLY %{!ansi:-DLANGUAGE_ASSEMBLY}} \
%{.cc:	-D_LANGUAGE_C_PLUS_PLUS} \
%{.cxx:	-D_LANGUAGE_C_PLUS_PLUS} \
%{.C:	-D_LANGUAGE_C_PLUS_PLUS} \
%{.m:	-D_LANGUAGE_OBJECTIVE_C} \
%{!.S: %{!.s: %{!.cc: %{!.cxx: %{!.C: %{!.m: -D_LANGUAGE_C %{!ansi:-DLANGUAGE_C}}}}}}} \
%{mlong64:-D__SIZE_TYPE__=long\\ unsigned\\ int -D__PTRDIFF_TYPE__=long\\ int} \
%{!mlong64:-D__SIZE_TYPE__=unsigned\\ int -D__PTRDIFF_TYPE__=int} \
%{mips3:-U__mips -D__mips=3}"

#define LIB_SPEC	\
	"%{!p:%{!pg:%{!static:%{!g*:-lc_s}} -lc}}%{p:-lc_p}%{pg:-lc_p} crtn.o%s"

#define MACHINE_TYPE	"Sony Playstation"

#define TARGET_ENDIAN_DEFAULT 0

/* A C statement to output something to the assembler file to switch to section
   NAME for object DECL which is either a FUNCTION_DECL, a VAR_DECL or
   NULL_TREE.  Some target formats do not support arbitrary sections.  Do not
   define this macro in such cases.  */

#define ASM_OUTPUT_SECTION_NAME(F, DECL, NAME, RELOC) \
do {								\
  extern FILE *asm_out_text_file;				\
  if ((DECL) && TREE_CODE (DECL) == FUNCTION_DECL)		\
    fprintf (asm_out_text_file, "\t.section %s,\"ax\",@progbits\n", (NAME)); \
  else if ((DECL) && TREE_READONLY (DECL))			\
    fprintf (F, "\t.section %s,\"a\",@progbits\n", (NAME));	\
  else								\
    fprintf (F, "\t.section %s,\"aw\",@progbits\n", (NAME));	\
} while (0)

/* GIL: R3000 machine supports ISA 1 */
#define MIPS_ISA_DEFAULT 1
/* GIL: set to r3000 */
#define MIPS_CPU_STRING_DEFAULT "R3000"

/* End of GIL for PSX */


#define SGI_TARGET 1		/* inform other mips files this is SGI */

/* Always use 1 for .file number.  I [meissner@osf.org] wonder why
   IRIS needs this.  */

#define SET_FILE_NUMBER() num_source_filenames = 1

/* Put out a label after a .loc.  I [meissner@osf.org] wonder why
   IRIS needs this.  */

#define LABEL_AFTER_LOC(STREAM) fprintf (STREAM, "LM%d:\n", ++sym_lineno)

#define STACK_ARGS_ADJUST(SIZE)                                         \
{                                                                       \
  SIZE.constant += 4;                                                   \
  if (SIZE.constant < 32)						\
    SIZE.constant = 32;                                                 \
}

/* Do not allow `$' in identifiers.  */

#define DOLLARS_IN_IDENTIFIERS 0

/* Tell G++ not to create constructors or destructors with $'s in them.  */

#define NO_DOLLAR_IN_LABEL 1

/* Specify wchar_t type.  */
#define WCHAR_TYPE	"unsigned char"
#define WCHAR_TYPE_SIZE BITS_PER_UNIT

/* Generate calls to memcpy, etc., not bcopy, etc.  */
#define TARGET_MEM_FUNCTIONS

/* Plain char is unsigned in the SGI compiler.  */
#define DEFAULT_SIGNED_CHAR 0

#include "mips/mips.h"

/* Force target file switching for this target.
   Enabling this puts all externs at the beginning of
   the generated .asm files. */
#undef TARGET_FILE_SWITCHING
#define TARGET_FILE_SWITCHING (1)
