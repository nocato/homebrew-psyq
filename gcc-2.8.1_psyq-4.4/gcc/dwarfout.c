/* This file contains code written by Ron Guilmette (rfg@ncd.com) for
   Network Computing Devices, August, September, October, November 1990.

   Output Dwarf format symbol table information from the GNU C compiler.
   Copyright (C) 1992 Free Software Foundation, Inc.

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

#include "config.h"

#ifdef DWARF_DEBUGGING_INFO
#include <stdio.h>
#include "dwarf.h"
#include "tree.h"
#include "flags.h"
#include "rtl.h"
#include "insn-config.h"
#include "reload.h"
#include "output.h"

/* #define NDEBUG 1 */
#include <assert.h>

#if defined(DWARF_TIMESTAMPS)
#if defined(POSIX)
#include <time.h>
#else /* !defined(POSIX) */
#include <sys/types.h>
#if defined(__STDC__)
extern time_t time (time_t *);
#else /* !defined(__STDC__) */
extern time_t time ();
#endif /* !defined(__STDC__) */
#endif /* !defined(POSIX) */
#endif /* defined(DWARF_TIMESTAMPS) */

#if defined(USG) || defined(POSIX)
#include <string.h>
#else
#include <strings.h>
#define strrchr rindex
#endif

char *getpwd ();


/* IMPORTANT NOTE: Please see the file README.DWARF for important details
   regarding the GNU implementation of Dwarf.  */

/* NOTE: In the comments in this file, many references are made to
   so called "Debugging Information Entries".  For the sake of brevity,
   this term is abbreviated to `DIE' throughout the remainder of this
   file.  */

/* Note that the implementation of C++ support herein is (as yet) unfinished.
   If you want to try to complete it, more power to you.  */

#if defined(__GNUC__) && (NDEBUG == 1)
#define inline static inline
#else
#define inline static
#endif

/* How to start an assembler comment.  */
#ifndef ASM_COMMENT_START
#define ASM_COMMENT_START ";#"
#endif

/* Define a macro which, when given a pointer to some BLOCK node, returns
   a pointer to the FUNCTION_DECL node from which the given BLOCK node
   was instantiated (as an inline expansion).  This macro needs to be
   defined properly in tree.h, however for the moment, we just fake it.  */

#define BLOCK_INLINE_FUNCTION(block) 0

/* Define a macro which returns non-zero for any tagged type which is
   used (directly or indirectly) in the specification of either some
   function's return type or some formal parameter of some function.
   We use this macro when we are operating in "terse" mode to help us
   know what tagged types have to be represented in Dwarf (even in
   terse mode) and which ones don't.

   A flag bit with this meaning really should be a part of the normal
   GCC ..._TYPE nodes, but at the moment, there is no such bit defined
   for these nodes.  For now, we have to just fake it.  It it safe for
   us to simply return zero for all complete tagged types (which will
   get forced out anyway if they were used in the specification of some
   formal or return type) and non-zero for all incomplete tagged types.
*/

#define TYPE_USED_FOR_FUNCTION(tagged_type) (TYPE_SIZE (tagged_type) == 0)

#define BITFIELD_OFFSET_BITS(DECL) \
  ((unsigned) TREE_INT_CST_LOW (DECL_FIELD_BITPOS (DECL)))
#define BITFIELD_OFFSET_UNITS(DECL) \
  (BITFIELD_OFFSET_BITS(DECL) / (unsigned) BITS_PER_UNIT)
#define BITFIELD_OFFSET_WORDS_IN_UNITS(DECL) \
  ((BITFIELD_OFFSET_BITS(DECL) / (unsigned) BITS_PER_WORD) * UNITS_PER_WORD)

extern int flag_traditional;
extern char *version_string;
extern char *language_string;

/* Maximum size (in bytes) of an artificially generated label.	*/

#define MAX_ARTIFICIAL_LABEL_BYTES	30

/* Make sure we know the sizes of the various types dwarf can describe.
   These are only defaults.  If the sizes are different for your target,
   you should override these values by defining the appropriate symbols
   in your tm.h file.  */

#ifndef CHAR_TYPE_SIZE
#define CHAR_TYPE_SIZE BITS_PER_UNIT
#endif

#ifndef SHORT_TYPE_SIZE
#define SHORT_TYPE_SIZE (BITS_PER_UNIT * 2)
#endif

#ifndef INT_TYPE_SIZE
#define INT_TYPE_SIZE BITS_PER_WORD
#endif

#ifndef LONG_TYPE_SIZE
#define LONG_TYPE_SIZE BITS_PER_WORD
#endif

#ifndef LONG_LONG_TYPE_SIZE
#define LONG_LONG_TYPE_SIZE (BITS_PER_WORD * 2)
#endif

#ifndef WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE INT_TYPE_SIZE
#endif

#ifndef WCHAR_UNSIGNED
#define WCHAR_UNSIGNED 0
#endif

#ifndef FLOAT_TYPE_SIZE
#define FLOAT_TYPE_SIZE BITS_PER_WORD
#endif

#ifndef DOUBLE_TYPE_SIZE
#define DOUBLE_TYPE_SIZE (BITS_PER_WORD * 2)
#endif

#ifndef LONG_DOUBLE_TYPE_SIZE
#define LONG_DOUBLE_TYPE_SIZE (BITS_PER_WORD * 2)
#endif

/* Structure to keep track of source filenames.  */

struct filename_entry {
  unsigned	number;
  char *	name;
};

typedef struct filename_entry filename_entry;

/* Pointer to an array of elements, each one having the structure above. */

static filename_entry *filename_table;

/* Total number of entries in the table (i.e. array) pointed to by
   `filename_table'.  This is the *total* and includes both used and
   unused slots.  */

static unsigned ft_entries_allocated;

/* Number of entries in the filename_table which are actually in use.  */

static unsigned ft_entries;

/* Size (in elements) of increments by which we may expand the filename
   table.  Actually, a single hunk of space of this size should be enough
   for most typical programs.	 */

#define FT_ENTRIES_INCREMENT 64

/* Local pointer to the name of the main input file.  Initialized in
   dwarfout_init.  */

static char *primary_filename;

/* Pointer to the most recent filename for which we produced some line info.  */

static char *last_filename;

/* For Dwarf output, we must assign lexical-blocks id numbers
   in the order in which their beginnings are encountered.
   We output Dwarf debugging info that refers to the beginnings
   and ends of the ranges of code for each lexical block with
   assembler labels ..Bn and ..Bn.e, where n is the block number.
   The labels themselves are generated in final.c, which assigns
   numbers to the blocks in the same way.  */

static unsigned next_block_number = 2;

/* Counter to generate unique names for DIEs. */

static unsigned next_unused_dienum = 1;

/* Number of the DIE which is currently being generated.  */

static unsigned current_dienum;

/* Number to use for the special "pubname" label on the next DIE which
   represents a function or data object defined in this compilation
   unit which has "extern" linkage.  */

static next_pubname_number = 0;

#define NEXT_DIE_NUM pending_sibling_stack[pending_siblings-1]

/* Pointer to a dynamically allocated list of pre-reserved and still
   pending sibling DIE numbers.	 Note that this list will grow as needed.  */

static unsigned *pending_sibling_stack;

/* Counter to keep track of the number of pre-reserved and still pending
   sibling DIE numbers.	 */

static unsigned pending_siblings;

/* The currently allocated size of the above list (expressed in number of
   list elements).  */

static unsigned pending_siblings_allocated;

/* Size (in elements) of increments by which we may expand the pending
   sibling stack.  Actually, a single hunk of space of this size should
   be enough for most typical programs.	 */

#define PENDING_SIBLINGS_INCREMENT 64

/* Non-zero if we are performing our file-scope finalization pass and if
   we should force out Dwarf decsriptions of any and all file-scope
   tagged types which are still incomplete types.  */

static int finalizing = 0;

/* A pointer to the base of a list of pending types which we haven't
   generated DIEs for yet, but which we will have to come back to
   later on.  */

static tree *pending_types_list;

/* Number of elements currently allocated for the pending_types_list.  */

static unsigned pending_types_allocated;

/* Number of elements of pending_types_list currently in use.  */

static unsigned pending_types;

/* Size (in elements) of increments by which we may expand the pending
   types list.  Actually, a single hunk of space of this size should
   be enough for most typical programs.	 */

#define PENDING_TYPES_INCREMENT 64

/* Pointer to an artifical RECORD_TYPE which we create in dwarfout_init.
   This is used in a hack to help us get the DIEs describing types of
   formal parameters to come *after* all of the DIEs describing the formal
   parameters themselves.  That's necessary in order to be compatible
   with what the brain-dammaged svr4 SDB debugger requires.  */

static tree fake_containing_scope;

/* The number of the current function definition that we are generating
   debugging information for.  These numbers range from 1 up to the maximum
   number of function definitions contained within the current compilation
   unit.  These numbers are used to create unique labels for various things
   contained within various function definitions.  */

static unsigned current_funcdef_number = 1;

/* Forward declarations for functions defined in this file.  */

static void output_type ();
static void type_attribute ();
static void output_decls_for_scope ();
static void output_decl ();
static unsigned lookup_filename ();

/* Definitions of defaults for assembler-dependent names of various
   pseudo-ops and section names.

   Theses may be overridden in your tm.h file (if necessary) for your
   particular assembler.  The default values provided here correspond to
   what is expected by "standard" AT&T System V.4 assemblers.  */

#ifndef FILE_ASM_OP
#define FILE_ASM_OP		".file"
#endif
#ifndef VERSION_ASM_OP
#define VERSION_ASM_OP		".version"
#endif
#ifndef SECTION_ASM_OP
#define SECTION_ASM_OP		".section"
#endif
#ifndef UNALIGNED_SHORT_ASM_OP
#define UNALIGNED_SHORT_ASM_OP	".2byte"
#endif
#ifndef UNALIGNED_INT_ASM_OP
#define UNALIGNED_INT_ASM_OP	".4byte"
#endif
#ifndef DEF_ASM_OP
#define DEF_ASM_OP		".set"
#endif

/* This macro is already used elsewhere and has a published default.  */
#ifndef ASM_BYTE_OP
#define ASM_BYTE_OP		"\t.byte"
#endif

/* Definitions of defaults for formats and names of various special
   (artificial) labels which may be generated within this file (when
   the -g options is used and DWARF_DEBUGGING_INFO is in effect.

   If necessary, these may be overridden from within your tm.h file,
   but typically, you should never need to override these.  */

#ifndef TEXT_BEGIN_LABEL
#define TEXT_BEGIN_LABEL	"._text_b"
#endif
#ifndef TEXT_END_LABEL
#define TEXT_END_LABEL		"._text_e"
#endif

#ifndef DATA_BEGIN_LABEL
#define DATA_BEGIN_LABEL	"._data_b"
#endif
#ifndef DATA_END_LABEL
#define DATA_END_LABEL		"._data_e"
#endif

#ifndef DATA1_BEGIN_LABEL
#define DATA1_BEGIN_LABEL	"._data1_b"
#endif
#ifndef DATA1_END_LABEL
#define DATA1_END_LABEL		"._data1_e"
#endif

#ifndef RODATA_BEGIN_LABEL
#define RODATA_BEGIN_LABEL	"._rodata_b"
#endif
#ifndef RODATA_END_LABEL
#define RODATA_END_LABEL	"._rodata_e"
#endif

#ifndef RODATA1_BEGIN_LABEL
#define RODATA1_BEGIN_LABEL	"._rodata1_b"
#endif
#ifndef RODATA1_END_LABEL
#define RODATA1_END_LABEL	"._rodata1_e"
#endif

#ifndef BSS_BEGIN_LABEL
#define BSS_BEGIN_LABEL		"._bss_b"
#endif
#ifndef BSS_END_LABEL
#define BSS_END_LABEL		"._bss_e"
#endif

#ifndef LINE_BEGIN_LABEL
#define LINE_BEGIN_LABEL	"._line_b"
#endif
#ifndef LINE_LAST_ENTRY_LABEL
#define LINE_LAST_ENTRY_LABEL	"._line_last"
#endif
#ifndef LINE_END_LABEL
#define LINE_END_LABEL		"._line_e"
#endif

#ifndef DEBUG_BEGIN_LABEL
#define DEBUG_BEGIN_LABEL	"._debug_b"
#endif
#ifndef SFNAMES_BEGIN_LABEL
#define SFNAMES_BEGIN_LABEL	"._sfnames_b"
#endif
#ifndef SRCINFO_BEGIN_LABEL
#define SRCINFO_BEGIN_LABEL	"._srcinfo_b"
#endif
#ifndef MACINFO_BEGIN_LABEL
#define MACINFO_BEGIN_LABEL	"._macinfo_b"
#endif

#ifndef DIE_BEGIN_LABEL_FMT
#define DIE_BEGIN_LABEL_FMT	"._D%u"
#endif
#ifndef DIE_END_LABEL_FMT
#define DIE_END_LABEL_FMT	"._D%u_e"
#endif
#ifndef PUB_DIE_LABEL_FMT
#define PUB_DIE_LABEL_FMT	"._P%u"
#endif
#ifndef INSN_LABEL_FMT
#define INSN_LABEL_FMT		"._I%u_%u"
#endif
#ifndef BLOCK_BEGIN_LABEL_FMT
#define BLOCK_BEGIN_LABEL_FMT	"._B%u"
#endif
#ifndef BLOCK_END_LABEL_FMT
#define BLOCK_END_LABEL_FMT	"._B%u_e"
#endif
#ifndef SS_BEGIN_LABEL_FMT
#define SS_BEGIN_LABEL_FMT	"._s%u"
#endif
#ifndef SS_END_LABEL_FMT
#define SS_END_LABEL_FMT	"._s%u_e"
#endif
#ifndef EE_BEGIN_LABEL_FMT
#define EE_BEGIN_LABEL_FMT	"._e%u"
#endif
#ifndef EE_END_LABEL_FMT
#define EE_END_LABEL_FMT	"._e%u_e"
#endif
#ifndef MT_BEGIN_LABEL_FMT
#define MT_BEGIN_LABEL_FMT	"._t%u"
#endif
#ifndef MT_END_LABEL_FMT
#define MT_END_LABEL_FMT	"._t%u_e"
#endif
#ifndef LOC_BEGIN_LABEL_FMT
#define LOC_BEGIN_LABEL_FMT	"._l%u"
#endif
#ifndef LOC_END_LABEL_FMT
#define LOC_END_LABEL_FMT	"._l%u_e"
#endif
#ifndef BOUND_BEGIN_LABEL_FMT
#define BOUND_BEGIN_LABEL_FMT	"._b%u_%u_%c"
#endif
#ifndef BOUND_END_LABEL_FMT
#define BOUND_END_LABEL_FMT	"._b%u_%u_%c_e"
#endif
#ifndef DERIV_BEGIN_LABEL_FMT
#define DERIV_BEGIN_LABEL_FMT	"._d%u"
#endif
#ifndef DERIV_END_LABEL_FMT
#define DERIV_END_LABEL_FMT	"._d%u_e"
#endif
#ifndef SL_BEGIN_LABEL_FMT
#define SL_BEGIN_LABEL_FMT	"._sl%u"
#endif
#ifndef SL_END_LABEL_FMT
#define SL_END_LABEL_FMT	"._sl%u_e"
#endif
#ifndef FUNC_END_LABEL_FMT
#define FUNC_END_LABEL_FMT	"._f%u_e"
#endif
#ifndef TYPE_NAME_FMT
#define TYPE_NAME_FMT		"._T%u"
#endif
#ifndef LINE_CODE_LABEL_FMT
#define LINE_CODE_LABEL_FMT	"._LC%u"
#endif
#ifndef SFNAMES_ENTRY_LABEL_FMT
#define SFNAMES_ENTRY_LABEL_FMT	"._F%u"
#endif
#ifndef LINE_ENTRY_LABEL_FMT
#define LINE_ENTRY_LABEL_FMT	"._LE%u"
#endif

/* Definitions of defaults for various types of primitive assembly language
   output operations.

   If necessary, these may be overridden from within your tm.h file,
   but typically, you should never need to override these.  */

#ifndef ASM_OUTPUT_SOURCE_FILENAME
#define ASM_OUTPUT_SOURCE_FILENAME(FILE,NAME) \
  fprintf ((FILE), "\t%s\t\"%s\"\n", FILE_ASM_OP, NAME)
#endif

#ifndef ASM_OUTPUT_DEF
#define ASM_OUTPUT_DEF(FILE,LABEL1,LABEL2)				\
 do {	fprintf ((FILE), "\t%s\t", DEF_ASM_OP);				\
	assemble_name (FILE, LABEL1);					\
	fprintf (FILE, ",");						\
	assemble_name (FILE, LABEL2);					\
	fprintf (FILE, "\n");						\
  } while (0)
#endif

#ifndef ASM_DWARF_DEBUG_SECTION
#define ASM_DWARF_DEBUG_SECTION(FILE) \
  fprintf ((FILE), "%s\t.debug\n", SECTION_ASM_OP)
#endif

#ifndef ASM_DWARF_LINE_SECTION
#define ASM_DWARF_LINE_SECTION(FILE) \
  fprintf ((FILE), "%s\t.line\n", SECTION_ASM_OP)
#endif

#ifndef ASM_DWARF_SFNAMES_SECTION
#define ASM_DWARF_SFNAMES_SECTION(FILE) \
  fprintf ((FILE), "%s\t.debug_sfnames\n", SECTION_ASM_OP)
#endif

#ifndef ASM_DWARF_SRCINFO_SECTION
#define ASM_DWARF_SRCINFO_SECTION(FILE) \
  fprintf ((FILE), "%s\t.debug_srcinfo\n", SECTION_ASM_OP)
#endif

#ifndef ASM_DWARF_MACINFO_SECTION
#define ASM_DWARF_MACINFO_SECTION(FILE) \
  fprintf ((FILE), "%s\t.debug_macinfo\n", SECTION_ASM_OP)
#endif

#ifndef ASM_DWARF_PUBNAMES_SECTION
#define ASM_DWARF_PUBNAMES_SECTION(FILE) \
  fprintf ((FILE), "%s\t.debug_pubnames\n", SECTION_ASM_OP)
#endif

#ifndef ASM_DWARF_ARANGES_SECTION
#define ASM_DWARF_ARANGES_SECTION(FILE) \
  fprintf ((FILE), "%s\t.debug_aranges\n", SECTION_ASM_OP)
#endif

#ifndef ASM_DWARF_TEXT_SECTION
#define ASM_DWARF_TEXT_SECTION(FILE) \
  fprintf ((FILE), "%s\t.text\n", SECTION_ASM_OP)
#endif

#ifndef ASM_DWARF_DATA_SECTION
#define ASM_DWARF_DATA_SECTION(FILE) \
  fprintf ((FILE), "%s\t.data\n", SECTION_ASM_OP)
#endif

#ifndef ASM_DWARF_DATA1_SECTION
#define ASM_DWARF_DATA1_SECTION(FILE) \
  fprintf ((FILE), "%s\t.data1\n", SECTION_ASM_OP)
#endif

#ifndef ASM_DWARF_RODATA_SECTION
#define ASM_DWARF_RODATA_SECTION(FILE) \
  fprintf ((FILE), "%s\t.rodata\n", SECTION_ASM_OP)
#endif

#ifndef ASM_DWARF_RODATA1_SECTION
#define ASM_DWARF_RODATA1_SECTION(FILE) \
  fprintf ((FILE), "%s\t.rodata1\n", SECTION_ASM_OP)
#endif

#ifndef ASM_DWARF_BSS_SECTION
#define ASM_DWARF_BSS_SECTION(FILE) \
  fprintf ((FILE), "%s\t.bss\n", SECTION_ASM_OP)
#endif

#ifndef ASM_DWARF_POP_SECTION
#define ASM_DWARF_POP_SECTION(FILE) \
  fprintf ((FILE), "\t.previous\n")
#endif

#ifndef ASM_OUTPUT_DWARF_DELTA2
#define ASM_OUTPUT_DWARF_DELTA2(FILE,LABEL1,LABEL2)			\
 do {	fprintf ((FILE), "\t%s\t", UNALIGNED_SHORT_ASM_OP);		\
	assemble_name (FILE, LABEL1);					\
	fprintf (FILE, "-");						\
	assemble_name (FILE, LABEL2);					\
	fprintf (FILE, "\n");						\
  } while (0)
#endif

#ifndef ASM_OUTPUT_DWARF_DELTA4
#define ASM_OUTPUT_DWARF_DELTA4(FILE,LABEL1,LABEL2)			\
 do {	fprintf ((FILE), "\t%s\t", UNALIGNED_INT_ASM_OP);		\
	assemble_name (FILE, LABEL1);					\
	fprintf (FILE, "-");						\
	assemble_name (FILE, LABEL2);					\
	fprintf (FILE, "\n");						\
  } while (0)
#endif

#ifndef ASM_OUTPUT_DWARF_TAG
#define ASM_OUTPUT_DWARF_TAG(FILE,TAG)					\
  fprintf ((FILE), "\t%s\t0x%x\t%s %s\n", UNALIGNED_SHORT_ASM_OP,	\
	(unsigned) TAG, ASM_COMMENT_START, tag_name (TAG))
#endif

#ifndef ASM_OUTPUT_DWARF_ATTRIBUTE
#define ASM_OUTPUT_DWARF_ATTRIBUTE(FILE,ATTRIBUTE)			\
  fprintf ((FILE), "\t%s\t0x%x\t%s %s\n", UNALIGNED_SHORT_ASM_OP,	\
	(unsigned) ATTRIBUTE, ASM_COMMENT_START, attribute_name (ATTRIBUTE))
#endif

#ifndef ASM_OUTPUT_DWARF_STACK_OP
#define ASM_OUTPUT_DWARF_STACK_OP(FILE,OP)				\
  fprintf ((FILE), "%s\t0x%x\t%s %s\n", ASM_BYTE_OP,			\
	(unsigned) OP, ASM_COMMENT_START, stack_op_name (OP))
#endif

#ifndef ASM_OUTPUT_DWARF_FUND_TYPE
#define ASM_OUTPUT_DWARF_FUND_TYPE(FILE,FT)				\
  fprintf ((FILE), "\t%s\t0x%x\t%s %s\n", UNALIGNED_SHORT_ASM_OP,	\
	(unsigned) FT, ASM_COMMENT_START, fundamental_type_name (FT))
#endif

#ifndef ASM_OUTPUT_DWARF_FMT_BYTE
#define ASM_OUTPUT_DWARF_FMT_BYTE(FILE,FMT)				\
  fprintf ((FILE), "%s\t0x%x\t%s %s\n", ASM_BYTE_OP,			\
	(unsigned) FMT, ASM_COMMENT_START, format_byte_name (FMT))
#endif

#ifndef ASM_OUTPUT_DWARF_TYPE_MODIFIER
#define ASM_OUTPUT_DWARF_TYPE_MODIFIER(FILE,MOD)			\
  fprintf ((FILE), "%s\t0x%x\t%s %s\n", ASM_BYTE_OP,			\
	(unsigned) MOD, ASM_COMMENT_START, modifier_name (MOD))
#endif

#ifndef ASM_OUTPUT_DWARF_ADDR
#define ASM_OUTPUT_DWARF_ADDR(FILE,LABEL)				\
 do {	fprintf ((FILE), "\t%s\t", UNALIGNED_INT_ASM_OP);		\
	assemble_name (FILE, LABEL);					\
	fprintf (FILE, "\n");						\
  } while (0)
#endif

#ifndef ASM_OUTPUT_DWARF_ADDR_CONST
#define ASM_OUTPUT_DWARF_ADDR_CONST(FILE,RTX)				\
  fprintf ((FILE), "\t%s\t", UNALIGNED_INT_ASM_OP);			\
  output_addr_const ((FILE), (RTX));					\
  fputc ('\n', (FILE))
#endif

#ifndef ASM_OUTPUT_DWARF_REF
#define ASM_OUTPUT_DWARF_REF(FILE,LABEL)				\
 do {	fprintf ((FILE), "\t%s\t", UNALIGNED_INT_ASM_OP);		\
	assemble_name (FILE, LABEL);					\
	fprintf (FILE, "\n");						\
  } while (0)
#endif

#ifndef ASM_OUTPUT_DWARF_DATA1
#define ASM_OUTPUT_DWARF_DATA1(FILE,VALUE) \
  fprintf ((FILE), "%s\t0x%x\n", ASM_BYTE_OP, VALUE)
#endif

#ifndef ASM_OUTPUT_DWARF_DATA2
#define ASM_OUTPUT_DWARF_DATA2(FILE,VALUE) \
  fprintf ((FILE), "\t%s\t0x%x\n", UNALIGNED_SHORT_ASM_OP, (unsigned) VALUE)
#endif

#ifndef ASM_OUTPUT_DWARF_DATA4
#define ASM_OUTPUT_DWARF_DATA4(FILE,VALUE) \
  fprintf ((FILE), "\t%s\t0x%x\n", UNALIGNED_INT_ASM_OP, (unsigned) VALUE)
#endif

#ifndef ASM_OUTPUT_DWARF_DATA8
#define ASM_OUTPUT_DWARF_DATA8(FILE,HIGH_VALUE,LOW_VALUE)		\
  do {									\
    if (WORDS_BIG_ENDIAN)						\
      {									\
	fprintf ((FILE), "\t%s\t0x%x\n", UNALIGNED_INT_ASM_OP, HIGH_VALUE); \
	fprintf ((FILE), "\t%s\t0x%x\n", UNALIGNED_INT_ASM_OP, LOW_VALUE);\
      }									\
    else								\
      {									\
	fprintf ((FILE), "\t%s\t0x%x\n", UNALIGNED_INT_ASM_OP, LOW_VALUE);\
	fprintf ((FILE), "\t%s\t0x%x\n", UNALIGNED_INT_ASM_OP, HIGH_VALUE); \
      }									\
  } while (0)
#endif

#ifndef ASM_OUTPUT_DWARF_STRING
#define ASM_OUTPUT_DWARF_STRING(FILE,P) \
  ASM_OUTPUT_ASCII ((FILE), P, strlen (P)+1)
#endif

/************************ general utility functions **************************/

inline char *
xstrdup (s)
     register char *s;
{
  register char *p = (char *) xmalloc (strlen (s) + 1);

  strcpy (p, s);
  return p;
}

static char *
tag_name (tag)
     register unsigned tag;
{
  switch (tag)
    {
    case TAG_padding:		return "TAG_padding";
    case TAG_array_type:	return "TAG_array_type";
    case TAG_class_type:	return "TAG_class_type";
    case TAG_entry_point:	return "TAG_entry_point";
    case TAG_enumeration_type:	return "TAG_enumeration_type";
    case TAG_formal_parameter:	return "TAG_formal_parameter";
    case TAG_global_subroutine:	return "TAG_global_subroutine";
    case TAG_global_variable:	return "TAG_global_variable";
    case TAG_imported_declaration:	return "TAG_imported_declaration";
    case TAG_label:		return "TAG_label";
    case TAG_lexical_block:	return "TAG_lexical_block";
    case TAG_local_variable:	return "TAG_local_variable";
    case TAG_member:		return "TAG_member";
    case TAG_pointer_type:	return "TAG_pointer_type";
    case TAG_reference_type:	return "TAG_reference_type";
    case TAG_compile_unit:	return "TAG_compile_unit";
    case TAG_string_type:	return "TAG_string_type";
    case TAG_structure_type:	return "TAG_structure_type";
    case TAG_subroutine:	return "TAG_subroutine";
    case TAG_subroutine_type:	return "TAG_subroutine_type";
    case TAG_typedef:		return "TAG_typedef";
    case TAG_union_type:	return "TAG_union_type";
    case TAG_unspecified_parameters:	return "TAG_unspecified_parameters";
    case TAG_variant:		return "TAG_variant";
    case TAG_format:		return "TAG_format";
    case TAG_with_stmt:		return "TAG_with_stmt";
    case TAG_set_type:		return "TAG_set_type";
    default:			return "<unknown tag>";
    }
}

static char *
attribute_name (attr)
     register unsigned attr;
{
  switch (attr)
    {
    case AT_sibling:		return "AT_sibling";
    case AT_location:		return "AT_location";
    case AT_name:		return "AT_name";
    case AT_fund_type:		return "AT_fund_type";
    case AT_mod_fund_type:	return "AT_mod_fund_type";
    case AT_user_def_type:	return "AT_user_def_type";
    case AT_mod_u_d_type:	return "AT_mod_u_d_type";
    case AT_ordering:		return "AT_ordering";
    case AT_subscr_data:	return "AT_subscr_data";
    case AT_byte_size:		return "AT_byte_size";
    case AT_bit_offset:		return "AT_bit_offset";
    case AT_bit_size:		return "AT_bit_size";
    case AT_element_list:	return "AT_element_list";
    case AT_stmt_list:		return "AT_stmt_list";
    case AT_low_pc:		return "AT_low_pc";
    case AT_high_pc:		return "AT_high_pc";
    case AT_language:		return "AT_language";
    case AT_member:		return "AT_member";
    case AT_discr:		return "AT_discr";
    case AT_discr_value:	return "AT_discr_value";
    case AT_visibility:		return "AT_visibility";
    case AT_import:		return "AT_import";
    case AT_string_length:	return "AT_string_length";
    case AT_comp_dir:		return "AT_comp_dir";
    case AT_producer:		return "AT_producer";
    case AT_frame_base:		return "AT_frame_base";
    case AT_start_scope:	return "AT_start_scope";
    case AT_stride_size:	return "AT_stride_size";
    case AT_src_info:		return "AT_src_info";
    case AT_prototyped:		return "AT_prototyped";
    case AT_const_value_block4:		return "AT_const_value_block4";
    case AT_sf_names:		return "AT_sf_names";
    case AT_mac_info:		return "AT_mac_info";
    default:			return "<unknown attribute>";
    }
}

static char *
stack_op_name (op)
     register unsigned op;
{
  switch (op)
    {
    case OP_REG:		return "OP_REG";
    case OP_BASEREG:		return "OP_BASEREG";
    case OP_ADDR:		return "OP_ADDR";
    case OP_CONST:		return "OP_CONST";
    case OP_DEREF2:		return "OP_DEREF2";
    case OP_DEREF4:		return "OP_DEREF4";
    case OP_ADD:		return "OP_ADD";
    default:			return "<unknown stack operator>";
    }
}

static char *
modifier_name (mod)
     register unsigned mod;
{
  switch (mod)
    {
    case MOD_pointer_to:	return "MOD_pointer_to";
    case MOD_reference_to:	return "MOD_reference_to";
    case MOD_const:		return "MOD_const";
    case MOD_volatile:		return "MOD_volatile";
    default:			return "<unknown modifier>";
    }
}

static char *
format_byte_name (fmt)
     register unsigned fmt;
{
  switch (fmt)
    {
    case FMT_FT_C_C:	return "FMT_FT_C_C";
    case FMT_FT_C_X:	return "FMT_FT_C_X";
    case FMT_FT_X_C:	return "FMT_FT_X_C";
    case FMT_FT_X_X:	return "FMT_FT_X_X";
    case FMT_UT_C_C:	return "FMT_UT_C_C";
    case FMT_UT_C_X:	return "FMT_UT_C_X";
    case FMT_UT_X_C:	return "FMT_UT_X_C";
    case FMT_UT_X_X:	return "FMT_UT_X_X";
    case FMT_ET:	return "FMT_ET";
    default:		return "<unknown array bound format byte>";
    }
}
static char *
fundamental_type_name (ft)
     register unsigned ft;
{
  switch (ft)
    {
    case FT_char:		return "FT_char";
    case FT_signed_char:	return "FT_signed_char";
    case FT_unsigned_char:	return "FT_unsigned_char";
    case FT_short:		return "FT_short";
    case FT_signed_short:	return "FT_signed_short";
    case FT_unsigned_short:	return "FT_unsigned_short";
    case FT_integer:		return "FT_integer";
    case FT_signed_integer:	return "FT_signed_integer";
    case FT_unsigned_integer:	return "FT_unsigned_integer";
    case FT_long:		return "FT_long";
    case FT_signed_long:	return "FT_signed_long";
    case FT_unsigned_long:	return "FT_unsigned_long";
    case FT_pointer:		return "FT_pointer";
    case FT_float:		return "FT_float";
    case FT_dbl_prec_float:	return "FT_dbl_prec_float";
    case FT_ext_prec_float:	return "FT_ext_prec_float";
    case FT_complex:		return "FT_complex";
    case FT_dbl_prec_complex:	return "FT_dbl_prec_complex";
    case FT_void:		return "FT_void";
    case FT_boolean:		return "FT_boolean";
    case FT_long_long:		return "FT_long_long";
    case FT_signed_long_long:	return "FT_signed_long_long";
    case FT_unsigned_long_long: return "FT_unsigned_long_long";
    default:			return "<unknown fundamental type>";
    }
}

/**************** utility functions for attribute functions ******************/

/* Given a pointer to a tree node for some type, return a Dwarf fundamental
   type code for the given type.

   This routine must only be called for GCC type nodes that correspond to
   Dwarf fundamental types.

   The current Dwarf draft specification calls for Dwarf fundamental types
   to accurately reflect the fact that a given type was either a "plain"
   integral type or an explicitly "signed" integral type.  Unfortuantely,
   we can't always do this, because GCC may already have thrown away the
   information about the precise way in which the type was originally
   specified, as in:

	typedef signed int field_type;

	struct s { field_type f; };

   Since we may be stuck here without enought information to do exactly
   what is called for in the Dwarf draft specification, we do the best
   that we can under the circumstances and always use the "plain" integral
   fundamental type codes for int, short, and long types.  That's probably
   good enough.  The additional accuracy called for in the current DWARF
   draft specification is probably never even useful in practice.  */

static int
fundamental_type_code (type)
     register tree type;
{
  if (TREE_CODE (type) == ERROR_MARK)
    return 0;

  switch (TREE_CODE (type))
    {
      case ERROR_MARK:
	return FT_void;

      case VOID_TYPE:
	return FT_void;

      case INTEGER_TYPE:
	/* Carefully distinguish all the standard types of C,
	   without messing up if the language is not C.
	   Note that we check only for the names that contain spaces;
	   other names might occur by coincidence in other languages.  */
	if (TYPE_NAME (type) != 0
	    && TREE_CODE (TYPE_NAME (type)) == TYPE_DECL
	    && DECL_NAME (TYPE_NAME (type)) != 0
	    && TREE_CODE (DECL_NAME (TYPE_NAME (type))) == IDENTIFIER_NODE)
	  {
	    char *name = IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (type)));

	    if (!strcmp (name, "unsigned char"))
	      return FT_unsigned_char;
	    if (!strcmp (name, "signed char"))
	      return FT_signed_char;
	    if (!strcmp (name, "unsigned int"))
	      return FT_unsigned_integer;
	    if (!strcmp (name, "short int"))
	      return FT_short;
	    if (!strcmp (name, "short unsigned int"))
	      return FT_unsigned_short;
	    if (!strcmp (name, "long int"))
	      return FT_long;
	    if (!strcmp (name, "long unsigned int"))
	      return FT_unsigned_long;
	    if (!strcmp (name, "long long int"))
	      return FT_long_long;		/* Not grok'ed by svr4 SDB */
	    if (!strcmp (name, "long long unsigned int"))
	      return FT_unsigned_long_long;	/* Not grok'ed by svr4 SDB */
	  }

	/* Most integer types will be sorted out above, however, for the
	   sake of special `array index' integer types, the following code
	   is also provided.  */

	if (TYPE_PRECISION (type) == INT_TYPE_SIZE)
	  return (TREE_UNSIGNED (type) ? FT_unsigned_integer : FT_integer);

	if (TYPE_PRECISION (type) == LONG_TYPE_SIZE)
	  return (TREE_UNSIGNED (type) ? FT_unsigned_long : FT_long);

	if (TYPE_PRECISION (type) == LONG_LONG_TYPE_SIZE)
	  return (TREE_UNSIGNED (type) ? FT_unsigned_long_long : FT_long_long);

	if (TYPE_PRECISION (type) == SHORT_TYPE_SIZE)
	  return (TREE_UNSIGNED (type) ? FT_unsigned_short : FT_short);

	if (TYPE_PRECISION (type) == CHAR_TYPE_SIZE)
	  return (TREE_UNSIGNED (type) ? FT_unsigned_char : FT_char);

	abort ();

      case REAL_TYPE:
	/* Carefully distinguish all the standard types of C,
	   without messing up if the language is not C.  */
	if (TYPE_NAME (type) != 0
	    && TREE_CODE (TYPE_NAME (type)) == TYPE_DECL
	    && DECL_NAME (TYPE_NAME (type)) != 0
	    && TREE_CODE (DECL_NAME (TYPE_NAME (type))) == IDENTIFIER_NODE)
	  {
	    char *name = IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (type)));

	    /* Note that here we can run afowl of a serious bug in "classic"
	       svr4 SDB debuggers.  They don't seem to understand the
	       FT_ext_prec_float type (even though they should).  */

	    if (!strcmp (name, "long double"))
	      return FT_ext_prec_float;
	  }

	if (TYPE_PRECISION (type) == DOUBLE_TYPE_SIZE)
	  return FT_dbl_prec_float;
	if (TYPE_PRECISION (type) == FLOAT_TYPE_SIZE)
	  return FT_float;

	/* Note that here we can run afowl of a serious bug in "classic"
	   svr4 SDB debuggers.  They don't seem to understand the
	   FT_ext_prec_float type (even though they should).  */

	if (TYPE_PRECISION (type) == LONG_DOUBLE_TYPE_SIZE)
	  return FT_ext_prec_float;
	abort ();

      case COMPLEX_TYPE:
	return FT_complex;	/* GNU FORTRAN COMPLEX type.  */

      case CHAR_TYPE:
	return FT_char;		/* GNU Pascal CHAR type.  Not used in C.  */

      case BOOLEAN_TYPE:
	return FT_boolean;	/* GNU FORTRAN BOOLEAN type.  */

      default:
	abort ();	/* No other TREE_CODEs are Dwarf fundamental types.  */
    }
  return 0;
}

/* Given a pointer to an arbitrary ..._TYPE tree node, return a pointer to
   the Dwarf "root" type for the given input type.  The Dwarf "root" type
   of a given type is generally the same as the given type, except that if
   the	given type is a pointer or reference type, then the root type of
   the given type is the root type of the "basis" type for the pointer or
   reference type.  (This definition of the "root" type is recursive.)
   Also, the root type of a `const' qualified type or a `volatile'
   qualified type is the root type of the given type without the
   qualifiers.  */

static tree
root_type (type)
     register tree type;
{
  if (TREE_CODE (type) == ERROR_MARK)
    return error_mark_node;

  switch (TREE_CODE (type))
    {
      case ERROR_MARK:
	return error_mark_node;

      case POINTER_TYPE:
      case REFERENCE_TYPE:
	return TYPE_MAIN_VARIANT (root_type (TREE_TYPE (type)));

      default:
	return TYPE_MAIN_VARIANT (type);
    }
}

/* Given a pointer to an arbitrary ..._TYPE tree node, write out a sequence
   of zero or more Dwarf "type-modifier" bytes applicable to the type.	*/

static void
write_modifier_bytes (type, decl_const, decl_volatile)
     register tree type;
     register int decl_const;
     register int decl_volatile;
{
  if (TREE_CODE (type) == ERROR_MARK)
    return;

  if (TYPE_READONLY (type) || decl_const)
    ASM_OUTPUT_DWARF_TYPE_MODIFIER (asm_out_file, MOD_const);
  if (TYPE_VOLATILE (type) || decl_volatile)
    ASM_OUTPUT_DWARF_TYPE_MODIFIER (asm_out_file, MOD_volatile);
  switch (TREE_CODE (type))
    {
      case POINTER_TYPE:
	ASM_OUTPUT_DWARF_TYPE_MODIFIER (asm_out_file, MOD_pointer_to);
	write_modifier_bytes (TREE_TYPE (type), 0, 0);
	return;

      case REFERENCE_TYPE:
	ASM_OUTPUT_DWARF_TYPE_MODIFIER (asm_out_file, MOD_reference_to);
	write_modifier_bytes (TREE_TYPE (type), 0, 0);
	return;

      case ERROR_MARK:
      default:
	return;
    }
}

/* Given a pointer to an arbitrary ..._TYPE tree node, return non-zero if the
   given input type is a Dwarf "fundamental" type.  Otherwise return zero.  */

inline int
type_is_fundamental (type)
     register tree type;
{
  switch (TREE_CODE (type))
    {
      case ERROR_MARK:
      case VOID_TYPE:
      case INTEGER_TYPE:
      case REAL_TYPE:
      case COMPLEX_TYPE:
      case BOOLEAN_TYPE:
      case CHAR_TYPE:
	return 1;

      case SET_TYPE:
      case ARRAY_TYPE:
      case RECORD_TYPE:
      case UNION_TYPE:
      case ENUMERAL_TYPE:
      case FUNCTION_TYPE:
      case METHOD_TYPE:
      case POINTER_TYPE:
      case REFERENCE_TYPE:
      case STRING_TYPE:
      case FILE_TYPE:
      case OFFSET_TYPE:
      case LANG_TYPE:
	return 0;

      default:
	abort ();
    }
  return 0;
}

/* Given a pointer to some ..._TYPE tree node, generate an assembly language
   equate directive which will associate an easily remembered symbolic name
   with the current DIE.

   The name used is an artificial label generated from the TYPE_UID number
   associated with the given type node.  The name it gets equated to is the
   symbolic label that we (previously) output at the start of the DIE that
   we are currently generating.

   Calling this function while generating some "type related" form of DIE
   makes it easy to later refer to the DIE which represents the given type
   simply by re-generating the alternative name from the ..._TYPE node's
   UID number.	*/

inline void
equate_type_number_to_die_number (type)
     register tree type;
{
  char type_label[MAX_ARTIFICIAL_LABEL_BYTES];
  char die_label[MAX_ARTIFICIAL_LABEL_BYTES];

  /* We are generating a DIE to represent the main variant of this type
     (i.e the type without any const or volatile qualifiers) so in order
     to get the equate to come out right, we need to get the main variant
     itself here.  */

  type = TYPE_MAIN_VARIANT (type);

  sprintf (type_label, TYPE_NAME_FMT, TYPE_UID (type));
  sprintf (die_label, DIE_BEGIN_LABEL_FMT, current_dienum);
  ASM_OUTPUT_DEF (asm_out_file, type_label, die_label);
}

/* The following routine is a nice and simple transducer.  It converts the
   RTL for a variable or parameter (resident in memory) into an equivalent
   Dwarf representation of a mechanism for getting the address of that same
   variable onto the top of a hypothetical "address evaluation" stack.

   When creating memory location descriptors, we are effectively trans-
   forming the RTL for a memory-resident object into its Dwarf postfix
   expression equivalent.  This routine just recursively descends an
   RTL tree, turning it into Dwarf postfix code as it goes.  */

static void
output_mem_loc_descriptor (rtl)
      register rtx rtl;
{
  /* Note that for a dynamically sized array, the location we will
     generate a description of here will be the lowest numbered location
     which is actually within the array.  That's *not* necessarily the
     same as the zeroth element of the array.  */

  switch (GET_CODE (rtl))
    {
      case SUBREG:

	/* The case of a subreg may arise when we have a local (register)
	   variable or a formal (register) parameter which doesn't quite
	   fill up an entire register.	For now, just assume that it is
	   legitimate to make the Dwarf info refer to the whole register
	   which contains the given subreg.  */

	rtl = XEXP (rtl, 0);
	/* Drop thru.  */

      case REG:

	/* Whenever a register number forms a part of the description of
	   the method for calculating the (dynamic) address of a memory
	   resident object, Dwarf rules require the register number to
	   be referred to as a "base register".  This distinction is not
	   based in any way upon what category of register the hardware
	   believes the given register belongs to.  This is strictly
	   Dwarf terminology we're dealing with here.  */

	ASM_OUTPUT_DWARF_STACK_OP (asm_out_file, OP_BASEREG);
        ASM_OUTPUT_DWARF_DATA4 (asm_out_file,
				DBX_REGISTER_NUMBER (REGNO (rtl)));
	break;

      case MEM:
	output_mem_loc_descriptor (XEXP (rtl, 0));
	ASM_OUTPUT_DWARF_STACK_OP (asm_out_file, OP_DEREF4);
	break;

      case CONST:
      case SYMBOL_REF:
	ASM_OUTPUT_DWARF_STACK_OP (asm_out_file, OP_ADDR);
	ASM_OUTPUT_DWARF_ADDR_CONST (asm_out_file, rtl);
	break;

      case PLUS:
	output_mem_loc_descriptor (XEXP (rtl, 0));
	output_mem_loc_descriptor (XEXP (rtl, 1));
	ASM_OUTPUT_DWARF_STACK_OP (asm_out_file, OP_ADD);
	break;

      case CONST_INT:
	ASM_OUTPUT_DWARF_STACK_OP (asm_out_file, OP_CONST);
	ASM_OUTPUT_DWARF_DATA4 (asm_out_file, INTVAL (rtl));
	break;

      default:
	abort ();
    }
}

/* Output a proper Dwarf location descriptor for a variable or parameter
   which is either allocated in a register or in a memory location.  For
   a register, we just generate an OP_REG and the register number.  For a
   memory location we provide a Dwarf postfix expression describing how to
   generate the (dynamic) address of the object onto the address stack.  */

static void
output_loc_descriptor (rtl)
     register rtx rtl;
{
  switch (GET_CODE (rtl))
    {
    case SUBREG:

	/* The case of a subreg may arise when we have a local (register)
	   variable or a formal (register) parameter which doesn't quite
	   fill up an entire register.	For now, just assume that it is
	   legitimate to make the Dwarf info refer to the whole register
	   which contains the given subreg.  */

	rtl = XEXP (rtl, 0);
	/* Drop thru.  */

    case REG:
	ASM_OUTPUT_DWARF_STACK_OP (asm_out_file, OP_REG);
        ASM_OUTPUT_DWARF_DATA4 (asm_out_file,
				DBX_REGISTER_NUMBER (REGNO (rtl)));
	break;

    case MEM:
      output_mem_loc_descriptor (XEXP (rtl, 0));
      break;

    default:
      abort ();		/* Should never happen */
    }
}

/* Given a tree node describing an array bound (either lower or upper)
   output a representation for that bound.  */

static void
output_bound_representation (bound, dim_num, u_or_l)
     register tree bound;
     register unsigned dim_num; /* For multi-dimensional arrays.  */
     register char u_or_l;	/* Designates upper or lower bound.  */
{
  switch (TREE_CODE (bound))
    {

      case ERROR_MARK:
	return;

      /* All fixed-bounds are represented by INTEGER_CST nodes.	 */

      case INTEGER_CST:
	ASM_OUTPUT_DWARF_DATA4 (asm_out_file,
				(unsigned) TREE_INT_CST_LOW (bound));
	break;

      /* Dynamic bounds may be represented by NOP_EXPR nodes containing
	 SAVE_EXPR nodes.  */

      case NOP_EXPR:
	bound = TREE_OPERAND (bound, 0);
	/* ... fall thru... */

      case SAVE_EXPR:
	{
	  char begin_label[MAX_ARTIFICIAL_LABEL_BYTES];
	  char end_label[MAX_ARTIFICIAL_LABEL_BYTES];

	  sprintf (begin_label, BOUND_BEGIN_LABEL_FMT,
				current_dienum, dim_num, u_or_l);

	  sprintf (end_label,	BOUND_END_LABEL_FMT,
				current_dienum, dim_num, u_or_l);

	  ASM_OUTPUT_DWARF_DELTA2 (asm_out_file, end_label, begin_label);
	  ASM_OUTPUT_LABEL (asm_out_file, begin_label);

	  /* If we are working on a bound for a dynamic dimension in C,
	     the dynamic dimension in question had better have a static
	     (zero) lower bound and a dynamic *upper* bound.  */

	  if (u_or_l != 'u')
	    abort ();

	  /* If optimization is turned on, the SAVE_EXPRs that describe
	     how to access the upper bound values are essentially bogus.
	     They only describe (at best) how to get at these values at
	     the points in the generated code right after they have just
	     been computed.  Worse yet, in the typical case, the upper
	     bound values will not even *be* computed in the optimized
	     code, so these SAVE_EXPRs are entirely bogus.

	     In order to compensate for this fact, we check here to see
	     if optimization is enabled, and if so, we effectively create
	     an empty location description for the (unknown and unknowable)
	     upper bound.

	     This should not cause too much trouble for existing (stupid?)
	     debuggers because they have to deal with empty upper bounds
	     location descriptions anyway in order to be able to deal with
	     incomplete array types.

	     Of course an intelligent debugger (GDB?) should be able to
	     comprehend that a missing upper bound specification in a
	     array type used for a storage class `auto' local array variable
	     indicates that the upper bound is both unknown (at compile-
	     time) and unknowable (at run-time) due to optimization.
	  */

	  if (! optimize)
	    output_loc_descriptor
	      (eliminate_regs (SAVE_EXPR_RTL (bound), 0, 0));

	  ASM_OUTPUT_LABEL (asm_out_file, end_label);
	}
	break;

      default:
	abort ();
    }
}

/* Recursive function to output a sequence of value/name pairs for
   enumeration constants in reversed order.  This is called from
   enumeration_type_die.  */

static void
output_enumeral_list (link)
     register tree link;
{
  if (link)
    {
      output_enumeral_list (TREE_CHAIN (link));
      ASM_OUTPUT_DWARF_DATA4 (asm_out_file,
			      (unsigned) TREE_INT_CST_LOW (TREE_VALUE (link)));
      ASM_OUTPUT_DWARF_STRING (asm_out_file,
			       IDENTIFIER_POINTER (TREE_PURPOSE (link)));
    }
}

/****************************** attributes *********************************/

/* The following routines are responsible for writing out the various types
   of Dwarf attributes (and any following data bytes associated with them).
   These routines are listed in order based on the numerical codes of their
   associated attributes.  */

/* Generate an AT_sibling attribute.  */

inline void
sibling_attribute ()
{
  char label[MAX_ARTIFICIAL_LABEL_BYTES];

  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_sibling);
  sprintf (label, DIE_BEGIN_LABEL_FMT, NEXT_DIE_NUM);
  ASM_OUTPUT_DWARF_REF (asm_out_file, label);
}

/* Output the form of location attributes suitable for whole variables and
   whole parameters.  Note that the location attributes for struct fields
   are generated by the routine `data_member_location_attribute' below.  */

static void
location_attribute (rtl)
     register rtx rtl;
{
  char begin_label[MAX_ARTIFICIAL_LABEL_BYTES];
  char end_label[MAX_ARTIFICIAL_LABEL_BYTES];

  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_location);
  sprintf (begin_label, LOC_BEGIN_LABEL_FMT, current_dienum);
  sprintf (end_label, LOC_END_LABEL_FMT, current_dienum);
  ASM_OUTPUT_DWARF_DELTA2 (asm_out_file, end_label, begin_label);
  ASM_OUTPUT_LABEL (asm_out_file, begin_label);

  /* Handle a special case.  If we are about to output a location descriptor
     for a variable or parameter which has been optimized out of existence,
     don't do that.  Instead we output a zero-length location descriptor
     value as part of the location attribute.  Note that we cannot simply
     suppress the entire location attribute, because the absence of a
     location attribute in certain kinds of DIEs is used to indicate some-
     thing entirely different... i.e. that the DIE represents an object
     declaration, but not a definition.  So sayeth the PLSIG.  */

  if (((GET_CODE (rtl) != REG) || (REGNO (rtl) < FIRST_PSEUDO_REGISTER))
      && ((GET_CODE (rtl) != SUBREG)
	  || (REGNO (XEXP (rtl, 0)) < FIRST_PSEUDO_REGISTER)))
    output_loc_descriptor (eliminate_regs (rtl, 0, 0));

  ASM_OUTPUT_LABEL (asm_out_file, end_label);
}

/* Output the specialized form of location attribute used for data members
   of struct types.  */

static void
data_member_location_attribute (decl)
     register tree decl;
{
  char begin_label[MAX_ARTIFICIAL_LABEL_BYTES];
  char end_label[MAX_ARTIFICIAL_LABEL_BYTES];

  if (TREE_CODE (decl) == ERROR_MARK)
    return;

  if (TREE_CODE (decl) != FIELD_DECL)
    abort ();

  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_location);
  sprintf (begin_label, LOC_BEGIN_LABEL_FMT, current_dienum);
  sprintf (end_label, LOC_END_LABEL_FMT, current_dienum);
  ASM_OUTPUT_DWARF_DELTA2 (asm_out_file, end_label, begin_label);
  ASM_OUTPUT_LABEL (asm_out_file, begin_label);
  ASM_OUTPUT_DWARF_STACK_OP (asm_out_file, OP_CONST);

  /* This is pretty strange, but existing compilers producing DWARF
     apparently calculate the byte offset of a field differently
     depending upon whether or not it is a bit-field.  If the given
     field is *not* a bit-field, then the offset is simply the
     the byte offset of the given field from the beginning of the
     struct.  For bit-fields however, the offset is the offset (in
     bytes) of the beginning of the *containing word* from the
     beginning of the whole struct.  */

  ASM_OUTPUT_DWARF_DATA4 (asm_out_file,
			  (DECL_BIT_FIELD_TYPE (decl))
				? BITFIELD_OFFSET_WORDS_IN_UNITS (decl)
				: BITFIELD_OFFSET_UNITS (decl));
  ASM_OUTPUT_DWARF_STACK_OP (asm_out_file, OP_ADD);
  ASM_OUTPUT_LABEL (asm_out_file, end_label);
}

/* Output an AT_const_value attribute for a variable or a parameter which
   does not have a "location" either in memory or in a register.  These
   things can arise in GNU C when a constant is passed as an actual
   parameter to an inlined function.  They can also arise in C++ where
   declared constants do not necessarily get memory "homes".  */

static void
const_value_attribute (rtl)
     register rtx rtl;
{
  char begin_label[MAX_ARTIFICIAL_LABEL_BYTES];
  char end_label[MAX_ARTIFICIAL_LABEL_BYTES];

  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_const_value_block4);
  sprintf (begin_label, LOC_BEGIN_LABEL_FMT, current_dienum);
  sprintf (end_label, LOC_END_LABEL_FMT, current_dienum);
  ASM_OUTPUT_DWARF_DELTA4 (asm_out_file, end_label, begin_label);
  ASM_OUTPUT_LABEL (asm_out_file, begin_label);

  switch (GET_CODE (rtl))
    {
      case CONST_INT:
	/* Note that a CONST_INT rtx could represent either an integer or
	   a floating-point constant.  A CONST_INT is used whenever the
	   constant will fit into a single word.  In all such cases, the
	   original mode of the constant value is wiped out, and the
	   CONST_INT rtx is assigned VOIDmode.  Since we no longer have
	   precise mode information for these constants, we always just
	   output them using 4 bytes.  */

	ASM_OUTPUT_DWARF_DATA4 (asm_out_file, (unsigned) INTVAL (rtl));
	break;

      case CONST_DOUBLE:
	/* Note that a CONST_DOUBLE rtx could represent either an integer
	   or a floating-point constant.  A CONST_DOUBLE is used whenever
	   the constant requires more than one word in order to be adequately
	   represented.  In all such cases, the original mode of the constant
	   value is preserved as the mode of the CONST_DOUBLE rtx, but for
	   simplicity we always just output CONST_DOUBLEs using 8 bytes.  */

	ASM_OUTPUT_DWARF_DATA8 (asm_out_file,
				(unsigned) CONST_DOUBLE_HIGH (rtl),
				(unsigned) CONST_DOUBLE_LOW (rtl));
	break;

      case CONST_STRING:
	ASM_OUTPUT_DWARF_STRING (asm_out_file, XSTR (rtl, 0));
	break;

      case SYMBOL_REF:
      case LABEL_REF:
      case CONST:
	ASM_OUTPUT_DWARF_ADDR_CONST (asm_out_file, rtl);
	break;

      case PLUS:
	/* In cases where an inlined instance of an inline function is passed
	   the address of an `auto' variable (which is local to the caller)
	   we can get a situation where the DECL_RTL of the artificial
	   local variable (for the inlining) which acts as a stand-in for
	   the corresponding formal parameter (of the inline function)
	   will look like (plus:SI (reg:SI FRAME_PTR) (const_int ...)).
	   This is not exactly a compile-time constant expression, but it
	   isn't the address of the (artificial) local variable either.
	   Rather, it represents the *value* which the artificial local
	   variable always has during its lifetime.  We currently have no
	   way to represent such quasi-constant values in Dwarf, so for now
	   we just punt and generate an AT_const_value attribute with form
	   FORM_BLOCK4 and a length of zero.  */
	break;
    }

  ASM_OUTPUT_LABEL (asm_out_file, end_label);
}

/* Generate *either* an AT_location attribute or else an AT_const_value
   data attribute for a variable or a parameter.  We generate the
   AT_const_value attribute only in those cases where the given
   variable or parameter does not have a true "location" either in
   memory or in a register.  This can happen (for example) when a
   constant is passed as an actual argument in a call to an inline
   function.  (It's possible that these things can crop up in other
   ways also.)  Note that one type of constant value which can be
   passed into an inlined function is a constant pointer.  This can
   happen for example if an actual argument in an inlined function
   call evaluates to a compile-time constant address.  */

static void
location_or_const_value_attribute (decl)
     register tree decl;
{
  register rtx rtl;

  if (TREE_CODE (decl) == ERROR_MARK)
    return;

  if ((TREE_CODE (decl) != VAR_DECL) && (TREE_CODE (decl) != PARM_DECL))
    abort ();

  /* It's not really clear what existing Dwarf debuggers need or expect
     as regards to location information for formal parameters.  A later
     version of the Dwarf specification should resolve such issues, but
     for the time being, we assume here that debuggers want information
     about the location where the parameter was passed into the function.
     That seems to be what USL's CI5 compiler generates.  Note that this
     will probably be different from the place where the parameter actual
     resides during function execution.  Dwarf Version 2 will provide us
     with a means to describe that location also, but for now we can only
     describe the "passing" location.  */

#if 1 /* This is probably right, but it leads to a lot of trouble.
	 Fixing one problem has been exposing another,
	 all of which seemed to have no ill effects before.
	 Let's try it again for now.  */
  rtl = (TREE_CODE (decl) == PARM_DECL)
	 ? DECL_INCOMING_RTL (decl)
	 : DECL_RTL (decl);
#else
  rtl = DECL_RTL (decl);
#endif

  if (rtl == NULL)
    return;

  switch (GET_CODE (rtl))
    {
    case CONST_INT:
    case CONST_DOUBLE:
    case CONST_STRING:
    case SYMBOL_REF:
    case LABEL_REF:
    case CONST:
    case PLUS:	/* DECL_RTL could be (plus (reg ...) (const_int ...)) */
      const_value_attribute (rtl);
      break;

    case MEM:
    case REG:
    case SUBREG:
      location_attribute (rtl);
      break;

    default:
      abort ();		/* Should never happen.  */
    }
}

/* Generate an AT_name attribute given some string value to be included as
   the value of the attribute.	If the name is null, don't do anything.	 */

inline void
name_attribute (name_string)
     register char *name_string;
{
  if (name_string && *name_string)
    {
      ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_name);
      ASM_OUTPUT_DWARF_STRING (asm_out_file, name_string);
    }
}

inline void
fund_type_attribute (ft_code)
     register unsigned ft_code;
{
  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_fund_type);
  ASM_OUTPUT_DWARF_FUND_TYPE (asm_out_file, ft_code);
}

static void
mod_fund_type_attribute (type, decl_const, decl_volatile)
     register tree type;
     register int decl_const;
     register int decl_volatile;
{
  char begin_label[MAX_ARTIFICIAL_LABEL_BYTES];
  char end_label[MAX_ARTIFICIAL_LABEL_BYTES];

  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_mod_fund_type);
  sprintf (begin_label, MT_BEGIN_LABEL_FMT, current_dienum);
  sprintf (end_label, MT_END_LABEL_FMT, current_dienum);
  ASM_OUTPUT_DWARF_DELTA2 (asm_out_file, end_label, begin_label);
  ASM_OUTPUT_LABEL (asm_out_file, begin_label);
  write_modifier_bytes (type, decl_const, decl_volatile);
  ASM_OUTPUT_DWARF_FUND_TYPE (asm_out_file,
			      fundamental_type_code (root_type (type)));
  ASM_OUTPUT_LABEL (asm_out_file, end_label);
}

inline void
user_def_type_attribute (type)
     register tree type;
{
  char ud_type_name[MAX_ARTIFICIAL_LABEL_BYTES];

  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_user_def_type);
  sprintf (ud_type_name, TYPE_NAME_FMT, TYPE_UID (type));
  ASM_OUTPUT_DWARF_REF (asm_out_file, ud_type_name);
}

static void
mod_u_d_type_attribute (type, decl_const, decl_volatile)
     register tree type;
     register int decl_const;
     register int decl_volatile;
{
  char begin_label[MAX_ARTIFICIAL_LABEL_BYTES];
  char end_label[MAX_ARTIFICIAL_LABEL_BYTES];
  char ud_type_name[MAX_ARTIFICIAL_LABEL_BYTES];

  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_mod_u_d_type);
  sprintf (begin_label, MT_BEGIN_LABEL_FMT, current_dienum);
  sprintf (end_label, MT_END_LABEL_FMT, current_dienum);
  ASM_OUTPUT_DWARF_DELTA2 (asm_out_file, end_label, begin_label);
  ASM_OUTPUT_LABEL (asm_out_file, begin_label);
  write_modifier_bytes (type, decl_const, decl_volatile);
  sprintf (ud_type_name, TYPE_NAME_FMT, TYPE_UID (root_type (type)));
  ASM_OUTPUT_DWARF_REF (asm_out_file, ud_type_name);
  ASM_OUTPUT_LABEL (asm_out_file, end_label);
}

inline void
ordering_attribute (ordering)
     register unsigned ordering;
{
  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_ordering);
  ASM_OUTPUT_DWARF_DATA2 (asm_out_file, ordering);
}

/* Note that the block of subscript information for an array type also
   includes information about the element type of type given array type.  */

static void
subscript_data_attribute (type)
     register tree type;
{
  register unsigned dimension_number;
  char begin_label[MAX_ARTIFICIAL_LABEL_BYTES];
  char end_label[MAX_ARTIFICIAL_LABEL_BYTES];

  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_subscr_data);
  sprintf (begin_label, SS_BEGIN_LABEL_FMT, current_dienum);
  sprintf (end_label, SS_END_LABEL_FMT, current_dienum);
  ASM_OUTPUT_DWARF_DELTA2 (asm_out_file, end_label, begin_label);
  ASM_OUTPUT_LABEL (asm_out_file, begin_label);

  /* The GNU compilers represent multidimensional array types as sequences
     of one dimensional array types whose element types are themselves array
     types.  Here we squish that down, so that each multidimensional array
     type gets only one array_type DIE in the Dwarf debugging info.  The
     draft Dwarf specification say that we are allowed to do this kind
     of compression in C (because there is no difference between an
     array or arrays and a multidimensional array in C) but for other
     source languages (e.g. Ada) we probably shouldn't do this.  */

  for (dimension_number = 0;
	TREE_CODE (type) == ARRAY_TYPE;
	type = TREE_TYPE (type), dimension_number++)
    {
      register tree domain = TYPE_DOMAIN (type);

      /* Arrays come in three flavors.	Unspecified bounds, fixed
	 bounds, and (in GNU C only) variable bounds.  Handle all
	 three forms here.  */

      if (domain)
	{
	  /* We have an array type with specified bounds.  */

	  register tree lower = TYPE_MIN_VALUE (domain);
	  register tree upper = TYPE_MAX_VALUE (domain);

	  /* Handle only fundamental types as index types for now.  */

	  if (! type_is_fundamental (domain))
	    abort ();

	  /* Output the representation format byte for this dimension. */

	  ASM_OUTPUT_DWARF_FMT_BYTE (asm_out_file,
				  FMT_CODE (1,
					    TREE_CODE (lower) == INTEGER_CST,
					    TREE_CODE (upper) == INTEGER_CST));

	  /* Output the index type for this dimension.	*/

	  ASM_OUTPUT_DWARF_FUND_TYPE (asm_out_file,
				      fundamental_type_code (domain));

	  /* Output the representation for the lower bound.  */

	  output_bound_representation (lower, dimension_number, 'l');

	  /* Output the representation for the upper bound.  */

	  output_bound_representation (upper, dimension_number, 'u');
	}
      else
	{
	  /* We have an array type with an unspecified length.	For C and
	     C++ we can assume that this really means that (a) the index
	     type is an integral type, and (b) the lower bound is zero.
	     Note that Dwarf defines the representation of an unspecified
	     (upper) bound as being a zero-length location description.	 */

	  /* Output the array-bounds format byte.  */

	  ASM_OUTPUT_DWARF_FMT_BYTE (asm_out_file, FMT_FT_C_X);

	  /* Output the (assumed) index type.  */

	  ASM_OUTPUT_DWARF_FUND_TYPE (asm_out_file, FT_integer);

	  /* Output the (assumed) lower bound (constant) value.	 */

	  ASM_OUTPUT_DWARF_DATA4 (asm_out_file, 0);

	  /* Output the (empty) location description for the upper bound.  */

	  ASM_OUTPUT_DWARF_DATA2 (asm_out_file, 0);
	}
    }

  /* Output the prefix byte that says that the element type is comming up.  */

  ASM_OUTPUT_DWARF_FMT_BYTE (asm_out_file, FMT_ET);

  /* Output a representation of the type of the elements of this array type.  */

  type_attribute (type, 0, 0);

  ASM_OUTPUT_LABEL (asm_out_file, end_label);
}

static void
byte_size_attribute (tree_node)
     register tree tree_node;
{
  register unsigned size;

  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_byte_size);
  switch (TREE_CODE (tree_node))
    {
      case ERROR_MARK:
	size = 0;
	break;

      case ENUMERAL_TYPE:
      case RECORD_TYPE:
      case UNION_TYPE:
	size = int_size_in_bytes (tree_node);
	break;

      case FIELD_DECL:
	{
	  register unsigned words;
	  register unsigned bits;

	  bits = TREE_INT_CST_LOW (DECL_SIZE (tree_node));
	  words = (bits + (BITS_PER_WORD-1)) / BITS_PER_WORD;
	  size = words * (BITS_PER_WORD / BITS_PER_UNIT);
	}
	break;

      default:
	abort ();
    }
  ASM_OUTPUT_DWARF_DATA4 (asm_out_file, size);
}

/* For a FIELD_DECL node which represents a bit field, output an attribute
   which specifies the distance in bits from the start of the *word*
   containing the given field to the first bit of the field.  */

inline void
bit_offset_attribute (decl)
    register tree decl;
{
  assert (TREE_CODE (decl) == FIELD_DECL);	/* Must be a field.  */
  assert (DECL_BIT_FIELD_TYPE (decl));		/* Must be a bit field.	 */

  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_bit_offset);
  ASM_OUTPUT_DWARF_DATA2 (asm_out_file,
	BITFIELD_OFFSET_BITS (decl) % (unsigned) BITS_PER_WORD);
}

/* For a FIELD_DECL node which represents a bit field, output an attribute
   which specifies the length in bits of the given field.  */

inline void
bit_size_attribute (decl)
    register tree decl;
{
  assert (TREE_CODE (decl) == FIELD_DECL);	/* Must be a field.  */
  assert (DECL_BIT_FIELD_TYPE (decl));		/* Must be a bit field.	 */

  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_bit_size);
  ASM_OUTPUT_DWARF_DATA4 (asm_out_file,
			  (unsigned) TREE_INT_CST_LOW (DECL_SIZE (decl)));
}

/* The following routine outputs the `element_list' attribute for enumeration
   type DIEs.  The element_lits attribute includes the names and values of
   all of the enumeration constants associated with the given enumeration
   type.  */

inline void
element_list_attribute (element)
     register tree element;
{
  char begin_label[MAX_ARTIFICIAL_LABEL_BYTES];
  char end_label[MAX_ARTIFICIAL_LABEL_BYTES];

  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_element_list);
  sprintf (begin_label, EE_BEGIN_LABEL_FMT, current_dienum);
  sprintf (end_label, EE_END_LABEL_FMT, current_dienum);
  ASM_OUTPUT_DWARF_DELTA4 (asm_out_file, end_label, begin_label);
  ASM_OUTPUT_LABEL (asm_out_file, begin_label);

  /* Here we output a list of value/name pairs for each enumeration constant
     defined for this enumeration type (as required), but we do it in REVERSE
     order.  The order is the one required by the draft #5 Dwarf specification
     published by the UI/PLSIG.  */

  output_enumeral_list (element);   /* Recursively output the whole list.  */

  ASM_OUTPUT_LABEL (asm_out_file, end_label);
}

/* Generate an AT_stmt_list attribute.	These are normally present only in
   DIEs with a TAG_compile_unit tag.  */

inline void
stmt_list_attribute (label)
    register char *label;
{
  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_stmt_list);
  /* Don't use ASM_OUTPUT_DWARF_DATA4 here.  */
  ASM_OUTPUT_DWARF_ADDR (asm_out_file, label);
}

/* Generate an AT_low_pc attribute for a label DIE, a lexical_block DIE or
   for a subroutine DIE.  */

inline void
low_pc_attribute (asm_low_label)
     register char *asm_low_label;
{
  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_low_pc);
  ASM_OUTPUT_DWARF_ADDR (asm_out_file, asm_low_label);
}

/* Generate an AT_high_pc attribute for a lexical_block DIE or for a
   subroutine DIE.  */

inline void
high_pc_attribute (asm_high_label)
    register char *asm_high_label;
{
  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_high_pc);
  ASM_OUTPUT_DWARF_ADDR (asm_out_file, asm_high_label);
}

/* Generate an AT_language attribute given a LANG value.  These attributes
   are used only within TAG_compile_unit DIEs.  */

inline void
language_attribute (language_code)
     register unsigned language_code;
{
  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_language);
  ASM_OUTPUT_DWARF_DATA4 (asm_out_file, language_code);
}

inline void
member_attribute (context)
    register tree context;
{
  char label[MAX_ARTIFICIAL_LABEL_BYTES];

  /* Generate this attribute only for members in C++.  */

  if (context != NULL
      && (TREE_CODE (context) == RECORD_TYPE
	  || TREE_CODE (context) == UNION_TYPE))
    {
      ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_member);
      sprintf (label, TYPE_NAME_FMT, TYPE_UID (context));
      ASM_OUTPUT_DWARF_REF (asm_out_file, label);
    }
}

inline void
string_length_attribute (upper_bound)
     register tree upper_bound;
{
  char begin_label[MAX_ARTIFICIAL_LABEL_BYTES];
  char end_label[MAX_ARTIFICIAL_LABEL_BYTES];

  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_string_length);
  sprintf (begin_label, SL_BEGIN_LABEL_FMT, current_dienum);
  sprintf (end_label, SL_END_LABEL_FMT, current_dienum);
  ASM_OUTPUT_DWARF_DELTA2 (asm_out_file, end_label, begin_label);
  ASM_OUTPUT_LABEL (asm_out_file, begin_label);
  output_bound_representation (upper_bound, 0, 'u');
  ASM_OUTPUT_LABEL (asm_out_file, end_label);
}

inline void
comp_dir_attribute (dirname)
     register char *dirname;
{
  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_comp_dir);
  ASM_OUTPUT_DWARF_STRING (asm_out_file, dirname);
}

inline void
sf_names_attribute (sf_names_start_label)
     register char *sf_names_start_label;
{
  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_sf_names);
  /* Don't use ASM_OUTPUT_DWARF_DATA4 here.  */
  ASM_OUTPUT_DWARF_ADDR (asm_out_file, sf_names_start_label);
}

inline void
src_info_attribute (src_info_start_label)
     register char *src_info_start_label;
{
  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_src_info);
  /* Don't use ASM_OUTPUT_DWARF_DATA4 here.  */
  ASM_OUTPUT_DWARF_ADDR (asm_out_file, src_info_start_label);
}

inline void
mac_info_attribute (mac_info_start_label)
     register char *mac_info_start_label;
{
  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_mac_info);
  /* Don't use ASM_OUTPUT_DWARF_DATA4 here.  */
  ASM_OUTPUT_DWARF_ADDR (asm_out_file, mac_info_start_label);
}

inline void
prototyped_attribute (func_type)
     register tree func_type;
{
  if ((strcmp (language_string, "GNU C") == 0)
      && (TYPE_ARG_TYPES (func_type) != NULL))
    {
      ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_prototyped);
      ASM_OUTPUT_DWARF_STRING (asm_out_file, "");
    }
}

inline void
producer_attribute (producer)
     register char *producer;
{
  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_producer);
  ASM_OUTPUT_DWARF_STRING (asm_out_file, producer);
}

inline void
inline_attribute (decl)
     register tree decl;
{
  if (TREE_INLINE (decl))
    {
      ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_inline);
      ASM_OUTPUT_DWARF_STRING (asm_out_file, "");
    }
}

inline void
containing_type_attribute (containing_type)
     register tree containing_type;
{
  char label[MAX_ARTIFICIAL_LABEL_BYTES];

  ASM_OUTPUT_DWARF_ATTRIBUTE (asm_out_file, AT_containing_type);
  sprintf (label, TYPE_NAME_FMT, TYPE_UID (containing_type));
  ASM_OUTPUT_DWARF_REF (asm_out_file, label);
}

/************************* end of attributes *****************************/

/********************* utility routines for DIEs *************************/

/* Many forms of DIEs contain a "type description" part.  The following
   routine writes out these "type descriptor" parts.  */

static void
type_attribute (type, decl_const, decl_volatile)
     register tree type;
     register int decl_const;
     register int decl_volatile;
{
  register enum tree_code code = TREE_CODE (type);
  register int root_type_modified;

  if (TREE_CODE (type) == ERROR_MARK)
    return;

  /* Handle a special case.  For functions whose return type is void,
     we generate *no* type attribute.  (Note that no object may have
     type `void', so this only applies to function return types.  */

  if (TREE_CODE (type) == VOID_TYPE)
    return;

  root_type_modified = (code == POINTER_TYPE || code == REFERENCE_TYPE
			|| decl_const || decl_volatile
			|| TYPE_READONLY (type) || TYPE_VOLATILE (type));

  if (type_is_fundamental (root_type (type)))
    if (root_type_modified)
	mod_fund_type_attribute (type, decl_const, decl_volatile);
    else
	fund_type_attribute (fundamental_type_code (type));
  else
    if (root_type_modified)
	mod_u_d_type_attribute (type, decl_const, decl_volatile);
    else
	user_def_type_attribute (type);
}

/* Given a tree pointer to a struct, class, union, or enum type node, return
   a pointer to the (string) tag name for the given type, or zero if the
   type was declared without a tag.  */

static char *
type_tag (type)
     register tree type;
{
  register char *name = 0;

  if (TYPE_NAME (type) != 0)
    {
      register tree t = 0;

      /* Find the IDENTIFIER_NODE for the type name.  */
      if (TREE_CODE (TYPE_NAME (type)) == IDENTIFIER_NODE)
	t = TYPE_NAME (type);
#if 0
      /* The g++ front end makes the TYPE_NAME of *each* tagged type point
	 to a TYPE_DECL node, regardless of whether or not a `typedef' was
	 involved.  This is distinctly different from what the gcc front-end
	 does.  It always makes the TYPE_NAME for each tagged type be either
	 NULL (signifying an anonymous tagged type) or else a pointer to an
	 IDENTIFIER_NODE.  Obviously, we would like to generate correct Dwarf
	 for both C and C++, but given this inconsistancy in the TREE
	 representation of tagged types for C and C++ in the GNU front-ends,
	 we cannot support both languages correctly unless we introduce some
	 front-end specific code here, and rms objects to that, so we can
	 only generate correct Dwarf for one of these two languages.  C is
	 more important, so for now we'll do the right thing for C and let
	 g++ go fish.  */

      else
	if (TREE_CODE (TYPE_NAME (type)) == TYPE_DECL)
	  t = DECL_NAME (TYPE_NAME (type));
#endif
      /* Now get the name as a string, or invent one.  */
      if (t != 0)
	name = IDENTIFIER_POINTER (t);
    }

  return (name == 0 || *name == '\0') ? 0 : name;
}

inline void
dienum_push ()
{
  /* Start by checking if the pending_sibling_stack needs to be expanded.
     If necessary, expand it.  */

  if (pending_siblings == pending_siblings_allocated)
    {
      pending_siblings_allocated += PENDING_SIBLINGS_INCREMENT;
      pending_sibling_stack
	= (unsigned *) xrealloc (pending_sibling_stack,
				 pending_siblings_allocated * sizeof(unsigned));
    }

  pending_siblings++;
  NEXT_DIE_NUM = next_unused_dienum++;
}

/* Pop the sibling stack so that the most recently pushed DIEnum becomes the
   NEXT_DIE_NUM.  */

inline void
dienum_pop ()
{
  pending_siblings--;
}

inline tree
member_declared_type (member)
     register tree member;
{
  return (DECL_BIT_FIELD_TYPE (member))
	   ? DECL_BIT_FIELD_TYPE (member)
	   : TREE_TYPE (member);
}

/******************************* DIEs ************************************/

/* Output routines for individual types of DIEs.  */

/* Note that every type of DIE (except a null DIE) gets a sibling.  */

static void
output_array_type_die (arg)
     register void *arg;
{
  register tree type = arg;

  ASM_OUTPUT_DWARF_TAG (asm_out_file, TAG_array_type);
  sibling_attribute ();
  equate_type_number_to_die_number (type);
  member_attribute (TYPE_CONTEXT (type));

  /* I believe that we can default the array ordering.  SDB will probably
     do the right things even if AT_ordering is not present.  It's not
     even an issue until we start to get into multidimensional arrays
     anyway.  If SDB is shown to do the wrong thing in those cases, then
     we'll have to put the AT_ordering attribute back in, but only for
     multidimensional array.  (After all, we don't want to waste space
     in the .debug section now do we?)  */

#if 0
  ordering_attribute (ORD_row_major);
#endif

  subscript_data_attribute (type);
}

static void
output_set_type_die (arg)
     register void *arg;
{
  register tree type = arg;

  ASM_OUTPUT_DWARF_TAG (asm_out_file, TAG_set_type);
  sibling_attribute ();
  equate_type_number_to_die_number (type);
  member_attribute (TYPE_CONTEXT (type));
  type_attribute (TREE_TYPE (type), 0, 0);
}

#if 0
/* Implement this when there is a GNU FORTRAN or GNU Ada front end.  */
static void
output_entry_point_die (arg)
     register void *arg;
{
  register tree decl = arg;
  register tree type = TREE_TYPE (decl);
  register tree return_type = TREE_TYPE (type);

  ASM_OUTPUT_DWARF_TAG (asm_out_file, TAG_entry_point);
  sibling_attribute ();
  dienum_push ();
  if (DECL_NAME (decl))
    name_attribute (IDENTIFIER_POINTER (DECL_NAME (decl)));
  member_attribute (DECL_CONTEXT (decl));
  type_attribute (return_type, 0, 0);
}
#endif

/* Output a DIE to represent an enumeration type.  Note that these DIEs
   include all of the information about the enumeration values also.
   This information is encoded into the element_list attribute.	 */

static void
output_enumeration_type_die (arg)
     register void *arg;
{
  register tree type = arg;

  ASM_OUTPUT_DWARF_TAG (asm_out_file, TAG_enumeration_type);
  sibling_attribute ();
  equate_type_number_to_die_number (type);
  name_attribute (type_tag (type));
  member_attribute (TYPE_CONTEXT (type));

  /* Handle a GNU C/C++ extension, i.e. incomplete enum types.  If the
     given enum type is incomplete, do not generate the AT_byte_size
     attribute or the AT_element_list attribute.  */

  if (TYPE_SIZE (type))
    {
      byte_size_attribute (type);
      element_list_attribute (TYPE_FIELDS (type));
    }
}

/* Output a DIE to represent either a real live formal parameter decl or
   to represent just the type of some formal parameter position in some
   function type.

   Note that this routine is a bit unusual because its argument may be
   either a PARM_DECL node or else some sort of a ..._TYPE node.  If it's
   the formar then this function is being called to output a real live
   formal parameter declaration.  If it's the latter, then this function
   is only being called to output a TAG_formal_parameter DIE to stand as
   a placeholder for some formal argument type of some subprogram type.  */

static void
output_formal_parameter_die (arg)
     register void *arg;
{
  register tree decl = arg;
  register tree type;

  if (TREE_CODE (decl) == PARM_DECL)
    type = TREE_TYPE (decl);
  else
    {
      type = decl;	/* we were called with a type, not a decl */
      decl = NULL;
    }

  ASM_OUTPUT_DWARF_TAG (asm_out_file, TAG_formal_parameter);
  sibling_attribute ();
  if (decl)
    {
      if (DECL_NAME (decl))
        name_attribute (IDENTIFIER_POINTER (DECL_NAME (decl)));
      type_attribute (type, TREE_READONLY (decl), TREE_THIS_VOLATILE (decl));
      location_or_const_value_attribute (decl);
    }
  else
    type_attribute (type, 0, 0);
}

/* Output a DIE to represent a declared function (either file-scope
   or block-local) which has "external linkage" (according to ANSI-C).  */

static void
output_global_subroutine_die (arg)
     register void *arg;
{
  register tree decl = arg;
  register tree type = TREE_TYPE (decl);
  register tree return_type = TREE_TYPE (type);

  ASM_OUTPUT_DWARF_TAG (asm_out_file, TAG_global_subroutine);
  sibling_attribute ();
  dienum_push ();
  if (DECL_NAME (decl))
    name_attribute (IDENTIFIER_POINTER (DECL_NAME (decl)));
  inline_attribute (decl);
  prototyped_attribute (type);
  member_attribute (DECL_CONTEXT (decl));
  type_attribute (return_type, 0, 0);
  if (!TREE_EXTERNAL (decl))
    {
      char func_end_label[MAX_ARTIFICIAL_LABEL_BYTES];

      low_pc_attribute (IDENTIFIER_POINTER (DECL_NAME (decl)));
      sprintf (func_end_label, FUNC_END_LABEL_FMT, current_funcdef_number);
      high_pc_attribute (func_end_label);
    }
}

/* Output a DIE to represent a declared data object (either file-scope
   or block-local) which has "external linkage" (according to ANSI-C).  */

static void
output_global_variable_die (arg)
     register void *arg;
{
  register tree decl = arg;
  register tree type = TREE_TYPE (decl);

  ASM_OUTPUT_DWARF_TAG (asm_out_file, TAG_global_variable);
  sibling_attribute ();
  if (DECL_NAME (decl))
    name_attribute (IDENTIFIER_POINTER (DECL_NAME (decl)));
  member_attribute (DECL_CONTEXT (decl));
  type_attribute (type, TREE_READONLY (decl), TREE_THIS_VOLATILE (decl));
  if (!TREE_EXTERNAL (decl))
    location_or_const_value_attribute (decl);
}

#if 0
/* TAG_inline_subroutine has been retired by the UI/PLSIG.  We're
   now supposed to use either TAG_subroutine or TAG_global_subroutine
   (depending on whether or not the function in question has internal
   or external linkage) and we're supposed to just put in an AT_inline
   attribute.  */
static void
output_inline_subroutine_die (arg)
     register void *arg;
{
  register tree decl = arg;
  register tree type = TREE_TYPE (decl);
  register tree return_type = TREE_TYPE (type);

  ASM_OUTPUT_DWARF_TAG (asm_out_file, TAG_inline_subroutine);
  sibling_attribute ();
  dienum_push ();
  if (DECL_NAME (decl))
    name_attribute (IDENTIFIER_POINTER (DECL_NAME (decl)));
  prototyped_attribute (type);
  member_attribute (DECL_CONTEXT (decl));
  type_attribute (return_type, 0, 0);

  /* Note:  For each inline function which gets an out-of-line body
     generated for it, we want to generate AT_low_pc and AT_high_pc
     attributes here for the function's out-of-line body.

     Unfortunately, the decision as to whether or not to generate an
     out-of-line body for any given inline function may not be made
     until we reach the end of the containing scope for the given
     inline function (because only then will it be known if the
     function was ever even called).

     For this reason, the output of DIEs representing file-scope inline
     functions gets delayed until a special post-pass which happens only
     after we have reached the end of the compilation unit.  Because of
     this mechanism, we can always be sure (by the time we reach here)
     that TREE_ASM_WRITTEN(decl) will correctly indicate whether or not
     there was an out-of-line body generated for this inline function.
  */

  if (!TREE_EXTERNAL (decl))
    {
      if (TREE_ASM_WRITTEN (decl))
        {
          char func_end_label[MAX_ARTIFICIAL_LABEL_BYTES];

          low_pc_attribute (IDENTIFIER_POINTER (DECL_NAME (decl)));
          sprintf (func_end_label, FUNC_END_LABEL_FMT, current_funcdef_number);
          high_pc_attribute (func_end_label);
        }
    }
}
#endif

static void
output_label_die (arg)
     register void *arg;
{
  register tree decl = arg;
  register rtx insn = DECL_RTL (decl);

  ASM_OUTPUT_DWARF_TAG (asm_out_file, TAG_label);
  sibling_attribute ();
  name_attribute (IDENTIFIER_POINTER (DECL_NAME (decl)));

  /* When optimization is enabled (with -O) the code in jump.c and in flow.c
     may cause insns representing one of more of the user's own labels to
     be deleted.  This happens whenever it is determined that a given label
     is unreachable.

     In such cases, we here generate an abbreviated form of a label DIE.
     This abbreviated version does *not* have a low_pc attribute.  This
     should signify to the debugger that the label has been optimized away.

     Note that a CODE_LABEL can get deleted either by begin converted into
     a NOTE_INSN_DELETED note, or by simply having its INSN_DELETED_P flag
     set to true.  We handle both cases here.
  */

  if (GET_CODE (insn) == CODE_LABEL && ! INSN_DELETED_P (insn))
    {
      char label[MAX_ARTIFICIAL_LABEL_BYTES];

      sprintf (label, INSN_LABEL_FMT, current_funcdef_number,
				      (unsigned) INSN_UID (insn));
      low_pc_attribute (label);
    }
}

static void
output_lexical_block_die (arg)
     register void *arg;
{
  register tree stmt = arg;
  char begin_label[MAX_ARTIFICIAL_LABEL_BYTES];
  char end_label[MAX_ARTIFICIAL_LABEL_BYTES];

  ASM_OUTPUT_DWARF_TAG (asm_out_file, TAG_lexical_block);
  sibling_attribute ();
  dienum_push ();
  sprintf (begin_label, BLOCK_BEGIN_LABEL_FMT, next_block_number);
  low_pc_attribute (begin_label);
  sprintf (end_label, BLOCK_END_LABEL_FMT, next_block_number);
  high_pc_attribute (end_label);
}

static void
output_inlined_subroutine_die (arg)
     register void *arg;
{
  register tree stmt = arg;
  char begin_label[MAX_ARTIFICIAL_LABEL_BYTES];
  char end_label[MAX_ARTIFICIAL_LABEL_BYTES];

  ASM_OUTPUT_DWARF_TAG (asm_out_file, TAG_inlined_subroutine);
  sibling_attribute ();
  dienum_push ();
  sprintf (begin_label, BLOCK_BEGIN_LABEL_FMT, next_block_number);
  low_pc_attribute (begin_label);
  sprintf (end_label, BLOCK_END_LABEL_FMT, next_block_number);
  high_pc_attribute (end_label);
}

/* Output a DIE to represent a declared data object (either file-scope
   or block-local) which has "internal linkage" (according to ANSI-C).  */

static void
output_local_variable_die (arg)
     register void *arg;
{
  register tree decl = arg;
  register tree type = TREE_TYPE (decl);

  ASM_OUTPUT_DWARF_TAG (asm_out_file, TAG_local_variable);
  sibling_attribute ();
  if (DECL_NAME (decl))
    name_attribute (IDENTIFIER_POINTER (DECL_NAME (decl)));
  member_attribute (DECL_CONTEXT (decl));
  type_attribute (type, TREE_READONLY (decl), TREE_THIS_VOLATILE (decl));
  location_or_const_value_attribute (decl);
}

static void
output_member_die (arg)
     register void *arg;
{
  register tree decl = arg;

  ASM_OUTPUT_DWARF_TAG (asm_out_file, TAG_member);
  sibling_attribute ();
  if (DECL_NAME (decl))
    name_attribute (IDENTIFIER_POINTER (DECL_NAME (decl)));
  member_attribute (DECL_CONTEXT (decl));
  type_attribute (member_declared_type (decl),
		  TREE_READONLY (decl), TREE_THIS_VOLATILE (decl));
  if (DECL_BIT_FIELD_TYPE (decl))	/* If this is a bit field... */
    {
      byte_size_attribute (decl);
      bit_size_attribute (decl);
      bit_offset_attribute (decl);
    }
  data_member_location_attribute (decl);
}

#if 0
/* Don't generate either pointer_type DIEs or reference_type DIEs.  According
   to the 4-4-90 Dwarf draft spec (just after requirement #47):

	These two type entries are not currently generated by any compiler.
	Since the only way to name a pointer (or reference) type is C or C++
	is via a "typedef", an entry with the "typedef" tag is generated
	instead.

   We keep this code here just in case these types of DIEs may be needed
   to represent certain things in other languages (e.g. Pascal) someday.
*/

static void
output_pointer_type_die (arg)
     register void *arg;
{
  register tree type = arg;

  ASM_OUTPUT_DWARF_TAG (asm_out_file, TAG_pointer_type);
  sibling_attribute ();
  equate_type_number_to_die_number (type);
  member_attribute (TYPE_CONTEXT (type));
  type_attribute (TREE_TYPE (type), 0, 0);
}

static void
output_reference_type_die (arg)
     register void *arg;
{
  register tree type = arg;

  ASM_OUTPUT_DWARF_TAG (asm_out_file, TAG_reference_type);
  sibling_attribute ();
  equate_type_number_to_die_number (type);
  member_attribute (TYPE_CONTEXT (type));
  type_attribute (TREE_TYPE (type), 0, 0);
}
#endif

output_ptr_to_mbr_type_die (arg)
     register void *arg;
{
  register tree type = arg;

  ASM_OUTPUT_DWARF_TAG (asm_out_file, TAG_ptr_to_member_type);
  sibling_attribute ();
  equate_type_number_to_die_number (type);
  member_attribute (TYPE_CONTEXT (type));
  containing_type_attribute (TYPE_OFFSET_BASETYPE (type));
  type_attribute (TREE_TYPE (type), 0, 0);
}

static void
output_compile_unit_die (arg)
     register void *arg;
{
  register char *main_input_filename = arg;

  ASM_OUTPUT_DWARF_TAG (asm_out_file, TAG_compile_unit);
  sibling_attribute ();
  dienum_push ();
  name_attribute (main_input_filename);

  {
    char producer[250];

    sprintf (producer, "%s %s", language_string, version_string);
    producer_attribute (producer);
  }

  if (strcmp (language_string, "GNU C++") == 0)
    language_attribute (LANG_C_PLUS_PLUS);
  else if (flag_traditional)
    language_attribute (LANG_C);
  else
    language_attribute (LANG_C89);
  low_pc_attribute (TEXT_BEGIN_LABEL);
  high_pc_attribute (TEXT_END_LABEL);
  if (debug_info_level >= DINFO_LEVEL_NORMAL)
    stmt_list_attribute (LINE_BEGIN_LABEL);
  last_filename = xstrdup (main_input_filename);

  {
    char *wd = getpwd ();
    if (wd)
      comp_dir_attribute (wd);
  }

  if (debug_info_level >= DINFO_LEVEL_NORMAL)
    {
      sf_names_attribute (SFNAMES_BEGIN_LABEL);
      src_info_attribute (SRCINFO_BEGIN_LABEL);
      if (debug_info_level >= DINFO_LEVEL_VERBOSE)
        mac_info_attribute (MACINFO_BEGIN_LABEL);
    }
}

static void
output_string_type_die (arg)
     register void *arg;
{
  register tree type = arg;

  ASM_OUTPUT_DWARF_TAG (asm_out_file, TAG_string_type);
  sibling_attribute ();
  member_attribute (TYPE_CONTEXT (type));

  /* Fudge the string length attribute for now.  */

  string_length_attribute (
	TYPE_MAX_VALUE (TYPE_DOMAIN (type)));
}

static void
output_structure_type_die (arg)
     register void *arg;
{
  register tree type = arg;

  ASM_OUTPUT_DWARF_TAG (asm_out_file, TAG_structure_type);
  sibling_attribute ();
  equate_type_number_to_die_number (type);
  name_attribute (type_tag (type));
  member_attribute (TYPE_CONTEXT (type));

  /* If this type has been completed, then give it a byte_size attribute
     and prepare to give a list of members.  Otherwise, don't do either of
     these things.  In the latter case, we will not be generating a list
     of members (since we don't have any idea what they might be for an
     incomplete type).	*/

  if (TYPE_SIZE (type))
    {
      dienum_push ();
      byte_size_attribute (type);
    }
}

/* Output a DIE to represent a declared function (either file-scope
   or block-local) which has "internal linkage" (according to ANSI-C).  */

static void
output_local_subroutine_die (arg)
     register void *arg;
{
  register tree decl = arg;
  register tree type = TREE_TYPE (decl);
  register tree return_type = TREE_TYPE (type);
  char func_end_label[MAX_ARTIFICIAL_LABEL_BYTES];

  ASM_OUTPUT_DWARF_TAG (asm_out_file, TAG_subroutine);
  sibling_attribute ();
  dienum_push ();
  if (DECL_NAME (decl))
    name_attribute (IDENTIFIER_POINTER (DECL_NAME (decl)));
  inline_attribute (decl);
  prototyped_attribute (type);
  member_attribute (DECL_CONTEXT (decl));
  type_attribute (return_type, 0, 0);

  /* Avoid getting screwed up in cases where a function was declared static
     but where no definition was ever given for it.  */

  if (TREE_ASM_WRITTEN (decl))
    {
      low_pc_attribute (IDENTIFIER_POINTER (DECL_NAME (decl)));
      sprintf (func_end_label, FUNC_END_LABEL_FMT, current_funcdef_number);
      high_pc_attribute (func_end_label);
    }
}

static void
output_subroutine_type_die (arg)
     register void *arg;
{
  register tree type = arg;
  register tree return_type = TREE_TYPE (type);

  ASM_OUTPUT_DWARF_TAG (asm_out_file, TAG_subroutine_type);
  sibling_attribute ();
  dienum_push ();
  equate_type_number_to_die_number (type);
  prototyped_attribute (type);
  member_attribute (TYPE_CONTEXT (type));
  type_attribute (return_type, 0, 0);
}

static void
output_typedef_die (arg)
     register void *arg;
{
  register tree decl = arg;
  register tree type = TREE_TYPE (decl);

  ASM_OUTPUT_DWARF_TAG (asm_out_file, TAG_typedef);
  sibling_attribute ();
  if (DECL_NAME (decl))
    name_attribute (IDENTIFIER_POINTER (DECL_NAME (decl)));
  member_attribute (DECL_CONTEXT (decl));
  type_attribute (type, TREE_READONLY (decl), TREE_THIS_VOLATILE (decl));
}

static void
output_union_type_die (arg)
     register void *arg;
{
  register tree type = arg;

  ASM_OUTPUT_DWARF_TAG (asm_out_file, TAG_union_type);
  sibling_attribute ();
  equate_type_number_to_die_number (type);
  name_attribute (type_tag (type));
  member_attribute (TYPE_CONTEXT (type));

  /* If this type has been completed, then give it a byte_size attribute
     and prepare to give a list of members.  Otherwise, don't do either of
     these things.  In the latter case, we will not be generating a list
     of members (since we don't have any idea what they might be for an
     incomplete type).	*/

  if (TYPE_SIZE (type))
    {
      dienum_push ();
      byte_size_attribute (type);
    }
}

/* Generate a special type of DIE used as a stand-in for a trailing ellipsis
   at the end of an (ANSI prototyped) formal parameters list.  */

static void
output_unspecified_parameters_die (arg)
     register void *arg;
{
  register tree decl_or_type = arg;

  ASM_OUTPUT_DWARF_TAG (asm_out_file, TAG_unspecified_parameters);
  sibling_attribute ();

  /* This kludge is here only for the sake of being compatible with what
     the USL CI5 C compiler does.  The specification of Dwarf Version 1
     doesn't say that TAG_unspecified_parameters DIEs should contain any
     attributes other than the AT_sibling attribute, but they are certainly
     allowed to contain additional attributes, and the CI5 compiler
     generates AT_name, AT_fund_type, and AT_location attributes within
     TAG_unspecified_parameters DIEs which appear in the child lists for
     DIEs representing function definitions, so we do likewise here.  */

  if (TREE_CODE (decl_or_type) == FUNCTION_DECL && DECL_INITIAL (decl_or_type))
    {
      name_attribute ("...");
      fund_type_attribute (FT_pointer);
      /* location_attribute (?); */
    }
}

static void
output_padded_null_die (arg)
     register void *arg;
{
  ASM_OUTPUT_ALIGN (asm_out_file, 2);	/* 2**2 == 4 */
}

/*************************** end of DIEs *********************************/

/* Generate some type of DIE.  This routine generates the generic outer
   wrapper stuff which goes around all types of DIE's (regardless of their
   TAGs.  All forms of DIEs start with a DIE-specific label, followed by a
   DIE-length word, followed by the guts of the DIE itself.  After the guts
   of the DIE, there must always be a terminator label for the DIE.  */

static void
output_die (die_specific_output_function, param)
     register void (*die_specific_output_function)();
     register void *param;
{
  char begin_label[MAX_ARTIFICIAL_LABEL_BYTES];
  char end_label[MAX_ARTIFICIAL_LABEL_BYTES];

  current_dienum = NEXT_DIE_NUM;
  NEXT_DIE_NUM = next_unused_dienum;

  sprintf (begin_label, DIE_BEGIN_LABEL_FMT, current_dienum);
  sprintf (end_label, DIE_END_LABEL_FMT, current_dienum);

  /* Write a label which will act as the name for the start of this DIE.  */

  ASM_OUTPUT_LABEL (asm_out_file, begin_label);

  /* Write the DIE-length word.	 */

  ASM_OUTPUT_DWARF_DELTA4 (asm_out_file, end_label, begin_label);

  /* Fill in the guts of the DIE.  */

  next_unused_dienum++;
  die_specific_output_function (param);

  /* Write a label which will act as the name for the end of this DIE.	*/

  ASM_OUTPUT_LABEL (asm_out_file, end_label);
}

static void
end_sibling_chain ()
{
  char begin_label[MAX_ARTIFICIAL_LABEL_BYTES];

  current_dienum = NEXT_DIE_NUM;
  NEXT_DIE_NUM = next_unused_dienum;

  sprintf (begin_label, DIE_BEGIN_LABEL_FMT, current_dienum);

  /* Write a label which will act as the name for the start of this DIE.  */

  ASM_OUTPUT_LABEL (asm_out_file, begin_label);

  /* Write the DIE-length word.	 */

  ASM_OUTPUT_DWARF_DATA4 (asm_out_file, 4);

  dienum_pop ();
}

/* Generate a list of nameless TAG_formal_parameter DIEs (and perhaps a
   TAG_unspecified_parameters DIE) to represent the types of the formal
   parameters as specified in some function type specification (except
   for those which appear as part of a function *definition*).

   Note that we must be careful here to output all of the parameter DIEs
   *before* we output any DIEs needed to represent the types of the formal
   parameters.  This keeps svr4 SDB happy because it (incorrectly) thinks
   that the first non-parameter DIE it sees ends the formal parameter list.
*/

static void
output_formal_types (function_or_method_type)
     register tree function_or_method_type;
{
  register tree link;
  register tree formal_type;
  register tree first_parm_type = TYPE_ARG_TYPES (function_or_method_type);

  /* In the case where we are generating a formal types list for a C++
     non-static member function type, skip over the first thing on the
     TYPE_ARG_TYPES list because it only represents the type of the
     hidden `this pointer'.  The debugger should be able to figure
     out (without being explicitly told) that this non-static member
     function type takes a `this pointer' and should be able to figure
     what the type of that hidden parameter is from the AT_member
     attribute of the parent TAG_subroutine_type DIE.  */

  if (TREE_CODE (function_or_method_type) == METHOD_TYPE)
    first_parm_type = TREE_CHAIN (first_parm_type);

  /* Make our first pass over the list of formal parameter types and output
     a TAG_formal_parameter DIE for each one.  */

  for (link = first_parm_type; link; link = TREE_CHAIN (link))
    {
      formal_type = TREE_VALUE (link);
      if (formal_type == void_type_node)
	break;

      /* Output a (nameless) DIE to represent the formal parameter itself.  */

      output_die (output_formal_parameter_die, formal_type);
    }

  /* If this function type has an ellipsis, add a TAG_unspecified_parameters
     DIE to the end of the parameter list.  */

  if (formal_type != void_type_node)
    output_die (output_unspecified_parameters_die, function_or_method_type);

  /* Make our second (and final) pass over the list of formal parameter types
     and output DIEs to represent those types (as necessary).  */

  for (link = TYPE_ARG_TYPES (function_or_method_type);
       link;
       link = TREE_CHAIN (link))
    {
      formal_type = TREE_VALUE (link);
      if (formal_type == void_type_node)
	break;

      output_type (formal_type, function_or_method_type);
    }
}

/* Remember a type in the pending_types_list.  */

static void
pend_type (type)
     register tree type;
{
  if (pending_types == pending_types_allocated)
    {
      pending_types_allocated += PENDING_TYPES_INCREMENT;
      pending_types_list
	= (tree *) xrealloc (pending_types_list,
			     sizeof (tree) * pending_types_allocated);
    }
  pending_types_list[pending_types++] = type;

  /* Mark the pending type as having been output already (even though
     it hasn't been).  This prevents the type from being added to the
     pending_types_list more than once.  */

  TREE_ASM_WRITTEN (type) = 1;
}

/* Return non-zero if it is legitimate to output DIEs to represent a
   given type while we are generating the list of child DIEs for some
   DIE associated with a given scope.

   This function returns non-zero if *either* of the following two conditions
   is satisfied:

	 o	the type actually belongs to the given scope (as evidenced
		by its TYPE_CONTEXT value), or

	 o	the type is anonymous, and the `scope' in question is *not*
		a RECORD_TYPE or UNION_TYPE.

   In theory, we should be able to generate DIEs for anonymous types
   *anywhere* (since the scope of an anonymous type is irrelevant)
   however svr4 SDB doesn't want to see other type DIEs within the
   lists of child DIEs for a TAG_structure_type or TAG_union_type DIE.

   Note that TYPE_CONTEXT(type) may be NULL (to indicate global scope)
   or it may point to a BLOCK node (for types local to a block), or to a
   FUNCTION_DECL node (for types local to the heading of some function
   definition), or to a FUNCTION_TYPE node (for types local to the
   prototyped parameter list of a function type specification), or to a
   RECORD_TYPE or UNION_TYPE node (in the case of C++ nested types).

   The `scope' parameter should likewise be NULL or should point to a
   BLOCK node, a FUNCTION_DECL node, a FUNCTION_TYPE node, a RECORD_TYPE
   node, or a UNION_TYPE node.

   This function is used only for deciding when to "pend" and when to
   "un-pend" types to/from the pending_types_list.

   Note that we sometimes make use of this "type pending" feature in a
   rather twisted way to temporarily delay the production of DIEs for the
   types of formal parameters.  (We do this just to make svr4 SDB happy.)
   It order to delay the production of DIEs representing types of formal
   parameters, callers of this function supply `fake_containing_scope' as
   the `scope' parameter to this function.  Given that fake_containing_scope
   is *not* the containing scope for *any* other type, the desired effect
   is achieved, i.e. output of DIEs representing types is temporarily
   suspended, and any type DIEs which would have been output otherwise
   are instead placed onto the pending_types_list.  Later on, we can force
   these (temporarily pended) types to be output simply by calling
   `output_pending_types_for_scope' with an actual argument equal to the
   true scope of the types we temporarily pended.
*/

static int
type_ok_for_scope (type, scope)
    register tree type;
    register tree scope;
{
  return (TYPE_CONTEXT (type) == scope
	  || (TYPE_NAME (type) == NULL
	      && TREE_CODE (scope) != RECORD_TYPE
	      && TREE_CODE (scope) != UNION_TYPE));
}

/* Output any pending types (from the pending_types list) which we can output
   now (given the limitations of the scope that we are working on now).

   For each type output, remove the given type from the pending_types_list
   *before* we try to output it.

   Note that we have to process the list in beginning-to-end order,
   because the call made here to output_type may cause yet more types
   to be added to the end of the list, and we may have to output some
   of them too.
*/

static void
output_pending_types_for_scope (containing_scope)
     register tree containing_scope;
{
  register unsigned i;

  for (i = 0; i < pending_types; )
    {
      register tree type = pending_types_list[i];

      if (type_ok_for_scope (type, containing_scope))
	{
	  register tree *mover;
	  register tree *limit;

	  pending_types--;
	  limit = &pending_types_list[pending_types];
	  for (mover = &pending_types_list[i]; mover < limit; mover++)
	    *mover = *(mover+1);

	  /* Un-mark the type as having been output already (because it
	     hasn't been, really).  Then call output_type to generate a
	     Dwarf representation of it.  */

	  TREE_ASM_WRITTEN (type) = 0;
	  output_type (type, containing_scope);

	  /* Don't increment the loop counter in this case because we
	     have shifted all of the subsequent pending types down one
	     element in the pending_types_list array.  */
	}
      else
	i++;
    }
}

static void
output_type (type, containing_scope)
     register tree type;
     register tree containing_scope;
{
  if (type == 0 || type == error_mark_node)
    return;

  /* We are going to output a DIE to represent the unqualified version of
     of this type (i.e. without any const or volatile qualifiers) so get
     the main variant (i.e. the unqualified version) of this type now.  */

  type = TYPE_MAIN_VARIANT (type);

  if (TREE_ASM_WRITTEN (type))
    return;

  /* Don't generate any DIEs for this type now unless it is OK to do so
     (based upon what `type_ok_for_scope' tells us).  */

  if (! type_ok_for_scope (type, containing_scope))
    {
      pend_type (type);
      return;
    }

  switch (TREE_CODE (type))
    {
      case ERROR_MARK:
	break;

      case POINTER_TYPE:
      case REFERENCE_TYPE:
	/* For these types, all that is required is that we output a DIE
	   (or a set of DIEs) to represent that "basis" type.  */
	output_type (TREE_TYPE (type), containing_scope);
	break;

      case OFFSET_TYPE:
	/* This code is used for C++ pointer-to-data-member types.  */
	/* Output a description of the relevant class type.  */
	output_type (TYPE_OFFSET_BASETYPE (type), containing_scope);
	/* Output a description of the type of the object pointed to.  */
	output_type (TREE_TYPE (type), containing_scope);
	/* Now output a DIE to represent this pointer-to-data-member type
	   itself.  */
	output_die (output_ptr_to_mbr_type_die, type);
	break;

      case SET_TYPE:
	output_type (TREE_TYPE (type), containing_scope);
	output_die (output_set_type_die, type);
	break;

      case FILE_TYPE:
	output_type (TREE_TYPE (type), containing_scope);
	abort ();	/* No way to reprsent these in Dwarf yet!  */
	break;

      case STRING_TYPE:
	output_type (TREE_TYPE (type), containing_scope);
	output_die (output_string_type_die, type);
	break;

      case FUNCTION_TYPE:
	/* Force out return type (in case it wasn't forced out already).  */
	output_type (TREE_TYPE (type), containing_scope);
	output_die (output_subroutine_type_die, type);
	output_formal_types (type);
	end_sibling_chain ();
	break;

      case METHOD_TYPE:
	/* Force out return type (in case it wasn't forced out already).  */
	output_type (TREE_TYPE (type), containing_scope);
	output_die (output_subroutine_type_die, type);
	output_formal_types (type);
	end_sibling_chain ();
	break;

      case ARRAY_TYPE:
	{
	  register tree element_type;

	  element_type = TREE_TYPE (type);
	  while (TREE_CODE (element_type) == ARRAY_TYPE)
	    element_type = TREE_TYPE (element_type);

	  output_type (element_type, containing_scope);
	  output_die (output_array_type_die, type);
	}
	break;

      case ENUMERAL_TYPE:
      case RECORD_TYPE:
      case UNION_TYPE:

	/* For a non-file-scope tagged type, we can always go ahead and
	   output a Dwarf description of this type right now, even if
	   the type in question is still incomplete, because if this
	   local type *was* ever completed anywhere within its scope,
	   that complete definition would already have been attached to
	   this RECORD_TYPE, UNION_TYPE or ENUMERAL_TYPE node by the
	   time we reach this point.  That's true because of the way the
	   front-end does its processing of file-scope declarations (of
	   functions and class types) within which other types might be
	   nested.  The C and C++ front-ends always gobble up such "local
	   scope" things en-mass before they try to output *any* debugging
	   information for any of the stuff contained inside them and thus,
	   we get the benefit here of what is (in effect) a pre-resolution
	   of forward references to tagged types in local scopes.

	   Note however that for file-scope tagged types we cannot assume
	   that such pre-resolution of forward references has taken place.
	   A given file-scope tagged type may appear to be incomplete when
	   we reach this point, but it may yet be given a full definition
	   (at file-scope) later on during compilation.  In order to avoid
	   generating a premature (and possibly incorrect) set of Dwarf
	   DIEs for such (as yet incomplete) file-scope tagged types, we
	   generate nothing at all for as-yet incomplete file-scope tagged
	   types here unless we are making our special "finalization" pass
	   for file-scope things at the very end of compilation.  At that
	   time, we will certainly know as much about each file-scope tagged
	   type as we are ever going to know, so at that point in time, we
	   can safely generate correct Dwarf descriptions for these file-
	   scope tagged types.
	*/

	if (TYPE_SIZE (type) == 0 && TYPE_CONTEXT (type) == NULL && !finalizing)
	  return;	/* EARLY EXIT!  Avoid setting TREE_ASM_WRITTEN.  */

	/* Prevent infinite recursion in cases where the type of some
	   member of this type is expressed in terms of this type itself.  */

	TREE_ASM_WRITTEN (type) = 1;

	/* Output a DIE to represent the tagged type itself.  */

	switch (TREE_CODE (type))
	  {
	  case ENUMERAL_TYPE:
	    output_die (output_enumeration_type_die, type);
	    return;  /* a special case -- nothing left to do so just return */

	  case RECORD_TYPE:
	    output_die (output_structure_type_die, type);
	    break;

	  case UNION_TYPE:
	    output_die (output_union_type_die, type);
	    break;
	  }

	/* If this is not an incomplete type, output descriptions of
	   each of its members.

	   Note that as we output the DIEs necessary to represent the
	   members of this record or union type, we will also be trying
	   to output DIEs to represent the *types* of those members.
	   However the `output_type' function (above) will specifically
	   avoid generating type DIEs for member types *within* the list
	   of member DIEs for this (containing) type execpt for those
	   types (of members) which are explicitly marked as also being
	   members of this (containing) type themselves.  The g++ front-
	   end can force any given type to be treated as a member of some
	   other (containing) type by setting the TYPE_CONTEXT of the
	   given (member) type to point to the TREE node representing the
	   appropriate (containing) type.
	*/

	if (TYPE_SIZE (type))
	  {
	    register tree member;

	    /* First output info about the data members and type members.  */

	    for (member = TYPE_FIELDS (type);
		 member;
		 member = TREE_CHAIN (member))
	      output_decl (member, type);

	    /* Now output info about the function members (if any).  */

	    if (TYPE_METHODS (type))
	      for (member = TREE_VEC_ELT (TYPE_METHODS (type), 0);
		   member;
		   member = TREE_CHAIN (member))
		output_decl (member, type);

	    end_sibling_chain ();	/* Terminate member chain.  */
	  }

	break;

      case VOID_TYPE:
      case INTEGER_TYPE:
      case REAL_TYPE:
      case COMPLEX_TYPE:
      case BOOLEAN_TYPE:
      case CHAR_TYPE:
	break;		/* No DIEs needed for fundamental types.  */

      case LANG_TYPE:	/* No Dwarf representation currently defined.  */
	break;

      default:
	abort ();
    }

  TREE_ASM_WRITTEN (type) = 1;
}

/* Output a TAG_lexical_block DIE followed by DIEs to represent all of
   the things which are local to the given block.  */

static void
output_block (stmt)
    register tree stmt;
{
  register int have_significant_locals = 0;

  /* Ignore blocks never really used to make RTL.  */

  if (! stmt || ! TREE_USED (stmt))
    return;

  /* Determine if this block contains any "significant" local declarations
     which we need to output DIEs for.  */

  if (BLOCK_INLINE_FUNCTION (stmt))
    /* The outer scopes for inlinings *must* always be represented.  */
    have_significant_locals = 1;
  else
    if (debug_info_level > DINFO_LEVEL_TERSE)
      have_significant_locals = (BLOCK_VARS (stmt) != NULL);
    else
      {
        register tree decl;

	for (decl = BLOCK_VARS (stmt); decl; decl = TREE_CHAIN (decl))
	  if (TREE_CODE (decl) == FUNCTION_DECL && DECL_INITIAL (decl))
	    {
	      have_significant_locals = 1;
	      break;
	    }
      }

  /* It would be a waste of space to generate a Dwarf TAG_lexical_block
     DIE for any block which contains no significant local declarations
     at all.  Rather, in such cases we just call `output_decls_for_scope'
     so that any needed Dwarf info for any sub-blocks will get properly
     generated.  Note that in terse mode, our definition of what constitutes
     a "significant" local declaration gets restricted to include only
     inlined function instances and local (nested) function definitions.  */

  if (have_significant_locals)
    {
      output_die (BLOCK_INLINE_FUNCTION (stmt)
			? output_inlined_subroutine_die
			: output_lexical_block_die,
		  stmt);
      output_decls_for_scope (stmt);
      end_sibling_chain ();
    }
  else
    output_decls_for_scope (stmt);
}

/* Output all of the decls declared within a given scope (also called
   a `binding contour') and (recursively) all of it's sub-blocks.  */

static void
output_decls_for_scope (stmt)
     register tree stmt;
{
  /* Ignore blocks never really used to make RTL.  */

  if (! stmt || ! TREE_USED (stmt))
    return;

  next_block_number++;

  /* Output the DIEs to represent all of the data objects, functions,
     typedefs, and tagged types declared directly within this block
     but not within any nested sub-blocks.  */

  {
    register tree decl;

    for (decl = BLOCK_VARS (stmt); decl; decl = TREE_CHAIN (decl))
      output_decl (decl, stmt);
  }

  output_pending_types_for_scope (stmt);

  /* Output the DIEs to represent all sub-blocks (and the items declared
     therein) of this block.	 */

  {
    register tree subblocks;

    for (subblocks = BLOCK_SUBBLOCKS (stmt);
         subblocks;
         subblocks = BLOCK_CHAIN (subblocks))
      output_block (subblocks);
  }
}

/* Output Dwarf .debug information for a decl described by DECL.  */

static void
output_decl (decl, containing_scope)
     register tree decl;
     register tree containing_scope;
{
  switch (TREE_CODE (decl))
    {
    case ERROR_MARK:
      break;

    case CONST_DECL:
      /* The individual enumerators of an enum type get output when we
	 output the Dwarf representation of the relevant enum type itself.  */
      break;

    case FUNCTION_DECL:
      /* If we are in terse mode, don't output any DIEs to represent
	 mere external function declarations.  */

      if (TREE_EXTERNAL (decl) && debug_info_level <= DINFO_LEVEL_TERSE)
	break;

      /* Before we describe the FUNCTION_DECL itself, make sure that we
	 have described its return type.  */

      output_type (TREE_TYPE (TREE_TYPE (decl)), containing_scope);

      /* If the following DIE will represent a function definition for a
	 function with "extern" linkage, output a special "pubnames" DIE
	 label just ahead of the actual DIE.  A reference to this label
	 was already generated in the .debug_pubnames section sub-entry
	 for this function definition.  */

      if (TREE_PUBLIC (decl))
	{
	  char label[MAX_ARTIFICIAL_LABEL_BYTES];

	  sprintf (label, PUB_DIE_LABEL_FMT, next_pubname_number++);
	  ASM_OUTPUT_LABEL (asm_out_file, label);
	}

      /* Now output a DIE to represent the function itself.  */

      output_die (TREE_PUBLIC (decl) || TREE_EXTERNAL (decl)
				? output_global_subroutine_die
				: output_local_subroutine_die,
		  decl);

      /* Now output descriptions of the arguments for this function.
	 This gets (unnecessarily?) complex because of the fact that
	 the DECL_ARGUMENT list for a FUNCTION_DECL doesn't indicate
	 cases where there was a trailing `...' at the end of the formal
	 parameter list.  In order to find out if there was a trailing
	 ellipsis or not, we must instead look at the type associated
	 with the FUNCTION_DECL.  This will be a node of type FUNCTION_TYPE.
	 If the chain of type nodes hanging off of this FUNCTION_TYPE node
	 ends with a void_type_node then there should *not* be an ellipsis
	 at the end.  */

      /* In the case where we are describing an external function, all
	 we need to do here (and all we *can* do here) is to describe
	 the *types* of its formal parameters.  */

      if (TREE_EXTERNAL (decl))
	output_formal_types (TREE_TYPE (decl));
      else
	{
	  register tree arg_decls = DECL_ARGUMENTS (decl);

	  /* In the case where the FUNCTION_DECL represents a C++ non-static
	     member function, skip over the first thing on the DECL_ARGUMENTS
	     chain.  It only represents the hidden `this pointer' parameter
	     and the debugger should know implicitly that non-static member
	     functions have such a thing, and should be able to figure out
	     exactly what the type of each `this pointer' is (from the
	     AT_member attribute of the parent TAG_subroutine DIE)  without
	     being explicitly told.  */

	  if (TREE_CODE (TREE_TYPE (decl)) == METHOD_TYPE)
	    arg_decls = TREE_CHAIN (arg_decls);

	  {
	    register tree last_arg;

	    last_arg = (arg_decls && TREE_CODE (arg_decls) != ERROR_MARK)
			? tree_last (arg_decls)
			: NULL;

	    /* Generate DIEs to represent all known formal parameters, but
	       don't do it if this looks like a varargs function.  A given
	       function is considered to be a varargs function if (and only
	       if) its last named argument is named `__builtin_va_alist'.  */

	    if (! last_arg
	        || ! DECL_NAME (last_arg)
	        || strcmp (IDENTIFIER_POINTER (DECL_NAME (last_arg)),
			   "__builtin_va_alist"))
	      {
	        register tree parm;

		/* WARNING!  Kludge zone ahead!  Here we have a special
		   hack for svr4 SDB compatibility.  Instead of passing the
		   current FUNCTION_DECL node as the second parameter (i.e.
		   the `containing_scope' parameter) to `output_decl' (as
		   we ought to) we instead pass a pointer to our own private
		   fake_containing_scope node.  That node is a RECORD_TYPE
		   node which NO OTHER TYPE may ever actually be a member of.

		   This pointer will ultimately get passed into `output_type'
		   as its `containing_scope' parameter.  `Output_type' will
		   then perform its part in the hack... i.e. it will pend
		   the type of the formal parameter onto the pending_types
		   list.  Later on, when we are done generating the whole
		   sequence of formal parameter DIEs for this function
		   definition, we will un-pend all previously pended types
		   of formal parameters for this function definition.

		   This whole kludge prevents any type DIEs from being
		   mixed in with the formal parameter DIEs.  That's good
		   because svr4 SDB believes that the list of formal
		   parameter DIEs for a function ends wherever the first
		   non-formal-parameter DIE appears.  Thus, we have to
		   keep the formal parameter DIEs segregated.  They must
		   all appear (consecutively) at the start of the list of
		   children for the DIE representing the function definition.
		   Then (and only then) may we output any additional DIEs
		   needed to represent the types of these formal parameters.
		*/

	        for (parm = arg_decls; parm; parm = TREE_CHAIN (parm))
		  if (TREE_CODE (parm) == PARM_DECL)
		    output_decl (parm, fake_containing_scope);

		/* Now that we have finished generating all of the DIEs to
		   represent the formal parameters themselves, force out
		   any DIEs needed to represent their types.  We do this
		   simply by un-pending all previously pended types which
		   can legitimately go into the chain of children DIEs for
		   the current FUNCTION_DECL.  */

		output_pending_types_for_scope (decl);
	      }
	  }

	  /* Now try to decide if we should put an ellipsis at the end. */

	  {
	    register int has_ellipsis = TRUE;	/* default assumption */
	    register tree fn_arg_types = TYPE_ARG_TYPES (TREE_TYPE (decl));

	    if (fn_arg_types)
	      {
		/* This function declaration/definition was prototyped.	 */

		/* If the list of formal argument types ends with a
		   void_type_node, then the formals list did *not* end
		   with an ellipsis.  */

		if (TREE_VALUE (tree_last (fn_arg_types)) == void_type_node)
		  has_ellipsis = FALSE;
	      }
	    else
	      {
		/* This function declaration/definition was not prototyped.  */

		/* Note that all non-prototyped function *declarations* are
		   assumed to represent varargs functions (until proven
		   otherwise).	*/

		if (DECL_INITIAL (decl)) /* if this is a func definition */
		  {
		    if (!arg_decls)
		      has_ellipsis = FALSE; /* no args == (void) */
		    else
		      {
			/* For a non-prototyped function definition which
			   declares one or more formal parameters, if the name
			   of the first formal parameter is *not*
			   __builtin_va_alist then we must assume that this
			   is *not* a varargs function.	 */

			if (DECL_NAME (arg_decls)
			  && strcmp (IDENTIFIER_POINTER (DECL_NAME (arg_decls)),
				     "__builtin_va_alist"))
			  has_ellipsis = FALSE;
		      }
		  }
	      }

	    if (has_ellipsis)
	      output_die (output_unspecified_parameters_die, decl);
	  }
	}

      /* Output Dwarf info for all of the stuff within the body of the
	 function (if it has one - it may be just a declaration).  */

      {
	register tree outer_scope = DECL_INITIAL (decl);

	if (outer_scope && TREE_CODE (outer_scope) != ERROR_MARK)
	  {
	    /* Note that here, `outer_scope' is a pointer to the outermost
	       BLOCK node created to represent the body of a function.
	       This outermost BLOCK actually represents the outermost
	       binding contour for the function, i.e. the contour in which
	       the function's formal parameters get declared.  Just within
	       this contour, there will be another (nested) BLOCK which
	       represents the function's outermost block.  We don't want
	       to generate a lexical_block DIE to represent the outermost
	       block of a function body, because that is not really an
	       independent scope according to ANSI C rules.  Rather, it is
	       the same scope in which the parameters were declared and
	       for Dwarf, we do not generate a TAG_lexical_block DIE for
	       that scope.  We must however see to it that the LABEL_DECLs
	       associated with `outer_scope' get DIEs generated for them.  */

	    {
	      register tree label;

	      for (label = BLOCK_VARS (outer_scope);
		   label;
		   label = TREE_CHAIN (label))
		output_decl (label, outer_scope);
	    }

	    output_decls_for_scope (BLOCK_SUBBLOCKS (outer_scope));

	    /* Finally, force out any pending types which are local to the
	       outermost block of this function definition.  These will
	       all have a TYPE_CONTEXT which points to the FUNCTION_DECL
	       node itself.  */

	    output_pending_types_for_scope (decl);
	  }
      }

      /* Generate a terminator for the list of stuff `owned' by this
	 function.  */

      end_sibling_chain ();

      break;

    case TYPE_DECL:
      /* If we are in terse mode, don't generate any DIEs to represent
	 any actual typedefs.  Note that even when we are in terse mode,
	 we must still output DIEs to represent those tagged types which
	 are used (directly or indirectly) in the specification of either
	 a return type or a formal parameter type of some function.  */

      if (debug_info_level <= DINFO_LEVEL_TERSE)
	if (DECL_NAME (decl) != NULL
	    || ! TYPE_USED_FOR_FUNCTION (TREE_TYPE (decl)))
          return;

      output_type (TREE_TYPE (decl), containing_scope);

      /* Note that unlike the gcc front end (which generates a NULL named
	 TYPE_DECL node for each complete tagged type, each array type,
	 and each function type node created) the g++ front end generates
	 a *named* TYPE_DECL node for each tagged type node created.
	 Unfortunately, these g++ TYPE_DECL nodes cause us to output many
	 superfluous and unnecessary TAG_typedef DIEs here.  When g++ is
	 fixed to stop generating these superfluous named TYPE_DECL nodes,
	 the superfluous TAG_typedef DIEs will likewise cease.  */

      if (DECL_NAME (decl))
	/* Output a DIE to represent the typedef itself.  */
	output_die (output_typedef_die, decl);
      break;

    case LABEL_DECL:
      if (debug_info_level >= DINFO_LEVEL_NORMAL)
	output_die (output_label_die, decl);
      break;

    case VAR_DECL:
      /* If we are in terse mode, don't generate any DIEs to represent
	 any variable declarations or definitions.  */

      if (debug_info_level <= DINFO_LEVEL_TERSE)
        break;

      /* Output any DIEs that are needed to specify the type of this data
	 object.  */

      output_type (TREE_TYPE (decl), containing_scope);

      /* If the following DIE will represent a data object definition for a
	 data object with "extern" linkage, output a special "pubnames" DIE
	 label just ahead of the actual DIE.  A reference to this label
	 was already generated in the .debug_pubnames section sub-entry
	 for this data object definition.  */

      if (TREE_PUBLIC (decl))
	{
	  char label[MAX_ARTIFICIAL_LABEL_BYTES];

	  sprintf (label, PUB_DIE_LABEL_FMT, next_pubname_number++);
	  ASM_OUTPUT_LABEL (asm_out_file, label);
	}

      /* Now output the DIE to represent the data object itself.  */

      output_die (TREE_PUBLIC (decl) || TREE_EXTERNAL (decl)
		   ? output_global_variable_die : output_local_variable_die,
		  decl);
      break;

    case FIELD_DECL:
      /* Ignore the nameless fields that are used to skip bits.  */
      if (DECL_NAME (decl) != 0)
	{
	  output_type (member_declared_type (decl), containing_scope);
          output_die (output_member_die, decl);
	}
      break;

    case PARM_DECL:
     /* Force out the type of this formal, if it was not forced out yet.
	Note that here we can run afowl of a bug in "classic" svr4 SDB.
	It should be able to grok the presence of type DIEs within a list
	of TAG_formal_parameter DIEs, but it doesn't.  */

      output_type (TREE_TYPE (decl), containing_scope);
      output_die (output_formal_parameter_die, decl);
      break;

    default:
      abort ();
    }
}

void
dwarfout_file_scope_decl (decl, set_finalizing)
     register tree decl;
     register int set_finalizing;
{
  switch (TREE_CODE (decl))
    {
    case FUNCTION_DECL:

      /* Ignore this FUNCTION_DECL if it refers to a builtin function.  */

      if (TREE_EXTERNAL (decl) && DECL_FUNCTION_CODE (decl))
        return;

      /* Ignore this FUNCTION_DECL if it refers to a file-scope extern
	 function declaration and if the declaration was never even
	 referenced from within this entire compilation unit.  We
	 suppress these DIEs in order to save space in the .debug section
	 (by eliminating entries which are probably useless).  Note that
	 we must not suppress block-local extern declarations (whether
	 used or not) because that would screw-up the debugger's name
	 lookup mechanism and cause it to miss things which really ought
	 to be in scope at a given point.  */

      if (TREE_EXTERNAL (decl) && !TREE_USED (decl))
	return;

      if (TREE_PUBLIC (decl) && ! TREE_EXTERNAL (decl))
	{
	  char label[MAX_ARTIFICIAL_LABEL_BYTES];

	  /* Output a .debug_pubnames entry for a public function
	     defined in this compilation unit.  */

	  fputc ('\n', asm_out_file);
	  ASM_DWARF_PUBNAMES_SECTION (asm_out_file);
	  sprintf (label, PUB_DIE_LABEL_FMT, next_pubname_number);
	  ASM_OUTPUT_DWARF_ADDR (asm_out_file, label);
	  ASM_OUTPUT_DWARF_STRING (asm_out_file,
				   IDENTIFIER_POINTER (DECL_NAME (decl)));
	  ASM_DWARF_POP_SECTION (asm_out_file);
	}

      break;

    case VAR_DECL:

      /* Ignore this VAR_DECL if it refers to a file-scope extern data
	 object declaration and if the declaration was never even
	 referenced from within this entire compilation unit.  We
	 suppress these DIEs in order to save space in the .debug section
	 (by eliminating entries which are probably useless).  Note that
	 we must not suppress block-local extern declarations (whether
	 used or not) because that would screw-up the debugger's name
	 lookup mechanism and cause it to miss things which really ought
	 to be in scope at a given point.  */

      if (TREE_EXTERNAL (decl) && !TREE_USED (decl))
	return;

      if (TREE_PUBLIC (decl) && ! TREE_EXTERNAL (decl))
	{
	  char label[MAX_ARTIFICIAL_LABEL_BYTES];

	  if (debug_info_level >= DINFO_LEVEL_NORMAL)
	    {
	      /* Output a .debug_pubnames entry for a public variable
	         defined in this compilation unit.  */

	      fputc ('\n', asm_out_file);
	      ASM_DWARF_PUBNAMES_SECTION (asm_out_file);
	      sprintf (label, PUB_DIE_LABEL_FMT, next_pubname_number);
	      ASM_OUTPUT_DWARF_ADDR (asm_out_file, label);
	      ASM_OUTPUT_DWARF_STRING (asm_out_file,
				       IDENTIFIER_POINTER (DECL_NAME (decl)));
	      ASM_DWARF_POP_SECTION (asm_out_file);
	    }

	  if (DECL_INITIAL (decl) == NULL)
	    {
	      /* Output a .debug_aranges entry for a public variable
		 which is tenatively defined in this compilation unit.  */

	      fputc ('\n', asm_out_file);
	      ASM_DWARF_ARANGES_SECTION (asm_out_file);
	      ASM_OUTPUT_DWARF_ADDR (asm_out_file,
				     IDENTIFIER_POINTER (DECL_NAME (decl)));
	      ASM_OUTPUT_DWARF_DATA4 (asm_out_file, 
			(unsigned) int_size_in_bytes (TREE_TYPE (decl)));
	      ASM_DWARF_POP_SECTION (asm_out_file);
	    }
	}

      /* If we are in terse mode, don't generate any DIEs to represent
	 any variable declarations or definitions.  */

      if (debug_info_level <= DINFO_LEVEL_TERSE)
        return;

      break;

    case TYPE_DECL:
      /* Don't generate any DIEs to represent the standard built-in types.  */

      if (DECL_SOURCE_LINE (decl) == 0)
	return;

      /* If we are in terse mode, don't generate any DIEs to represent
	 any actual typedefs.  Note that even when we are in terse mode,
	 we must still output DIEs to represent those tagged types which
	 are used (directly or indirectly) in the specification of either
	 a return type or a formal parameter type of some function.  */

      if (debug_info_level <= DINFO_LEVEL_TERSE)
	if (DECL_NAME (decl) != NULL
	    || ! TYPE_USED_FOR_FUNCTION (TREE_TYPE (decl)))
          return;

      break;

    default:
      return;
    }

  fputc ('\n', asm_out_file);
  ASM_DWARF_DEBUG_SECTION (asm_out_file);
  finalizing = set_finalizing;
  output_decl (decl, NULL);

  /* NOTE:  The call above to `output_decl' may have caused one or more
     file-scope named types (i.e. tagged types) to be placed onto the
     pending_types_list.  We have to get those types off of that list
     at some point, and this is the perfect time to do it.  If we didn't
     take them off now, they might still be on the list when cc1 finally
     exits.  That might be OK if it weren't for the fact that when we put
     types onto the pending_types_list, we set the TREE_ASM_WRITTEN flag
     for these types, and that causes them never to be output unless
     `output_pending_types_for_scope' takes them off of the list and un-sets
     their TREE_ASM_WRITTEN flags.  */

  output_pending_types_for_scope (NULL);

  /* The above call should have totally emptied the pending_types_list.  */

  assert (pending_types == 0);

  ASM_DWARF_POP_SECTION (asm_out_file);

  if (TREE_CODE (decl) == FUNCTION_DECL && DECL_INITIAL (decl) != NULL)
    current_funcdef_number++;
}

/* Output a marker (i.e. a label) for the beginning of the generated code
   for a lexical block.	 */

void
dwarfout_begin_block (blocknum)
     register unsigned blocknum;
{
  char label[MAX_ARTIFICIAL_LABEL_BYTES];

  text_section ();
  sprintf (label, BLOCK_BEGIN_LABEL_FMT, blocknum);
  ASM_OUTPUT_LABEL (asm_out_file, label);
}

/* Output a marker (i.e. a label) for the end of the generated code
   for a lexical block.	 */

void
dwarfout_end_block (blocknum)
     register unsigned blocknum;
{
  char label[MAX_ARTIFICIAL_LABEL_BYTES];

  text_section ();
  sprintf (label, BLOCK_END_LABEL_FMT, blocknum);
  ASM_OUTPUT_LABEL (asm_out_file, label);
}

/* Output a marker (i.e. a label) at a point in the assembly code which
   corresponds to a given source level label.  */

void
dwarfout_label (insn)
     register rtx insn;
{
  if (debug_info_level >= DINFO_LEVEL_NORMAL)
    {
      char label[MAX_ARTIFICIAL_LABEL_BYTES];

      text_section ();
      sprintf (label, INSN_LABEL_FMT, current_funcdef_number,
				      (unsigned) INSN_UID (insn));
      ASM_OUTPUT_LABEL (asm_out_file, label);
    }
}

/* Output a marker (i.e. a label) for the absolute end of the generated code
   for a function definition.  This gets called *after* the epilogue code
   has been generated.	*/

void
dwarfout_end_epilogue ()
{
  char label[MAX_ARTIFICIAL_LABEL_BYTES];

  /* Output a label to mark the endpoint of the code generated for this
     function.	*/

  sprintf (label, FUNC_END_LABEL_FMT, current_funcdef_number);
  ASM_OUTPUT_LABEL (asm_out_file, label);
}

static void
shuffle_filename_entry (new_zeroth)
     register filename_entry *new_zeroth;
{
  filename_entry temp_entry;
  register filename_entry *limit_p;
  register filename_entry *move_p;

  if (new_zeroth == &filename_table[0])
    return;

  temp_entry = *new_zeroth;

  /* Shift entries up in the table to make room at [0].  */

  limit_p = &filename_table[0];
  for (move_p = new_zeroth; move_p > limit_p; move_p--)
    *move_p = *(move_p-1);

  /* Install the found entry at [0].  */

  filename_table[0] = temp_entry;
}

/* Create a new (string) entry for the .debug_sfnames section.  */

static void
generate_new_sfname_entry ()
{
  char label[MAX_ARTIFICIAL_LABEL_BYTES];

  fputc ('\n', asm_out_file);
  ASM_DWARF_SFNAMES_SECTION (asm_out_file);
  sprintf (label, SFNAMES_ENTRY_LABEL_FMT, filename_table[0].number);
  ASM_OUTPUT_LABEL (asm_out_file, label);
  ASM_OUTPUT_DWARF_STRING (asm_out_file,
    			   filename_table[0].name
			     ? filename_table[0].name
			     : "");
  ASM_DWARF_POP_SECTION (asm_out_file);
}

/* Lookup a filename (in the list of filenames that we know about here in
   dwarfout.c) and return its "index".  The index of each (known) filename
   is just a unique number which is associated with only that one filename.
   We need such numbers for the sake of generating labels (in the
   .debug_sfnames section) and references to those unique labels (in the
   .debug_srcinfo and .debug_macinfo sections).

   If the filename given as an argument is not found in our current list,
   add it to the list and assign it the next available unique index number.

   Whatever we do (i.e. whether we find a pre-existing filename or add a new
   one), we shuffle the filename found (or added) up to the zeroth entry of
   our list of filenames (which is always searched linearly).  We do this so
   as to optimize the most common case for these filename lookups within
   dwarfout.c.  The most common case by far is the case where we call
   lookup_filename to lookup the very same filename that we did a lookup
   on the last time we called lookup_filename.  We make sure that this
   common case is fast because such cases will constitute 99.9% of the
   lookups we ever do (in practice).

   If we add a new filename entry to our table, we go ahead and generate
   the corresponding entry in the .debug_sfnames section right away.
   Doing so allows us to avoid tickling an assembler bug (present in some
   m68k assemblers) which yields assembly-time errors in cases where the
   difference of two label addresses is taken and where the two labels
   are in a section *other* than the one where the difference is being
   calculated, and where at least one of the two symbol references is a
   forward reference.  (This bug could be tickled by our .debug_srcinfo
   entries if we don't output their corresponding .debug_sfnames entries
   before them.)
*/

static unsigned
lookup_filename (file_name)
     char *file_name;
{
  register filename_entry *search_p;
  register filename_entry *limit_p = &filename_table[ft_entries];

  for (search_p = filename_table; search_p < limit_p; search_p++)
    if (!strcmp (file_name, search_p->name))
      {
	/* When we get here, we have found the filename that we were
	   looking for in the filename_table.  Now we want to make sure
	   that it gets moved to the zero'th entry in the table (if it
	   is not already there) so that subsequent attempts to find the
	   same filename will find it as quickly as possible.  */

	shuffle_filename_entry (search_p);
        return filename_table[0].number;
      }

  /* We come here whenever we have a new filename which is not registered
     in the current table.  Here we add it to the table.  */

  /* Prepare to add a new table entry by making sure there is enough space
     in the table to do so.  If not, expand the current table.  */

  if (ft_entries == ft_entries_allocated)
    {
      ft_entries_allocated += FT_ENTRIES_INCREMENT;
      filename_table
	= (filename_entry *)
	  xrealloc (filename_table,
		    ft_entries_allocated * sizeof (filename_entry));
    }

  /* Initially, add the new entry at the end of the filename table.  */

  filename_table[ft_entries].number = ft_entries;
  filename_table[ft_entries].name = xstrdup (file_name);

  /* Shuffle the new entry into filename_table[0].  */

  shuffle_filename_entry (&filename_table[ft_entries]);

  if (debug_info_level >= DINFO_LEVEL_NORMAL)
    generate_new_sfname_entry ();

  ft_entries++;
  return filename_table[0].number;
}

static void
generate_srcinfo_entry (line_entry_num, files_entry_num)
     unsigned line_entry_num;
     unsigned files_entry_num;
{
  char label[MAX_ARTIFICIAL_LABEL_BYTES];

  fputc ('\n', asm_out_file);
  ASM_DWARF_SRCINFO_SECTION (asm_out_file);
  sprintf (label, LINE_ENTRY_LABEL_FMT, line_entry_num);
  ASM_OUTPUT_DWARF_DELTA4 (asm_out_file, label, LINE_BEGIN_LABEL);
  sprintf (label, SFNAMES_ENTRY_LABEL_FMT, files_entry_num);
  ASM_OUTPUT_DWARF_DELTA4 (asm_out_file, label, SFNAMES_BEGIN_LABEL);
  ASM_DWARF_POP_SECTION (asm_out_file);
}

void
dwarfout_line (filename, line)
     register char *filename;
     register unsigned line;
{
  if (debug_info_level >= DINFO_LEVEL_NORMAL)
    {
      char label[MAX_ARTIFICIAL_LABEL_BYTES];
      static unsigned last_line_entry_num = 0;
      static unsigned prev_file_entry_num = (unsigned) -1;
      register unsigned this_file_entry_num = lookup_filename (filename);

      text_section ();
      sprintf (label, LINE_CODE_LABEL_FMT, ++last_line_entry_num);
      ASM_OUTPUT_LABEL (asm_out_file, label);

      fputc ('\n', asm_out_file);
      ASM_DWARF_LINE_SECTION (asm_out_file);

      if (this_file_entry_num != prev_file_entry_num)
        {
          char line_entry_label[MAX_ARTIFICIAL_LABEL_BYTES];

          sprintf (line_entry_label, LINE_ENTRY_LABEL_FMT, last_line_entry_num);
          ASM_OUTPUT_LABEL (asm_out_file, line_entry_label);
        }

      {
        register char *tail = strrchr (filename, '/');

        if (tail != NULL)
          filename = tail;
      }

      fprintf (asm_out_file, "\t%s\t%u\t%s %s:%u\n",
	       UNALIGNED_INT_ASM_OP, line, ASM_COMMENT_START,
	       filename, line);
      ASM_OUTPUT_DWARF_DATA2 (asm_out_file, 0xffff);
      ASM_OUTPUT_DWARF_DELTA4 (asm_out_file, label, TEXT_BEGIN_LABEL);
      ASM_DWARF_POP_SECTION (asm_out_file);

      if (this_file_entry_num != prev_file_entry_num)
        generate_srcinfo_entry (last_line_entry_num, this_file_entry_num);
      prev_file_entry_num = this_file_entry_num;
    }
}

/* Generate an entry in the .debug_macinfo section.  */

static void
generate_macinfo_entry (type_and_offset, string)
     register char *type_and_offset;
     register char *string;
{
  fputc ('\n', asm_out_file);
  ASM_DWARF_MACINFO_SECTION (asm_out_file);
  fprintf (asm_out_file, "\t%s\t%s\n", UNALIGNED_INT_ASM_OP, type_and_offset);
  ASM_OUTPUT_DWARF_STRING (asm_out_file, string);
  ASM_DWARF_POP_SECTION (asm_out_file);
}

void
dwarfout_start_new_source_file (filename)
     register char *filename;
{
  char label[MAX_ARTIFICIAL_LABEL_BYTES];
  char type_and_offset[MAX_ARTIFICIAL_LABEL_BYTES*3];

  sprintf (label, SFNAMES_ENTRY_LABEL_FMT, lookup_filename (filename));
  sprintf (type_and_offset, "0x%08x+%s-%s",
	   ((unsigned) MACINFO_start << 24), label, SFNAMES_BEGIN_LABEL);
  generate_macinfo_entry (type_and_offset, "");
}

void
dwarfout_resume_previous_source_file (lineno)
     register unsigned lineno;
{
  char type_and_offset[MAX_ARTIFICIAL_LABEL_BYTES*2];

  sprintf (type_and_offset, "0x%08x+%u",
	   ((unsigned) MACINFO_resume << 24), lineno);
  generate_macinfo_entry (type_and_offset, "");
}

/* Called from check_newline in c-parse.y.  The `buffer' parameter
   contains the tail part of the directive line, i.e. the part which
   is past the initial whitespace, #, whitespace, directive-name,
   whitespace part.  */

void
dwarfout_define (lineno, buffer)
     register unsigned lineno;
     register char *buffer;
{
  static int initialized = 0;
  char type_and_offset[MAX_ARTIFICIAL_LABEL_BYTES*2];

  if (!initialized)
    {
      dwarfout_start_new_source_file (primary_filename);
      initialized = 1;
    }
  sprintf (type_and_offset, "0x%08x+%u",
	   ((unsigned) MACINFO_define << 24), lineno);
  generate_macinfo_entry (type_and_offset, buffer);
}

/* Called from check_newline in c-parse.y.  The `buffer' parameter
   contains the tail part of the directive line, i.e. the part which
   is past the initial whitespace, #, whitespace, directive-name,
   whitespace part.  */

void
dwarfout_undef (lineno, buffer)
     register unsigned lineno;
     register char *buffer;
{
  char type_and_offset[MAX_ARTIFICIAL_LABEL_BYTES*2];

  sprintf (type_and_offset, "0x%08x+%u",
	   ((unsigned) MACINFO_undef << 24), lineno);
  generate_macinfo_entry (type_and_offset, buffer);
}

/* Set up for Dwarf output at the start of compilation.	 */

void
dwarfout_init (asm_out_file, main_input_filename)
     register FILE *asm_out_file;
     register char *main_input_filename;
{
  /* Remember the name of the primary input file.  */

  primary_filename = main_input_filename;

  /* Allocate the initial hunk of the pending_sibling_stack.  */

  pending_sibling_stack
    = (unsigned *)
	xmalloc (PENDING_SIBLINGS_INCREMENT * sizeof (unsigned));
  pending_siblings_allocated = PENDING_SIBLINGS_INCREMENT;
  pending_siblings = 1;

  /* Allocate the initial hunk of the filename_table.  */

  filename_table
    = (filename_entry *)
	xmalloc (FT_ENTRIES_INCREMENT * sizeof (filename_entry));
  ft_entries_allocated = FT_ENTRIES_INCREMENT;
  ft_entries = 0;

  /* Allocate the initial hunk of the pending_types_list.  */

  pending_types_list
    = (tree *) xmalloc (PENDING_TYPES_INCREMENT * sizeof (tree));
  pending_types_allocated = PENDING_TYPES_INCREMENT;
  pending_types = 0;

  /* Create an artificial RECORD_TYPE node which we can use in our hack
     to get the DIEs representing types of formal parameters to come out
     only *after* the DIEs for the formal parameters themselves.  */

  fake_containing_scope = make_node (RECORD_TYPE);

  /* Output a starting label for the .text section.  */

  fputc ('\n', asm_out_file);
  ASM_DWARF_TEXT_SECTION (asm_out_file);
  ASM_OUTPUT_LABEL (asm_out_file, TEXT_BEGIN_LABEL);
  ASM_DWARF_POP_SECTION (asm_out_file);

  /* Output a starting label for the .data section.  */

  fputc ('\n', asm_out_file);
  ASM_DWARF_DATA_SECTION (asm_out_file);
  ASM_OUTPUT_LABEL (asm_out_file, DATA_BEGIN_LABEL);
  ASM_DWARF_POP_SECTION (asm_out_file);

  /* Output a starting label for the .data1 section.  */

  fputc ('\n', asm_out_file);
  ASM_DWARF_DATA1_SECTION (asm_out_file);
  ASM_OUTPUT_LABEL (asm_out_file, DATA1_BEGIN_LABEL);
  ASM_DWARF_POP_SECTION (asm_out_file);

  /* Output a starting label for the .rodata section.  */

  fputc ('\n', asm_out_file);
  ASM_DWARF_RODATA_SECTION (asm_out_file);
  ASM_OUTPUT_LABEL (asm_out_file, RODATA_BEGIN_LABEL);
  ASM_DWARF_POP_SECTION (asm_out_file);

  /* Output a starting label for the .rodata1 section.  */

  fputc ('\n', asm_out_file);
  ASM_DWARF_RODATA1_SECTION (asm_out_file);
  ASM_OUTPUT_LABEL (asm_out_file, RODATA1_BEGIN_LABEL);
  ASM_DWARF_POP_SECTION (asm_out_file);

  /* Output a starting label for the .bss section.  */

  fputc ('\n', asm_out_file);
  ASM_DWARF_BSS_SECTION (asm_out_file);
  ASM_OUTPUT_LABEL (asm_out_file, BSS_BEGIN_LABEL);
  ASM_DWARF_POP_SECTION (asm_out_file);

  if (debug_info_level >= DINFO_LEVEL_NORMAL)
    {
      /* Output a starting label and an initial (compilation directory)
	 entry for the .debug_sfnames section.  The starting label will be
	 referenced by the initial entry in the .debug_srcinfo section.  */
    
      fputc ('\n', asm_out_file);
      ASM_DWARF_SFNAMES_SECTION (asm_out_file);
      ASM_OUTPUT_LABEL (asm_out_file, SFNAMES_BEGIN_LABEL);
      {
	register char *pwd = getpwd ();
	register unsigned len = strlen (pwd);
	register char *dirname = (char *) xmalloc (len + 2);
    
	strcpy (dirname, pwd);
	strcpy (dirname + len, "/");
        ASM_OUTPUT_DWARF_STRING (asm_out_file, dirname);
        free (dirname);
      }
      ASM_DWARF_POP_SECTION (asm_out_file);
    
      if (debug_info_level >= DINFO_LEVEL_VERBOSE)
	{
          /* Output a starting label for the .debug_macinfo section.  This
	     label will be referenced by the AT_mac_info attribute in the
	     TAG_compile_unit DIE.  */
        
          fputc ('\n', asm_out_file);
          ASM_DWARF_MACINFO_SECTION (asm_out_file);
          ASM_OUTPUT_LABEL (asm_out_file, MACINFO_BEGIN_LABEL);
          ASM_DWARF_POP_SECTION (asm_out_file);
	}

      /* Generate the initial entry for the .line section.  */
    
      fputc ('\n', asm_out_file);
      ASM_DWARF_LINE_SECTION (asm_out_file);
      ASM_OUTPUT_LABEL (asm_out_file, LINE_BEGIN_LABEL);
      ASM_OUTPUT_DWARF_DELTA4 (asm_out_file, LINE_END_LABEL, LINE_BEGIN_LABEL);
      ASM_OUTPUT_DWARF_ADDR (asm_out_file, TEXT_BEGIN_LABEL);
      ASM_DWARF_POP_SECTION (asm_out_file);
    
      /* Generate the initial entry for the .debug_srcinfo section.  */
    
      fputc ('\n', asm_out_file);
      ASM_DWARF_SRCINFO_SECTION (asm_out_file);
      ASM_OUTPUT_LABEL (asm_out_file, SRCINFO_BEGIN_LABEL);
      ASM_OUTPUT_DWARF_ADDR (asm_out_file, LINE_BEGIN_LABEL);
      ASM_OUTPUT_DWARF_ADDR (asm_out_file, SFNAMES_BEGIN_LABEL);
      ASM_OUTPUT_DWARF_ADDR (asm_out_file, TEXT_BEGIN_LABEL);
      ASM_OUTPUT_DWARF_ADDR (asm_out_file, TEXT_END_LABEL);
#ifdef DWARF_TIMESTAMPS
      ASM_OUTPUT_DWARF_DATA4 (asm_out_file, time (NULL));
#else
      ASM_OUTPUT_DWARF_DATA4 (asm_out_file, -1);
#endif
      ASM_DWARF_POP_SECTION (asm_out_file);
    
      /* Generate the initial entry for the .debug_pubnames section.  */
    
      fputc ('\n', asm_out_file);
      ASM_DWARF_PUBNAMES_SECTION (asm_out_file);
      ASM_OUTPUT_DWARF_ADDR (asm_out_file, DEBUG_BEGIN_LABEL);
      ASM_DWARF_POP_SECTION (asm_out_file);
    
      /* Generate the initial entry for the .debug_aranges section.  */
    
      fputc ('\n', asm_out_file);
      ASM_DWARF_ARANGES_SECTION (asm_out_file);
      ASM_OUTPUT_DWARF_ADDR (asm_out_file, DEBUG_BEGIN_LABEL);
      ASM_DWARF_POP_SECTION (asm_out_file);
    }

  /* Setup first DIE number == 1.  */
  NEXT_DIE_NUM = next_unused_dienum++;

  /* Generate the initial DIE for the .debug section.  Note that the
     (string) value given in the AT_name attribute of the TAG_compile_unit
     DIE will (typically) be a relative pathname and that this pathname
     should be taken as being relative to the directory from which the
     compiler was invoked when the given (base) source file was compiled.  */

  fputc ('\n', asm_out_file);
  ASM_DWARF_DEBUG_SECTION (asm_out_file);
  ASM_OUTPUT_LABEL (asm_out_file, DEBUG_BEGIN_LABEL);
  output_die (output_compile_unit_die, main_input_filename);
  ASM_DWARF_POP_SECTION (asm_out_file);

  fputc ('\n', asm_out_file);
}

/* Output stuff that dwarf requires at the end of every file.  */

void
dwarfout_finish ()
{
  char label[MAX_ARTIFICIAL_LABEL_BYTES];

  fputc ('\n', asm_out_file);
  ASM_DWARF_DEBUG_SECTION (asm_out_file);

  /* Mark the end of the chain of siblings which represent all file-scope
     declarations in this compilation unit.  */

  /* The (null) DIE which represents the terminator for the (sibling linked)
     list of file-scope items is *special*.  Normally, we would just call
     end_sibling_chain at this point in order to output a word with the
     value `4' and that word would act as the terminator for the list of
     DIEs describing file-scope items.  Unfortunately, if we were to simply
     do that, the label that would follow this DIE in the .debug section
     (i.e. `..D2') would *not* be properly aligned (as it must be on some
     machines) to a 4 byte boundary.

     In order to force the label `..D2' to get aligned to a 4 byte boundary,
     the trick used is to insert extra (otherwise useless) padding bytes
     into the (null) DIE that we know must preceed the ..D2 label in the
     .debug section.  The amount of padding required can be anywhere between
     0 and 3 bytes.  The length word at the start of this DIE (i.e. the one
     with the padding) would normally contain the value 4, but now it will
     also have to include the padding bytes, so it will instead have some
     value in the range 4..7.

     Fortunately, the rules of Dwarf say that any DIE whose length word
     contains *any* value less than 8 should be treated as a null DIE, so
     this trick works out nicely.  Clever, eh?  Don't give me any credit
     (or blame).  I didn't think of this scheme.  I just conformed to it.
  */

  output_die (output_padded_null_die, (void *)0);
  dienum_pop ();

  sprintf (label, DIE_BEGIN_LABEL_FMT, NEXT_DIE_NUM);
  ASM_OUTPUT_LABEL (asm_out_file, label);	/* should be ..D2 */
  ASM_DWARF_POP_SECTION (asm_out_file);

  /* Output a terminator label for the .text section.  */

  fputc ('\n', asm_out_file);
  ASM_DWARF_TEXT_SECTION (asm_out_file);
  ASM_OUTPUT_LABEL (asm_out_file, TEXT_END_LABEL);
  ASM_DWARF_POP_SECTION (asm_out_file);

  /* Output a terminator label for the .data section.  */

  fputc ('\n', asm_out_file);
  ASM_DWARF_DATA_SECTION (asm_out_file);
  ASM_OUTPUT_LABEL (asm_out_file, DATA_END_LABEL);
  ASM_DWARF_POP_SECTION (asm_out_file);

  /* Output a terminator label for the .data1 section.  */

  fputc ('\n', asm_out_file);
  ASM_DWARF_DATA1_SECTION (asm_out_file);
  ASM_OUTPUT_LABEL (asm_out_file, DATA1_END_LABEL);
  ASM_DWARF_POP_SECTION (asm_out_file);

  /* Output a terminator label for the .rodata section.  */

  fputc ('\n', asm_out_file);
  ASM_DWARF_RODATA_SECTION (asm_out_file);
  ASM_OUTPUT_LABEL (asm_out_file, RODATA_END_LABEL);
  ASM_DWARF_POP_SECTION (asm_out_file);

  /* Output a terminator label for the .rodata1 section.  */

  fputc ('\n', asm_out_file);
  ASM_DWARF_RODATA1_SECTION (asm_out_file);
  ASM_OUTPUT_LABEL (asm_out_file, RODATA1_END_LABEL);
  ASM_DWARF_POP_SECTION (asm_out_file);

  /* Output a terminator label for the .bss section.  */

  fputc ('\n', asm_out_file);
  ASM_DWARF_BSS_SECTION (asm_out_file);
  ASM_OUTPUT_LABEL (asm_out_file, BSS_END_LABEL);
  ASM_DWARF_POP_SECTION (asm_out_file);

  if (debug_info_level >= DINFO_LEVEL_NORMAL)
    {
      /* Output a terminating entry for the .line section.  */
    
      fputc ('\n', asm_out_file);
      ASM_DWARF_LINE_SECTION (asm_out_file);
      ASM_OUTPUT_LABEL (asm_out_file, LINE_LAST_ENTRY_LABEL);
      ASM_OUTPUT_DWARF_DATA4 (asm_out_file, 0);
      ASM_OUTPUT_DWARF_DATA2 (asm_out_file, 0xffff);
      ASM_OUTPUT_DWARF_DELTA4 (asm_out_file, TEXT_END_LABEL, TEXT_BEGIN_LABEL);
      ASM_OUTPUT_LABEL (asm_out_file, LINE_END_LABEL);
      ASM_DWARF_POP_SECTION (asm_out_file);
    
      /* Output a terminating entry for the .debug_srcinfo section.  */
    
      fputc ('\n', asm_out_file);
      ASM_DWARF_SRCINFO_SECTION (asm_out_file);
      ASM_OUTPUT_DWARF_DELTA4 (asm_out_file,
			       LINE_LAST_ENTRY_LABEL, LINE_BEGIN_LABEL);
      ASM_OUTPUT_DWARF_DATA4 (asm_out_file, -1);
      ASM_DWARF_POP_SECTION (asm_out_file);

      if (debug_info_level >= DINFO_LEVEL_VERBOSE)
	{
	  /* Output terminating entries for the .debug_macinfo section.  */
	
	  dwarfout_resume_previous_source_file (0);

	  fputc ('\n', asm_out_file);
	  ASM_DWARF_MACINFO_SECTION (asm_out_file);
	  ASM_OUTPUT_DWARF_DATA4 (asm_out_file, 0);
	  ASM_OUTPUT_DWARF_STRING (asm_out_file, "");
	  ASM_DWARF_POP_SECTION (asm_out_file);
	}
    
      /* Generate the terminating entry for the .debug_pubnames section.  */
    
      fputc ('\n', asm_out_file);
      ASM_DWARF_PUBNAMES_SECTION (asm_out_file);
      ASM_OUTPUT_DWARF_DATA4 (asm_out_file, 0);
      ASM_OUTPUT_DWARF_STRING (asm_out_file, "");
      ASM_DWARF_POP_SECTION (asm_out_file);
    
      /* Generate the terminating entries for the .debug_aranges section.

	 Note that we want to do this only *after* we have output the end
	 labels (for the various program sections) which we are going to
	 refer to here.  This allows us to work around a bug in the m68k
	 svr4 assembler.  That assembler gives bogus assembly-time errors
	 if (within any given section) you try to take the difference of
	 two relocatable symbols, both of which are located within some
	 other section, and if one (or both?) of the symbols involved is
	 being forward-referenced.  By generating the .debug_aranges
	 entries at this late point in the assembly output, we skirt the
	 issue simply by avoiding forward-references.
      */
    
      fputc ('\n', asm_out_file);
      ASM_DWARF_ARANGES_SECTION (asm_out_file);

      ASM_OUTPUT_DWARF_ADDR (asm_out_file, TEXT_BEGIN_LABEL);
      ASM_OUTPUT_DWARF_DELTA4 (asm_out_file, TEXT_END_LABEL, TEXT_BEGIN_LABEL);

      ASM_OUTPUT_DWARF_ADDR (asm_out_file, DATA_BEGIN_LABEL);
      ASM_OUTPUT_DWARF_DELTA4 (asm_out_file, DATA_END_LABEL, DATA_BEGIN_LABEL);

      ASM_OUTPUT_DWARF_ADDR (asm_out_file, DATA1_BEGIN_LABEL);
      ASM_OUTPUT_DWARF_DELTA4 (asm_out_file, DATA1_END_LABEL,
					     DATA1_BEGIN_LABEL);

      ASM_OUTPUT_DWARF_ADDR (asm_out_file, RODATA_BEGIN_LABEL);
      ASM_OUTPUT_DWARF_DELTA4 (asm_out_file, RODATA_END_LABEL,
					     RODATA_BEGIN_LABEL);

      ASM_OUTPUT_DWARF_ADDR (asm_out_file, RODATA1_BEGIN_LABEL);
      ASM_OUTPUT_DWARF_DELTA4 (asm_out_file, RODATA1_END_LABEL,
					     RODATA1_BEGIN_LABEL);

      ASM_OUTPUT_DWARF_ADDR (asm_out_file, BSS_BEGIN_LABEL);
      ASM_OUTPUT_DWARF_DELTA4 (asm_out_file, BSS_END_LABEL, BSS_BEGIN_LABEL);

      ASM_OUTPUT_DWARF_DATA4 (asm_out_file, 0);
      ASM_OUTPUT_DWARF_DATA4 (asm_out_file, 0);

      ASM_DWARF_POP_SECTION (asm_out_file);
    }
}

#endif /* DWARF_DEBUGGING_INFO */
