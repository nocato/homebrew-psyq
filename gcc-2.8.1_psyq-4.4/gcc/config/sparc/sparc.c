/* Subroutines for insn-output.c for Sun SPARC.
   Copyright (C) 1987, 1988, 1989, 1992 Free Software Foundation, Inc.
   Contributed by Michael Tiemann (tiemann@cygnus.com)

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
#include "flags.h"
#include "expr.h"
#include "recog.h"

/* Global variables for machine-dependent things.  */

/* Save the operands last given to a compare for use when we
   generate a scc or bcc insn.  */

rtx sparc_compare_op0, sparc_compare_op1;

/* We may need an epilogue if we spill too many registers.
   If this is non-zero, then we branch here for the epilogue.  */
static rtx leaf_label;

#ifdef LEAF_REGISTERS

/* Vector to say how input registers are mapped to output
   registers.  FRAME_POINTER_REGNUM cannot be remapped by
   this function to eliminate it.  You must use -fomit-frame-pointer
   to get that.  */
char leaf_reg_remap[] =
{ 0, 1, 2, 3, 4, 5, 6, 7,
  -1, -1, -1, -1, -1, -1, 14, -1,
  -1, -1, -1, -1, -1, -1, -1, -1,
  8, 9, 10, 11, 12, 13, -1, 15,

  32, 33, 34, 35, 36, 37, 38, 39,
  40, 41, 42, 43, 44, 45, 46, 47,
  48, 49, 50, 51, 52, 53, 54, 55,
  56, 57, 58, 59, 60, 61, 62, 63};

char leaf_reg_backmap[] =
{ 0, 1, 2, 3, 4, 5, 6, 7,
  24, 25, 26, 27, 28, 29, 14, 31,
  -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1,

  32, 33, 34, 35, 36, 37, 38, 39,
  40, 41, 42, 43, 44, 45, 46, 47,
  48, 49, 50, 51, 52, 53, 54, 55,
  56, 57, 58, 59, 60, 61, 62, 63};
#endif

/* Global variables set by FUNCTION_PROLOGUE.  */
/* Size of frame.  Need to know this to emit return insns from
   leaf procedures.  */
int apparent_fsize;
int actual_fsize;

/* Name of where we pretend to think the frame pointer points.
   Normally, this is "%fp", but if we are in a leaf procedure,
   this is "%sp+something".  */
char *frame_base_name;

static rtx find_addr_reg ();

/* Return non-zero only if OP is a register of mode MODE,
   or const0_rtx.  */
int
reg_or_0_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (op == const0_rtx || register_operand (op, mode))
    return 1;
  if (GET_CODE (op) == CONST_DOUBLE
      && CONST_DOUBLE_HIGH (op) == 0
      && CONST_DOUBLE_LOW (op) == 0)
    return 1;
  return 0;
}

/* Nonzero if OP can appear as the dest of a RESTORE insn.  */
int
restore_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (GET_CODE (op) == REG && GET_MODE (op) == mode
	  && (REGNO (op) < 8 || (REGNO (op) >= 24 && REGNO (op) < 32)));
}

/* PC-relative call insn on SPARC is independent of `memory_operand'.  */

int
call_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (GET_CODE (op) != MEM)
    abort ();
  op = XEXP (op, 0);
  return (REG_P (op) || CONSTANT_P (op));
}

int
call_operand_address (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (REG_P (op) || CONSTANT_P (op));
}

/* Returns 1 if OP is either a symbol reference or a sum of a symbol
   reference and a constant.  */

int
symbolic_operand (op, mode)
     register rtx op;
     enum machine_mode mode;
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

      /* This clause seems to be irrelevant.  */
    case CONST_DOUBLE:
      return GET_MODE (op) == mode;

    default:
      return 0;
    }
}

/* Return truth value of statement that OP is a symbolic memory
   operand of mode MODE.  */

int
symbolic_memory_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (GET_CODE (op) == SUBREG)
    op = SUBREG_REG (op);
  if (GET_CODE (op) != MEM)
    return 0;
  op = XEXP (op, 0);
  return (GET_CODE (op) == SYMBOL_REF || GET_CODE (op) == CONST
	  || GET_CODE (op) == HIGH || GET_CODE (op) == LABEL_REF);
}

/* Return 1 if the operand is either a register or a memory operand that is
   not symbolic.  */

int
reg_or_nonsymb_mem_operand (op, mode)
    register rtx op;
    enum machine_mode mode;
{
  if (register_operand (op, mode))
    return 1;

  if (memory_operand (op, mode) && ! symbolic_memory_operand (op, mode))
    return 1;

  return 0;
}

int
sparc_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (register_operand (op, mode))
    return 1;
  if (GET_CODE (op) == CONST_INT)
    return SMALL_INT (op);
  if (GET_MODE (op) != mode)
    return 0;
  if (GET_CODE (op) == SUBREG)
    op = SUBREG_REG (op);
  if (GET_CODE (op) != MEM)
    return 0;

  op = XEXP (op, 0);
  if (GET_CODE (op) == LO_SUM)
    return (GET_CODE (XEXP (op, 0)) == REG
	    && symbolic_operand (XEXP (op, 1), Pmode));
  return memory_address_p (mode, op);
}

int
move_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (mode == DImode && arith_double_operand (op, mode))
    return 1;
  if (register_operand (op, mode))
    return 1;
  if (GET_CODE (op) == CONST_INT)
    return (SMALL_INT (op) || (INTVAL (op) & 0x3ff) == 0);

  if (GET_MODE (op) != mode)
    return 0;
  if (GET_CODE (op) == SUBREG)
    op = SUBREG_REG (op);
  if (GET_CODE (op) != MEM)
    return 0;
  op = XEXP (op, 0);
  if (GET_CODE (op) == LO_SUM)
    return (register_operand (XEXP (op, 0), Pmode)
	    && CONSTANT_P (XEXP (op, 1)));
  return memory_address_p (mode, op);
}

int
move_pic_label (op, mode)
     rtx op;
     enum machine_mode mode;
{
  /* Special case for PIC.  */
  if (flag_pic && GET_CODE (op) == LABEL_REF)
    return 1;
  return 0;
}

/* The rtx for the global offset table which is a special form
   that *is* a position independent symbolic constant.  */
rtx pic_pc_rtx;

/* Ensure that we are not using patterns that are not OK with PIC.  */

int
check_pic (i)
     int i;
{
  switch (flag_pic)
    {
    case 1:
      if (GET_CODE (recog_operand[i]) == SYMBOL_REF
	  || (GET_CODE (recog_operand[i]) == CONST
	      && ! rtx_equal_p (pic_pc_rtx, recog_operand[i])))
	abort ();
    case 2:
    default:
      return 1;
    }
}

/* Return true if X is an address which needs a temporary register when 
   reloaded while generating PIC code.  */

int
pic_address_needs_scratch (x)
     rtx x;
{
  /* An address which is a symbolic plus a non SMALL_INT needs a temp reg.  */
  if (GET_CODE (x) == CONST && GET_CODE (XEXP (x, 0)) == PLUS
      && GET_CODE (XEXP (XEXP (x, 0), 0)) == SYMBOL_REF
      && GET_CODE (XEXP (XEXP (x, 0), 1)) == CONST_INT
      && ! SMALL_INT (XEXP (XEXP (x, 0), 1)))
    return 1;

  return 0;
}

int
memop (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (GET_CODE (op) == MEM)
    return (mode == VOIDmode || mode == GET_MODE (op));
  return 0;
}

/* Return truth value of whether OP is EQ or NE.  */

int
eq_or_neq (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (GET_CODE (op) == EQ || GET_CODE (op) == NE);
}

/* Return 1 if this is a comparison operator, but not an EQ, NE, GEU,
   or LTU for non-floating-point.  We handle those specially.  */

int
normal_comp_operator (op, mode)
     rtx op;
     enum machine_mode mode;
{
  enum rtx_code code = GET_CODE (op);

  if (GET_RTX_CLASS (code) != '<')
    return 0;

  if (GET_MODE (XEXP (op, 0)) == CCFPmode)
    return 1;

  return (code != NE && code != EQ && code != GEU && code != LTU);
}

/* Return 1 if this is a comparison operator.  This allows the use of
   MATCH_OPERATOR to recognize all the branch insns.  */

int
noov_compare_op (op, mode)
    register rtx op;
    enum machine_mode mode;
{
  enum rtx_code code = GET_CODE (op);

  if (GET_RTX_CLASS (code) != '<')
    return 0;

  if (GET_MODE (XEXP (op, 0)) == CC_NOOVmode)
    /* These are the only branches which work with CC_NOOVmode.  */
    return (code == EQ || code == NE || code == GE || code == LT);
  return 1;
}

/* Return 1 if this is a SIGN_EXTEND or ZERO_EXTEND operation.  */

int
extend_op (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return GET_CODE (op) == SIGN_EXTEND || GET_CODE (op) == ZERO_EXTEND;
}

/* Return nonzero if OP is an operator of mode MODE which can set
   the condition codes explicitly.  We do not include PLUS and MINUS
   because these require CC_NOOVmode, which we handle explicitly.  */

int
cc_arithop (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (GET_CODE (op) == AND
      || GET_CODE (op) == IOR
      || GET_CODE (op) == XOR)
    return 1;

  return 0;
}

/* Return nonzero if OP is an operator of mode MODE which can bitwise
   complement its second operand and set the condition codes explicitly.  */

int
cc_arithopn (op, mode)
     rtx op;
     enum machine_mode mode;
{
  /* XOR is not here because combine canonicalizes (xor (not ...) ...)
     and (xor ... (not ...)) to (not (xor ...)).   */
  return (GET_CODE (op) == AND
	  || GET_CODE (op) == IOR);
}

/* Return truth value of whether OP can be used as an operands in a three
   address arithmetic insn (such as add %o1,7,%l2) of mode MODE.  */

int
arith_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (register_operand (op, mode)
	  || (GET_CODE (op) == CONST_INT && SMALL_INT (op)));
}

/* Return truth value of whether OP is a register or a CONST_DOUBLE.  */

int
arith_double_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (register_operand (op, mode)
	  || (GET_CODE (op) == CONST_DOUBLE
	      && (GET_MODE (op) == mode || GET_MODE (op) == VOIDmode)
	      && (unsigned) (CONST_DOUBLE_LOW (op) + 0x1000) < 0x2000
	      && ((CONST_DOUBLE_HIGH (op) == -1
		   && (CONST_DOUBLE_LOW (op) & 0x1000) == 0x1000)
		  || (CONST_DOUBLE_HIGH (op) == 0
		      && (CONST_DOUBLE_LOW (op) & 0x1000) == 0)))
	  || (GET_CODE (op) == CONST_INT
	      && (GET_MODE (op) == mode || GET_MODE (op) == VOIDmode)
	      && (unsigned) (INTVAL (op) + 0x1000) < 0x2000));
}

/* Return truth value of whether OP is a integer which fits the
   range constraining immediate operands in three-address insns.  */

int
small_int (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (GET_CODE (op) == CONST_INT && SMALL_INT (op));
}

/* Return truth value of statement that OP is a call-clobbered register.  */
int
clobbered_register (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (GET_CODE (op) == REG && call_used_regs[REGNO (op)]);
}

/* X and Y are two things to compare using CODE.  Emit the compare insn and
   return the rtx for register 0 in the proper mode.  */

rtx
gen_compare_reg (code, x, y)
     enum rtx_code code;
     rtx x, y;
{
  enum machine_mode mode = SELECT_CC_MODE (code, x);
  rtx cc_reg = gen_rtx (REG, mode, 0);

  emit_insn (gen_rtx (SET, VOIDmode, cc_reg,
		      gen_rtx (COMPARE, mode, x, y)));

  return cc_reg;
}

/* Return nonzero if a return peephole merging return with
   setting of output register is ok.  */
int
leaf_return_peephole_ok ()
{
  return (actual_fsize == 0);
}

/* Return nonzero if TRIAL can go into the function epilogue's
   delay slot.  SLOT is the slot we are trying to fill.  */

int
eligible_for_epilogue_delay (trial, slot)
     rtx trial;
     int slot;
{
  static char *this_function_name;
  rtx pat, src;

  if (slot >= 1)
    return 0;
  if (GET_CODE (trial) != INSN
      || GET_CODE (PATTERN (trial)) != SET)
    return 0;
  if (get_attr_length (trial) != 1)
    return 0;

  /* In the case of a true leaf function, anything can
     go into the delay slot.  */
  if (leaf_function)
    {
      if (leaf_return_peephole_ok ())
	return (get_attr_in_branch_delay (trial) == IN_BRANCH_DELAY_TRUE);
      return 0;
    }

  /* Otherwise, only operations which can be done in tandem with
     a `restore' insn can go into the delay slot.  */
  pat = PATTERN (trial);
  if (GET_CODE (SET_DEST (pat)) != REG
      || REGNO (SET_DEST (pat)) == 0
      || (leaf_function
	  && REGNO (SET_DEST (pat)) < 32
	  && REGNO (SET_DEST (pat)) >= 16)
      || (! leaf_function
	  && (REGNO (SET_DEST (pat)) >= 32
	      || REGNO (SET_DEST (pat)) < 24)))
    return 0;
  src = SET_SRC (pat);
  if (arith_operand (src, GET_MODE (src)))
    return GET_MODE_SIZE (GET_MODE (src)) <= GET_MODE_SIZE (SImode);
  if (arith_double_operand (src, GET_MODE (src)))
    return GET_MODE_SIZE (GET_MODE (src)) <= GET_MODE_SIZE (DImode);
  if (GET_CODE (src) == PLUS)
    {
      if (register_operand (XEXP (src, 0), SImode)
	  && arith_operand (XEXP (src, 1), SImode))
	return 1;
      if (register_operand (XEXP (src, 1), SImode)
	  && arith_operand (XEXP (src, 0), SImode))
	return 1;
      if (register_operand (XEXP (src, 0), DImode)
	  && arith_double_operand (XEXP (src, 1), DImode))
	return 1;
      if (register_operand (XEXP (src, 1), DImode)
	  && arith_double_operand (XEXP (src, 0), DImode))
	return 1;
    }
  if (GET_CODE (src) == MINUS
      && register_operand (XEXP (src, 0), SImode)
      && small_int (XEXP (src, 1), VOIDmode))
    return 1;
  if (GET_CODE (src) == MINUS
      && register_operand (XEXP (src, 0), DImode)
      && !register_operand (XEXP (src, 1), DImode)
      && arith_double_operand (XEXP (src, 1), DImode))
    return 1;
  return 0;
}

int
short_branch (uid1, uid2)
     int uid1, uid2;
{
  unsigned int delta = insn_addresses[uid1] - insn_addresses[uid2];
  if (delta + 1024 < 2048)
    return 1;
  /* warning ("long branch, distance %d", delta); */
  return 0;
}

/* Return non-zero if REG is not used after INSN.
   We assume REG is a reload reg, and therefore does
   not live past labels or calls or jumps.  */
int
reg_unused_after (reg, insn)
     rtx reg;
     rtx insn;
{
  enum rtx_code code, prev_code = UNKNOWN;

  while (insn = NEXT_INSN (insn))
    {
      if (prev_code == CALL_INSN && call_used_regs[REGNO (reg)])
	return 1;

      code = GET_CODE (insn);
      if (GET_CODE (insn) == CODE_LABEL)
	return 1;

      if (GET_RTX_CLASS (code) == 'i')
	{
	  rtx set = single_set (insn);
	  int in_src = set && reg_overlap_mentioned_p (reg, SET_SRC (set));
	  if (set && in_src)
	    return 0;
	  if (set && reg_overlap_mentioned_p (reg, SET_DEST (set)))
	    return 1;
	  if (set == 0 && reg_overlap_mentioned_p (reg, PATTERN (insn)))
	    return 0;
	}
      prev_code = code;
    }
  return 1;
}

/* Legitimize PIC addresses.  If the address is already position-independent,
   we return ORIG.  Newly generated position-independent addresses go into a
   reg.  This is REG if non zero, otherwise we allocate register(s) as
   necessary.  If this is called during reload, and we need a second temp
   register, then we use SCRATCH, which is provided via the
   SECONDARY_INPUT_RELOAD_CLASS mechanism.  */

rtx
legitimize_pic_address (orig, mode, reg, scratch)
     rtx orig;
     enum machine_mode mode;
     rtx reg, scratch;
{
  if (GET_CODE (orig) == SYMBOL_REF)
    {
      rtx pic_ref, address;
      rtx insn;

      if (reg == 0)
	{
	  if (reload_in_progress || reload_completed)
	    abort ();
	  else
	    reg = gen_reg_rtx (Pmode);
	}

      if (flag_pic == 2)
	{
	  /* If not during reload, allocate another temp reg here for loading
	     in the address, so that these instructions can be optimized
	     properly.  */
	  rtx temp_reg = ((reload_in_progress || reload_completed)
			  ? reg : gen_reg_rtx (Pmode));

	  /* Must put the SYMBOL_REF inside an UNSPEC here so that cse
	     won't get confused into thinking that these two instructions
	     are loading in the true address of the symbol.  If in the
	     future a PIC rtx exists, that should be used instead.  */
	  emit_insn (gen_rtx (SET, VOIDmode, temp_reg,
			      gen_rtx (HIGH, Pmode,
				       gen_rtx (UNSPEC, Pmode,
						gen_rtvec (1, orig),
						0))));
	  emit_insn (gen_rtx (SET, VOIDmode, temp_reg,
			      gen_rtx (LO_SUM, Pmode, temp_reg,
				       gen_rtx (UNSPEC, Pmode,
						gen_rtvec (1, orig),
						0))));
	  address = temp_reg;
	}
      else
	address = orig;

      pic_ref = gen_rtx (MEM, Pmode,
			 gen_rtx (PLUS, Pmode,
				  pic_offset_table_rtx, address));
      current_function_uses_pic_offset_table = 1;
      RTX_UNCHANGING_P (pic_ref) = 1;
      insn = emit_move_insn (reg, pic_ref);
      /* Put a REG_EQUAL note on this insn, so that it can be optimized
	 by loop.  */
      REG_NOTES (insn) = gen_rtx (EXPR_LIST, REG_EQUAL, orig,
				  REG_NOTES (insn));
      return reg;
    }
  else if (GET_CODE (orig) == CONST)
    {
      rtx base, offset;

      if (GET_CODE (XEXP (orig, 0)) == PLUS
	  && XEXP (XEXP (orig, 0), 0) == pic_offset_table_rtx)
	return orig;

      if (reg == 0)
	{
	  if (reload_in_progress || reload_completed)
	    abort ();
	  else
	    reg = gen_reg_rtx (Pmode);
	}

      if (GET_CODE (XEXP (orig, 0)) == PLUS)
	{
	  base = legitimize_pic_address (XEXP (XEXP (orig, 0), 0), Pmode,
					 reg, 0);
	  offset = legitimize_pic_address (XEXP (XEXP (orig, 0), 1), Pmode,
					 base == reg ? 0 : reg, 0);
	}
      else
	abort ();

      if (GET_CODE (offset) == CONST_INT)
	{
	  if (SMALL_INT (offset))
	    return plus_constant_for_output (base, INTVAL (offset));
	  else if (! reload_in_progress && ! reload_completed)
	    offset = force_reg (Pmode, offset);
	  /* We can't create any new registers during reload, so use the
	     SCRATCH reg provided by the reload_insi pattern.  */
	  else if (scratch)
	    {
	      emit_move_insn (scratch, offset);
	      offset = scratch;
	    }
	  else
	    /* If we reach here, then the SECONDARY_INPUT_RELOAD_CLASS
	       macro needs to be adjusted so that a scratch reg is provided
	       for this address.  */
	    abort ();
	}
      return gen_rtx (PLUS, Pmode, base, offset);
    }
  else if (GET_CODE (orig) == LABEL_REF)
    current_function_uses_pic_offset_table = 1;

  return orig;
}

/* Set up PIC-specific rtl.  This should not cause any insns
   to be emitted.  */

void
initialize_pic ()
{
}

/* Emit special PIC prologues and epilogues.  */

void
finalize_pic ()
{
  /* The table we use to reference PIC data.  */
  rtx global_offset_table;
  /* Labels to get the PC in the prologue of this function.  */
  rtx l1, l2;
  rtx seq;
  int orig_flag_pic = flag_pic;

  if (current_function_uses_pic_offset_table == 0)
    return;

  if (! flag_pic)
    abort ();

  flag_pic = 0;
  l1 = gen_label_rtx ();
  l2 = gen_label_rtx ();

  start_sequence ();

  emit_label (l1);
  /* Note that we pun calls and jumps here!  */
  emit_jump_insn (gen_rtx (PARALLEL, VOIDmode,
                         gen_rtvec (2,
                                    gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (LABEL_REF, VOIDmode, l2)),
                                    gen_rtx (SET, VOIDmode, gen_rtx (REG, SImode, 15), gen_rtx (LABEL_REF, VOIDmode, l2)))));
  emit_label (l2);

  /* Initialize every time through, since we can't easily
     know this to be permanent.  */
  global_offset_table = gen_rtx (SYMBOL_REF, Pmode, "*__GLOBAL_OFFSET_TABLE_");
  pic_pc_rtx = gen_rtx (CONST, Pmode,
			gen_rtx (MINUS, Pmode,
				 global_offset_table,
				 gen_rtx (CONST, Pmode,
					  gen_rtx (MINUS, Pmode,
						   gen_rtx (LABEL_REF, VOIDmode, l1),
						   pc_rtx))));

  emit_insn (gen_rtx (SET, VOIDmode, pic_offset_table_rtx,
		      gen_rtx (HIGH, Pmode, pic_pc_rtx)));
  emit_insn (gen_rtx (SET, VOIDmode,
		      pic_offset_table_rtx,
		      gen_rtx (LO_SUM, Pmode,
			       pic_offset_table_rtx, pic_pc_rtx)));
  emit_insn (gen_rtx (SET, VOIDmode,
		      pic_offset_table_rtx,
		      gen_rtx (PLUS, Pmode,
			       pic_offset_table_rtx, gen_rtx (REG, Pmode, 15))));
  /* emit_insn (gen_rtx (ASM_INPUT, VOIDmode, "!#PROLOGUE# 1")); */
  LABEL_PRESERVE_P (l1) = 1;
  LABEL_PRESERVE_P (l2) = 1;
  flag_pic = orig_flag_pic;

  seq = gen_sequence ();
  end_sequence ();
  emit_insn_after (seq, get_insns ());

  /* Need to emit this whether or not we obey regdecls,
     since setjmp/longjmp can cause life info to screw up.  */
  emit_insn (gen_rtx (USE, VOIDmode, pic_offset_table_rtx));
}

/* For the SPARC, REG and REG+CONST is cost 0, REG+REG is cost 1,
   and addresses involving symbolic constants are cost 2.

   We make REG+REG slightly more expensive because it might keep
   a register live for longer than we might like.

   PIC addresses are very expensive.

   It is no coincidence that this has the same structure
   as GO_IF_LEGITIMATE_ADDRESS.  */
int
sparc_address_cost (X)
     rtx X;
{
#if 0
  /* Handled before calling here.  */
  if (GET_CODE (X) == REG)
    { return 1; }
#endif
  if (GET_CODE (X) == PLUS)
    {
      if (GET_CODE (XEXP (X, 0)) == REG
	  && GET_CODE (XEXP (X, 1)) == REG)
	return 2;
      return 1;
    }
  else if (GET_CODE (X) == LO_SUM)
    return 1;
  else if (GET_CODE (X) == HIGH)
    return 2;
  return 4;
}

/* Emit insns to move operands[1] into operands[0].

   Return 1 if we have written out everything that needs to be done to
   do the move.  Otherwise, return 0 and the caller will emit the move
   normally.

   SCRATCH_REG if non zero can be used as a scratch register for the move
   operation.  It is provided by a SECONDARY_RELOAD_* macro if needed.  */

int
emit_move_sequence (operands, mode, scratch_reg)
     rtx *operands;
     enum machine_mode mode;
     rtx scratch_reg;
{
  register rtx operand0 = operands[0];
  register rtx operand1 = operands[1];

  /* Handle most common case first: storing into a register.  */
  if (register_operand (operand0, mode))
    {
      if (register_operand (operand1, mode)
	  || (GET_CODE (operand1) == CONST_INT && SMALL_INT (operand1))
	  || (GET_CODE (operand1) == CONST_DOUBLE
	      && arith_double_operand (operand1, DImode))
	  || (GET_CODE (operand1) == HIGH && GET_MODE (operand1) != DImode)
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
      if (! reload_in_progress)
	{
	  operands[0] = validize_mem (operand0);
	  operands[1] = operand1 = force_reg (mode, operand1);
	}
    }

  /* Simplify the source if we need to.  Must handle DImode HIGH operators
     here because such a move needs a clobber added.  */
  if ((GET_CODE (operand1) != HIGH && immediate_operand (operand1, mode))
      || (GET_CODE (operand1) == HIGH && GET_MODE (operand1) == DImode))
    {
      if (flag_pic && symbolic_operand (operand1, mode))
	{
	  rtx temp_reg = reload_in_progress ? operand0 : 0;

	  operands[1] = legitimize_pic_address (operand1, mode, temp_reg,
						scratch_reg);
	}
      else if (GET_CODE (operand1) == CONST_INT
	       ? (! SMALL_INT (operand1)
		  && (INTVAL (operand1) & 0x3ff) != 0)
	       : (GET_CODE (operand1) == CONST_DOUBLE
		  ? ! arith_double_operand (operand1, DImode)
		  : 1))
	{
	  /* For DImode values, temp must be operand0 because of the way
	     HI and LO_SUM work.  The LO_SUM operator only copies half of
	     the LSW from the dest of the HI operator.  If the LO_SUM dest is
	     not the same as the HI dest, then the MSW of the LO_SUM dest will
	     never be set.

	     ??? The real problem here is that the ...(HI:DImode pattern emits
	     multiple instructions, and the ...(LO_SUM:DImode pattern emits
	     one instruction.  This fails, because the compiler assumes that
	     LO_SUM copies all bits of the first operand to its dest.  Better
	     would be to have the HI pattern emit one instruction and the
	     LO_SUM pattern multiple instructions.  Even better would be
	     to use four rtl insns.  */
	  rtx temp = ((reload_in_progress || mode == DImode)
		      ? operand0 : gen_reg_rtx (mode));

	  emit_insn (gen_rtx (SET, VOIDmode, temp,
			      gen_rtx (HIGH, mode, operand1)));
	  operands[1] = gen_rtx (LO_SUM, mode, temp, operand1);
	}
    }

  if (GET_CODE (operand1) == LABEL_REF && flag_pic)
    {
      /* The procedure for doing this involves using a call instruction to
	 get the pc into o7.  We need to indicate this explicitly because
	 the tablejump pattern assumes that it can use this value also.  */
      emit_insn (gen_rtx (PARALLEL, VOIDmode,
			  gen_rtvec (2,
				     gen_rtx (SET, VOIDmode, operand0,
					      operand1),
				     gen_rtx (SET, VOIDmode,
					      gen_rtx (REG, mode, 15),
					      pc_rtx))));
      return 1;
    }

  /* Now have insn-emit do whatever it normally does.  */
  return 0;
}

/* Return the best assembler insn template
   for moving operands[1] into operands[0] as a fullword.  */

char *
singlemove_string (operands)
     rtx *operands;
{
  if (GET_CODE (operands[0]) == MEM)
    {
      if (GET_CODE (operands[1]) != MEM)
	return "st %r1,%0";
      else
	abort ();
    }
  if (GET_CODE (operands[1]) == MEM)
    return "ld %1,%0";
  if (GET_CODE (operands[1]) == CONST_INT
      && ! CONST_OK_FOR_LETTER_P (INTVAL (operands[1]), 'I'))
    {
      int i = INTVAL (operands[1]);

      /* If all low order 12 bits are clear, then we only need a single
	 sethi insn to load the constant.  */
      if (i & 0x00000FFF)
	return "sethi %%hi(%a1),%0\n\tor %0,%%lo(%a1),%0";
      else
	return "sethi %%hi(%a1),%0";
    }
  /* ??? Wrong if target is DImode?  */
  return "mov %1,%0";
}

/* Output assembler code to perform a doubleword move insn
   with operands OPERANDS.  */

char *
output_move_double (operands)
     rtx *operands;
{
  enum { REGOP, OFFSOP, MEMOP, PUSHOP, POPOP, CNSTOP, RNDOP } optype0, optype1;
  rtx latehalf[2];
  rtx addreg0 = 0, addreg1 = 0;

  /* First classify both operands.  */

  if (REG_P (operands[0]))
    optype0 = REGOP;
  else if (offsettable_memref_p (operands[0]))
    optype0 = OFFSOP;
  else if (GET_CODE (operands[0]) == MEM)
    optype0 = MEMOP;
  else
    optype0 = RNDOP;

  if (REG_P (operands[1]))
    optype1 = REGOP;
  else if (CONSTANT_P (operands[1]))
    optype1 = CNSTOP;
  else if (offsettable_memref_p (operands[1]))
    optype1 = OFFSOP;
  else if (GET_CODE (operands[1]) == MEM)
    optype1 = MEMOP;
  else
    optype1 = RNDOP;

  /* Check for the cases that the operand constraints are not
     supposed to allow to happen.  Abort if we get one,
     because generating code for these cases is painful.  */

  if (optype0 == RNDOP || optype1 == RNDOP)
    abort ();

  /* If an operand is an unoffsettable memory ref, find a register
     we can increment temporarily to make it refer to the second word.  */

  if (optype0 == MEMOP)
    addreg0 = find_addr_reg (XEXP (operands[0], 0));

  if (optype1 == MEMOP)
    addreg1 = find_addr_reg (XEXP (operands[1], 0));

  /* Ok, we can do one word at a time.
     Normally we do the low-numbered word first,
     but if either operand is autodecrementing then we
     do the high-numbered word first.

     In either case, set up in LATEHALF the operands to use for the
     high-numbered (least significant) word and in some cases alter the
     operands in OPERANDS to be suitable for the low-numbered word.  */

  if (optype0 == REGOP)
    latehalf[0] = gen_rtx (REG, SImode, REGNO (operands[0]) + 1);
  else if (optype0 == OFFSOP)
    latehalf[0] = adj_offsettable_operand (operands[0], 4);
  else
    latehalf[0] = operands[0];

  if (optype1 == REGOP)
    latehalf[1] = gen_rtx (REG, SImode, REGNO (operands[1]) + 1);
  else if (optype1 == OFFSOP)
    latehalf[1] = adj_offsettable_operand (operands[1], 4);
  else if (optype1 == CNSTOP)
    split_double (operands[1], &operands[1], &latehalf[1]);
  else
    latehalf[1] = operands[1];

  /* If the first move would clobber the source of the second one,
     do them in the other order.

     RMS says "This happens only for registers;
     such overlap can't happen in memory unless the user explicitly
     sets it up, and that is an undefined circumstance."

     but it happens on the sparc when loading parameter registers,
     so I am going to define that circumstance, and make it work
     as expected.  */

  /* Easy case: try moving both words at once.  */
  /* First check for moving between an even/odd register pair
     and a memory location.  */
  if ((optype0 == REGOP && optype1 != REGOP && optype1 != CNSTOP
       && (REGNO (operands[0]) & 1) == 0)
      || (optype0 != REGOP && optype0 != CNSTOP && optype1 == REGOP
	  && (REGNO (operands[1]) & 1) == 0))
    {
      rtx op1, op2;
      rtx base = 0, offset = const0_rtx;

      /* OP1 gets the register pair, and OP2 gets the memory address.  */
      if (optype0 == REGOP)
	op1 = operands[0], op2 = operands[1];
      else
	op1 = operands[1], op2 = operands[0];

      /* Now see if we can trust the address to be 8-byte aligned.  */
      /* Trust double-precision floats in global variables.  */

      if (GET_CODE (XEXP (op2, 0)) == LO_SUM && GET_MODE (op2) == DFmode)
	{
	  if (final_sequence)
	    abort ();
	  return (op1 == operands[0] ? "ldd %1,%0" : "std %1,%0");
	}

      if (GET_CODE (XEXP (op2, 0)) == PLUS)
	{
	  rtx temp = XEXP (op2, 0);
	  if (GET_CODE (XEXP (temp, 0)) == REG)
	    base = XEXP (temp, 0), offset = XEXP (temp, 1);
	  else if (GET_CODE (XEXP (temp, 1)) == REG)
	    base = XEXP (temp, 1), offset = XEXP (temp, 0);
	}

      /* Trust round enough offsets from the stack or frame pointer.  */
      if (base
	  && (REGNO (base) == FRAME_POINTER_REGNUM
	      || REGNO (base) == STACK_POINTER_REGNUM))
	{
	  if (GET_CODE (offset) == CONST_INT
	      && (INTVAL (offset) & 0x7) == 0)
	    {
	      if (op1 == operands[0])
		return "ldd %1,%0";
	      else
		return "std %1,%0";
	    }
	}
      /* We know structs not on the stack are properly aligned.  Since a
	 double asks for 8-byte alignment, we know it must have got that
	 if it is in a struct.  But a DImode need not be 8-byte aligned,
	 because it could be a struct containing two ints or pointers.  */
      else if (GET_CODE (operands[1]) == MEM
	       && GET_MODE (operands[1]) == DFmode
	       && (CONSTANT_P (XEXP (operands[1], 0))
		   /* Let user ask for it anyway.  */
		   || TARGET_HOPE_ALIGN))
	return "ldd %1,%0";
      else if (GET_CODE (operands[0]) == MEM
	       && GET_MODE (operands[0]) == DFmode
	       && (CONSTANT_P (XEXP (operands[0], 0))
		   || TARGET_HOPE_ALIGN))
	return "std %1,%0";
    }

  if (optype0 == REGOP && optype1 == REGOP
      && REGNO (operands[0]) == REGNO (latehalf[1]))
    {
      /* Make any unoffsettable addresses point at high-numbered word.  */
      if (addreg0)
	output_asm_insn ("add %0,0x4,%0", &addreg0);
      if (addreg1)
	output_asm_insn ("add %0,0x4,%0", &addreg1);

      /* Do that word.  */
      output_asm_insn (singlemove_string (latehalf), latehalf);

      /* Undo the adds we just did.  */
      if (addreg0)
	output_asm_insn ("add %0,-0x4,%0", &addreg0);
      if (addreg1)
	output_asm_insn ("add %0,-0x4,%0", &addreg1);

      /* Do low-numbered word.  */
      return singlemove_string (operands);
    }
  else if (optype0 == REGOP && optype1 != REGOP
	   && reg_overlap_mentioned_p (operands[0], operands[1]))
    {
      /* Do the late half first.  */
      output_asm_insn (singlemove_string (latehalf), latehalf);
      /* Then clobber.  */
      return singlemove_string (operands);
    }

  /* Normal case: do the two words, low-numbered first.  */

  output_asm_insn (singlemove_string (operands), operands);

  /* Make any unoffsettable addresses point at high-numbered word.  */
  if (addreg0)
    output_asm_insn ("add %0,0x4,%0", &addreg0);
  if (addreg1)
    output_asm_insn ("add %0,0x4,%0", &addreg1);

  /* Do that word.  */
  output_asm_insn (singlemove_string (latehalf), latehalf);

  /* Undo the adds we just did.  */
  if (addreg0)
    output_asm_insn ("add %0,-0x4,%0", &addreg0);
  if (addreg1)
    output_asm_insn ("add %0,-0x4,%0", &addreg1);

  return "";
}

char *
output_fp_move_double (operands)
     rtx *operands;
{
  rtx addr;

  if (FP_REG_P (operands[0]))
    {
      if (FP_REG_P (operands[1]))
	return "fmovs %1,%0\n\tfmovs %R1,%R0";
      if (GET_CODE (operands[1]) == REG)
	{
	  if ((REGNO (operands[1]) & 1) == 0)
	    return "std %1,[%@-8]\n\tldd [%@-8],%0";
	  else
	    return "st %R1,[%@-4]\n\tst %1,[%@-8]\n\tldd [%@-8],%0";
	}
      addr = XEXP (operands[1], 0);

      /* Use ldd if known to be aligned.  */
      if (TARGET_HOPE_ALIGN
	  || (GET_CODE (addr) == PLUS
	      && (((XEXP (addr, 0) == frame_pointer_rtx
		    || XEXP (addr, 0) == stack_pointer_rtx)
		   && GET_CODE (XEXP (addr, 1)) == CONST_INT
		   && (INTVAL (XEXP (addr, 1)) & 0x7) == 0)
		  /* Arrays are known to be aligned,
		     and reg+reg addresses are used (on this machine)
		     only for array accesses.  */
		  || (REG_P (XEXP (addr, 0)) && REG_P (XEXP (addr, 1)))))
	  || (GET_MODE (operands[0]) == DFmode
	      && (GET_CODE (addr) == LO_SUM || CONSTANT_P (addr))))
	return "ldd %1,%0";

      /* Otherwise use two ld insns.  */
      operands[2]
	= gen_rtx (MEM, GET_MODE (operands[1]),
		   plus_constant_for_output (addr, 4));
	return "ld %1,%0\n\tld %2,%R0";
    }
  else if (FP_REG_P (operands[1]))
    {
      if (GET_CODE (operands[0]) == REG)
	{
	  if ((REGNO (operands[0]) & 1) == 0)
	    return "std %1,[%@-8]\n\tldd [%@-8],%0";
	  else
	    return "std %1,[%@-8]\n\tld [%@-4],%R0\n\tld [%@-8],%0";
	}
      addr = XEXP (operands[0], 0);

      /* Use std if we can be sure it is well-aligned.  */
      if (TARGET_HOPE_ALIGN
	  || (GET_CODE (addr) == PLUS
	      && (((XEXP (addr, 0) == frame_pointer_rtx
		    || XEXP (addr, 0) == stack_pointer_rtx)
		   && GET_CODE (XEXP (addr, 1)) == CONST_INT
		   && (INTVAL (XEXP (addr, 1)) & 0x7) == 0)
		  /* Arrays are known to be aligned,
		     and reg+reg addresses are used (on this machine)
		     only for array accesses.  */
		  || (REG_P (XEXP (addr, 0)) && REG_P (XEXP (addr, 1)))))
	  || (GET_MODE (operands[1]) == DFmode
	      && (GET_CODE (addr) == LO_SUM || CONSTANT_P (addr))))
	return "std %1,%0";

      /* Otherwise use two st insns.  */
      operands[2]
	= gen_rtx (MEM, GET_MODE (operands[0]),
		   plus_constant_for_output (addr, 4));
      return "st %r1,%0\n\tst %R1,%2";
    }
  else abort ();
}

/* Return a REG that occurs in ADDR with coefficient 1.
   ADDR can be effectively incremented by incrementing REG.  */

static rtx
find_addr_reg (addr)
     rtx addr;
{
  while (GET_CODE (addr) == PLUS)
    {
      /* We absolutely can not fudge the frame pointer here, because the
	 frame pointer must always be 8 byte aligned.  It also confuses
	 debuggers.  */
      if (GET_CODE (XEXP (addr, 0)) == REG
	  && REGNO (XEXP (addr, 0)) != FRAME_POINTER_REGNUM)
	addr = XEXP (addr, 0);
      else if (GET_CODE (XEXP (addr, 1)) == REG
	       && REGNO (XEXP (addr, 1)) != FRAME_POINTER_REGNUM)
	addr = XEXP (addr, 1);
      else if (CONSTANT_P (XEXP (addr, 0)))
	addr = XEXP (addr, 1);
      else if (CONSTANT_P (XEXP (addr, 1)))
	addr = XEXP (addr, 0);
      else
	abort ();
    }
  if (GET_CODE (addr) == REG)
    return addr;
  abort ();
}

void
output_sized_memop (opname, mode, signedp)
     char *opname;
     enum machine_mode mode;
     int signedp;
{
  static char *ld_size_suffix_u[] = { "ub", "uh", "", "?", "d" };
  static char *ld_size_suffix_s[] = { "sb", "sh", "", "?", "d" };
  static char *st_size_suffix[] = { "b", "h", "", "?", "d" };
  char **opnametab, *modename;

  if (opname[0] == 'l')
    if (signedp)
      opnametab = ld_size_suffix_s;
    else
      opnametab = ld_size_suffix_u;
  else
    opnametab = st_size_suffix;
  modename = opnametab[GET_MODE_SIZE (mode) >> 1];

  fprintf (asm_out_file, "\t%s%s", opname, modename);
}

void
output_move_with_extension (operands)
     rtx *operands;
{
  if (GET_MODE (operands[2]) == HImode)
    output_asm_insn ("sll %2,0x10,%0", operands);
  else if (GET_MODE (operands[2]) == QImode)
    output_asm_insn ("sll %2,0x18,%0", operands);
  else
    abort ();
}

/* Load the address specified by OPERANDS[3] into the register
   specified by OPERANDS[0].

   OPERANDS[3] may be the result of a sum, hence it could either be:

   (1) CONST
   (2) REG
   (2) REG + CONST_INT
   (3) REG + REG + CONST_INT
   (4) REG + REG  (special case of 3).

   Note that (3) is not a legitimate address.
   All cases are handled here.  */

void
output_load_address (operands)
     rtx *operands;
{
  rtx base, offset;

  if (CONSTANT_P (operands[3]))
    {
      output_asm_insn ("set %3,%0", operands);
      return;
    }

  if (REG_P (operands[3]))
    {
      if (REGNO (operands[0]) != REGNO (operands[3]))
	output_asm_insn ("mov %3,%0", operands);
      return;
    }

  if (GET_CODE (operands[3]) != PLUS)
    abort ();

  base = XEXP (operands[3], 0);
  offset = XEXP (operands[3], 1);

  if (GET_CODE (base) == CONST_INT)
    {
      rtx tmp = base;
      base = offset;
      offset = tmp;
    }

  if (GET_CODE (offset) != CONST_INT)
    {
      /* Operand is (PLUS (REG) (REG)).  */
      base = operands[3];
      offset = const0_rtx;
    }

  if (REG_P (base))
    {
      operands[6] = base;
      operands[7] = offset;
      if (SMALL_INT (offset))
	output_asm_insn ("add %6,%7,%0", operands);
      else
	output_asm_insn ("set %7,%0\n\tadd %0,%6,%0", operands);
    }
  else if (GET_CODE (base) == PLUS)
    {
      operands[6] = XEXP (base, 0);
      operands[7] = XEXP (base, 1);
      operands[8] = offset;

      if (SMALL_INT (offset))
	output_asm_insn ("add %6,%7,%0\n\tadd %0,%8,%0", operands);
      else
	output_asm_insn ("set %8,%0\n\tadd %0,%6,%0\n\tadd %0,%7,%0", operands);
    }
  else
    abort ();
}

/* Output code to place a size count SIZE in register REG.
   ALIGN is the size of the unit of transfer.

   Because block moves are pipelined, we don't include the
   first element in the transfer of SIZE to REG.  */

static void
output_size_for_block_move (size, reg, align)
     rtx size, reg;
     rtx align;
{
  rtx xoperands[3];

  xoperands[0] = reg;
  xoperands[1] = size;
  xoperands[2] = align;
  if (GET_CODE (size) == REG)
    output_asm_insn ("sub %1,%2,%0", xoperands);
  else
    {
      xoperands[1]
	= gen_rtx (CONST_INT, VOIDmode, INTVAL (size) - INTVAL (align));
      output_asm_insn ("set %1,%0", xoperands);
    }
}

/* Emit code to perform a block move.

   OPERANDS[0] is the destination.
   OPERANDS[1] is the source.
   OPERANDS[2] is the size.
   OPERANDS[3] is the alignment safe to use.
   OPERANDS[4] is a register we can safely clobber as a temp.  */

char *
output_block_move (operands)
     rtx *operands;
{
  /* A vector for our computed operands.  Note that load_output_address
     makes use of (and can clobber) up to the 8th element of this vector.  */
  rtx xoperands[10];
  rtx zoperands[10];
  static int movstrsi_label = 0;
  int i;
  rtx temp1 = operands[4];
  rtx sizertx = operands[2];
  rtx alignrtx = operands[3];
  int align = INTVAL (alignrtx);

  xoperands[0] = operands[0];
  xoperands[1] = operands[1];
  xoperands[2] = temp1;

  /* We can't move more than this many bytes at a time because we have only
     one register, %g1, to move them through.  */
  if (align > UNITS_PER_WORD)
    {
      align = UNITS_PER_WORD;
      alignrtx = gen_rtx (CONST_INT, VOIDmode, UNITS_PER_WORD);
    }

  /* We consider 8 ld/st pairs, for a total of 16 inline insns to be
     reasonable here.  (Actually will emit a maximum of 18 inline insns for
     the case of size == 31 and align == 4).  */

  if (GET_CODE (sizertx) == CONST_INT && (INTVAL (sizertx) / align) <= 8
      && memory_address_p (QImode, plus_constant_for_output (xoperands[0],
							     INTVAL (sizertx)))
      && memory_address_p (QImode, plus_constant_for_output (xoperands[1],
							     INTVAL (sizertx))))
    {
      int size = INTVAL (sizertx);
      int offset = 0;

      /* We will store different integers into this particular RTX.  */
      xoperands[2] = rtx_alloc (CONST_INT);
      PUT_MODE (xoperands[2], VOIDmode);

      /* This case is currently not handled.  Abort instead of generating
	 bad code.  */
      if (align > 4)
	abort ();

      if (align >= 4)
	{
	  for (i = (size >> 2) - 1; i >= 0; i--)
	    {
	      INTVAL (xoperands[2]) = (i << 2) + offset;
	      output_asm_insn ("ld [%a1+%2],%%g1\n\tst %%g1,[%a0+%2]",
			       xoperands);
	    }
	  offset += (size & ~0x3);
	  size = size & 0x3;
	  if (size == 0)
	    return "";
	}

      if (align >= 2)
	{
	  for (i = (size >> 1) - 1; i >= 0; i--)
	    {
	      INTVAL (xoperands[2]) = (i << 1) + offset;
	      output_asm_insn ("lduh [%a1+%2],%%g1\n\tsth %%g1,[%a0+%2]",
			       xoperands);
	    }
	  offset += (size & ~0x1);
	  size = size & 0x1;
	  if (size == 0)
	    return "";
	}

      if (align >= 1)
	{
	  for (i = size - 1; i >= 0; i--)
	    {
	      INTVAL (xoperands[2]) = i + offset;
	      output_asm_insn ("ldub [%a1+%2],%%g1\n\tstb %%g1,[%a0+%2]",
			       xoperands);
	    }
	  return "";
	}

      /* We should never reach here.  */
      abort ();
    }

  /* If the size isn't known to be a multiple of the alignment,
     we have to do it in smaller pieces.  If we could determine that
     the size was a multiple of 2 (or whatever), we could be smarter
     about this.  */
  if (GET_CODE (sizertx) != CONST_INT)
    align = 1;
  else
    {
      int size = INTVAL (sizertx);
      while (size % align)
	align >>= 1;
    }

  if (align != INTVAL (alignrtx))
    alignrtx = gen_rtx (CONST_INT, VOIDmode, align);

  xoperands[3] = gen_rtx (CONST_INT, VOIDmode, movstrsi_label++);
  xoperands[4] = gen_rtx (CONST_INT, VOIDmode, align);
  xoperands[5] = gen_rtx (CONST_INT, VOIDmode, movstrsi_label++);

  /* This is the size of the transfer.  Emit code to decrement the size
     value by ALIGN, and store the result in the temp1 register.  */
  output_size_for_block_move (sizertx, temp1, alignrtx);

  /* Must handle the case when the size is zero or negative, so the first thing
     we do is compare the size against zero, and only copy bytes if it is
     zero or greater.  Note that we have already subtracted off the alignment
     once, so we must copy 1 alignment worth of bytes if the size is zero
     here.

     The SUN assembler complains about labels in branch delay slots, so we
     do this before outputting the load address, so that there will always
     be a harmless insn between the branch here and the next label emitted
     below.  */

#ifdef NO_UNDERSCORES
  output_asm_insn ("cmp %2,0\n\tbl .Lm%5", xoperands);
#else
  output_asm_insn ("cmp %2,0\n\tbl Lm%5", xoperands);
#endif

  zoperands[0] = operands[0];
  zoperands[3] = plus_constant_for_output (operands[0], align);
  output_load_address (zoperands);

  /* ??? This might be much faster if the loops below were preconditioned
     and unrolled.

     That is, at run time, copy enough bytes one at a time to ensure that the
     target and source addresses are aligned to the the largest possible
     alignment.  Then use a preconditioned unrolled loop to copy say 16
     bytes at a time.  Then copy bytes one at a time until finish the rest.  */

  /* Output the first label separately, so that it is spaced properly.  */

#ifdef NO_UNDERSCORES
  ASM_OUTPUT_INTERNAL_LABEL (asm_out_file, ".Lm", INTVAL (xoperands[3]));
#else
  ASM_OUTPUT_INTERNAL_LABEL (asm_out_file, "Lm", INTVAL (xoperands[3]));
#endif

#ifdef NO_UNDERSCORES
  if (align == 1)
    output_asm_insn ("ldub [%1+%2],%%g1\n\tsubcc %2,%4,%2\n\tbge .Lm%3\n\tstb %%g1,[%0+%2]\n.Lm%5:", xoperands);
  else if (align == 2)
    output_asm_insn ("lduh [%1+%2],%%g1\n\tsubcc %2,%4,%2\n\tbge .Lm%3\n\tsth %%g1,[%0+%2]\n.Lm%5:", xoperands);
  else
    output_asm_insn ("ld [%1+%2],%%g1\n\tsubcc %2,%4,%2\n\tbge .Lm%3\n\tst %%g1,[%0+%2]\n.Lm%5:", xoperands);
  return "";
#else
  if (align == 1)
    output_asm_insn ("ldub [%1+%2],%%g1\n\tsubcc %2,%4,%2\n\tbge Lm%3\n\tstb %%g1,[%0+%2]\nLm%5:", xoperands);
  else if (align == 2)
    output_asm_insn ("lduh [%1+%2],%%g1\n\tsubcc %2,%4,%2\n\tbge Lm%3\n\tsth %%g1,[%0+%2]\nLm%5:", xoperands);
  else
    output_asm_insn ("ld [%1+%2],%%g1\n\tsubcc %2,%4,%2\n\tbge Lm%3\n\tst %%g1,[%0+%2]\nLm%5:", xoperands);
  return "";
#endif
}

/* Output reasonable peephole for set-on-condition-code insns.
   Note that these insns assume a particular way of defining
   labels.  Therefore, *both* sparc.h and this function must
   be changed if a new syntax is needed.    */

char *
output_scc_insn (operands, insn)
     rtx operands[];
     rtx insn;
{
  static char string[100];
  rtx label = 0, next = insn;
  int need_label = 0;

  /* Try doing a jump optimization which jump.c can't do for us
     because we did not expose that setcc works by using branches.

     If this scc insn is followed by an unconditional branch, then have
     the jump insn emitted here jump to that location, instead of to
     the end of the scc sequence as usual.  */

  do
    {
      if (GET_CODE (next) == CODE_LABEL)
	label = next;
      next = NEXT_INSN (next);
      if (next == 0)
	break;
    }
  while (GET_CODE (next) == NOTE || GET_CODE (next) == CODE_LABEL);

  /* If we are in a sequence, and the following insn is a sequence also,
     then just following the current insn's next field will take us to the
     first insn of the next sequence, which is the wrong place.  We don't
     want to optimize with a branch that has had its delay slot filled.
     Avoid this by verifying that NEXT_INSN (PREV_INSN (next)) == next
     which fails only if NEXT is such a branch.  */

  if (next && GET_CODE (next) == JUMP_INSN && simplejump_p (next)
      && (! final_sequence || NEXT_INSN (PREV_INSN (next)) == next))
    label = JUMP_LABEL (next);
  /* If not optimizing, jump label fields are not set.  To be safe, always
     check here to whether label is still zero.  */
  if (label == 0)
    {
      label = gen_label_rtx ();
      need_label = 1;
    }

  LABEL_NUSES (label) += 1;

  operands[2] = label;

  /* If we are in a delay slot, assume it is the delay slot of an fpcc
     insn since our type isn't allowed anywhere else.  */

  /* ??? Fpcc instructions no longer have delay slots, so this code is
     probably obsolete.  */

  /* The fastest way to emit code for this is an annulled branch followed
     by two move insns.  This will take two cycles if the branch is taken,
     and three cycles if the branch is not taken.

     However, if we are in the delay slot of another branch, this won't work,
     because we can't put a branch in the delay slot of another branch.
     The above sequence would effectively take 3 or 4 cycles respectively
     since a no op would have be inserted between the two branches.
     In this case, we want to emit a move, annulled branch, and then the
     second move.  This sequence always takes 3 cycles, and hence is faster
     when we are in a branch delay slot.  */

  if (final_sequence)
    {
      strcpy (string, "mov 0,%0\n\t");
      strcat (string, output_cbranch (operands[1], 2, 0, 1, 0));
      strcat (string, "\n\tmov 1,%0");
    }
  else
    {
      strcpy (string, output_cbranch (operands[1], 2, 0, 1, 0));
      strcat (string, "\n\tmov 1,%0\n\tmov 0,%0");
    }

  if (need_label)
    strcat (string, "\n%l2:");

  return string;
}

/* Vectors to keep interesting information about registers where
   it can easily be got.  */

/* Modes for condition codes.  */
#define C_MODES		\
  ((1 << (int) CCmode) | (1 << (int) CC_NOOVmode) | (1 << (int) CCFPmode))

/* Modes for single-word (and smaller) quantities.  */
#define S_MODES						\
 (~C_MODES						\
  & ~ ((1 << (int) DImode) | (1 << (int) TImode)	\
      | (1 << (int) DFmode) | (1 << (int) TFmode)))

/* Modes for double-word (and smaller) quantities.  */
#define D_MODES					\
  (~C_MODES					\
   & ~ ((1 << (int) TImode) | (1 << (int) TFmode)))

/* Modes for quad-word quantities.  */
#define T_MODES (~C_MODES)

/* Modes for single-float quantities.  */
#define SF_MODES ((1 << (int) SFmode))

/* Modes for double-float quantities.  */
#define DF_MODES (SF_MODES | (1 << (int) DFmode) | (1 << (int) SCmode))

/* Modes for quad-float quantities.  */
#define TF_MODES (DF_MODES | (1 << (int) TFmode) | (1 << (int) DCmode))

/* Value is 1 if register/mode pair is acceptable on sparc.
   The funny mixture of D and T modes is because integer operations
   do not specially operate on tetra quantities, so non-quad-aligned
   registers can hold quadword quantities (except %o4 and %i4 because
   they cross fixed registers.  */

int hard_regno_mode_ok[] = {
  C_MODES, S_MODES, T_MODES, S_MODES, T_MODES, S_MODES, D_MODES, S_MODES,
  T_MODES, S_MODES, T_MODES, S_MODES, D_MODES, S_MODES, D_MODES, S_MODES,
  T_MODES, S_MODES, T_MODES, S_MODES, T_MODES, S_MODES, D_MODES, S_MODES,
  T_MODES, S_MODES, T_MODES, S_MODES, D_MODES, S_MODES, D_MODES, S_MODES,

  TF_MODES, SF_MODES, DF_MODES, SF_MODES, TF_MODES, SF_MODES, DF_MODES, SF_MODES,
  TF_MODES, SF_MODES, DF_MODES, SF_MODES, TF_MODES, SF_MODES, DF_MODES, SF_MODES,
  TF_MODES, SF_MODES, DF_MODES, SF_MODES, TF_MODES, SF_MODES, DF_MODES, SF_MODES,
  TF_MODES, SF_MODES, DF_MODES, SF_MODES, TF_MODES, SF_MODES, DF_MODES, SF_MODES};

#ifdef __GNUC__
inline
#endif
static int
save_regs (file, low, high, base, offset, n_fregs)
     FILE *file;
     int low, high;
     char *base;
     int offset;
     int n_fregs;
{
  int i;

  for (i = low; i < high; i += 2)
    {
      if (regs_ever_live[i] && ! call_used_regs[i])
	if (regs_ever_live[i+1] && ! call_used_regs[i+1])
	  fprintf (file, "\tstd %s,[%s+%d]\n",
		   reg_names[i], base, offset + 4 * n_fregs),
	  n_fregs += 2;
	else
	  fprintf (file, "\tst %s,[%s+%d]\n",
		   reg_names[i], base, offset + 4 * n_fregs),
	  n_fregs += 2;
      else if (regs_ever_live[i+1] && ! call_used_regs[i+1])
	fprintf (file, "\tst %s,[%s+%d]\n",
		 reg_names[i+1], base, offset + 4 * n_fregs),
	n_fregs += 2;
    }
  return n_fregs;
}

#ifdef __GNUC__
inline
#endif
static int
restore_regs (file, low, high, base, offset, n_fregs)
     FILE *file;
     int low, high;
     char *base;
     int offset;
{
  int i;

  for (i = low; i < high; i += 2)
    {
      if (regs_ever_live[i] && ! call_used_regs[i])
	if (regs_ever_live[i+1] && ! call_used_regs[i+1])
	  fprintf (file, "\tldd [%s+%d], %s\n",
		   base, offset + 4 * n_fregs, reg_names[i]),
	  n_fregs += 2;
	else
	  fprintf (file, "\tld [%s+%d],%s\n",
		   base, offset + 4 * n_fregs, reg_names[i]),
	  n_fregs += 2;
      else if (regs_ever_live[i+1] && ! call_used_regs[i+1])
	fprintf (file, "\tld [%s+%d],%s\n",
		 base, offset + 4 * n_fregs, reg_names[i+1]),
	n_fregs += 2;
    }
  return n_fregs;
}

/* Static variables we want to share between prologue and epilogue.  */

/* Number of live floating point registers needed to be saved.  */
static int num_fregs;

/* Nonzero if any floating point register was ever used.  */
static int fregs_ever_live;

int
compute_frame_size (size, leaf_function)
     int size;
     int leaf_function;
{
  int fregs_ever_live = 0;
  int n_fregs = 0, i;
  int outgoing_args_size = (current_function_outgoing_args_size
			    + REG_PARM_STACK_SPACE (current_function_decl));

  apparent_fsize = ((size) + 7 - STARTING_FRAME_OFFSET) & -8;
  for (i = 32; i < FIRST_PSEUDO_REGISTER; i += 2)
    fregs_ever_live |= regs_ever_live[i]|regs_ever_live[i+1];

  if (TARGET_EPILOGUE && fregs_ever_live)
    {
      for (i = 32; i < FIRST_PSEUDO_REGISTER; i += 2)
	if ((regs_ever_live[i] && ! call_used_regs[i])
	    || (regs_ever_live[i+1] && ! call_used_regs[i+1]))
	  n_fregs += 2;
    }

  /* Set up values for use in `function_epilogue'.  */
  num_fregs = n_fregs;

  apparent_fsize += (outgoing_args_size+7) & -8;
  if (leaf_function && n_fregs == 0
      && apparent_fsize == (REG_PARM_STACK_SPACE (current_function_decl)
			    - STARTING_FRAME_OFFSET))
    apparent_fsize = 0;

  actual_fsize = apparent_fsize + n_fregs*4;

  /* Make sure nothing can clobber our register windows.
     If a SAVE must be done, or there is a stack-local variable,
     the register window area must be allocated.  */
  if (leaf_function == 0 || size > 0)
    actual_fsize += (16 * UNITS_PER_WORD)+8;

  return actual_fsize;
}

void
output_function_prologue (file, size, leaf_function)
     FILE *file;
     int size;
{
  if (leaf_function)
    frame_base_name = "%sp+80";
  else
    frame_base_name = "%fp";

  actual_fsize = compute_frame_size (size, leaf_function);

  fprintf (file, "\t!#PROLOGUE# 0\n");
  if (actual_fsize == 0) /* do nothing.  */ ;
  else if (actual_fsize < 4096)
    {
      if (! leaf_function)
	fprintf (file, "\tsave %%sp,-%d,%%sp\n", actual_fsize);
      else
	fprintf (file, "\tadd %%sp,-%d,%%sp\n", actual_fsize);
    }
  else if (! leaf_function)
    {
      /* Need to use actual_fsize, since we are also allocating space for
	 our callee (and our own register save area).  */
      fprintf (file, "\tsethi %%hi(%d),%%g1\n\tor %%g1,%%lo(%d),%%g1\n",
	       -actual_fsize, -actual_fsize);
      fprintf (file, "\tsave %%sp,%%g1,%%sp\n");
    }
  else
    {
      /* Put pointer to parameters into %g4, and allocate
	 frame space using result computed into %g1.  actual_fsize
	 used instead of apparent_fsize for reasons stated above.  */
      abort ();

      fprintf (file, "\tsethi %%hi(%d),%%g1\n\tor %%g1,%%lo(%d),%%g1\n",
	       -actual_fsize, -actual_fsize);
      fprintf (file, "\tadd %%sp,64,%%g4\n\tadd %%sp,%%g1,%%sp\n");
    }

  /* If doing anything with PIC, do it now.  */
  if (! flag_pic)
    fprintf (file, "\t!#PROLOGUE# 1\n");

  /* Figure out where to save any special registers.  */
  if (num_fregs)
    {
      int offset, n_fregs = num_fregs;

      if (! leaf_function)
	offset = -apparent_fsize;
      else
	offset = 0;

      if (TARGET_EPILOGUE && ! leaf_function)
	n_fregs = save_regs (file, 0, 16, frame_base_name, offset, 0);
      else if (leaf_function)
	n_fregs = save_regs (file, 0, 32, frame_base_name, offset, 0);
      if (TARGET_EPILOGUE)
	save_regs (file, 32, FIRST_PSEUDO_REGISTER,
		   frame_base_name, offset, n_fregs);
    }

  if (regs_ever_live[62])
    fprintf (file, "\tst %s,[%s-16]\n\tst %s,[%s-12]\n",
	     reg_names[0], frame_base_name,
	     reg_names[0], frame_base_name);

  leaf_label = 0;
  if (leaf_function && actual_fsize != 0)
    {
      /* warning ("leaf procedure with frame size %d", actual_fsize); */
      if (! TARGET_EPILOGUE)
	leaf_label = gen_label_rtx ();
    }
}

void
output_function_epilogue (file, size, leaf_function, true_epilogue)
     FILE *file;
     int size;
{
  int n_fregs, i;
  char *ret;

  if (leaf_label)
    {
      if (leaf_function < 0)
	abort ();
      emit_label_after (leaf_label, get_last_insn ());
      final_scan_insn (get_last_insn (), file, 0, 0, 1);
    }

  if (num_fregs)
    {
      int offset, n_fregs = num_fregs;

      if (! leaf_function)
	offset = -apparent_fsize;
      else
	offset = 0;

      if (TARGET_EPILOGUE && ! leaf_function)
	n_fregs = restore_regs (file, 0, 16, frame_base_name, offset, 0);
      else if (leaf_function)
	n_fregs = restore_regs (file, 0, 32, frame_base_name, offset, 0);
      if (TARGET_EPILOGUE)
	restore_regs (file, 32, FIRST_PSEUDO_REGISTER,
		      frame_base_name, offset, n_fregs);
    }

  /* Work out how to skip the caller's unimp instruction if required.  */
  if (leaf_function)
    ret = (current_function_returns_struct ? "jmp %o7+12" : "retl");
  else
    ret = (current_function_returns_struct ? "jmp %i7+12" : "ret");

  /* Tail calls have to do this work themselves.  */
  if (leaf_function >= 0)
    {
      if (TARGET_EPILOGUE || leaf_label)
	{
	  int old_target_epilogue = TARGET_EPILOGUE;
	  target_flags &= ~old_target_epilogue;

	  if (! leaf_function)
	    {
	      /* If we wound up with things in our delay slot,
		 flush them here.  */
	      if (current_function_epilogue_delay_list)
		{
		  rtx insn = emit_jump_insn_after (gen_rtx (RETURN, VOIDmode),
						   get_last_insn ());
		  PATTERN (insn) = gen_rtx (PARALLEL, VOIDmode,
					    gen_rtvec (2,
						       PATTERN (XEXP (current_function_epilogue_delay_list, 0)),
						       PATTERN (insn)));
		  final_scan_insn (insn, file, 1, 0, 1);
		}
	      else
		fprintf (file, "\t%s\n\trestore\n", ret);
	    }
	  else if (actual_fsize < 4096)
	    {
	      if (current_function_epilogue_delay_list)
		{
		  fprintf (file, "\t%s\n", ret);
		  final_scan_insn (XEXP (current_function_epilogue_delay_list, 0),
				   file, 1, 0, 1);
		}
	      else
		fprintf (file, "\t%s\n\tadd %%sp,%d,%%sp\n",
			 ret, actual_fsize);
	    }
	  else
	    {
	      if (current_function_epilogue_delay_list)
		abort ();
	      fprintf (file, "\tsethi %%hi(%d),%%g1\n\tor %%g1,%%lo(%d),%%g1\n\t%s\n\tadd %%sp,%%g1,%%sp\n",
		       actual_fsize, actual_fsize, ret);
	    }
	  target_flags |= old_target_epilogue;
	}
    }
  else if (true_epilogue)
    {
      /* We may still need a return insn!  Somebody could jump around
	 the tail-calls that this function makes.  */
      if (TARGET_EPILOGUE)
	{
	  rtx last = get_last_insn ();

	  last = prev_nonnote_insn (last);
	  if (last == 0
	      || (GET_CODE (last) != JUMP_INSN && GET_CODE (last) != BARRIER))
	    fprintf (file, "\t%s\n\tnop\n", ret);
	}
    }
}

/* Return the string to output a conditional branch to LABEL, which is
   the operand number of the label.  OP is the conditional expression.  The
   mode of register 0 says what kind of comparison we made.

   REVERSED is non-zero if we should reverse the sense of the comparison.

   ANNUL is non-zero if we should generate an annulling branch.

   NOOP is non-zero if we have to follow this branch by a noop.  */

char *
output_cbranch (op, label, reversed, annul, noop)
     rtx op;
     int label;
     int reversed, annul, noop;
{
  static char string[20];
  enum rtx_code code = GET_CODE (op);
  enum machine_mode mode = GET_MODE (XEXP (op, 0));
  static char labelno[] = " %lX";

  /* ??? FP branches can not be preceded by another floating point insn.
     Because there is currently no concept of pre-delay slots, we can fix
     this only by always emitting a nop before a floating point branch.  */

  if (mode == CCFPmode)
    strcpy (string, "nop\n\t");

  /* If not floating-point or if EQ or NE, we can just reverse the code.  */
  if (reversed && (mode != CCFPmode || code == EQ || code == NE))
    code = reverse_condition (code), reversed = 0;

  /* Start by writing the branch condition.  */
  switch (code)
    {
    case NE:
      if (mode == CCFPmode)
	strcat (string, "fbne");
      else
	strcpy (string, "bne");
      break;

    case EQ:
      if (mode == CCFPmode)
	strcat (string, "fbe");
      else
	strcpy (string, "be");
      break;

    case GE:
      if (mode == CCFPmode)
	{
	  if (reversed)
	    strcat (string, "fbul");
	  else
	    strcat (string, "fbge");
	}
      else if (mode == CC_NOOVmode)
	strcpy (string, "bpos");
      else
	strcpy (string, "bge");
      break;

    case GT:
      if (mode == CCFPmode)
	{
	  if (reversed)
	    strcat (string, "fbule");
	  else
	    strcat (string, "fbg");
	}
      else
	strcpy (string, "bg");
      break;

    case LE:
      if (mode == CCFPmode)
	{
	  if (reversed)
	    strcat (string, "fbug");
	  else
	    strcat (string, "fble");
	}
      else
	strcpy (string, "ble");
      break;

    case LT:
      if (mode == CCFPmode)
	{
	  if (reversed)
	    strcat (string, "fbuge");
	  else
	    strcat (string, "fbl");
	}
      else if (mode == CC_NOOVmode)
	strcpy (string, "bneg");
      else
	strcpy (string, "bl");
      break;

    case GEU:
      strcpy (string, "bgeu");
      break;

    case GTU:
      strcpy (string, "bgu");
      break;

    case LEU:
      strcpy (string, "bleu");
      break;

    case LTU:
      strcpy (string, "blu");
      break;
    }

  /* Now add the annulling, the label, and a possible noop.  */
  if (annul)
    strcat (string, ",a");

  labelno[3] = label + '0';
  strcat (string, labelno);

  if (noop)
    strcat (string, "\n\tnop");

  return string;
}

char *
output_return (operands)
     rtx *operands;
{
  if (leaf_label)
    {
      operands[0] = leaf_label;
      return "b,a %l0";
    }
  else if (leaf_function)
    {
      operands[0] = gen_rtx (CONST_INT, VOIDmode, actual_fsize);
      if (actual_fsize < 4096)
	{
	  if (current_function_returns_struct)
	    return "jmp %%o7+12\n\tadd %%sp,%0,%%sp";
	  else
	    return "retl\n\tadd %%sp,%0,%%sp";
	}
      else
	{
	  if (current_function_returns_struct)
	    return "sethi %%hi(%a0),%%g1\n\tor %%g1,%%lo(%a0),%%g1\n\tjmp %%o7+12\n\tadd %%sp,%%g1,%%sp";
	  else
	    return "sethi %%hi(%a0),%%g1\n\tor %%g1,%%lo(%a0),%%g1\n\tretl\n\tadd %%sp,%%g1,%%sp";
	}
    }
  else
    {
      if (current_function_returns_struct)
	return "jmp %%i7+12\n\trestore";
      else
	return "ret\n\trestore";
    }
}

char *
output_floatsisf2 (operands)
     rtx *operands;
{
  if (GET_CODE (operands[1]) == MEM)
    return "ld %1,%0\n\tfitos %0,%0";
  else if (FP_REG_P (operands[1]))
    return "fitos %1,%0";
  return "st %r1,[%%fp-4]\n\tld [%%fp-4],%0\n\tfitos %0,%0";
}

char *
output_floatsidf2 (operands)
     rtx *operands;
{
  if (GET_CODE (operands[1]) == MEM)
    return "ld %1,%0\n\tfitod %0,%0";
  else if (FP_REG_P (operands[1]))
    return "fitod %1,%0";
  return "st %r1,[%%fp-4]\n\tld [%%fp-4],%0\n\tfitod %0,%0";
}

int
tail_call_valid_p ()
{
  static int checked = 0;
  static int valid_p = 0;

  if (! checked)
    {
      register int i;

      checked = 1;
      for (i = 32; i < FIRST_PSEUDO_REGISTER; i++)
	if (! fixed_regs[i] && ! call_used_regs[i])
	  return 0;
      valid_p = 1;
    }
  return valid_p;
}

/* Leaf functions and non-leaf functions have different needs.  */

static int
reg_leaf_alloc_order[] = REG_LEAF_ALLOC_ORDER;

static int
reg_nonleaf_alloc_order[] = REG_ALLOC_ORDER;

static int *reg_alloc_orders[] = {
  reg_leaf_alloc_order,
  reg_nonleaf_alloc_order};

void
order_regs_for_local_alloc ()
{
  static int last_order_nonleaf = 1;

  if (regs_ever_live[15] != last_order_nonleaf)
    {
      last_order_nonleaf = !last_order_nonleaf;
      bcopy (reg_alloc_orders[last_order_nonleaf], reg_alloc_order,
	     FIRST_PSEUDO_REGISTER * sizeof (int));
    }
}

/* Machine dependent routines for the branch probability, arc profiling
   code.  */

/* The label used by the arc profiling code.  */

static rtx profiler_label;

void
init_arc_profiler ()
{
  /* Generate and save a copy of this so it can be shared.  */
  profiler_label = gen_rtx (SYMBOL_REF, Pmode, "*LPBX2");
}

void
output_arc_profiler (arcno, insert_after)
     int arcno;
     rtx insert_after;
{
  rtx profiler_target_addr
    = gen_rtx (CONST, Pmode,
	       gen_rtx (PLUS, Pmode, profiler_label,
			gen_rtx (CONST_INT, VOIDmode, 4 * arcno)));
  register rtx profiler_reg = gen_reg_rtx (SImode);
  register rtx address_reg = gen_reg_rtx (Pmode);
  rtx mem_ref;

  insert_after = emit_insn_after (gen_rtx (SET, VOIDmode, address_reg,
					   gen_rtx (HIGH, Pmode,
						    profiler_target_addr)),
				  insert_after);

  mem_ref = gen_rtx (MEM, SImode, gen_rtx (LO_SUM, Pmode, address_reg,
					   profiler_target_addr));
  insert_after = emit_insn_after (gen_rtx (SET, VOIDmode, profiler_reg,
					   mem_ref),
				  insert_after);

  insert_after = emit_insn_after (gen_rtx (SET, VOIDmode, profiler_reg,
					   gen_rtx (PLUS, SImode, profiler_reg,
						    const1_rtx)),
				  insert_after);

  /* This is the same rtx as above, but it is not legal to share this rtx.  */
  mem_ref = gen_rtx (MEM, SImode, gen_rtx (LO_SUM, Pmode, address_reg,
					   profiler_target_addr));
  emit_insn_after (gen_rtx (SET, VOIDmode, mem_ref, profiler_reg),
		   insert_after);
}

/* All the remaining routines in this file have been turned off.  */
#if 0
char *
output_tail_call (operands, insn)
     rtx *operands;
     rtx insn;
{
  int this_fsize = actual_fsize;
  rtx next;
  int need_nop_at_end = 0;

  next = next_real_insn (insn);
  while (next && GET_CODE (next) == CODE_LABEL)
    next = next_real_insn (insn);

  if (final_sequence && this_fsize > 0)
    {
      rtx xoperands[1];

      /* If we have to restore any registers, don't take any chances
	 restoring a register before we discharge it into
	 its home.  If the frame size is only 88, we are guaranteed
	 that the epilogue will fit in the delay slot.  */
      rtx delay_insn = XVECEXP (final_sequence, 0, 1);
      if (GET_CODE (PATTERN (delay_insn)) == SET)
	{
	  rtx dest = SET_DEST (PATTERN (delay_insn));
	  if (GET_CODE (dest) == REG
	      && reg_mentioned_p (dest, insn))
	    abort ();
	}
      else if (GET_CODE (PATTERN (delay_insn)) == PARALLEL)
	abort ();
      xoperands[0] = operands[0];
      final_scan_insn (delay_insn, asm_out_file, 0, 0, 1);
      operands[0] = xoperands[0];
      final_sequence = 0;
    }

  /* Make sure we are clear to return.  */
  output_function_epilogue (asm_out_file, get_frame_size (), -1, 0);

  /* Strip the MEM.  */
  operands[0] = XEXP (operands[0], 0);

  if (final_sequence == 0
      && (next == 0
	  || GET_CODE (next) == CALL_INSN
	  || GET_CODE (next) == JUMP_INSN))
    need_nop_at_end = 1;

  if (flag_pic)
    return output_pic_sequence_2 (2, 3, 0, "jmpl %%g1+%3", operands, need_nop_at_end);

  if (GET_CODE (operands[0]) == REG)
    output_asm_insn ("jmpl %a0,%%g0", operands);
  else if (TARGET_TAIL_CALL)
    {
      /* We assume all labels will be within 16 MB of our call.  */
      if (need_nop_at_end || final_sequence)
	output_asm_insn ("b %a0", operands);
      else
	output_asm_insn ("b,a %a0", operands);
    }
  else if (! final_sequence)
    {
      output_asm_insn ("sethi %%hi(%a0),%%g1\n\tjmpl %%g1+%%lo(%a0),%%g1",
		       operands);
    }
  else
    {
      int i;
      rtx x = PATTERN (XVECEXP (final_sequence, 0, 1));
      for (i = 1; i < 32; i++)
	if ((i == 1 || ! fixed_regs[i])
	    && call_used_regs[i]
	    && ! refers_to_regno_p (i, i+1, x, 0))
	  break;
      if (i == 32)
	abort ();
      operands[1] = gen_rtx (REG, SImode, i);
      output_asm_insn ("sethi %%hi(%a0),%1\n\tjmpl %1+%%lo(%a0),%1", operands);
    }
  return (need_nop_at_end ? "nop" : "");
}
#endif

/* Print operand X (an rtx) in assembler syntax to file FILE.
   CODE is a letter or dot (`z' in `%z0') or 0 if no letter was specified.
   For `%' followed by punctuation, CODE is the punctuation and X is null.  */

void
print_operand (file, x, code)
     FILE *file;
     rtx x;
     int code;
{
  switch (code)
    {
    case '#':
      /* Output a 'nop' if there's nothing for the delay slot.  */
      if (dbr_sequence_length () == 0)
	fputs ("\n\tnop", file);
      return;
    case '*':
      /* Output an annul flag if there's nothing for the delay slot.  */
      if (dbr_sequence_length () == 0)
        fputs (",a", file);
      return;
    case 'Y':
      /* Adjust the operand to take into account a RESTORE operation.  */
      if (GET_CODE (x) != REG)
	abort ();
      if (REGNO (x) < 8)
	fputs (reg_names[REGNO (x)], file);
      else if (REGNO (x) >= 24 && REGNO (x) < 32)
	fputs (reg_names[REGNO (x)-16], file);
      else
	abort ();
      return;
    case '@':
      /* Print out what we are using as the frame pointer.  This might
	 be %fp, or might be %sp+offset.  */
      fputs (frame_base_name, file);
      return;
    case 'R':
      /* Print out the second register name of a register pair.
	 I.e., R (%o0) => %o1.  */
      fputs (reg_names[REGNO (x)+1], file);
      return;
    case 'm':
      /* Print the operand's address only.  */
      output_address (XEXP (x, 0));
      return;
    case 'r':
      /* In this case we need a register.  Use %g0 if the
	 operand in const0_rtx.  */
      if (x == const0_rtx)
	{
	  fputs ("%g0", file);
	  return;
	}
      else
	break;

    case  'A':
      switch (GET_CODE (x))
	{
	case IOR: fputs ("or", file); break;
	case AND: fputs ("and", file); break;
	case XOR: fputs ("xor", file); break;
	default: abort ();
	}
      return;

    case 'B':
      switch (GET_CODE (x))
	{
	case IOR: fputs ("orn", file); break;
	case AND: fputs ("andn", file); break;
	case XOR: fputs ("xnor", file); break;
	default: abort ();
	}
      return;

    case 'b':
      {
	/* Print a sign-extended character.  */
	int i = INTVAL (x) & 0xff;
	if (i & 0x80)
	  i |= 0xffffff00;
	fprintf (file, "%d", i);
	return;
      }

    case 0:
      /* Do nothing special.  */
      break;

    default:
      /* Undocumented flag.  */
      abort ();
    }

  if (GET_CODE (x) == REG)
    fputs (reg_names[REGNO (x)], file);
  else if (GET_CODE (x) == MEM)
    {
      fputc ('[', file);
      if (CONSTANT_P (XEXP (x, 0)))
	/* Poor Sun assembler doesn't understand absolute addressing.  */
	fputs ("%g0+", file);
      output_address (XEXP (x, 0));
      fputc (']', file);
    }
  else if (GET_CODE (x) == HIGH)
    {
      fputs ("%hi(", file);
      output_addr_const (file, XEXP (x, 0));
      fputc (')', file);
    }
  else if (GET_CODE (x) == LO_SUM)
    {
      print_operand (file, XEXP (x, 0), 0);
      fputs ("+%lo(", file);
      output_addr_const (file, XEXP (x, 1));
      fputc (')', file);
    }
  else if (GET_CODE (x) == CONST_DOUBLE)
    {
      if (CONST_DOUBLE_HIGH (x) == 0)
	fprintf (file, "%u", CONST_DOUBLE_LOW (x));
      else if (CONST_DOUBLE_HIGH (x) == -1
	       && CONST_DOUBLE_LOW (x) < 0)
	fprintf (file, "%d", CONST_DOUBLE_LOW (x));
      else
	abort ();
    }
  else { output_addr_const (file, x); }
}

/* This function outputs assembler code for VALUE to FILE, where VALUE is
   a 64 bit (DImode) value.  */

/* ??? If there is a 64 bit counterpart to .word that the assembler
   understands, then using that would simply this code greatly.  */

void
output_double_int (file, value)
     FILE *file;
     rtx value;
{
  if (GET_CODE (value) == CONST_INT)
    {
      if (INTVAL (value) < 0)
	ASM_OUTPUT_INT (file, constm1_rtx);
      else
	ASM_OUTPUT_INT (file, const0_rtx);
      ASM_OUTPUT_INT (file, value);
    }
  else if (GET_CODE (value) == CONST_DOUBLE)
    {
      ASM_OUTPUT_INT (file, gen_rtx (CONST_INT, VOIDmode,
				     CONST_DOUBLE_HIGH (value)));
      ASM_OUTPUT_INT (file, gen_rtx (CONST_INT, VOIDmode,
				     CONST_DOUBLE_LOW (value)));
    }
  else if (GET_CODE (value) == SYMBOL_REF
	   || GET_CODE (value) == CONST
	   || GET_CODE (value) == PLUS)
    {
      /* Addresses are only 32 bits.  */
      ASM_OUTPUT_INT (file, const0_rtx);
      ASM_OUTPUT_INT (file, value);
    }
  else
    abort ();
}

