/* Subroutines for insn-output.c for Motorola 88000.
   Copyright (C) 1988, 1989, 1990, 1991 Free Software Foundation, Inc.
   Contributed by Michael Tiemann (tiemann@mcc.com)
   Enhanced by Michael Meissner (meissner@osf.org)
   Currently supported by Tom Wood (wood@dg-rtp.dg.com)

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

#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <ctype.h>

#include "config.h"
#include "rtl.h"
#include "regs.h"
#include "hard-reg-set.h"
#include "real.h"
#include "insn-config.h"
#include "conditions.h"
#include "insn-flags.h"
#include "output.h"
#include "insn-attr.h"
#include "tree.h"
#include "c-tree.h"
#include "expr.h"
#include "hard-reg-set.h"
#include "flags.h"

extern char *version_string;
extern time_t time ();
extern char *ctime ();
extern int flag_traditional;
extern FILE *asm_out_file;

static char out_sccs_id[] = "@(#)m88k.c	2.0.3.4 19 Mar 1992 11:11:58";
static char tm_sccs_id [] = TM_SCCS_ID;

char *m88k_pound_sign = "";	/* Either # for SVR4 or empty for SVR3 */
char *m88k_short_data;

int m88k_gp_threshold;
int m88k_prologue_done	= 0;	/* Ln directives can now be emitted */
int m88k_function_number = 0;	/* Counter unique to each function */
int m88k_fp_offset	= 0;	/* offset of frame pointer if used */
int m88k_stack_size	= 0;	/* size of allocated stack (including frame) */
int m88k_case_index;

rtx m88k_compare_reg;		/* cmp output pseudo register */
rtx m88k_compare_op0;		/* cmpsi operand 0 */
rtx m88k_compare_op1;		/* cmpsi operand 1 */

enum attr_cpu m88k_cpu;		/* target cpu */

/* Determine what instructions are needed to manufacture the integer VALUE
   in the given MODE.  */

enum m88k_instruction
classify_integer (mode, value)
     enum machine_mode mode;
     register int value;
{
  register int mask;

  if (value == 0)
    return m88k_zero;
  else if (SMALL_INTVAL (value))
    return m88k_or;
  else if (SMALL_INTVAL (-value))
    return m88k_subu;
  else if (mode == HImode)
    return m88k_or_lo16;
  else if (mode == QImode)
    return m88k_or_lo8;
  else if ((value & 0xffff) == 0)
    return m88k_oru_hi16;
  else if (integer_ok_for_set (value))
    return m88k_set;
  else
    return m88k_oru_or;
}

int
integer_ok_for_set (value)
     register unsigned value;
{
  /* All the "one" bits must be contiguous.  If so, MASK + 1 will be
     a power of two or zero.  */
  register unsigned mask = (value | (value - 1));
  return (value && POWER_OF_2_or_0 (mask + 1));
}

char *
output_load_const_int (mode, operands)
     enum machine_mode mode;
     rtx *operands;
{
  static char *patterns[] =
    { "or %0,%#r0,0",
      "or %0,%#r0,%1",
      "subu %0,%#r0,%n1",
      "or %0,%#r0,%h1",
      "or %0,%#r0,%q1",
      "set %0,%#r0,%s1",
      "or.u %0,%#r0,%X1",
      "or.u %0,%#r0,%X1\n\tor %0,%0,%x1",
    };

  if (! REG_P (operands[0])
      || GET_CODE (operands[1]) != CONST_INT)
    abort ();
  return patterns[classify_integer (mode, INTVAL (operands[1]))];
}

/* These next two routines assume that floating point numbers are represented
   in a manner which is consistent between host and target machines.  */

char *
output_load_const_float (operands)
     rtx *operands;
{
  /* These can return 0 under some circumstances when cross-compiling.  */
  operands[0] = operand_subword (operands[0], 0, 0, SFmode);
  operands[1] = operand_subword (operands[1], 0, 0, SFmode);

  return output_load_const_int (SImode, operands);
}

char *
output_load_const_double (operands)
     rtx *operands;
{
  rtx latehalf[2];

  /* These can return zero on some cross-compilers, but there's nothing
     we can do about it.  */
  latehalf[0] = operand_subword (operands[0], 1, 0, DFmode);
  latehalf[1] = operand_subword (operands[1], 1, 0, DFmode);

  operands[0] = operand_subword (operands[0], 0, 0, DFmode);
  operands[1] = operand_subword (operands[1], 0, 0, DFmode);

  output_asm_insn (output_load_const_int (SImode, operands), operands);

  operands[0] = latehalf[0];
  operands[1] = latehalf[1];

  return output_load_const_int (SImode, operands);
}

char *
output_load_const_dimode (operands)
     rtx *operands;
{
  rtx latehalf[2];

  latehalf[0] = operand_subword (operands[0], 1, 0, DImode);
  latehalf[1] = operand_subword (operands[1], 1, 0, DImode);

  operands[0] = operand_subword (operands[0], 0, 0, DImode);
  operands[1] = operand_subword (operands[1], 0, 0, DImode);

  output_asm_insn (output_load_const_int (SImode, operands), operands);

  operands[0] = latehalf[0];
  operands[1] = latehalf[1];

  return output_load_const_int (SImode, operands);
}

/* Emit insns to move operands[1] into operands[0].

   Return 1 if we have written out everything that needs to be done to
   do the move.  Otherwise, return 0 and the caller will emit the move
   normally.  */

int
emit_move_sequence (operands, mode)
     rtx *operands;
     enum machine_mode mode;
{
  register rtx operand0 = operands[0];
  register rtx operand1 = operands[1];

  /* Handle most common case first: storing into a register.  */
  if (register_operand (operand0, mode))
    {
      if (register_operand (operand1, mode)
	  || (GET_CODE (operand1) == CONST_INT && SMALL_INT (operand1))
	  || GET_CODE (operand1) == HIGH
	  /* Only `general_operands' can come here, so MEM is ok.  */
	  || GET_CODE (operand1) == MEM)
	{
	  /* Run this case quickly.  */
	  emit_insn (gen_rtx (SET, VOIDmode, operand0, operand1));
	  return 1;
	}
    }
  else if (GET_CODE (operand0) == MEM)
    {
      if (register_operand (operand1, mode) || operand1 == const0_rtx)
	{
	  /* Run this case quickly.  */
	  emit_insn (gen_rtx (SET, VOIDmode, operand0, operand1));
	  return 1;
	}
      if (! reload_in_progress && ! reload_completed)
	{
	  operands[0] = validize_mem (operand0);
	  operands[1] = operand1 = force_reg (mode, operand1);
	}
    }

  /* Simplify the source if we need to.  */
  if (GET_CODE (operand1) != HIGH && immediate_operand (operand1, mode))
    {
	if (GET_CODE (operand1) != CONST_INT
	    && GET_CODE (operand1) != CONST_DOUBLE)
	  {
	    rtx temp = ((reload_in_progress || reload_completed)
			? operand0 : gen_reg_rtx (Pmode));
	    operands[1] = legitimize_address (flag_pic
					      && symbolic_address_p (operand1),
					      operand1, temp);
	    if (mode != SImode)
	      operands[1] = gen_rtx (SUBREG, mode, operands[1], 0);
	  }
    }

  /* Now have insn-emit do whatever it normally does.  */
  return 0;
}

/* Return a legitimate reference for ORIG (either an address or a MEM) using
   the register REG.  If PIC and the address is already position-independent,
   use ORIG.  */

struct rtx_def *
legitimize_address (pic, orig, reg)
     int pic;
     rtx orig;
     rtx reg;
{
  rtx addr = (GET_CODE (orig) == MEM ? XEXP (orig, 0) : orig);
  rtx new = orig;
  rtx temp;

  if (pic)
    {
      if (GET_CODE (addr) == SYMBOL_REF
	  || GET_CODE (addr) == LABEL_REF)
	{
	  if (reg == 0) abort ();

	  if (flag_pic == 2)
	    {
	      emit_insn (gen_rtx (SET, VOIDmode,
				  reg, gen_rtx (HIGH, SImode, addr)));
	      emit_insn (gen_rtx (SET, VOIDmode,
				  reg, gen_rtx (LO_SUM, SImode, reg, addr)));
	      addr = reg;
	    }
	  new = gen_rtx (MEM, Pmode,
			 gen_rtx (PLUS, SImode,
				  pic_offset_table_rtx, addr));
	  current_function_uses_pic_offset_table = 1;
	  RTX_UNCHANGING_P (new) = 1;
	  {
	    rtx insn = emit_move_insn (reg, new);
	    /* Put a REG_EQUAL note on this insn, so that it can be optimized
	       by loop.  */
	    REG_NOTES (insn) = gen_rtx (EXPR_LIST, REG_EQUAL, orig,
					REG_NOTES (insn));
	  }
	  new = reg;
	}
      else if (GET_CODE (addr) == CONST)
	{
	  rtx base, offset;

	  if (GET_CODE (XEXP (addr, 0)) == PLUS
	      && XEXP (XEXP (addr, 0), 0) == pic_offset_table_rtx)
	    return orig;

	  if (reg == 0)
	    abort ();

	  if (GET_CODE (XEXP (addr, 0)) != PLUS) abort ();

	  base = legitimize_address (1, XEXP (XEXP (addr, 0), 0), reg);
	  addr = legitimize_address (1, XEXP (XEXP (addr, 0), 1),
				     base == reg ? 0 : reg);

	  if (GET_CODE (addr) == CONST_INT)
	    new = plus_constant_for_output (base, INTVAL (addr));
	  else
	    new = gen_rtx (PLUS, SImode, base, addr);
	  /* Should we set special REG_NOTEs here?  */
	}
    }
  else if (! SHORT_ADDRESS_P (addr, temp))
    {
      emit_insn (gen_rtx (SET, VOIDmode,
			  reg, gen_rtx (HIGH, SImode, addr)));
      new = gen_rtx (LO_SUM, SImode, reg, addr);
    }

  if (new != orig
      && GET_CODE (orig) == MEM)
    {
      new = gen_rtx (MEM, GET_MODE (orig), new);
      RTX_UNCHANGING_P (new) = RTX_UNCHANGING_P (orig);
      MEM_VOLATILE_P (new) = MEM_VOLATILE_P (orig);
      MEM_IN_STRUCT_P (new) = MEM_IN_STRUCT_P (orig);
    }
  return new;
}

/* Support functions for code to emit a block move.  There are four methods
   used to perform the block move:
   + call memcpy
   + call the looping library function, e.g. __movstrSI64n8
   + call a non-looping library function, e.g. __movstrHI15x11
   + produce an inline sequence of ld/st instructions

   The parameters below describe the library functions produced by
   movstr-m88k.sh.  */

#define MOVSTR_LOOP	64 /* __movstrSI64n68 .. __movstrSI64n8 */
#define MOVSTR_QI	16 /* __movstrQI16x16 .. __movstrQI16x2 */
#define MOVSTR_HI	48 /* __movstrHI48x48 .. __movstrHI48x4 */
#define MOVSTR_SI	96 /* __movstrSI96x96 .. __movstrSI96x8 */
#define MOVSTR_ODD_SI	48 /* __movstrSI47x47 .. __movstrSI47x11,
			      __movstrSI46x46 .. __movstrSI46x10,
			      __movstrSI45x45 .. __movstrSI45x9 */
#define MOVSTR_ODD_HI	16 /* __movstrHI15x15 .. __movstrHI15x5 */

/* Break even points where memcpy will do just as well.  */
#define MOVSTR_QI_LIMIT	13
#define MOVSTR_HI_LIMIT	38
#define MOVSTR_SI_LIMIT	MOVSTR_SI

static enum machine_mode mode_from_bytes[] =
			      {VOIDmode, QImode, HImode, VOIDmode, SImode};
static int max_from_bytes[] = {0, MOVSTR_QI, MOVSTR_HI, 0, MOVSTR_SI};
static int all_from_bytes[] = {0, MOVSTR_QI, MOVSTR_ODD_HI, 0, MOVSTR_ODD_SI};
static int best_from_bytes[] =
		{0, MOVSTR_QI_LIMIT, MOVSTR_HI_LIMIT, 0, MOVSTR_SI_LIMIT};

static void block_move_loop ();
static void block_move_no_loop ();
static void block_move_sequence ();

/* Emit code to perform a block move.  Choose the best method.

   OPERANDS[0] is the destination.
   OPERANDS[1] is the source.
   OPERANDS[2] is the size.
   OPERANDS[3] is the alignment safe to use.  */

void
expand_block_move (dest_mem, src_mem, operands)
     rtx dest_mem;
     rtx src_mem;
     rtx *operands;
{
  int align = INTVAL (operands[3]);
  int constp = (GET_CODE (operands[2]) == CONST_INT);
  int bytes = (constp ? INTVAL (operands[2]) : 0);

  if (constp && bytes <= 0)
    return;

  /* Determine machine mode to do move with.  */
  if (align > 4)
    align = 4;
  else if (align <= 0 || align == 3)
    abort ();	/* block move invalid alignment.  */

  if (constp && bytes <= 3 * align)
    block_move_sequence (operands[0], dest_mem, operands[1], src_mem,
			 bytes, align, 0);

  else if (constp && bytes <= best_from_bytes[align])
    block_move_no_loop (operands[0], dest_mem, operands[1], src_mem,
			bytes, align);

  else if (constp && align == 4)
    block_move_loop (operands[0], dest_mem, operands[1], src_mem,
		     bytes, align);

  else
    {
#ifdef TARGET_MEM_FUNCTIONS
      emit_library_call (gen_rtx (SYMBOL_REF, Pmode, "memcpy"), 0,
			 VOIDmode, 3,
			 operands[0], Pmode,
			 operands[1], Pmode,
			 operands[2], SImode);
#else
      emit_library_call (gen_rtx (SYMBOL_REF, Pmode, "bcopy"), 0,
			 VOIDmode, 3,
			 operands[1], Pmode,
			 operands[0], Pmode,
			 operands[2], SImode);
#endif
    }
}

/* Emit code to perform a block move by calling a looping movstr library
   function.  SIZE and ALIGN are known constants.  DEST and SRC are
   registers.  */

static void
block_move_loop (dest, dest_mem, src, src_mem, size, align)
     rtx dest, dest_mem;
     rtx src, src_mem;
     int size;
     int align;
{
  enum machine_mode mode;
  int count;
  int units;
  int remainder;
  rtx offset_rtx;
  rtx value_rtx;
  char entry[30];
  tree entry_name;

  /* Determine machine mode to do move with.  */
  if (align != 4)
    abort ();

  /* Determine the structure of the loop.  */
  count = size / MOVSTR_LOOP;
  units = (size - count * MOVSTR_LOOP) / align;

  if (units < 2)
    {
      count--;
      units += MOVSTR_LOOP / align;
    }

  if (count <= 0)
    {
      block_move_no_loop (dest, dest_mem, src, src_mem, size, align);
      return;
    }

  remainder = size - count * MOVSTR_LOOP - units * align;

  mode = mode_from_bytes[align];
  sprintf (entry, "__movstr%s%dn%d",
	   GET_MODE_NAME (mode), MOVSTR_LOOP, units * align);
  entry_name = get_identifier (entry);

  offset_rtx = gen_rtx (CONST_INT, VOIDmode,
			MOVSTR_LOOP + (1 - units) * align);

  value_rtx = gen_rtx (MEM, mode,
		       gen_rtx (PLUS, Pmode,
				gen_rtx (REG, Pmode, 3),
				offset_rtx));
  RTX_UNCHANGING_P (value_rtx) = RTX_UNCHANGING_P (src_mem);
  MEM_VOLATILE_P (value_rtx) = MEM_VOLATILE_P (src_mem);
  MEM_IN_STRUCT_P (value_rtx) = MEM_IN_STRUCT_P (src_mem);

  emit_insn (gen_call_block_move_loop
	     (gen_rtx (SYMBOL_REF, Pmode, IDENTIFIER_POINTER (entry_name)),
	      dest, src, offset_rtx, value_rtx,
	      gen_rtx (REG, GET_MODE (value_rtx), ((units & 1) ? 4 : 5)),
	      gen_rtx (CONST_INT, VOIDmode, count)));

  if (remainder)
    block_move_sequence (gen_rtx (REG, Pmode, 2), dest_mem,
			 gen_rtx (REG, Pmode, 3), src_mem,
			 remainder, align, MOVSTR_LOOP + align);
}

/* Emit code to perform a block move by calling a non-looping library
   function.  SIZE and ALIGN are known constants.  DEST and SRC are
   registers.  OFFSET is the known starting point for the output pattern.  */

static void
block_move_no_loop (dest, dest_mem, src, src_mem, size, align)
     rtx dest, dest_mem;
     rtx src, src_mem;
     int size;
     int align;
{
  enum machine_mode mode = mode_from_bytes[align];
  int units = size / align;
  int remainder = size - units * align;
  int most;
  int evenp;
  rtx offset_rtx;
  rtx value_rtx;
  char entry[30];
  tree entry_name;

  if (remainder && size <= all_from_bytes[align])
    {
      most = all_from_bytes[align] - (align - remainder);
      remainder = 0;
    }
  else
    {
      most = max_from_bytes[align];
    }

  sprintf (entry, "__movstr%s%dx%d",
	   GET_MODE_NAME (mode), most, size - remainder);
  entry_name = get_identifier (entry);

  offset_rtx = gen_rtx (CONST_INT, VOIDmode, most - (size - remainder));

  value_rtx = gen_rtx (MEM, mode,
		       gen_rtx (PLUS, Pmode,
				gen_rtx (REG, Pmode, 3),
				offset_rtx));
  RTX_UNCHANGING_P (value_rtx) = RTX_UNCHANGING_P (src_mem);
  MEM_VOLATILE_P (value_rtx) = MEM_VOLATILE_P (src_mem);
  MEM_IN_STRUCT_P (value_rtx) = MEM_IN_STRUCT_P (src_mem);

  evenp = ((most - (size - remainder)) / align) & 1;

  emit_insn (gen_call_block_move
	     (gen_rtx (SYMBOL_REF, Pmode, IDENTIFIER_POINTER (entry_name)),
	      dest, src, offset_rtx, value_rtx,
	      gen_rtx (REG, GET_MODE (value_rtx), (evenp ? 4 : 5))));

  if (remainder)
    block_move_sequence (gen_rtx (REG, Pmode, 2), dest_mem,
			 gen_rtx (REG, Pmode, 3), src_mem,
			 remainder, align, most);
}

/* Emit code to perform a block move with an offset sequence of ld/st
   instructions (..., ld 0, st 1, ld 1, st 0, ...).  SIZE and ALIGN are
   known constants.  DEST and SRC are registers.  OFFSET is the known
   starting point for the output pattern.  */

static void
block_move_sequence (dest, dest_mem, src, src_mem, size, align, offset)
     rtx dest, dest_mem;
     rtx src, src_mem;
     int size;
     int align;
     int offset;
{
  rtx temp[2];
  enum machine_mode mode[2];
  int amount[2];
  int active[2];
  int phase = 0;
  int next;
  int offset_ld = offset;
  int offset_st = offset;

  active[0] = active[1] = FALSE;

  /* Establish parameters for the first load and for the second load if
     it is known to be the same mode as the first.  */
  amount[0] = amount[1] = align;
  mode[0] = mode_from_bytes[align];
  temp[0] = gen_reg_rtx (mode[0]);
  if (size >= 2 * align)
    {
      mode[1] = mode[0];
      temp[1] = gen_reg_rtx (mode[1]);
    }

  do
    {
      rtx srcp, dstp;
      next = phase;
      phase = !phase;

      if (size > 0)
	{
	  /* Change modes as the sequence tails off.  */
	  if (size < amount[next])
	    {
	      amount[next] = (size >= 2 ? 2 : 1);
	      mode[next] = mode_from_bytes[amount[next]];
	      temp[next] = gen_reg_rtx (mode[next]);
	    }
	  size -= amount[next];
	  srcp = gen_rtx (MEM, mode[next],
			  gen_rtx (PLUS, Pmode, src,
				   gen_rtx (CONST_INT, SImode, offset_ld)));
	  RTX_UNCHANGING_P (srcp) = RTX_UNCHANGING_P (src_mem);
	  MEM_VOLATILE_P (srcp) = MEM_VOLATILE_P (src_mem);
	  MEM_IN_STRUCT_P (srcp) = MEM_IN_STRUCT_P (src_mem);
	  emit_move_insn (temp[next], srcp);
	  offset_ld += amount[next];
	  active[next] = TRUE;
	}

      if (active[phase])
	{
	  active[phase] = FALSE;
	  dstp = gen_rtx (MEM, mode[phase],
			  gen_rtx (PLUS, Pmode, dest,
				   gen_rtx (CONST_INT, SImode, offset_st)));
	  RTX_UNCHANGING_P (dstp) = RTX_UNCHANGING_P (dest_mem);
	  MEM_VOLATILE_P (dstp) = MEM_VOLATILE_P (dest_mem);
	  MEM_IN_STRUCT_P (dstp) = MEM_IN_STRUCT_P (dest_mem);
	  emit_move_insn (dstp, temp[phase]);
	  offset_st += amount[phase];
	}
    }
  while (active[next]);
}

/* Emit the code to do an AND operation.  */

char *
output_and (operands)
     rtx operands[];
{
  unsigned int value;

  if (REG_P (operands[2]))
    return "and %0,%1,%2";

  value = INTVAL (operands[2]);
  if (SMALL_INTVAL (value))
    return "mask %0,%1,%2";
  else if ((value & 0xffff0000) == 0xffff0000)
    return "and %0,%1,%x2";
  else if ((value & 0xffff) == 0xffff)
    return "and.u %0,%1,%X2";
  else if ((value & 0xffff) == 0)
    return "mask.u %0,%1,%X2";
  else if (integer_ok_for_set (~value))
    return "clr %0,%1,%S2";
  else
    return "and.u %0,%1,%X2\n\tand %0,%0,%x2";
}

/* Emit the code to do an inclusive OR operation.  */

char *
output_ior (operands)
     rtx operands[];
{
  unsigned int value;

  if (REG_P (operands[2]))
    return "or %0,%1,%2";

  value = INTVAL (operands[2]);
  if (SMALL_INTVAL (value))
    return "or %0,%1,%2";
  else if ((value & 0xffff) == 0)
    return "or.u %0,%1,%X2";
  else if (integer_ok_for_set (value))
    return "set %0,%1,%s2";
  else
    return "or.u %0,%1,%X2\n\tor %0,%0,%x2";
}

/* Emit the instructions for doing an XOR.  */

char *
output_xor (operands)
     rtx operands[];
{
  unsigned int value;

  if (REG_P (operands[2]))
    return "xor %0,%1,%2";

  value = INTVAL (operands[2]);
  if (SMALL_INTVAL (value))
    return "xor %0,%1,%2";
  else if ((value & 0xffff) == 0)
    return "xor.u %0,%1,%X2";
  else
    return "xor.u %0,%1,%X2\n\txor %0,%0,%x2";
}

/* Output a call.  Normally this is just bsr or jsr, but this also deals with
   accomplishing a branch after the call by incrementing r1.  This requires
   that various assembler bugs be accommodated.  The 4.30 DG/UX assembler
   requires that forward references not occur when computing the difference of
   two labels.  The [version?] Motorola assembler computes a word difference.
   No doubt there's more to come!

   It would seem the same idea could be used to tail call, but in this case,
   the epilogue will be non-null.  */

static rtx sb_name = 0;
static rtx sb_high = 0;
static rtx sb_low = 0;

char *
output_call (operands, addr)
     rtx operands[];
     rtx addr;
{
  operands[0] = addr;
  if (final_sequence)
    {
      rtx jump;

      /* This can be generalized, but there is currently no need.  */
      if (XVECLEN (final_sequence, 0) != 2)
	abort ();

      jump = XVECEXP (final_sequence, 0, 1);
      if (GET_CODE (jump) == JUMP_INSN)
	{
	  rtx low, high;
	  char *last;
	  rtx dest = XEXP (SET_SRC (PATTERN (jump)), 0);
	  int delta = 4 * (insn_addresses[INSN_UID (dest)]
			   - insn_addresses[INSN_UID (jump)]);
#if (MONITOR_GCC & 0x2) /* How often do long branches happen?  */
	  if ((unsigned) (delta + 0x8000) >= 0x10000)
	    warning ("Internal gcc monitor: short-branch(%x)", delta);
#endif

	  /* Delete the jump.  */
	  PUT_CODE (jump, NOTE);
	  NOTE_LINE_NUMBER (jump) = NOTE_INSN_DELETED;
	  NOTE_SOURCE_FILE (jump) = 0;

	  /* If we loose, we must use the non-delay form.  This is unlikely
	     to ever happen.  If it becomes a problem, claim that a call
	     has two delay slots and only the second can be filled with
	     a jump.  */
#ifdef AS_BUG_IMMEDIATE_LABEL /* The assembler restricts immediate values.  */
	  if (! ADD_INTVAL (delta * 2))
#else
	  if (! ADD_INTVAL (delta))
#endif
	    {
	      operands[1] = dest;
	      return (REG_P (addr)
		      ? "jsr %0\n\tbr %l1"
		      : (flag_pic
			 ? "bsr %0#plt\n\tbr %l1"
			 : "bsr %0\n\tbr %l1"));
	    }

	  /* Output the short branch form.  */
	  output_asm_insn ((REG_P (addr)
			    ? "jsr.n %0"
			    : (flag_pic ? "bsr.n %0#plt" : "bsr.n %0")),
			   operands);

	  operands[0] = gen_label_rtx ();
	  operands[1] = gen_label_rtx ();
	  if (delta < 0)
	    {
	      low = dest;
	      high = operands[1];
	      last = "subu %#r1,%#r1,%l0\n%l1:";
	    }
	  else
	    {
	      low = operands[1];
	      high = dest;
	      last = "addu %#r1,%#r1,%l0\n%l1:";
	    }

	  /* Record the values to be computed later as "def name,high-low".  */
	  sb_name = gen_rtx (EXPR_LIST, VOIDmode, operands[0], sb_name);
	  sb_high = gen_rtx (EXPR_LIST, VOIDmode, high, sb_high);
	  sb_low = gen_rtx (EXPR_LIST, VOIDmode, low, sb_low);

	  return last;
	}
    }
  return (REG_P (addr)
	  ? "jsr%. %0"
	  : (flag_pic ? "bsr%. %0#plt" : "bsr%. %0"));
}

static void
output_short_branch_defs (stream)
     FILE *stream;
{
  char name[256], high[256], low[256];

  for (; sb_name && sb_high && sb_low;
       sb_name = XEXP (sb_name, 1),
       sb_high = XEXP (sb_high, 1),
       sb_low = XEXP (sb_low, 1))
    {
      ASM_GENERATE_INTERNAL_LABEL
	(name, "L", CODE_LABEL_NUMBER (XEXP (sb_name, 0)));
      ASM_GENERATE_INTERNAL_LABEL
	(high, "L", CODE_LABEL_NUMBER (XEXP (sb_high, 0)));
      ASM_GENERATE_INTERNAL_LABEL
	(low, "L", CODE_LABEL_NUMBER (XEXP (sb_low, 0)));
      /* This will change as the assembler requirements become known.  */
      fprintf (stream, "\t%s\t %s,%s-%s\n",
	       DEF_ASM_OP, &name[1], &high[1], &low[1]);
    }
  if (sb_name || sb_high || sb_low)
    abort ();
}

/* Report errors on floating point, if we are given NaN's, or such.  Leave
   the number as is, though, since we output the number in hex, and the
   assembler won't choke on it.  */

void
check_float_value (mode, value)
     enum machine_mode mode;
     REAL_VALUE_TYPE value;
{
  union {
    REAL_VALUE_TYPE d;
    struct {
      unsigned sign	    :  1;
      unsigned exponent  : 11;
      unsigned mantissa1 : 20;
      unsigned mantissa2;
    } s;
  } u;

  if (mode == DFmode)
    {
      u.d = value;
      if (u.s.mantissa1 != 0 || u.s.mantissa2 != 0)
	{
	  if (u.s.exponent == 0x7ff)	/* Not a Number */
	    warning ("floating point number is not valid for IEEE double precision");
	  else if (u.s.exponent == 0)
	    warning ("denormalized double precision floating point number");
	}
    }
  else if (mode == SFmode)
    {
      u.d = REAL_VALUE_TRUNCATE (mode, value);
      if (u.s.mantissa1 != 0 || u.s.mantissa2 != 0)
	{
	  if (u.s.exponent == 0x7ff)	/* Not a Number */
	    warning ("floating point number is not valid for IEEE double precision");
	  else if (u.s.exponent == 0)
	    warning ("denormalized single precision floating point number");
	}
      else if (u.s.exponent == 0x7ff)	/* Infinity */
	warning ("floating point number exceeds range of `float'");
    }
}

/* Return true if the operand is a power of two and is a floating
   point type (to optimize division by power of two into multiplication).  */

int
real_power_of_2_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  union {
    REAL_VALUE_TYPE d;
    int i[sizeof (REAL_VALUE_TYPE) / sizeof (int)];
    struct {				/* IEEE double precision format */
      unsigned sign	 :  1;
      unsigned exponent  : 11;
      unsigned mantissa1 : 20;
      unsigned mantissa2;
    } s;
    struct {				/* IEEE double format to quick check */
      unsigned sign	 :  1;		/* if it fits in a float */
      unsigned exponent1 :  4;
      unsigned exponent2 :  7;
      unsigned mantissa1 : 20;
      unsigned mantissa2;
    } s2;
  } u;

  if (GET_MODE (op) != DFmode && GET_MODE (op) != SFmode)
    return 0;

  if (GET_CODE (op) != CONST_DOUBLE)
    return 0;

  u.i[0] = CONST_DOUBLE_LOW  (op);
  u.i[1] = CONST_DOUBLE_HIGH (op);

  if (u.s.mantissa1 != 0 || u.s.mantissa2 != 0	/* not a power of two */
      || u.s.exponent == 0			/* constant 0.0 */
      || u.s.exponent == 0x7ff			/* NAN */
      || (u.s2.exponent1 != 0x8 && u.s2.exponent1 != 0x7))
    return 0;					/* const won't fit in float */

  return 1;
}

/* Make OP legitimate for mode MODE.  Currently this only deals with DFmode
   operands, putting them in registers and making CONST_DOUBLE values
   SFmode where possible.  */

struct rtx_def *
legitimize_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  rtx temp;
  union {
    union real_extract r;
    struct {				/* IEEE double precision format */
      unsigned sign	 :  1;
      unsigned exponent  : 11;
      unsigned mantissa1 : 20;
      unsigned mantissa2;
    } d;
    struct {				/* IEEE double format to quick check */
      unsigned sign	 :  1;		/* if it fits in a float */
      unsigned exponent1 :  4;
      unsigned exponent2 :  7;
      unsigned mantissa1 : 20;
      unsigned mantissa2;
    } s;
  } u;

  if (GET_CODE (op) == REG || mode != DFmode)
    return op;

  if (GET_CODE (op) == CONST_DOUBLE)
    {
      bcopy (&CONST_DOUBLE_LOW (op), &u.r, sizeof u);
      if (u.d.exponent != 0x7ff /* NaN */
	  && u.d.mantissa2 == 0 /* Mantissa fits */
	  && (u.s.exponent1 == 0x8 || u.s.exponent1 == 0x7) /* Exponent fits */
	  && (temp = simplify_unary_operation (FLOAT_TRUNCATE, SFmode,
					       op, mode)) != 0)
	return gen_rtx (FLOAT_EXTEND, mode, force_reg (SFmode, temp));
    }
  else if (register_operand (op, mode))
    return op;

  return force_reg (mode, op);
}

/* Return true if OP is a suitable input for a move insn.  */

int
move_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (register_operand (op, mode))
    return 1;
  if (GET_CODE (op) == CONST_INT)
    return (classify_integer (mode, INTVAL (op)) < m88k_oru_hi16);
  if (GET_MODE (op) != mode)
    return 0;
  if (GET_CODE (op) == SUBREG)
    op = SUBREG_REG (op);
  if (GET_CODE (op) != MEM)
    return 0;

  op = XEXP (op, 0);
  if (GET_CODE (op) == LO_SUM)
    return (REG_P (XEXP (op, 0))
	    && symbolic_address_p (XEXP (op, 1)));
  return memory_address_p (mode, op);
}

/* Return true if OP is suitable for a call insn.  */

int
call_address_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (REG_P (op) || symbolic_address_p (op));
}

/* Returns true if OP is either a symbol reference or a sum of a symbol
   reference and a constant.  */

int
symbolic_address_p (op)
     register rtx op;
{
  switch (GET_CODE (op))
    {
    case SYMBOL_REF:
    case LABEL_REF:
      return 1;

    case CONST:
      op = XEXP (op, 0);
      return ((GET_CODE (XEXP (op, 0)) == SYMBOL_REF
	       || GET_CODE (XEXP (op, 0)) == LABEL_REF)
	      && GET_CODE (XEXP (op, 1)) == CONST_INT);

    default:
      return 0;
    }
}

/* Return true if OP is a register or const0_rtx.  */

int
reg_or_0_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (op == const0_rtx || register_operand (op, mode));
}

/* Nonzero if OP is a valid second operand for an arithmetic insn.  */

int
arith_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (register_operand (op, mode)
	  || (GET_CODE (op) == CONST_INT && SMALL_INT (op)));
}

/* Return true if OP is a  register or 5 bit integer.  */

int
arith5_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (register_operand (op, mode)
	  || (GET_CODE (op) == CONST_INT && (unsigned) INTVAL (op) < 32));
}

int
arith32_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (register_operand (op, mode) || GET_CODE (op) == CONST_INT);
}

int
arith64_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (register_operand (op, mode)
	  || GET_CODE (op) == CONST_INT
	  || (GET_CODE (op) == CONST_DOUBLE && GET_MODE (op) == DImode));
}

int
int5_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (GET_CODE (op) == CONST_INT && (unsigned) INTVAL (op) < 32);
}

int
int32_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (GET_CODE (op) == CONST_INT);
}

/* Return true if OP is a register or a valid immediate operand for
   addu or subu.  */

int
add_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (register_operand (op, mode)
	  || (GET_CODE (op) == CONST_INT && ADD_INT (op)));
}

/* Nonzero if this is a bitmask filling the bottom bits, for optimizing and +
   shift left combinations into a single mak instruction.  */

int
mak_mask_p (value)
     int value;
{
  return (value && POWER_OF_2_or_0 (value + 1));
}

int
reg_or_bbx_mask_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  int value;
  if (register_operand (op, mode))
    return 1;
  if (GET_CODE (op) != CONST_INT)
    return 0;

  value = INTVAL (op);
  if (POWER_OF_2 (value))
    return 1;

  return 0;
}

/* Return true if OP is valid to use in the context of a floating
   point operation.  Special case 0.0, since we can use r0.  */

int
real_or_0_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (mode != SFmode && mode != DFmode)
    return 0;

  return (register_operand (op, mode)
	  || (GET_CODE (op) == CONST_DOUBLE
	      && op == CONST0_RTX (mode)));
}

/* Return true if OP is a relational operator.  */

int
relop (op, mode)
     rtx op;
     enum machine_mode mode;
{
  switch (GET_CODE (op))
    {
    case EQ:
    case NE:
    case LT:
    case LE:
    case GE:
    case GT:
    case LTU:
    case LEU:
    case GEU:
    case GTU:
      return 1;
    default:
      return 0;
    }
}

/* Return true if OP is a relational operator, and is not an unsigned
   relational operator.  */

int
relop_no_unsigned (op, mode)
     rtx op;
     enum machine_mode mode;
{
  switch (GET_CODE (op))
    {
    case EQ:
    case NE:
    case LT:
    case LE:
    case GE:
    case GT:
      /* @@ What is this test doing?  Why not use `mode'?  */
      if (GET_MODE_CLASS (GET_MODE (op)) == MODE_FLOAT
	  || GET_MODE (op) == DImode
	  || GET_MODE_CLASS (GET_MODE (XEXP (op, 0))) == MODE_FLOAT
	  || GET_MODE (XEXP (op, 0)) == DImode
	  || GET_MODE_CLASS (GET_MODE (XEXP (op, 1))) == MODE_FLOAT
	  || GET_MODE (XEXP (op, 1)) == DImode)
	return 0;
      return 1;
    default:
      return 0;
    }
}

/* Return true if the code of this rtx pattern is EQ or NE.  */

int
equality_op (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (GET_CODE (op) == EQ || GET_CODE (op) == NE);
}

/* Return true if the code of this rtx pattern is pc or label_ref.  */

int
pc_or_label_ref (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (GET_CODE (op) == PC || GET_CODE (op) == LABEL_REF);
}

/* Output to FILE the start of the assembler file.  */

struct option
{
  char *string;
  int *variable;
  int on_value;
};

static int
output_option (file, sep, type, name, indent, pos, max)
     FILE *file;
     char *sep;
     char *type;
     char *name;
     char *indent;
     int pos;
     int max;
{
  if (strlen (sep) + strlen (type) + strlen (name) + pos > max)
    {
      fprintf (file, indent);
      return fprintf (file, "%s%s", type, name);
    }
  return pos + fprintf (file, "%s%s%s", sep, type, name);
}

static struct { char *name; int value; } m_options[] = TARGET_SWITCHES;

static void
output_options (file, f_options, f_len, W_options, W_len,
		pos, max, sep, indent, term)
     FILE *file;
     struct option *f_options;
     struct option *W_options;
     int f_len, W_len;
     int pos;
     int max;
     char *indent;
     char *term;
{
  register int j;

  if (optimize)
    pos = output_option (file, sep, "-O", "", indent, pos, max);
  if (write_symbols != NO_DEBUG)
    pos = output_option (file, sep, "-g", "", indent, pos, max);
  if (flag_traditional)
    pos = output_option (file, sep, "-traditional", "", indent, pos, max);
  if (profile_flag)
    pos = output_option (file, sep, "-p", "", indent, pos, max);

  for (j = 0; j < f_len; j++)
    if (*f_options[j].variable == f_options[j].on_value)
      pos = output_option (file, sep, "-f", f_options[j].string,
			   indent, pos, max);

  for (j = 0; j < W_len; j++)
    if (*W_options[j].variable == W_options[j].on_value)
      pos = output_option (file, sep, "-W", W_options[j].string,
			   indent, pos, max);

  for (j = 0; j < sizeof m_options / sizeof m_options[0]; j++)
    if (m_options[j].name[0] != '\0'
	&& m_options[j].value > 0
	&& ((m_options[j].value & target_flags)
	    == m_options[j].value))
      pos = output_option (file, sep, "-m", m_options[j].name,
			   indent, pos, max);

  if (m88k_short_data)
    pos = output_option (file, sep, "-mshort-data-", m88k_short_data,
			 indent, pos, max);

  fprintf (file, term);
}

void
output_file_start (file, f_options, f_len, W_options, W_len)
     FILE *file;
     struct option *f_options;
     struct option *W_options;
     int f_len, W_len;
{
  register int pos;

  ASM_FIRST_LINE (file);
  output_file_directive (file, main_input_filename);
  /* Switch to the data section so that the coffsem symbol and the
     gcc2_compiled. symbol aren't in the text section.  */
  data_section ();
  ASM_COFFSEM (file);

  pos = fprintf (file, "\n; cc1 (%s) arguments:", VERSION_STRING);
  output_options (file, f_options, f_len, W_options, W_len,
		  pos, 75, " ", "\n; ", "\n\n");

  if (TARGET_IDENTIFY_REVISION)
    {
      char indent[256];

      time_t now = time ((time_t *)0);
      sprintf (indent, "]\"\n\t%s\t \"@(#)%s [", IDENT_ASM_OP, main_input_filename);
      fprintf (file, indent+3);
      pos = fprintf (file, "gcc %s, %.24s,", VERSION_STRING, ctime (&now));
      output_options (file, f_options, f_len, W_options, W_len,
		      pos, 150 - strlen (indent), " ", indent, "]\"\n\n");
    }
}

/* Output an ascii string.  */

void
output_ascii (file, p, size)
     FILE *file;
     unsigned char *p;
     int size;
{
  int i;

  register int num = 0;

  fprintf (file, "\t%s\t \"", ASCII_DATA_ASM_OP);
  for (i = 0; i < size; i++)
    {
      register int c = p[i];

      if (num > 48)
	{
	  fprintf (file, "\"\n\t%s\t \"", ASCII_DATA_ASM_OP);
	  num = 0;
	}

      if (c == '\"' || c == '\\')
	{
	  putc ('\\', file);
	  num++;
	}

      if (c >= ' ' && c < 0177)
	{
	  putc (c, file);
	  num++;
	}
      else
	{
	  fprintf (file, "\\%03o", c);
	  num += 4;
	  /* After an octal-escape, if a digit follows,
	     terminate one string constant and start another.
	     The Vax assembler fails to stop reading the escape
	     after three digits, so this is the only way we
	     can get it to parse the data properly.  */
	  if (i < size - 1 && p[i + 1] >= '0' && p[i + 1] <= '9')
	    num = 32767;	/* next pass will start a new string */
	}
    }
  fprintf (file, "\"\n");
}

/* Output a label (allows insn-output.c to be compiled without including
   m88k.c or needing to include stdio.h).  */

void
output_label (label_number)
     int label_number;
{
  ASM_OUTPUT_INTERNAL_LABEL (asm_out_file, "L", label_number);
}

/* Handle a pragma directive.  HANDLE_PRAGMA conspires to parse the input
   following #pragma into tokens based on yylex.  */

void
m88k_handle_pragma_token (string, token)
     char *string;
     tree token;
{
  static enum pragma_state
    {
      ps_start,
      ps_done,
      ps_bad,
      ps_weak,
      ps_name,
      ps_equals,
      ps_value
    } state;
  static char *name;
  static char *value;

  if (HANDLE_PRAGMA_WEAK)
    {
      if (string == 0)
	{
	  if (state == ps_name || state == ps_value)
	    {
	      fprintf (asm_out_file, "\t%s\t ", WEAK_ASM_OP);
	      ASM_OUTPUT_LABELREF (asm_out_file, name);
	      fputc ('\n', asm_out_file);
	      if (state == ps_value)
		{
		  fprintf (asm_out_file, "\t%s\t ", DEF_ASM_OP);
		  ASM_OUTPUT_LABELREF (asm_out_file, name);
		  fputc (',', asm_out_file);
		  ASM_OUTPUT_LABELREF (asm_out_file, value);
		  fputc ('\n', asm_out_file);
		}
	    }
	  else if (! (state == ps_done || state == ps_start))
	    warning ("ignoring malformed #pragma weak symbol [=value]");
	  state = ps_start;
	}
      else
	switch (state)
	  {
	  case ps_start:
	    if (token
		&& TREE_CODE (token) == IDENTIFIER_NODE
		&& !strcmp (IDENTIFIER_POINTER (token), "weak"))
	      state = ps_weak;
	    else
	      state = ps_done;
	    break;

	  case ps_weak:
	    if (token
		&& TREE_CODE (token) == IDENTIFIER_NODE)
	      {
		name = IDENTIFIER_POINTER (token);
		state = ps_name;
	      }
	    else
	      state = ps_bad;
	    break;

	  case ps_name:
	    state = (strcmp (string, "=") ? ps_bad : ps_equals);
	    break;

	  case ps_equals:
	    if (token
		&& TREE_CODE (token) == IDENTIFIER_NODE)
	      {
		value = IDENTIFIER_POINTER (token);
		state = ps_value;
	      }
	    else
	      state = ps_bad;
	    break;

	  case ps_value:
	    state = ps_bad;
	  case ps_bad:
	  case ps_done:
	    break;

	  default:
	    abort ();
	  }
    }
}

/* Generate the assembly code for function entry.

   The prologue is responsible for setting up the stack frame,
   initializing the frame pointer register, saving registers that must be
   saved, and allocating SIZE additional bytes of storage for the
   local variables.  SIZE is an integer.  FILE is a stdio
   stream to which the assembler code should be output.

   The label for the beginning of the function need not be output by this
   macro.  That has already been done when the macro is run.

   To determine which registers to save, the macro can refer to the array
   `regs_ever_live': element R is nonzero if hard register
   R is used anywhere within the function.  This implies the
   function prologue should save register R, but not if it is one
   of the call-used registers.

   On machines where functions may or may not have frame-pointers, the
   function entry code must vary accordingly; it must set up the frame
   pointer if one is wanted, and not otherwise.  To determine whether a
   frame pointer is in wanted, the macro can refer to the variable
   `frame_pointer_needed'.  The variable's value will be 1 at run
   time in a function that needs a frame pointer.

   On machines where an argument may be passed partly in registers and
   partly in memory, this macro must examine the variable
   `current_function_pretend_args_size', and allocate that many bytes
   of uninitialized space on the stack just underneath the first argument
   arriving on the stack.  (This may not be at the very end of the stack,
   if the calling sequence has pushed anything else since pushing the stack
   arguments.  But usually, on such machines, nothing else has been pushed
   yet, because the function prologue itself does all the pushing.)

   If `ACCUMULATE_OUTGOING_ARGS' is defined, the variable
   `current_function_outgoing_args_size' contains the size in bytes
   required for the outgoing arguments.  This macro must add that
   amount of uninitialized space to very bottom of the stack.

   The stack frame we use looks like this:

 caller                                                  callee
        |==============================================|
        |                caller's frame                |
        |==============================================|
        |     [caller's outgoing memory arguments]     |
        |==============================================|
        |  caller's outgoing argument area (32 bytes)  |
  sp -> |==============================================| <- ap
        |            [local variable space]            |
        |----------------------------------------------|
        |            [return address (r1)]             |
        |----------------------------------------------|
        |        [previous frame pointer (r30)]        |
        |==============================================| <- fp
        |       [preserved registers (r25..r14)]       |
        |==============================================|
        |    [dynamically allocated space (alloca)]    |
        |==============================================|
        |     [callee's outgoing memory arguments]     |
        |==============================================|
        | [callee's outgoing argument area (32 bytes)] |
        |==============================================| <- sp

  Notes:

  r1 and r30 must be saved if debugging.

  fp (if present) is located two words down from the local
  variable space.
  */

static void output_reg_adjust ();
static void preserve_registers ();
static void output_tdesc ();

static int  nregs;
static char save_regs[FIRST_PSEUDO_REGISTER];
static int  frame_laid_out;
static int  frame_size;
static int  variable_args_p;

extern char call_used_regs[];
extern int  current_function_pretend_args_size;
extern int  current_function_outgoing_args_size;
extern int  frame_pointer_needed;

#define FIRST_OCS_PRESERVE_REGISTER	14
#define LAST_OCS_PRESERVE_REGISTER	30

#define STACK_UNIT_BOUNDARY (STACK_BOUNDARY / BITS_PER_UNIT)
#define ROUND_CALL_BLOCK_SIZE(BYTES) \
  (((BYTES) + (STACK_UNIT_BOUNDARY - 1)) & ~(STACK_UNIT_BOUNDARY - 1))

/* Establish the position of the FP relative to the SP.  This is done
   either during FUNCTION_PROLOGUE or by INITIAL_ELIMINATION_OFFSET.  */

void
m88k_layout_frame ()
{
  int regno, sp_size;

  frame_laid_out++;

  bzero ((char *) &save_regs[0], sizeof (save_regs));
  sp_size = nregs = 0;
  frame_size = get_frame_size ();

  /* Since profiling requires a call, make sure r1 is saved.  */
  if (profile_flag)
    save_regs[1] = 1;

  /* If we are producing debug information, store r1 and r30 where the
     debugger wants to find them (r30 at r30+0, r1 at r30+4).  Space has
     already been reserved for r1/r30 in STARTING_FRAME_OFFSET.  */
  if (write_symbols != NO_DEBUG && !TARGET_OCS_FRAME_POSITION)
    save_regs[1] = 1;

  /* If there is a call, alloca is used, __builtin_alloca is used, or
     a dynamic-sized object is defined, add the 8 additional words
     for the callee's argument area.  The common denominator is that the
     FP is required.  may_call_alloca only gets calls to alloca;
     current_function_calls_alloca gets alloca and __builtin_alloca.  */
  if (regs_ever_live[1] || frame_pointer_needed)
    {
      save_regs[1] = 1;
      sp_size += REG_PARM_STACK_SPACE (0);
    }

  /* If we are producing PIC, save the addressing base register and r1.  */
  if (flag_pic && current_function_uses_pic_offset_table)
    {
      save_regs[PIC_OFFSET_TABLE_REGNUM] = 1;
      nregs++;
    }

  /* If a frame is requested, save the previous FP, and the return
     address (r1), so that a traceback can be done without using tdesc
     information.  */
  if (frame_pointer_needed)
    save_regs[FRAME_POINTER_REGNUM] = save_regs[1] = 1;

  /* Figure out which normal register(s) needs to be saved.  */
  for (regno = 2; regno < FRAME_POINTER_REGNUM; regno++)
    if (regs_ever_live[regno] && ! call_used_regs[regno])
      {
	save_regs[regno] = 1;
	nregs++;
      }

  /* Achieve greatest use of double memory ops.  Either we end up saving
     r30 or we use that slot to align the regsters we do save.  */
  if (nregs >= 2 && save_regs[1] && !save_regs[FRAME_POINTER_REGNUM])
    sp_size += 4;

  nregs += save_regs[1] + save_regs[FRAME_POINTER_REGNUM];
  sp_size += 4 * nregs;
  sp_size += current_function_outgoing_args_size;

  /* The first two saved registers are placed above the new frame pointer
     if any.  In the only case this matters, they are r1 and r30. */
  if (frame_pointer_needed || sp_size)
    {
      m88k_fp_offset = ROUND_CALL_BLOCK_SIZE (sp_size - STARTING_FRAME_OFFSET);
      m88k_stack_size = m88k_fp_offset + STARTING_FRAME_OFFSET;
    }
  else
    {
      m88k_stack_size = m88k_fp_offset = 0;
    }

  /* First, combine m88k_stack_size and size.  If m88k_stack_size is
     non-zero, align the frame size to 8 mod 16; otherwise align the
     frame size to 0 mod 16.  (If stacks are 8 byte aligned, this ends
     up as a NOP.  */
  {
    int need
      = ((m88k_stack_size ? STACK_UNIT_BOUNDARY - STARTING_FRAME_OFFSET : 0)
	 - (frame_size % STACK_UNIT_BOUNDARY));
    if (need)
      {
	if (need < 0)
	  need += STACK_UNIT_BOUNDARY;
	(void) assign_stack_local (BLKmode, need, BITS_PER_UNIT);
	frame_size = get_frame_size ();
      }
    m88k_stack_size
      = ROUND_CALL_BLOCK_SIZE (m88k_stack_size + frame_size
			       + current_function_pretend_args_size);
  }
}

/* Return true if this function is known to have a null epilogue.  */

int
null_epilogue ()
{
  if (! reload_completed)
    return 0;
  if (! frame_laid_out)
    m88k_layout_frame ();
  return (! frame_pointer_needed
	  && nregs == 0
	  && m88k_stack_size == 0);
}

/* Determine the number of instructions needed for the function epilogue.  */

#define MAX_EPILOGUE_DELAY_INSNS 4

static char epilogue_dead_regs[FIRST_PSEUDO_REGISTER];

delay_slots_for_epilogue ()
{
  register int insns = save_regs[1] + save_regs[FRAME_POINTER_REGNUM];
  register int regs = nregs - insns;

  if (regs > 3)
    insns += 1 + (regs & 1);
  else if (nregs == 4)
    /* This is a special cases of ld/ld/ld.d which has no start-up delay.  */
    return 0;

  if (insns)
    {
      bzero ((char *) &epilogue_dead_regs[0], sizeof (epilogue_dead_regs));
      epilogue_dead_regs[1] = save_regs[1];
      epilogue_dead_regs[STACK_POINTER_REGNUM] = frame_pointer_needed;
      epilogue_dead_regs[TEMP_REGNUM] = ! ADD_INTVAL (m88k_fp_offset);
    }

  return insns;
}

/* Return 1 if X is safe to use as an epilogue insn.  */

int
ok_for_epilogue_p (x)
     rtx x;
{
  register char *fmt;
  register int i, j;

  switch (GET_CODE (x))
    {
    case REG:
      for (i = REGNO (x), j = i + HARD_REGNO_NREGS (i, GET_MODE (x));
	   i < j;
	   i++)
	if (epilogue_dead_regs[i])
	  return 0;

    case CONST_INT:
    case CONST_DOUBLE:
    case CONST:
    case PC:
    case CC0:
    case LABEL_REF:
    case SYMBOL_REF:
    case CODE_LABEL:
      return 1;
    }

  fmt = GET_RTX_FORMAT (GET_CODE (x));
  for (i = GET_RTX_LENGTH (GET_CODE (x)) - 1; i >= 0; i--)
    {
      if (fmt[i] == 'e')
	{
	  if (!ok_for_epilogue_p (XEXP (x, i)))
	    return 0;
	}
      else if (fmt[i] == 'E')
	{
	  for (j = XVECLEN (x, i) - 1; j >= 0; j--)
	    if (!ok_for_epilogue_p (XVECEXP (x, i, j)))
	      return 0;
	}
    }
  return 1;
}

int
eligible_for_epilogue_delay (insn)
     rtx insn;
{
  switch (get_attr_type (insn))
    {
    case TYPE_STORE:
    case TYPE_LOADA:
    case TYPE_ARITH:
    case TYPE_MARITH:
    case TYPE_MSTORE:
      return ok_for_epilogue_p (PATTERN (insn));
    default:
      return 0;
    }
}

/* Determine if the current function has any references to the arg pointer.
   This is done indirectly by examining the DECL_ARGUMENTS' DECL_RTL.
   It is OK to return TRUE if there are no references, but FALSE must be
   correct.  */

static int
uses_arg_area_p ()
{
  register tree parm;

  if (current_function_decl == 0
      || current_function_varargs
      || variable_args_p)
    return 1;

  for (parm = DECL_ARGUMENTS (current_function_decl);
       parm;
       parm = TREE_CHAIN (parm))
    {
      if (DECL_RTL (parm) == 0
	  || GET_CODE (DECL_RTL (parm)) == MEM)
	return 1;

      if (DECL_INCOMING_RTL (parm) == 0
	  || GET_CODE (DECL_INCOMING_RTL (parm)) == MEM)
	return 1;
    }
  return 0;
}

void
m88k_output_prologue (stream, size)
     FILE *stream;
     int size;
{
  int old_fp_offset = m88k_fp_offset;
  int old_stack_size = m88k_stack_size;

  m88k_layout_frame ();
#if (MONITOR_GCC & 0x8) /* Watch for suspicious register elimination changes.  */
  if (frame_laid_out > 1)
    {
      if (old_fp_offset != m88k_fp_offset)
	warning ("Internal gcc error: FP offset has changed by %d bytes",
		 m88k_fp_offset - old_fp_offset);
      if (old_stack_size != m88k_stack_size)
	warning ("Internal gcc error: stack size has changed by %d bytes",
		 m88k_stack_size - old_stack_size);
    }
#endif
  frame_laid_out = 0;

  if (TARGET_OPTIMIZE_ARG_AREA
      && m88k_stack_size
      && ! uses_arg_area_p ())
    {
      /* The incoming argument area is used for stack space if it is not
	 used (or if -mno-use-arg-area is given).  */
      if ((m88k_stack_size -= REG_PARM_STACK_SPACE (0)) < 0)
	m88k_stack_size = 0;
    }

  if (m88k_stack_size)
    output_reg_adjust (stream, 31, 31, -m88k_stack_size, 0);

  if (nregs)
    preserve_registers (stream, m88k_fp_offset + 4, 1);

  if (frame_pointer_needed)
    output_reg_adjust (stream, 30, 31, m88k_fp_offset, 0);

  if (TARGET_OCS_DEBUG_INFO)
    PUT_OCS_FUNCTION_START (stream);

  if (flag_pic && save_regs[PIC_OFFSET_TABLE_REGNUM])
    {
      char label[256];

      if (! save_regs[1])
	fprintf (stream, "\tor\t %s,%s,0\n",
		 reg_names[TEMP_REGNUM], reg_names[1]);
      ASM_GENERATE_INTERNAL_LABEL (label, "Lab", m88k_function_number);
      fprintf (stream, "\tbsr.n\t %s\n", &label[1]);
      fprintf (stream, "\tor.u\t %s,%s,%shi16(%s#abdiff)\n",
	       reg_names[PIC_OFFSET_TABLE_REGNUM], reg_names[0],
	       m88k_pound_sign, &label[1]);
      ASM_OUTPUT_INTERNAL_LABEL (stream, "Lab", m88k_function_number);
      fprintf (stream, "\tor\t %s,%s,%slo16(%s#abdiff)\n",
	       reg_names[PIC_OFFSET_TABLE_REGNUM],
	       reg_names[PIC_OFFSET_TABLE_REGNUM],
	       m88k_pound_sign, &label[1]);
      fprintf (stream, "\taddu\t %s,%s,%s\n",
	       reg_names[PIC_OFFSET_TABLE_REGNUM],
	       reg_names[PIC_OFFSET_TABLE_REGNUM], reg_names[1]);
      if (! save_regs[1])
	fprintf (stream, "\tor\t %s,%s,0\n",
		 reg_names[1], reg_names[TEMP_REGNUM]);
    }

  m88k_prologue_done = 1;	/* it's ok now to put out ln directives */
}

/* This function generates the assembly code for function exit,
   on machines that need it.  Args are same as for FUNCTION_PROLOGUE.

   The function epilogue should not depend on the current stack pointer!
   It should use the frame pointer only, if there is a frame pointer.
   This is mandatory because of alloca; we also take advantage of it to
   omit stack adjustments before returning.  */

void
m88k_output_epilogue (stream, size)
     FILE *stream;
     int size;
{
  rtx insn = get_last_insn ();
#if (MONITOR_GCC & 0x4) /* What are interesting prologue/epilogue values?  */
  fprintf (stream, "; size = %d, m88k_fp_offset = %d, m88k_stack_size = %d\n",
	   size, m88k_fp_offset, m88k_stack_size);
#endif

  output_short_branch_defs (stream);

  if (TARGET_OCS_DEBUG_INFO)
    PUT_OCS_FUNCTION_END (stream);

  /* If the last insn was a BARRIER, we don't have to write any code.  */
  if (GET_CODE (insn) == NOTE)
    insn = prev_nonnote_insn (insn);
  if (insn && GET_CODE (insn) == BARRIER)
    {
      if (current_function_epilogue_delay_list)
	abort ();
    }
  else
    {
      if (frame_pointer_needed)
	output_reg_adjust (stream, 31, 30, -m88k_fp_offset, 0);

      if (nregs)
	preserve_registers (stream, m88k_fp_offset + 4, 0);

      output_reg_adjust (stream, 31, 31, m88k_stack_size, 1);
    }

  fprintf (stream, "\n");

  if (TARGET_OCS_DEBUG_INFO)
    output_tdesc (stream, m88k_fp_offset + 4);

  m88k_function_number++;
  m88k_prologue_done	= 0;		/* don't put out ln directives */
  variable_args_p	= 0;		/* has variable args */
}

/* Output code to STREAM to set DSTREG to SRCREG + AMOUNT.  Issue
   a return instruction and use it's delay slot based on RETURN_P.  */

static void
output_reg_adjust (stream, dstreg, srcreg, amount, return_p)
     FILE *stream;
     int dstreg, srcreg, amount, return_p;
{
  char *opname;
  char incr[256];

  if (amount < 0)
    {
      opname = "subu";
      amount = -amount;
    }
  else
    opname = "addu";

  if (amount == 0 && dstreg == srcreg)
    {
      if (return_p)
	fprintf (stream, "\tjmp\t %s\n", reg_names[1]);
      return;
    }
  else if (SMALL_INTVAL (amount))
    sprintf (incr, "\t%s\t %s,%s,%d", opname,
	     reg_names[dstreg], reg_names[srcreg], amount);
  else
    {
      rtx operands[2];

      operands[0] = gen_rtx (REG, SImode, TEMP_REGNUM);
      operands[1] = gen_rtx (CONST_INT, VOIDmode, amount);
      output_asm_insn (output_load_const_int (SImode, operands),
		       operands);
      sprintf (incr, "\t%s\t %s,%s,%s", opname,
	       reg_names[dstreg], reg_names[srcreg], reg_names[TEMP_REGNUM]);
    }

  if (!return_p)
    fprintf (stream, "%s\n", incr);
  else if (flag_delayed_branch)
    fprintf (stream, "\tjmp.n\t %s\n%s\n", reg_names[1], incr);
  else
    fprintf (stream, "%s\n\tjmp\t %s\n", incr, reg_names[1]);
}

/* Save/restore the preserve registers.  base is the highest offset from
   r31 at which a register is stored.  store_p is true if stores are to
   be done; otherwise loads.  When loading, output the epilogue delay
   insns.  */

static void
preserve_registers (stream, base, store_p)
     FILE *stream;
     int base;
     int store_p;
{
  int regno, offset;
  char *fmt = (store_p ? "\tst%s\t %s,%s,%d\n" : "\tld%s\t %s,%s,%d\n");
  struct mem_op {
    int regno;
    int nregs;
    int offset;
  } mem_op[FIRST_PSEUDO_REGISTER];
  struct mem_op *mo_ptr = mem_op;

  /* The 88open OCS mandates that preserved registers be stored in
     increasing order.  For compatibility with current practice,
     the order is r1, r30, then the preserve registers.  */

  offset = base;
  if (save_regs[1])
    {
      /* An extra word is given in this case to make best use of double
	 memory ops.  */
      if (nregs > 2 && !save_regs[FRAME_POINTER_REGNUM])
	offset -= 4;
      fprintf (stream, fmt, "", reg_names[1], reg_names[31], offset);
      offset -= 4;
      base = offset;
    }

  /* Walk the registers to save recording all single memory operations.  */
  for (regno = FRAME_POINTER_REGNUM; regno > 1; regno--)
    if (save_regs[regno])
      {
	if ((offset & 7) != 4 || (regno & 1) != 1 || !save_regs[regno-1])
	  {
	    mo_ptr->nregs = 1;
	    mo_ptr->regno = regno;
	    mo_ptr->offset = offset;
	    mo_ptr++;
	    offset -= 4;
	  }
        else
	  {
	    regno--;
	    offset -= 2*4;
	  }
      }

  /* Walk the registers to save recording all double memory operations.
     This avoids a delay in the epilogue (ld.d/ld).  */
  offset = base;
  for (regno = FRAME_POINTER_REGNUM; regno > 1; regno--)
    if (save_regs[regno])
      {
	if ((offset & 7) != 4 || (regno & 1) != 1 || !save_regs[regno-1])
	  {
	    offset -= 4;
	  }
        else
	  {
	    mo_ptr->nregs = 2;
	    mo_ptr->regno = regno-1;
	    mo_ptr->offset = offset-4;
	    mo_ptr++;
	    regno--;
	    offset -= 2*4;
	  }
      }
  mo_ptr->regno = 0;

  /* Output the delay insns interleaved with the memory operations.  */
  if (! store_p && current_function_epilogue_delay_list)
    {
      rtx delay_insns = current_function_epilogue_delay_list;
      rtx insn;

      /* The first delay insn goes after the restore of r1.  */
      if (save_regs[1])
	{
	  final_scan_insn (XEXP (delay_insns, 0), stream, 1, 0, 1);
	  delay_insns = XEXP (delay_insns, 1);
	}

      while (delay_insns)
	{
	  /* Find a memory operation that doesn't conflict with this insn.  */
	  for (mo_ptr = mem_op; mo_ptr->regno != 0; mo_ptr++)
	    {
	      if (mo_ptr->nregs)
		{
		  rtx ok_insns = delay_insns;
		  int i;

		  for (i = 0; i < mo_ptr->nregs; i++)
		    epilogue_dead_regs[mo_ptr->regno + i] = 1;

		  while (ok_insns)
		    {
		      insn = XEXP (ok_insns, 0);
		      ok_insns = XEXP (ok_insns, 1);

		      if (! ok_for_epilogue_p (PATTERN (insn)))
			{
			  for (i = 0; i < mo_ptr->nregs; i++)
			    epilogue_dead_regs[mo_ptr->regno + i] = 0;
			  insn = 0;
			  break; /* foreach delay insn */
			}
		    }
		  if (insn)
		    {
		      fprintf (stream, fmt, mo_ptr->nregs > 1 ? ".d" : "",
			       reg_names[mo_ptr->regno], reg_names[31],
			       mo_ptr->offset);
		      mo_ptr->nregs = 0;
		      break; /* foreach memory operation */
		    }
		}
	    }
	  final_scan_insn (XEXP (delay_insns, 0), stream, 1, 0, 1);
	  delay_insns = XEXP (delay_insns, 1);
	}
    }

  /* Output the memory operations.  */
  for (mo_ptr = mem_op; mo_ptr->regno; mo_ptr++)
    {
      if (mo_ptr->nregs)
	fprintf (stream, fmt, mo_ptr->nregs > 1 ? ".d" : "",
		 reg_names[mo_ptr->regno], reg_names[31], mo_ptr->offset);
    }
}

/* Convert the address expression REG to a CFA offset.  */

int
m88k_debugger_offset (reg, offset)
     register rtx reg;
     register int offset;
{
  if (GET_CODE (reg) == PLUS)
    {
      offset = INTVAL (XEXP (reg, 1));
      reg = XEXP (reg, 0);
    }

  /* Put the offset in terms of the CFA (arg pointer).  */
  if (reg == frame_pointer_rtx)
    offset += m88k_fp_offset - m88k_stack_size;
  else if (reg == stack_pointer_rtx)
    offset -= m88k_stack_size;
  else if (reg != arg_pointer_rtx)
    {
#if (MONITOR_GCC & 0x10) /* Watch for suspicious symbolic locations.  */
      if (! (GET_CODE (reg) == REG
	     && REGNO (reg) >= FIRST_PSEUDO_REGISTER))
	warning ("Internal gcc error: Can't express symbolic location");
#endif
      return 0;
    }

  return offset;
}

/* Output the 88open OCS proscribed text description information.
   The information is:
        0  8: zero
	0 22: info-byte-length (16 bytes)
	0  2: info-alignment (word 2)
	1 32: info-protocol (version 1)
	2 32: starting-address (inclusive, not counting prologue)
	3 32: ending-address (exclusive, not counting epilog)
	4  8: info-variant (version 1)
	4 17: register-save-mask (from register 14 to 30)
	4  1: zero
	4  1: return-address-info-discriminant
	4  5: frame-address-register
	5 32: frame-address-offset
	6 32: return-address-info
	7 32: register-save-offset */

static void
output_tdesc (file, offset)
     FILE *file;
     int offset;
{
  int regno, i;
  long mask, return_address_info, register_save_offset;
  char buf[256];

  for (mask = 0, i = 0, regno = FIRST_OCS_PRESERVE_REGISTER;
       regno <= LAST_OCS_PRESERVE_REGISTER;
       regno++)
    {
      mask <<= 1;
      if (save_regs[regno])
	{
	  mask |= 1;
	  i++;
	}
    }

  if (save_regs[1])
    {
      if (nregs > 2 && !save_regs[FRAME_POINTER_REGNUM])
	offset -= 4;
      return_address_info = - m88k_stack_size + offset;
      register_save_offset = return_address_info - i*4;
    }
  else
    {
      return_address_info = 1;
      register_save_offset = - m88k_stack_size + offset + 4 - i*4;
    }

  tdesc_section ();

  fprintf (file, "\t%s\t %d", INT_ASM_OP, (16 << 2) | 2 /* 8:0,22:16,2:2 */);
  fprintf (file, ",%d", flag_pic ? 2 : 1);

  ASM_GENERATE_INTERNAL_LABEL (buf, OCS_START_PREFIX, m88k_function_number);
  fprintf (file, ",%s%s", buf+1, flag_pic ? "#rel" : "");
  ASM_GENERATE_INTERNAL_LABEL (buf, OCS_END_PREFIX, m88k_function_number);
  fprintf (file, ",%s%s", buf+1, flag_pic ? "#rel" : "");

  fprintf (file, ",0x%x", /* 8:1,17:0x%.3x,1:0,1:%d,5:%d */
	   (1 << (17+1+1+5)) |
	   (mask << (1+1+5)) |
	   ((!!save_regs[1]) << 5) |
	   ((frame_pointer_needed
	      ? FRAME_POINTER_REGNUM
	      : STACK_POINTER_REGNUM)));

  fprintf (file, ",0x%x", (m88k_stack_size
			   - (frame_pointer_needed ? m88k_fp_offset : 0)));
  fprintf (file, ",0x%x", return_address_info);
  fprintf (file, ",0x%x\n", register_save_offset);

  text_section ();
}

/* Output assembler code to FILE to increment profiler label # LABELNO
   for profiling a function entry.  NAME is the mcount function name
   (varies), SAVEP indicates whether the parameter registers need to
   be saved and restored.  */

void
output_function_profiler (file, labelno, name, savep)
     FILE *file;
     int labelno;
     char *name;
     int savep;
{
  char label[256];
  char dbi[256];
  char *temp = (savep ? reg_names[2] : reg_names[10]);

  if (savep)
    {
      fprintf (file, "\tsubu\t %s,%s,64\n", reg_names[31], reg_names[31]);
      fprintf (file, "\tst.d\t %s,%s,32\n", reg_names[2], reg_names[31]);
      fprintf (file, "\tst.d\t %s,%s,40\n", reg_names[4], reg_names[31]);
      fprintf (file, "\tst.d\t %s,%s,48\n", reg_names[6], reg_names[31]);
      fprintf (file, "\tst.d\t %s,%s,56\n", reg_names[8], reg_names[31]);
    }

  ASM_GENERATE_INTERNAL_LABEL (label, "LP", labelno);
  if (flag_pic == 2)
    {
      fprintf (file, "\tor.u\t %s,%s,%shi16(%s#got_rel)\n",
	       temp, reg_names[0], m88k_pound_sign, &label[1]);
      fprintf (file, "\tor\t %s,%s,%slo16(%s#got_rel)\n",
	       temp, temp, m88k_pound_sign, &label[1]);
      sprintf (dbi, "\tld\t %s,%s,%s\n", temp,
	       reg_names[PIC_OFFSET_TABLE_REGNUM], temp);
    }
  else if (flag_pic)
    {
      sprintf (dbi, "\tld\t %s,%s,%s#got_rel\n", temp,
	       reg_names[PIC_OFFSET_TABLE_REGNUM], &label[1]);
    }
  else
    {
      fprintf (file, "\tor.u\t %s,%s,%shi16(%s)\n",
	       temp, reg_names[0], m88k_pound_sign, &label[1]);
      sprintf (dbi, "\tor\t %s,%s,%slo16(%s)\n",
	       temp, temp, m88k_pound_sign, &label[1]);
    }

  if (flag_pic)
    fprintf (file, "\tbsr.n\t %s#plt\n", name);
  else
    fprintf (file, "\tbsr.n\t %s\n", name);
  fputs (dbi, file);

  if (savep)
    {
      fprintf (file, "\tld.d\t %s,%s,32\n", reg_names[2], reg_names[31]);
      fprintf (file, "\tld.d\t %s,%s,40\n", reg_names[4], reg_names[31]);
      fprintf (file, "\tld.d\t %s,%s,48\n", reg_names[6], reg_names[31]);
      fprintf (file, "\tld.d\t %s,%s,56\n", reg_names[8], reg_names[31]);
      fprintf (file, "\taddu\t %s,%s,64\n", reg_names[31], reg_names[31]);
    }
}

/* Output assembler code to FILE to initialize basic-block profiling for
   the current module.  LABELNO is unique to each instance.  */

void
output_function_block_profiler (file, labelno)
     FILE *file;
     int labelno;
{
  char block[256];
  char label[256];

  ASM_GENERATE_INTERNAL_LABEL (block, "LPBX", 0);
  ASM_GENERATE_INTERNAL_LABEL (label, "LPY", labelno);

  /* @@ Need to deal with PIC.  I'm not sure what the requirements are on
     register usage, so I used r26/r27 to be safe.  */
  fprintf (file, "\tor.u\t %s,%s,%shi16(%s)\n", reg_names[27], reg_names[0],
		 m88k_pound_sign, &block[1]);
  fprintf (file, "\tld\t %s,%s,%slo16(%s)\n", reg_names[26], reg_names[27],
		 m88k_pound_sign, &block[1]);
  fprintf (file, "\tbcnd\t %sne0,%s,%s\n",
		 m88k_pound_sign, reg_names[26], &label[1]);
  fputs ("\tbsr.n\t ", file);
  ASM_OUTPUT_LABELREF (file, "__bb_init_func");
  putc ('\n', file);
  fprintf (file, "\tor\t %s,%s,%slo16(%s)\n", reg_names[2], reg_names[27],
		 m88k_pound_sign, &block[1]);
  ASM_OUTPUT_INTERNAL_LABEL (file, "LPY", labelno);
}

/* Output assembler code to FILE to increment the count associated with
   the basic block number BLOCKNO.  */

void
output_block_profiler (file, blockno)
     FILE *file;
     int blockno;
{
  char block[256];

  ASM_GENERATE_INTERNAL_LABEL (block, "LPBX", 0);

  /* @@ Need to deal with PIC.  I'm not sure what the requirements are on
     register usage, so I used r26/r27 to be safe.  */
  fprintf (file, "\tor.u\t %s,%s,%shi16(%s+%d)\n", reg_names[27], reg_names[0],
		 m88k_pound_sign, &block[1], 4 * blockno);
  fprintf (file, "\tld\t %s,%s,%slo16(%s+%d)\n", reg_names[26], reg_names[27],
		 m88k_pound_sign, &block[1], 4 * blockno);
  fprintf (file, "\taddu\t %s,%s,1\n", reg_names[26], reg_names[26]);
  fprintf (file, "\tst\t %s,%s,%slo16(%s+%d)\n", reg_names[26], reg_names[27],
		 m88k_pound_sign, &block[1], 4 * blockno);
}

/* Determine whether a function argument is passed in a register, and
   which register.

   The arguments are CUM, which summarizes all the previous
   arguments; MODE, the machine mode of the argument; TYPE,
   the data type of the argument as a tree node or 0 if that is not known
   (which happens for C support library functions); and NAMED,
   which is 1 for an ordinary argument and 0 for nameless arguments that
   correspond to `...' in the called function's prototype.

   The value of the expression should either be a `reg' RTX for the
   hard register in which to pass the argument, or zero to pass the
   argument on the stack.

   On the m88000 the first eight words of args are normally in registers
   and the rest are pushed.  Double precision floating point must be
   double word aligned (and if in a register, starting on an even
   register). Structures and unions which are not 4 byte, and word
   aligned are passed in memory rather than registers, even if they
   would fit completely in the registers under OCS rules.

   Note that FUNCTION_ARG and FUNCTION_INCOMING_ARG were different.
   For structures that are passed in memory, but could have been
   passed in registers, we first load the structure into the
   register, and then when the last argument is passed, we store
   the registers into the stack locations.  This fixes some bugs
   where GCC did not expect to have register arguments, followed
   by stack arguments, followed by register arguments.  */

struct rtx_def *
m88k_function_arg (args_so_far, mode, type, named)
     CUMULATIVE_ARGS args_so_far;
     enum machine_mode mode;
     tree type;
     int named;
{
  int bytes, words;

  if (type != 0			/* undo putting struct in register */
      && (TREE_CODE (type) == RECORD_TYPE || TREE_CODE (type) == UNION_TYPE))
    mode = BLKmode;

  if (mode == BLKmode && TARGET_WARN_PASS_STRUCT)
    warning ("argument #%d is a structure", args_so_far + 1);

  if ((args_so_far & 1) != 0
      && (mode == DImode || mode == DFmode
	  || (type != 0 && TYPE_ALIGN (type) > 32)))
    args_so_far++;

#ifdef ESKIT
  if (no_reg_params)
    return (rtx) 0;             /* don't put args in registers */
#endif

  if (type == 0 && mode == BLKmode)
    abort ();	/* m88k_function_arg argument `type' is NULL for BLKmode. */

  bytes = (mode != BLKmode) ? GET_MODE_SIZE (mode) : int_size_in_bytes (type);
  words = (bytes + 3) / 4;

  if (args_so_far + words > 8)
    return (rtx) 0;             /* args have exhausted registers */

  else if (mode == BLKmode
	   && (TYPE_ALIGN (type) != BITS_PER_WORD
	       || bytes != UNITS_PER_WORD))
    return (rtx) 0;

  return gen_rtx (REG,
		  ((mode == BLKmode) ? TYPE_MODE (type) : mode),
		  2 + args_so_far);
}

/* Do what is necessary for `va_start'.  The argument is ignored;
   We look at the current function to determine if stdargs or varargs
   is used and fill in an initial va_list.  A pointer to this constructor
   is returned.  */

struct rtx_def *
m88k_builtin_saveregs (arglist)
     tree arglist;
{
  rtx block, addr, argsize;
  tree fntype = TREE_TYPE (current_function_decl);
  int argadj = ((!(TYPE_ARG_TYPES (fntype) != 0
		   && (TREE_VALUE (tree_last (TYPE_ARG_TYPES (fntype)))
		       != void_type_node)))
		? -UNITS_PER_WORD : 0) + UNITS_PER_WORD - 1;
  int fixed;
  variable_args_p = 1;

  if (CONSTANT_P (current_function_arg_offset_rtx))
    {
      fixed = (XINT (current_function_arg_offset_rtx, 0)
	       + argadj) / UNITS_PER_WORD;
      argsize = gen_rtx (CONST_INT, VOIDmode, fixed);
    }
  else
    {
      fixed = 0;
      argsize = plus_constant (current_function_arg_offset_rtx, argadj);
      argsize = expand_shift (RSHIFT_EXPR, Pmode, argsize,
			      build_int_2 (2, 0), argsize, 0);
    }

  /* Allocate the va_list constructor */
  block = assign_stack_local (BLKmode, 3 * UNITS_PER_WORD, BITS_PER_WORD);
  RTX_UNCHANGING_P (block) = 1;
  RTX_UNCHANGING_P (XEXP (block, 0)) = 1;

  /* Store the argsize as the __va_arg member.  */
  emit_move_insn (change_address (block, SImode, XEXP (block, 0)),
		  argsize);

  /* Store the arg pointer in the __va_stk member.  */
  emit_move_insn (change_address (block, Pmode,
				  plus_constant (XEXP (block, 0),
						 UNITS_PER_WORD)),
		  copy_to_reg (virtual_incoming_args_rtx));

  /* Allocate the register space, and store it as the __va_reg member.  */
  addr = assign_stack_local (BLKmode, 8 * UNITS_PER_WORD, -1);
  MEM_IN_STRUCT_P (addr) = 1;
  RTX_UNCHANGING_P (addr) = 1;
  RTX_UNCHANGING_P (XEXP (addr, 0)) = 1;
  emit_move_insn (change_address (block, Pmode,
				  plus_constant (XEXP (block, 0),
						 2 * UNITS_PER_WORD)),
		  copy_to_reg (XEXP (addr, 0)));

  /* Now store the incoming registers and return the address of the
     va_list constructor.  */
  if (fixed < 8)
      move_block_from_reg
	(2 + fixed,
	 change_address (addr, Pmode,
			 plus_constant (XEXP (addr, 0),
					fixed * UNITS_PER_WORD)),
	 8 - fixed);

  return copy_to_reg (XEXP (block, 0));
}

/* If cmpsi has not been generated, emit code to do the test.  Return the
   expression describing the test of operator OP.  */

rtx
emit_test (op, mode)
     enum rtx_code op;
     enum machine_mode mode;
{
  if (m88k_compare_reg == 0)
    emit_insn (gen_test (m88k_compare_op0, m88k_compare_op1));
  return (gen_rtx (op, mode, m88k_compare_reg, const0_rtx));
}

/* Determine how to best perform cmpsi/bxx, where cmpsi has a constant
   operand.  All tests with zero (albeit swapped) and all equality tests
   with a constant are done with bcnd.  The remaining cases are swapped
   as needed.  */

void
emit_bcnd (op, label)
     enum rtx_code op;
     rtx label;
{
  if (m88k_compare_op1 == const0_rtx)
    emit_jump_insn (optimize
		    ? gen_bxx (emit_test (op, VOIDmode), label)
		    : gen_bcnd (gen_rtx (op, VOIDmode,
					 m88k_compare_op0, const0_rtx),
				label));
  else if (m88k_compare_op0 == const0_rtx)
    emit_jump_insn (optimize
		    ? gen_bxx (emit_test (op, VOIDmode), label)
		    : gen_bcnd (gen_rtx (swap_condition (op), VOIDmode,
					 m88k_compare_op1, const0_rtx),
				label));
  else if (op != EQ && op != NE)
    emit_jump_insn (gen_bxx (emit_test (op, VOIDmode), label));
  else
    {
      rtx zero = gen_reg_rtx (SImode);
      rtx reg, constant;
      int value;

      if (GET_CODE (m88k_compare_op1) == CONST_INT)
	{
	  reg = force_reg (SImode, m88k_compare_op0);
	  constant = m88k_compare_op1;
	}
      else
	{
	  reg = force_reg (SImode, m88k_compare_op1);
	  constant = m88k_compare_op0;
	}
      value = INTVAL (constant);

      /* Perform an arithmetic computation to make the compared-to value
	 zero, but avoid loosing if the bcnd is later changed into sxx.  */
      if (SMALL_INTVAL (value))
	emit_jump_insn (gen_bxx (emit_test (op, VOIDmode), label));
      else
	{
	  if (SMALL_INTVAL (-value))
	    emit_insn (gen_addsi3 (zero, reg,
				   gen_rtx (CONST_INT, VOIDmode, -value)));
	  else
	    emit_insn (gen_xorsi3 (zero, reg, constant));

	  emit_jump_insn (gen_bcnd (gen_rtx (op, VOIDmode,
					     zero, const0_rtx),
				    label));
	}
    }
}

/* Print an operand.  Recognize special options, documented below.  */

void
print_operand (file, x, code)
    FILE *file;
    rtx x;
    char code;
{
  enum rtx_code xc = (x ? GET_CODE (x) : UNKNOWN);
  register int value = (xc == CONST_INT ? INTVAL (x) : 0);
  static int sequencep;
  static int reversep;

  if (sequencep)
    {
      if (code < 'B' || code > 'E')
	output_operand_lossage ("%R not followed by %B/C/D/E");
      if (reversep)
	xc = reverse_condition (xc);
      sequencep = 0;
    }

  switch (code)
    {
    case '*': /* addressing base register for PIC */
      fputs (reg_names[PIC_OFFSET_TABLE_REGNUM], file); return;

    case '#': /* SVR4 pound-sign syntax character (empty if SVR3) */
      fputs (m88k_pound_sign, file); return;

    case 'X': /* print the upper 16 bits... */
      value >>= 16;
    case 'x': /* print the lower 16 bits of the integer constant in hex */
      if (xc != CONST_INT)
	output_operand_lossage ("invalid %x/X value");
      fprintf (file, "0x%x", value & 0xffff); return;

    case 'H': /* print the low 16 bits of the negated integer constant */
      if (xc != CONST_INT)
	output_operand_lossage ("invalid %H value");
      value = -value;
    case 'h': /* print the register or low 16 bits of the integer constant */
      if (xc == REG)
	goto reg;
      if (xc != CONST_INT)
	output_operand_lossage ("invalid %h value");
      fprintf (file, "%d", value & 0xffff);
      return;

    case 'Q': /* print the low 8 bits of the negated integer constant */
      if (xc != CONST_INT)
	output_operand_lossage ("invalid %Q value");
      value = -value;
    case 'q': /* print the register or low 8 bits of the integer constant */
      if (xc == REG)
	goto reg;
      if (xc != CONST_INT)
	output_operand_lossage ("invalid %q value");
      fprintf (file, "%d", value & 0xff);
      return;

    case 'w': /* print the integer constant (X == 32 ? 0 : 32 - X) */
      if (xc != CONST_INT)
	output_operand_lossage ("invalid %o value");
      fprintf (file, "%d", value == 32 ? 0 : 32 - value);
      return;

    case 'p': /* print the logarithm of the integer constant */
      if (xc != CONST_INT
	  || (value = exact_log2 (value)) < 0)
	output_operand_lossage ("invalid %p value");
      fprintf (file, "%d", value);
      return;

    case 'S': /* compliment the value and then... */
      value = ~value;
    case 's': /* print the width and offset values forming the integer
		 constant with a SET instruction.  See integer_ok_for_set. */
      {
	register unsigned mask, uval = value;
	register int top, bottom;

	if (xc != CONST_INT)
	  output_operand_lossage ("invalid %s/S value");
	/* All the "one" bits must be contiguous.  If so, MASK will be
	   a power of two or zero.  */
	mask = (uval | (uval - 1)) + 1;
	if (!(uval && POWER_OF_2_or_0 (mask)))
	  output_operand_lossage ("invalid %s/S value");
	top = mask ? exact_log2 (mask) : 32;
	bottom = exact_log2 (uval & ~(uval - 1));
	fprintf (file,"%d<%d>", top - bottom, bottom);
	return;
      }

    case 'P': /* print nothing if pc_rtx; output label_ref */
      if (xc == LABEL_REF)
	output_addr_const (file, x);
      else if (xc != PC)
	output_operand_lossage ("invalid %P operand");
      return;

    case 'L': /* print 0 or 1 if operand is label_ref and then...  */
      fputc (xc == LABEL_REF ? '1' : '0', file);
    case '.': /* print .n if delay slot is used */
      fputs ((final_sequence
	      && ! INSN_ANNULLED_BRANCH_P (XVECEXP (final_sequence, 0, 0)))
	     ? ".n\t" : "\t", file);
      return;

    case 'R': /* reverse the condition of the next print_operand
		 if operand is a label_ref.  */
      sequencep++;
      reversep = (xc == LABEL_REF);
      return;

    case 'B': /* bcnd branch values */
      fputs (m88k_pound_sign, file);
      switch (xc)
	{
	case EQ: fputs ("eq0", file); return;
	case NE: fputs ("ne0", file); return;
	case GT: fputs ("gt0", file); return;
	case LE: fputs ("le0", file); return;
	case LT: fputs ("lt0", file); return;
	case GE: fputs ("ge0", file); return;
	default: output_operand_lossage ("invalid %B value");
	}

    case 'C': /* bb0/bb1 branch values for comparisons */
      fputs (m88k_pound_sign, file);
      switch (xc)
	{
	case EQ:  fputs ("eq", file); return;
	case NE:  fputs ("ne", file); return;
	case GT:  fputs ("gt", file); return;
	case LE:  fputs ("le", file); return;
	case LT:  fputs ("lt", file); return;
	case GE:  fputs ("ge", file); return;
	case GTU: fputs ("hi", file); return;
	case LEU: fputs ("ls", file); return;
	case LTU: fputs ("lo", file); return;
	case GEU: fputs ("hs", file); return;
	default:  output_operand_lossage ("invalid %C value");
	}

    case 'D': /* bcnd branch values for float comparisons */
      switch (xc)
	{
	case EQ: fputs ("0xa", file); return;
	case NE: fputs ("0x5", file); return;
	case GT: fputs (m88k_pound_sign, file);
	  fputs ("gt0", file); return;
	case LE: fputs ("0xe", file); return;
	case LT: fputs ("0x4", file); return;
	case GE: fputs ("0xb", file); return;
	default: output_operand_lossage ("invalid %D value");
	}

    case 'E': /* bcnd branch values for special integers */
      switch (xc)
	{
	case EQ: fputs ("0x8", file); return;
	case NE: fputs ("0x7", file); return;
	default: output_operand_lossage ("invalid %E value");
	}

    case 'd': /* second register of a two register pair */
      if (xc != REG)
	output_operand_lossage ("`%d' operand isn't a register");
      fputs (reg_names[REGNO (x) + 1], file);
      return;

    case 'r': /* an immediate 0 should be represented as `r0' */
      if (x == const0_rtx)
	{
	  fputs (reg_names[0], file);
	  return;
	}
      else if (xc != REG)
	output_operand_lossage ("invalid %r value");
    case 0:
    name:
      if (xc == REG)
	{
	reg:
	  if (REGNO (x) == ARG_POINTER_REGNUM)
	    output_operand_lossage ("operand is r0");
	  else
	    fputs (reg_names[REGNO (x)], file);
	}
      else if (xc == PLUS)
	output_address (x);
      else if (xc == MEM)
	output_address (XEXP (x, 0));
      else if (xc == CONST_DOUBLE)
	output_operand_lossage ("operand is const_double");
      else
	output_addr_const (file, x);
      return;

    case 'g': /* append #got_rel as needed */
      if (flag_pic && (xc == SYMBOL_REF || xc == LABEL_REF))
	{
	  output_addr_const (file, x);
	  fputs ("#got_rel", file);
	  return;
	}
      goto name;

    case 'a': /* (standard), assume operand is an address */
    case 'c': /* (standard), assume operand is an immediate value */
    case 'l': /* (standard), assume operand is a label_ref */
    case 'n': /* (standard), like %c, except negate first */
    default:
      output_operand_lossage ("invalid code");
    }
}

void
print_operand_address (file, addr)
    FILE *file;
    rtx addr;
{
  register rtx reg0, reg1, temp;

  switch (GET_CODE (addr))
    {
    case REG:
      if (REGNO (addr) == ARG_POINTER_REGNUM)
	abort ();
      else
	fprintf (file, "%s,%s", reg_names[0], reg_names [REGNO (addr)]);
      break;

    case LO_SUM:
      fprintf (file, "%s,%slo16(",
	       reg_names[REGNO (XEXP (addr, 0))], m88k_pound_sign);
      output_addr_const (file, XEXP (addr, 1));
      fputc (')', file);
      break;

    case PLUS:
      reg0 = XEXP (addr, 0);
      reg1 = XEXP (addr, 1);
      if (GET_CODE (reg0) == MULT || GET_CODE (reg0) == CONST_INT)
	{
	  rtx tmp = reg0;
	  reg0 = reg1;
	  reg1 = tmp;
	}

      if ((REG_P (reg0) && REGNO (reg0) == ARG_POINTER_REGNUM)
	  || (REG_P (reg1) && REGNO (reg1) == ARG_POINTER_REGNUM))
	abort ();

      else if (REG_P (reg0))
	{
	  if (REG_P (reg1))
	    fprintf (file, "%s,%s",
		     reg_names [REGNO (reg0)], reg_names [REGNO (reg1)]);

	  else if (GET_CODE (reg1) == CONST_INT)
	    fprintf (file, "%s,%d",
		     reg_names [REGNO (reg0)], INTVAL (reg1));

	  else if (GET_CODE (reg1) == MULT)
	    {
	      rtx mreg = XEXP (reg1, 0);
	      if (REGNO (mreg) == ARG_POINTER_REGNUM)
		abort ();

	      fprintf (file, "%s[%s]", reg_names[REGNO (reg0)],
		       reg_names[REGNO (mreg)]);
	    }

	  else if (GET_CODE (reg1) == ZERO_EXTRACT)
	    {
	      fprintf (file, "%s,%slo16(",
		       reg_names[REGNO (reg0)], m88k_pound_sign);
	      output_addr_const (file, XEXP (reg1, 0));
	      fputc (')', file);
	    }

	  else if (flag_pic)
	    {
	      fprintf (file, "%s,", reg_names[REGNO (reg0)]);
	      output_addr_const (file, reg1);
	      fputs ("#got_rel", file);
	    }
	  else abort ();
	}

      else
	abort ();
      break;

    case MULT:
      if (REGNO (XEXP (addr, 0)) == ARG_POINTER_REGNUM)
	abort ();

      fprintf (file, "%s[%s]",
	       reg_names[0], reg_names[REGNO (XEXP (addr, 0))]);
      break;

    case LSHIFT:
      fprintf (file, "%s,%shi16(", reg_names[0], m88k_pound_sign);
      output_addr_const (file, XEXP (addr, 0));
      fputc (')', file);
      break;

    case CONST_INT:
      fprintf (file, "%s,%d", reg_names[0], INTVAL (addr));
      break;

    default:
      fprintf (file, "%s,", reg_names[0]);
      if (SHORT_ADDRESS_P (addr, temp))
	{
	  fprintf (file, "%siw16(", m88k_pound_sign);
	  output_addr_const (file, addr);
	  fputc (')', file);
	}
      else
	  output_addr_const (file, addr);
    }
}
