/* Implements exception handling.
   Copyright (C) 1989, 92-95, 1996 Free Software Foundation, Inc.
   Contributed by Mike Stump <mrs@cygnus.com>.

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


/* An exception is an event that can be signaled from within a
   function. This event can then be "caught" or "trapped" by the
   callers of this function. This potentially allows program flow to
   be transferred to any arbitrary code assocated with a function call
   several levels up the stack.

   The intended use for this mechanism is for signaling "exceptional
   events" in an out-of-band fashion, hence its name. The C++ language
   (and many other OO-styled or functional languages) practically
   requires such a mechanism, as otherwise it becomes very difficult
   or even impossible to signal failure conditions in complex
   situations.  The traditional C++ example is when an error occurs in
   the process of constructing an object; without such a mechanism, it
   is impossible to signal that the error occurs without adding global
   state variables and error checks around every object construction.

   The act of causing this event to occur is referred to as "throwing
   an exception". (Alternate terms include "raising an exception" or
   "signaling an exception".) The term "throw" is used because control
   is returned to the callers of the function that is signaling the
   exception, and thus there is the concept of "throwing" the
   exception up the call stack.

   It is appropriate to speak of the "context of a throw". This
   context refers to the address where the exception is thrown from,
   and is used to determine which exception region will handle the
   exception.

   Regions of code within a function can be marked such that if it
   contains the context of a throw, control will be passed to a
   designated "exception handler". These areas are known as "exception
   regions".  Exception regions cannot overlap, but they can be nested
   to any arbitrary depth. Also, exception regions cannot cross
   function boundaries.

   Each object file that is compiled with exception handling contains a
   static array of exception handlers named __EXCEPTION_TABLE__. Each entry
   contains the starting and ending addresses of the exception region,
   and the address of the handler designated for that region.

   At program startup each object file invokes a function named
   __register_exceptions with the address of its local
   __EXCEPTION_TABLE__. __register_exceptions is defined in libgcc2.c,
   and is responsible for recording all of the exception regions into
   one list (which is kept in a static variable named exception_table_list).

   The function __throw () is actually responsible for doing the
   throw. In the C++ frontend, __throw () is generated on a
   per-object-file basis for each source file compiled with
   -fexceptions. 

   __throw () attempts to find the appropriate exception handler for the 
   PC value stored in __eh_pc by calling __find_first_exception_table_match
   (which is defined in libgcc2.c). If an appropriate handler is
   found, __throw jumps directly to it.

   If a handler for the address being thrown from can't be found,
   __throw is responsible for unwinding the stack, determining the
   address of the caller of the current function (which will be used
   as the new context to throw from), and then searching for a handler
   for the new context. __throw may also call abort () if it is unable
   to unwind the stack, and can also call an external library function
   named __terminate if it reaches the top of the stack without
   finding an appropriate handler.

   Note that some of the regions and handlers are implicitly
   generated. The handlers for these regions perform necessary
   cleanups (in C++ these cleanups are responsible for invoking
   necessary object destructors) before rethrowing the exception to
   the outer exception region.

   Internal implementation details:

   The start of an exception region is indicated by calling
   expand_eh_region_start (). expand_eh_region_end (handler) is
   subsequently invoked to end the region and to associate a handler
   with the region. This is used to create a region that has an
   associated cleanup routine for performing tasks like object
   destruction.

   To associate a user-defined handler with a block of statements, the
   function expand_start_try_stmts () is used to mark the start of the
   block of statements with which the handler is to be associated
   (which is usually known as a "try block"). All statements that
   appear afterwards will be associated with the try block.

   A call to expand_start_all_catch () will mark the end of the try
   block, and also marks the start of the "catch block" associated
   with the try block. This catch block will only be invoked if an
   exception is thrown through the try block. The instructions for the
   catch block are kept as a separate sequence, and will be emitted at
   the end of the function along with the handlers specified via
   expand_eh_region_end (). The end of the catch block is marked with
   expand_end_all_catch ().

   Any data associated with the exception must currently be handled by
   some external mechanism maintained in the frontend.  For example,
   the C++ exception mechanism passes an arbitrary value along with
   the exception, and this is handled in the C++ frontend by using a
   global variable to hold the value.

   Internally-generated exception regions are marked by calling
   expand_eh_region_start () to mark the start of the region, and
   expand_eh_region_end () is used to both designate the end of the
   region and to associate a handler/cleanup with the region. These
   functions generate the appropriate RTL sequences to mark the start
   and end of the exception regions and ensure that an appropriate
   exception region entry will be added to the exception region table.
   expand_eh_region_end () also queues the provided handler to be
   emitted at the end of the current function.

   TARGET_EXPRs can also be used to designate exception regions. A
   TARGET_EXPR gives an unwind-protect style interface commonly used
   in functional languages such as LISP. The associated expression is
   evaluated, and if it (or any of the functions that it calls) throws
   an exception it is caught by the associated cleanup. The backend
   also takes care of the details of associating an exception table
   entry with the expression and generating the necessary code.

   The generated RTL for an exception region includes
   NOTE_INSN_EH_REGION_BEG and NOTE_INSN_EH_REGION_END notes that mark
   the start and end of the exception region. A unique label is also
   generated at the start of the exception region.

   In the current implementation, an exception can only be thrown from
   a function call (since the mechanism used to actually throw an
   exception involves calling __throw).  If an exception region is
   created but no function calls occur within that region, the region
   can be safely optimized away since no exceptions can ever be caught
   in that region.

   Unwinding the stack:

   The details of unwinding the stack to the next frame can be rather
   complex. While in many cases a generic __unwind_function () routine
   can be used by the generated exception handling code to do this, it
   is often necessary to generate inline code to do the unwinding.

   Whether or not these inlined unwinders are necessary is
   target-specific.

   By default, if the target-specific backend doesn't supply a
   definition for __unwind_function (), inlined unwinders will be used
   instead. The main tradeoff here is in text space utilization.
   Obviously, if inline unwinders have to be generated repeatedly,
   this uses more space than if a single routine is used.

   The backend macro DOESNT_NEED_UNWINDER is used to conditionalize
   whether or not per-function unwinders are needed. If DOESNT_NEED_UNWINDER
   is defined and has a non-zero value, a per-function unwinder is
   not emitted for the current function.

   On some platforms it is possible that neither __unwind_function ()
   nor inlined unwinders are available. For these platforms it is not
   possible to throw through a function call, and abort () will be
   invoked instead of performing the throw. */


#include "config.h"
#include <stdio.h>
#include "rtl.h"
#include "tree.h"
#include "flags.h"
#include "except.h"
#include "function.h"
#include "insn-flags.h"
#include "expr.h"
#include "insn-codes.h"
#include "regs.h"
#include "hard-reg-set.h"
#include "insn-config.h"
#include "recog.h"
#include "output.h"
#include "assert.h"

/* A list of labels used for exception handlers.  Created by
   find_exception_handler_labels for the optimization passes.  */

rtx exception_handler_labels;

/* Nonzero means that __throw was invoked. 

   This is used by the C++ frontend to know if code needs to be emitted
   for __throw or not.  */

int throw_used;

/* A stack used for keeping track of the currectly active exception
   handling region.  As each exception region is started, an entry
   describing the region is pushed onto this stack.  The current
   region can be found by looking at the top of the stack, and as we
   exit regions, the corresponding entries are popped. 

   Entries cannot overlap; they must be nested. So there is only one
   entry at most that corresponds to the current instruction, and that
   is the entry on the top of the stack.  */

struct eh_stack ehstack;

/* A queue used for tracking which exception regions have closed but
   whose handlers have not yet been expanded. Regions are emitted in
   groups in an attempt to improve paging performance.

   As we exit a region, we enqueue a new entry. The entries are then
   dequeued during expand_leftover_cleanups () and expand_start_all_catch (),

   We should redo things so that we either take RTL for the handler,
   or we expand the handler expressed as a tree immediately at region
   end time.  */

struct eh_queue ehqueue;

/* Insns for all of the exception handlers for the current function.
   They are currently emitted by the frontend code. */

rtx catch_clauses;

/* A TREE_CHAINed list of handlers for regions that are not yet
   closed. The TREE_VALUE of each entry contains the handler for the
   corresponding entry on the ehstack. */

static tree protect_list;

/* Stacks to keep track of various labels.  */

/* Keeps track of the label to resume to should one want to resume
   normal control flow out of a handler (instead of, say, returning to
   the caller of the current function or exiting the program).  Also
   used as the context of a throw to rethrow an exception to the outer
   exception region. */

struct label_node *caught_return_label_stack = NULL;

/* A random data area for the front end's own use.  */

struct label_node *false_label_stack = NULL;

/* The rtx and the tree for the saved PC value.  */

rtx eh_saved_pc_rtx;
tree eh_saved_pc;

rtx expand_builtin_return_addr	PROTO((enum built_in_function, int, rtx));

/* Various support routines to manipulate the various data structures
   used by the exception handling code.  */

/* Push a label entry onto the given STACK.  */

void
push_label_entry (stack, rlabel, tlabel)
     struct label_node **stack;
     rtx rlabel;
     tree tlabel;
{
  struct label_node *newnode
    = (struct label_node *) xmalloc (sizeof (struct label_node));

  if (rlabel)
    newnode->u.rlabel = rlabel;
  else
    newnode->u.tlabel = tlabel;
  newnode->chain = *stack;
  *stack = newnode;
}

/* Pop a label entry from the given STACK.  */

rtx
pop_label_entry (stack)
     struct label_node **stack;
{
  rtx label;
  struct label_node *tempnode;

  if (! *stack)
    return NULL_RTX;

  tempnode = *stack;
  label = tempnode->u.rlabel;
  *stack = (*stack)->chain;
  free (tempnode);

  return label;
}

/* Return the top element of the given STACK.  */

tree
top_label_entry (stack)
     struct label_node **stack;
{
  if (! *stack)
    return NULL_TREE;

  return (*stack)->u.tlabel;
}

/* Make a copy of ENTRY using xmalloc to allocate the space.  */

static struct eh_entry *
copy_eh_entry (entry)
     struct eh_entry *entry;
{
  struct eh_entry *newentry;

  newentry = (struct eh_entry *) xmalloc (sizeof (struct eh_entry));
  bcopy ((char *) entry, (char *) newentry, sizeof (struct eh_entry));

  return newentry;
}

/* Push a new eh_node entry onto STACK, and return the start label for
   the entry. */

static rtx
push_eh_entry (stack)
     struct eh_stack *stack;
{
  struct eh_node *node = (struct eh_node *) xmalloc (sizeof (struct eh_node));
  struct eh_entry *entry = (struct eh_entry *) xmalloc (sizeof (struct eh_entry));

  entry->start_label = gen_label_rtx ();
  entry->end_label = gen_label_rtx ();
  entry->exception_handler_label = gen_label_rtx ();
  entry->finalization = NULL_TREE;

  node->entry = entry;
  node->chain = stack->top;
  stack->top = node;

  return entry->start_label;
}

/* Pop an entry from the given STACK.  */

static struct eh_entry *
pop_eh_entry (stack)
     struct eh_stack *stack;
{
  struct eh_node *tempnode;
  struct eh_entry *tempentry;
  
  tempnode = stack->top;
  tempentry = tempnode->entry;
  stack->top = stack->top->chain;
  free (tempnode);

  return tempentry;
}

/* Enqueue an ENTRY onto the given QUEUE.  */

static void
enqueue_eh_entry (queue, entry)
     struct eh_queue *queue;
     struct eh_entry *entry;
{
  struct eh_node *node = (struct eh_node *) xmalloc (sizeof (struct eh_node));

  node->entry = entry;
  node->chain = NULL;

  if (queue->head == NULL)
    {
      queue->head = node;
    }
  else
    {
      queue->tail->chain = node;
    }
  queue->tail = node;
}

/* Dequeue an entry from the given QUEUE.  */

static struct eh_entry *
dequeue_eh_entry (queue)
     struct eh_queue *queue;
{
  struct eh_node *tempnode;
  struct eh_entry *tempentry;

  if (queue->head == NULL)
    return NULL;

  tempnode = queue->head;
  queue->head = queue->head->chain;

  tempentry = tempnode->entry;
  free (tempnode);

  return tempentry;
}

/* Routine to see if exception exception handling is turned on.
   DO_WARN is non-zero if we want to inform the user that exception
   handling is turned off. 

   This is used to ensure that -fexceptions has been specified if the
   compiler tries to use any exception-specific functions. */

int
doing_eh (do_warn)
     int do_warn;
{
  if (! flag_exceptions)
    {
      static int warned = 0;
      if (! warned && do_warn)
	{
	  error ("exception handling disabled, use -fexceptions to enable");
	  warned = 1;
	}
      return 0;
    }
  return 1;
}

/* Given a return address in ADDR, determine the address we should use
   to find the corresponding EH region. */

rtx
eh_outer_context (addr)
     rtx addr;
{
  /* First mask out any unwanted bits.  */
#ifdef MASK_RETURN_ADDR
  emit_insn (gen_rtx (SET, Pmode,
		      addr,
		      gen_rtx (AND, Pmode,
			       addr, MASK_RETURN_ADDR)));
#endif

  /* Then subtract out enough to get into the appropriate region.  If
     this is defined, assume we don't need to subtract anything as it
     is already within the correct region.  */
#if ! defined (RETURN_ADDR_OFFSET)
  addr = plus_constant (addr, -1);
#endif

  return addr;
}

/* Start a new exception region and push the HANDLER for the region
   onto protect_list. All of the regions created with add_partial_entry
   will be ended when end_protect_partials () is invoked. */

void
add_partial_entry (handler)
     tree handler;
{
  expand_eh_region_start ();

  /* Make sure the entry is on the correct obstack. */
  push_obstacks_nochange ();
  resume_temporary_allocation ();
  protect_list = tree_cons (NULL_TREE, handler, protect_list);
  pop_obstacks ();
}

/* Output a note marking the start of an exception handling region.
   All instructions emitted after this point are considered to be part
   of the region until expand_eh_region_end () is invoked. */

void
expand_eh_region_start ()
{
  rtx note;

  /* This is the old code.  */
  if (! doing_eh (0))
    return;

#if 0
  /* Maybe do this to prevent jumping in and so on...  */
  pushlevel (0);
#endif

  note = emit_note (NULL_PTR, NOTE_INSN_EH_REGION_BEG);
  emit_label (push_eh_entry (&ehstack));
  NOTE_BLOCK_NUMBER (note)
    = CODE_LABEL_NUMBER (ehstack.top->entry->exception_handler_label);
}

/* Output a note marking the end of the exception handling region on
   the top of ehstack.

   HANDLER is either the cleanup for the exception region, or if we're
   marking the end of a try block, HANDLER is integer_zero_node.

   HANDLER will be transformed to rtl when expand_leftover_cleanups ()
   is invoked. */

void
expand_eh_region_end (handler)
     tree handler;
{
  rtx note;

  struct eh_entry *entry;

  if (! doing_eh (0))
    return;

  entry = pop_eh_entry (&ehstack);

  note = emit_note (NULL_PTR, NOTE_INSN_EH_REGION_END);
  NOTE_BLOCK_NUMBER (note) = CODE_LABEL_NUMBER (entry->exception_handler_label);

  /* Emit a label marking the end of this exception region. */
  emit_label (entry->end_label);

  /* Put in something that takes up space, as otherwise the end
     address for this EH region could have the exact same address as
     its outer region. This would cause us to miss the fact that
     resuming exception handling with this PC value would be inside
     the outer region.  */
  emit_insn (gen_nop ());

  entry->finalization = handler;

  enqueue_eh_entry (&ehqueue, entry);

#if 0
  /* Maybe do this to prevent jumping in and so on...  */
  poplevel (1, 0, 0);
#endif
}

/* Emit a call to __throw and note that we threw something, so we know
   we need to generate the necessary code for __throw.  

   Before invoking throw, the __eh_pc variable must have been set up
   to contain the PC being thrown from. This address is used by
   __throw () to determine which exception region (if any) is
   responsible for handling the exception. */

static void
emit_throw ()
{
#ifdef JUMP_TO_THROW
  emit_indirect_jump (throw_libfunc);
#else
  SYMBOL_REF_USED (throw_libfunc) = 1;
  emit_library_call (throw_libfunc, 0, VOIDmode, 0);
#endif
  throw_used = 1;
  emit_barrier ();
}

/* An internal throw with an indirect CONTEXT we want to throw from.
   CONTEXT evaluates to the context of the throw. */

static void
expand_internal_throw_indirect (context)
     rtx context;
{
  assemble_external (eh_saved_pc);
  emit_move_insn (eh_saved_pc_rtx, context);
  emit_throw ();
}

/* An internal throw with a direct CONTEXT we want to throw from.
   CONTEXT must be a label; its address will be used as the context of
   the throw. */

void
expand_internal_throw (context)
     rtx context;
{
  expand_internal_throw_indirect (gen_rtx (LABEL_REF, Pmode, context));
}

/* Called from expand_exception_blocks and expand_end_catch_block to
   emit any pending handlers/cleanups queued from expand_eh_region_end (). */

void
expand_leftover_cleanups ()
{
  struct eh_entry *entry;

  while ((entry = dequeue_eh_entry (&ehqueue)) != 0)
    {
      rtx prev;

      /* A leftover try block. Shouldn't be one here.  */
      if (entry->finalization == integer_zero_node)
	abort ();

      /* Output the label for the start of the exception handler. */
      emit_label (entry->exception_handler_label);

      /* And now generate the insns for the handler. */
      expand_expr (entry->finalization, const0_rtx, VOIDmode, 0);

      prev = get_last_insn ();
      if (! (prev && GET_CODE (prev) == BARRIER))
	{
	  /* The below can be optimized away, and we could just fall into the
	     next EH handler, if we are certain they are nested.  */
	  /* Emit code to throw to the outer context if we fall off
	     the end of the handler.  */
	  expand_internal_throw (entry->end_label);
	}

      free (entry);
    }
}

/* Called at the start of a block of try statements. */
void
expand_start_try_stmts ()
{
  if (! doing_eh (1))
    return;

  expand_eh_region_start ();
}

/* Generate RTL for the start of a group of catch clauses. 

   It is responsible for starting a new instruction sequence for the
   instructions in the catch block, and expanding the handlers for the
   internally-generated exception regions nested within the try block
   corresponding to this catch block. */

void
expand_start_all_catch ()
{
  struct eh_entry *entry;
  tree label;

  if (! doing_eh (1))
    return;

  /* End the try block. */
  expand_eh_region_end (integer_zero_node);

  emit_line_note (input_filename, lineno);
  label = build_decl (LABEL_DECL, NULL_TREE, NULL_TREE);

  /* The label for the exception handling block that we will save.
     This is Lresume in the documention.  */
  expand_label (label);
  
  /* Put in something that takes up space, as otherwise the end
     address for the EH region could have the exact same address as
     the outer region, causing us to miss the fact that resuming
     exception handling with this PC value would be inside the outer
     region.  */
  emit_insn (gen_nop ());

  /* Push the label that points to where normal flow is resumed onto
     the top of the label stack. */
  push_label_entry (&caught_return_label_stack, NULL_RTX, label);

  /* Start a new sequence for all the catch blocks.  We will add this
     to the global sequence catch_clauses when we have completed all
     the handlers in this handler-seq.  */
  start_sequence ();

  while (1)
    {
      rtx prev;

      entry = dequeue_eh_entry (&ehqueue);
      /* Emit the label for the exception handler for this region, and
	 expand the code for the handler. 

	 Note that a catch region is handled as a side-effect here;
	 for a try block, entry->finalization will contain
	 integer_zero_node, so no code will be generated in the
	 expand_expr call below. But, the label for the handler will
	 still be emitted, so any code emitted after this point will
	 end up being the handler. */
      emit_label (entry->exception_handler_label);
      expand_expr (entry->finalization, const0_rtx, VOIDmode, 0);

      /* When we get down to the matching entry for this try block, stop.  */
      if (entry->finalization == integer_zero_node)
	{
	  /* Don't forget to free this entry. */
	  free (entry);
	  break;
	}

      prev = get_last_insn ();
      if (prev == NULL || GET_CODE (prev) != BARRIER)
	{
	  /* Code to throw out to outer context when we fall off end
	     of the handler. We can't do this here for catch blocks,
	     so it's done in expand_end_all_catch () instead.

	     The below can be optimized away (and we could just fall
	     into the next EH handler) if we are certain they are
	     nested.  */

	  expand_internal_throw (entry->end_label);
	}
      free (entry);
    }
}

/* Finish up the catch block.  At this point all the insns for the
   catch clauses have already been generated, so we only have to add
   them to the catch_clauses list. We also want to make sure that if
   we fall off the end of the catch clauses that we rethrow to the
   outer EH region. */

void
expand_end_all_catch ()
{
  rtx new_catch_clause;

  if (! doing_eh (1))
    return;

  /* Code to throw out to outer context, if we fall off end of catch
     handlers.  This is rethrow (Lresume, same id, same obj) in the
     documentation. We use Lresume because we know that it will throw
     to the correct context.

     In other words, if the catch handler doesn't exit or return, we
     do a "throw" (using the address of Lresume as the point being
     thrown from) so that the outer EH region can then try to process
     the exception. */

  expand_internal_throw (DECL_RTL (top_label_entry (&caught_return_label_stack)));

  /* Now we have the complete catch sequence.  */
  new_catch_clause = get_insns ();
  end_sequence ();
  
  /* This level of catch blocks is done, so set up the successful
     catch jump label for the next layer of catch blocks.  */
  pop_label_entry (&caught_return_label_stack);

  /* Add the new sequence of catches to the main one for this function.  */
  push_to_sequence (catch_clauses);
  emit_insns (new_catch_clause);
  catch_clauses = get_insns ();
  end_sequence ();
  
  /* Here we fall through into the continuation code.  */
}

/* End all the pending exception regions on protect_list. The handlers
   will be emitted when expand_leftover_cleanups () is invoked. */

void
end_protect_partials ()
{
  while (protect_list)
    {
      expand_eh_region_end (TREE_VALUE (protect_list));
      protect_list = TREE_CHAIN (protect_list);
    }
}

/* The exception table that we build that is used for looking up and
   dispatching exceptions, the current number of entries, and its
   maximum size before we have to extend it. 

   The number in eh_table is the code label number of the exception
   handler for the region. This is added by add_eh_table_entry () and
   used by output_exception_table_entry (). */

static int *eh_table;
static int eh_table_size;
static int eh_table_max_size;

/* Note the need for an exception table entry for region N.  If we
   don't need to output an explicit exception table, avoid all of the
   extra work.

   Called from final_scan_insn when a NOTE_INSN_EH_REGION_BEG is seen.
   N is the NOTE_BLOCK_NUMBER of the note, which comes from the code
   label number of the exception handler for the region. */

void
add_eh_table_entry (n)
     int n;
{
#ifndef OMIT_EH_TABLE
  if (eh_table_size >= eh_table_max_size)
    {
      if (eh_table)
	{
	  eh_table_max_size += eh_table_max_size>>1;

	  if (eh_table_max_size < 0)
	    abort ();

	  if ((eh_table = (int *) realloc (eh_table,
					   eh_table_max_size * sizeof (int)))
	      == 0)
	    fatal ("virtual memory exhausted");
	}
      else
	{
	  eh_table_max_size = 252;
	  eh_table = (int *) xmalloc (eh_table_max_size * sizeof (int));
	}
    }
  eh_table[eh_table_size++] = n;
#endif
}

/* Return a non-zero value if we need to output an exception table.

   On some platforms, we don't have to output a table explicitly.
   This routine doesn't mean we don't have one.  */

int
exception_table_p ()
{
  if (eh_table)
    return 1;

  return 0;
}

/* Output the entry of the exception table corresponding to to the
   exception region numbered N to file FILE. 

   N is the code label number corresponding to the handler of the
   region. */

static void
output_exception_table_entry (file, n)
     FILE *file;
     int n;
{
  char buf[256];
  rtx sym;

  ASM_GENERATE_INTERNAL_LABEL (buf, "LEHB", n);
  sym = gen_rtx (SYMBOL_REF, Pmode, buf);
  assemble_integer (sym, POINTER_SIZE / BITS_PER_UNIT, 1);

  ASM_GENERATE_INTERNAL_LABEL (buf, "LEHE", n);
  sym = gen_rtx (SYMBOL_REF, Pmode, buf);
  assemble_integer (sym, POINTER_SIZE / BITS_PER_UNIT, 1);

  ASM_GENERATE_INTERNAL_LABEL (buf, "L", n);
  sym = gen_rtx (SYMBOL_REF, Pmode, buf);
  assemble_integer (sym, POINTER_SIZE / BITS_PER_UNIT, 1);

  putc ('\n', file);		/* blank line */
}

/* Output the exception table if we have and need one. */

void
output_exception_table ()
{
  int i;
  extern FILE *asm_out_file;

  if (! doing_eh (0))
    return;

  exception_section ();

  /* Beginning marker for table.  */
  assemble_align (GET_MODE_ALIGNMENT (ptr_mode));
  assemble_label ("__EXCEPTION_TABLE__");

  assemble_integer (const0_rtx, POINTER_SIZE / BITS_PER_UNIT, 1);
  assemble_integer (const0_rtx, POINTER_SIZE / BITS_PER_UNIT, 1);
  assemble_integer (const0_rtx, POINTER_SIZE / BITS_PER_UNIT, 1);
  putc ('\n', asm_out_file);		/* blank line */

  for (i = 0; i < eh_table_size; ++i)
    output_exception_table_entry (asm_out_file, eh_table[i]);

  free (eh_table);

  /* Ending marker for table.  */
  assemble_label ("__EXCEPTION_END__");
  assemble_integer (constm1_rtx, POINTER_SIZE / BITS_PER_UNIT, 1);
  assemble_integer (constm1_rtx, POINTER_SIZE / BITS_PER_UNIT, 1);
  assemble_integer (constm1_rtx, POINTER_SIZE / BITS_PER_UNIT, 1);
  putc ('\n', asm_out_file);		/* blank line */
}

/* Generate code to initialize the exception table at program startup
   time.  */

void
register_exception_table ()
{
  emit_library_call (gen_rtx (SYMBOL_REF, Pmode, "__register_exceptions"), 0,
		     VOIDmode, 1,
		     gen_rtx (SYMBOL_REF, Pmode, "__EXCEPTION_TABLE__"),
		     Pmode);
}

/* Emit the RTL for the start of the per-function unwinder for the
   current function. See emit_unwinder () for further information.

   DOESNT_NEED_UNWINDER is a target-specific macro that determines if
   the current function actually needs a per-function unwinder or not.
   By default, all functions need one. */

void
start_eh_unwinder ()
{
#ifdef DOESNT_NEED_UNWINDER
  if (DOESNT_NEED_UNWINDER)
    return;
#endif

  expand_eh_region_start ();
}

/* Emit insns for the end of the per-function unwinder for the
   current function.  */

void
end_eh_unwinder ()
{
  tree expr;
  rtx return_val_rtx, ret_val, label, end, insns;

  if (! doing_eh (0))
    return;

#ifdef DOESNT_NEED_UNWINDER
  if (DOESNT_NEED_UNWINDER)
    return;
#endif

  assemble_external (eh_saved_pc);

  expr = make_node (RTL_EXPR);
  TREE_TYPE (expr) = void_type_node;
  RTL_EXPR_RTL (expr) = const0_rtx;
  TREE_SIDE_EFFECTS (expr) = 1;
  start_sequence_for_rtl_expr (expr);

  /* ret_val will contain the address of the code where the call
     to the current function occurred. */
  ret_val = expand_builtin_return_addr (BUILT_IN_RETURN_ADDRESS,
					0, hard_frame_pointer_rtx);
  return_val_rtx = copy_to_reg (ret_val);

  /* Get the address we need to use to determine what exception
     handler should be invoked, and store it in __eh_pc. */
  return_val_rtx = eh_outer_context (return_val_rtx);
  emit_move_insn (eh_saved_pc_rtx, return_val_rtx);
  
  /* Either set things up so we do a return directly to __throw, or
     we return here instead. */
#ifdef JUMP_TO_THROW
  emit_move_insn (ret_val, throw_libfunc);
#else
  label = gen_label_rtx ();
  emit_move_insn (ret_val, gen_rtx (LABEL_REF, Pmode, label));
#endif

#ifdef RETURN_ADDR_OFFSET
  return_val_rtx = plus_constant (ret_val, -RETURN_ADDR_OFFSET);
  if (return_val_rtx != ret_val)
    emit_move_insn (ret_val, return_val_rtx);
#endif
  
  end = gen_label_rtx ();
  emit_jump (end);  

  RTL_EXPR_SEQUENCE (expr) = get_insns ();
  end_sequence ();
  expand_eh_region_end (expr);

  emit_jump (end);

#ifndef JUMP_TO_THROW
  emit_label (label);
  emit_throw ();
#endif
  
  expand_leftover_cleanups ();

  emit_label (end);
}

/* If necessary, emit insns for the per function unwinder for the
   current function.  Called after all the code that needs unwind
   protection is output.  

   The unwinder takes care of catching any exceptions that have not
   been previously caught within the function, unwinding the stack to
   the next frame, and rethrowing using the address of the current
   function's caller as the context of the throw.

   On some platforms __throw can do this by itself (or with the help
   of __unwind_function) so the per-function unwinder is
   unnecessary.
  
   We cannot place the unwinder into the function until after we know
   we are done inlining, as we don't want to have more than one
   unwinder per non-inlined function.  */

void
emit_unwinder ()
{
  rtx insns, insn;

  start_sequence ();
  start_eh_unwinder ();
  insns = get_insns ();
  end_sequence ();

  /* We place the start of the exception region associated with the
     per function unwinder at the top of the function.  */
  if (insns)
    emit_insns_after (insns, get_insns ());

  start_sequence ();
  end_eh_unwinder ();
  insns = get_insns ();
  end_sequence ();

  /* And we place the end of the exception region before the USE and
     CLOBBER insns that may come at the end of the function.  */
  if (insns == 0)
    return;

  insn = get_last_insn ();
  while (GET_CODE (insn) == NOTE
	 || (GET_CODE (insn) == INSN
	     && (GET_CODE (PATTERN (insn)) == USE
		 || GET_CODE (PATTERN (insn)) == CLOBBER)))
    insn = PREV_INSN (insn);

  if (GET_CODE (insn) == CODE_LABEL
      && GET_CODE (PREV_INSN (insn)) == BARRIER)
    {
      insn = PREV_INSN (insn);
    }
  else
    {
      rtx label = gen_label_rtx ();
      emit_label_after (label, insn);
      insn = emit_jump_insn_after (gen_jump (label), insn);
      insn = emit_barrier_after (insn);
    }
    
  emit_insns_after (insns, insn);
}

/* Scan the current insns and build a list of handler labels. The
   resulting list is placed in the global variable exception_handler_labels.

   It is called after the last exception handling region is added to
   the current function (when the rtl is almost all built for the
   current function) and before the jump optimization pass.  */

void
find_exception_handler_labels ()
{
  rtx insn;
  int max_labelno = max_label_num ();
  int min_labelno = get_first_label_num ();
  rtx *labels;

  exception_handler_labels = NULL_RTX;

  /* If we aren't doing exception handling, there isn't much to check.  */
  if (! doing_eh (0))
    return;

  /* Generate a handy reference to each label.  */

  labels = (rtx *) alloca ((max_labelno - min_labelno) * sizeof (rtx));

  /* Eeeeeeew. */
  labels -= min_labelno;

  for (insn = get_insns (); insn; insn = NEXT_INSN (insn))
    {
      if (GET_CODE (insn) == CODE_LABEL)
	if (CODE_LABEL_NUMBER (insn) >= min_labelno
	    && CODE_LABEL_NUMBER (insn) < max_labelno)
	  labels[CODE_LABEL_NUMBER (insn)] = insn;
    }

  /* For each start of a region, add its label to the list.  */

  for (insn = get_insns (); insn; insn = NEXT_INSN (insn))
    {
      if (GET_CODE (insn) == NOTE
	  && NOTE_LINE_NUMBER (insn) == NOTE_INSN_EH_REGION_BEG)
	{
	  rtx label = NULL_RTX;

	  if (NOTE_BLOCK_NUMBER (insn) >= min_labelno
	      && NOTE_BLOCK_NUMBER (insn) < max_labelno)
	    {
	      label = labels[NOTE_BLOCK_NUMBER (insn)];

	      if (label)
		exception_handler_labels
		  = gen_rtx (EXPR_LIST, VOIDmode,
			     label, exception_handler_labels);
	      else
		warning ("didn't find handler for EH region %d",
			 NOTE_BLOCK_NUMBER (insn));
	    }
	  else
	    warning ("mismatched EH region %d", NOTE_BLOCK_NUMBER (insn));
	}
    }
}

/* Perform sanity checking on the exception_handler_labels list.

   Can be called after find_exception_handler_labels is called to
   build the list of exception handlers for the current function and
   before we finish processing the current function.  */

void
check_exception_handler_labels ()
{
  rtx insn, handler;

  /* If we aren't doing exception handling, there isn't much to check.  */
  if (! doing_eh (0))
    return;

  /* Ensure that the CODE_LABEL_NUMBER for the CODE_LABEL entry point
     in each handler corresponds to the CODE_LABEL_NUMBER of the
     handler. */

  for (handler = exception_handler_labels;
       handler;
       handler = XEXP (handler, 1))
    {
      for (insn = get_insns (); insn; insn = NEXT_INSN (insn))
	{
	  if (GET_CODE (insn) == CODE_LABEL)
	    {
	      if (CODE_LABEL_NUMBER (insn)
		  == CODE_LABEL_NUMBER (XEXP (handler, 0)))
		{
		  if (insn != XEXP (handler, 0))
		    warning ("mismatched handler %d",
			     CODE_LABEL_NUMBER (insn));
		  break;
		}
	    }
	}
      if (insn == NULL_RTX)
	warning ("handler not found %d",
		 CODE_LABEL_NUMBER (XEXP (handler, 0)));
    }

  /* Now go through and make sure that for each region there is a
     corresponding label.  */
  for (insn = get_insns (); insn; insn = NEXT_INSN (insn))
    {
      if (GET_CODE (insn) == NOTE
	  && (NOTE_LINE_NUMBER (insn) == NOTE_INSN_EH_REGION_BEG ||
	      NOTE_LINE_NUMBER (insn) == NOTE_INSN_EH_REGION_END))
	{
	  for (handler = exception_handler_labels;
	       handler;
	       handler = XEXP (handler, 1))
	    {
	      if (CODE_LABEL_NUMBER (XEXP (handler, 0))
		  == NOTE_BLOCK_NUMBER (insn))
		break;
	    }
	  if (handler == NULL_RTX)
	    warning ("region exists, no handler %d",
		     NOTE_BLOCK_NUMBER (insn));
	}
    }
}

/* This group of functions initializes the exception handling data
   structures at the start of the compilation, initializes the data
   structures at the start of a function, and saves and restores the
   exception handling data structures for the start/end of a nested
   function.  */

/* Toplevel initialization for EH things.  */ 

void
init_eh ()
{
  /* Generate rtl to reference the variable in which the PC of the
     current context is saved. */
  tree type = build_pointer_type (make_node (VOID_TYPE));

  eh_saved_pc = build_decl (VAR_DECL, get_identifier ("__eh_pc"), type);
  DECL_EXTERNAL (eh_saved_pc) = 1;
  TREE_PUBLIC (eh_saved_pc) = 1;
  make_decl_rtl (eh_saved_pc, NULL_PTR, 1);
  eh_saved_pc_rtx = DECL_RTL (eh_saved_pc);
}

/* Initialize the per-function EH information. */

void
init_eh_for_function ()
{
  ehstack.top = 0;
  ehqueue.head = ehqueue.tail = 0;
  catch_clauses = NULL_RTX;
  false_label_stack = 0;
  caught_return_label_stack = 0;
  protect_list = NULL_TREE;
}

/* Save some of the per-function EH info into the save area denoted by
   P. 

   This is currently called from save_stmt_status (). */

void
save_eh_status (p)
     struct function *p;
{
  assert (p != NULL);

  p->ehstack = ehstack;
  p->ehqueue = ehqueue;
  p->catch_clauses = catch_clauses;
  p->false_label_stack = false_label_stack;
  p->caught_return_label_stack = caught_return_label_stack;
  p->protect_list = protect_list;

  init_eh ();
}

/* Restore the per-function EH info saved into the area denoted by P.  

   This is currently called from restore_stmt_status. */

void
restore_eh_status (p)
     struct function *p;
{
  assert (p != NULL);

  protect_list = p->protect_list;
  caught_return_label_stack = p->caught_return_label_stack;
  false_label_stack = p->false_label_stack;
  catch_clauses	= p->catch_clauses;
  ehqueue = p->ehqueue;
  ehstack = p->ehstack;
}

/* This section is for the exception handling specific optimization
   pass.  First are the internal routines, and then the main
   optimization pass.  */

/* Determine if the given INSN can throw an exception.  */

static int
can_throw (insn)
     rtx insn;
{
  /* Calls can always potentially throw exceptions. */
  if (GET_CODE (insn) == CALL_INSN)
    return 1;

#ifdef ASYNCH_EXCEPTIONS
  /* If we wanted asynchronous exceptions, then everything but NOTEs
     and CODE_LABELs could throw. */
  if (GET_CODE (insn) != NOTE && GET_CODE (insn) != CODE_LABEL)
    return 1;
#endif

  return 0;
}

/* Scan a exception region looking for the matching end and then
   remove it if possible. INSN is the start of the region, N is the
   region number, and DELETE_OUTER is to note if anything in this
   region can throw.

   Regions are removed if they cannot possibly catch an exception.
   This is determined by invoking can_throw () on each insn within the
   region; if can_throw returns true for any of the instructions, the
   region can catch an exception, since there is an insn within the
   region that is capable of throwing an exception.

   Returns the NOTE_INSN_EH_REGION_END corresponding to this region, or
   calls abort () if it can't find one.

   Can abort if INSN is not a NOTE_INSN_EH_REGION_BEGIN, or if N doesn't
   correspond to the region number, or if DELETE_OUTER is NULL. */

static rtx
scan_region (insn, n, delete_outer)
     rtx insn;
     int n;
     int *delete_outer;
{
  rtx start = insn;

  /* Assume we can delete the region.  */
  int delete = 1;

  assert (insn != NULL_RTX
	  && GET_CODE (insn) == NOTE
	  && NOTE_LINE_NUMBER (insn) == NOTE_INSN_EH_REGION_BEG
	  && NOTE_BLOCK_NUMBER (insn) == n
	  && delete_outer != NULL);

  insn = NEXT_INSN (insn);

  /* Look for the matching end.  */
  while (! (GET_CODE (insn) == NOTE
	    && NOTE_LINE_NUMBER (insn) == NOTE_INSN_EH_REGION_END))
    {
      /* If anything can throw, we can't remove the region.  */
      if (delete && can_throw (insn))
	{
	  delete = 0;
	}

      /* Watch out for and handle nested regions.  */
      if (GET_CODE (insn) == NOTE
	  && NOTE_LINE_NUMBER (insn) == NOTE_INSN_EH_REGION_BEG)
	{
	  insn = scan_region (insn, NOTE_BLOCK_NUMBER (insn), &delete);
	}

      insn = NEXT_INSN (insn);
    }

  /* The _BEG/_END NOTEs must match and nest.  */
  if (NOTE_BLOCK_NUMBER (insn) != n)
    abort ();

  /* If anything in this exception region can throw, we can throw.  */
  if (! delete)
    *delete_outer = 0;
  else
    {
      /* Delete the start and end of the region.  */
      delete_insn (start);
      delete_insn (insn);

      /* Only do this part if we have built the exception handler
         labels.  */
      if (exception_handler_labels)
	{
	  rtx x, *prev = &exception_handler_labels;

	  /* Find it in the list of handlers.  */
	  for (x = exception_handler_labels; x; x = XEXP (x, 1))
	    {
	      rtx label = XEXP (x, 0);
	      if (CODE_LABEL_NUMBER (label) == n)
		{
		  /* If we are the last reference to the handler,
                     delete it.  */
		  if (--LABEL_NUSES (label) == 0)
		    delete_insn (label);

		  if (optimize)
		    {
		      /* Remove it from the list of exception handler
			 labels, if we are optimizing.  If we are not, then
			 leave it in the list, as we are not really going to
			 remove the region.  */
		      *prev = XEXP (x, 1);
		      XEXP (x, 1) = 0;
		      XEXP (x, 0) = 0;
		    }

		  break;
		}
	      prev = &XEXP (x, 1);
	    }
	}
    }
  return insn;
}

/* Perform various interesting optimizations for exception handling
   code.

   We look for empty exception regions and make them go (away). The
   jump optimization code will remove the handler if nothing else uses
   it. */

void
exception_optimize ()
{
  rtx insn, regions = NULL_RTX;
  int n;

  /* Remove empty regions.  */
  for (insn = get_insns (); insn; insn = NEXT_INSN (insn))
    {
      if (GET_CODE (insn) == NOTE
	  && NOTE_LINE_NUMBER (insn) == NOTE_INSN_EH_REGION_BEG)
	{
	  /* Since scan_region () will return the NOTE_INSN_EH_REGION_END
	     insn, we will indirectly skip through all the insns
	     inbetween. We are also guaranteed that the value of insn
	     returned will be valid, as otherwise scan_region () won't
	     return. */
	  insn = scan_region (insn, NOTE_BLOCK_NUMBER (insn), &n);
	}
    }
}
