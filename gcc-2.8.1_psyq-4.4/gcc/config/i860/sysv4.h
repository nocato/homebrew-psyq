/* Target definitions for GNU compiler for Intel 80860 running System V.4
   Copyright (C) 1991 Free Software Foundation, Inc.

   Written by Ron Guilmette (rfg@ncd.com).

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
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "i860.h"
#include "svr4.h"

#undef TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (i860 System V Release 4)");

/* Provide a set of pre-definitions and pre-assertions appropriate for
   the i860 running svr4.  Note that the symbol `__SVR4__' MUST BE
   DEFINED!  It is needed so that the va_list struct in va-i860.h
   will get correctly defined for the svr4 (ABI compliant) case rather
   than for the previous (svr3, svr2, ...) case.  It also needs to be
   defined so that the correct (svr4) version of __builtin_saveregs
   will be selected when we are building gnulib2.c.
   __svr4__ is our extension.  */

#define CPP_PREDEFINES \
  "-Di860 -Dunix -DSVR4 -D__svr4__ -Asystem(unix) -Acpu(i860) -Amachine(i860)"

/* The prefix to be used in assembler output for all names of registers.
   This string gets prepended to all i860 register names (svr4 only).  */

#define I860_REG_PREFIX	"%"

#define ASM_COMMENT_START "#"

#undef TYPE_OPERAND_FMT
#define TYPE_OPERAND_FMT      "\"%s\""

#define DBX_REGISTER_NUMBER(REGNO) (REGNO)

/* The following macro definition overrides the one in i860.h
   because the svr4 i860 assembler requires a different syntax
   for getting parts of constant/relocatable values.  */

#undef PRINT_OPERAND_PART
#define PRINT_OPERAND_PART(FILE, X, PART_CODE)				\
  do { fprintf (FILE, "[");						\
	output_address (X);						\
	fprintf (FILE, "]@%s", PART_CODE);				\
  } while (0)

#undef ASM_FILE_START
#define ASM_FILE_START(FILE)						\
  do {	output_file_directive (FILE, main_input_filename);		\
	fprintf (FILE, "\t.version\t\"01.01\"\n");			\
  } while (0)

/* Output the special word the svr4 SDB wants to see just before
   the first word of each function's prologue code.  */

extern char *current_function_original_name;

/* This special macro is used to output a magic word just before the
   first word of each function.  On some versions of UNIX running on
   the i860, this word can be any word that looks like a NOP, however
   under svr4, this neds to be an `shr r0,r0,r0' instruction in which
   the normally unused low-order bits contain the length of the function
   prologue code (in bytes).  This is needed to make the svr4 SDB debugger
   happy.  */

#undef ASM_OUTPUT_FUNCTION_PREFIX
#define ASM_OUTPUT_FUNCTION_PREFIX(FILE, FNNAME)			\
  do {	ASM_OUTPUT_ALIGN (FILE, 2);					\
  	fprintf ((FILE), "\t.long\t.ep.");				\
	assemble_name (FILE, FNNAME);					\
	fprintf (FILE, "-");						\
	assemble_name (FILE, FNNAME);					\
	fprintf (FILE, "+0xc8000000\n");				\
	current_function_original_name = (FNNAME);			\
  } while (0)

/* Output the special label that must go just after each function's
   prologue code to support svr4 SDB.  */

#define ASM_OUTPUT_PROLOGUE_SUFFIX(FILE)				\
  do {	fprintf (FILE, ".ep.");						\
	assemble_name (FILE, current_function_original_name);		\
	fprintf (FILE, ":\n");						\
  } while (0)

#undef CTORS_SECTION_ASM_OP
#define CTORS_SECTION_ASM_OP	".section\t.ctors,\"a\",\"progbits\""
#undef DTORS_SECTION_ASM_OP
#define DTORS_SECTION_ASM_OP	".section\t.dtors,\"a\",\"progbits\""

/* Add definitions to support the .tdesc section as specified in the svr4
   ABI for the i860.  */

#define TDESC_SECTION_ASM_OP    ".section\t.tdesc"

#undef EXTRA_SECTIONS
#define EXTRA_SECTIONS in_const, in_ctors, in_dtors, in_tdesc

#undef EXTRA_SECTION_FUNCTIONS
#define EXTRA_SECTION_FUNCTIONS						\
  CONST_SECTION_FUNCTION						\
  CTORS_SECTION_FUNCTION						\
  DTORS_SECTION_FUNCTION						\
  TDESC_SECTION_FUNCTION

#define TDESC_SECTION_FUNCTION						\
void									\
tdesc_section ()							\
{									\
  if (in_section != in_tdesc)						\
    {									\
      fprintf (asm_out_file, "%s\n", TDESC_SECTION_ASM_OP);		\
      in_section = in_tdesc;						\
    }									\
}
