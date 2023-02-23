/* Output assembler code conversion from UNIX line endings to DOS line endings.
   Copyright (C) 1993, 1994, 1996, 1997, 2023 Free Software Foundation, Inc.

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

#include "flags.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* The following file allows for converting the output
   assembler code from UNIX line endings to DOS line endings.

   This is required for compatiblity with original PsyQ utilites
   (such as ASPSX.EXE).

   The entire GCC code relies on a global asm_out_file
   to emit the assembly. It was not practical to modify each
   fwrite to it in the codebase to emit the correct line ending.

   Therefore, this code switches asm_out_file to a temporary file
   and at the end of compilation it performs the UNIX line ending
   to DOS line ending conversion. The output is written to the original
   asm_out_file.  */

extern void fatal_io_error (char *);

/* File used for outputting assembler code.  */
extern FILE *asm_out_file;

/* Name for output file of assembly code, specified with -o.  */
extern char *asm_file_name;

/* Copy of the original asm_out_file before switching asm_out_file
   to a temporary file. This file will have DOS line endings
   while the switched asm_out_file will have UNIX line endings.
   After the conversion is finished, asm_out_file is restored
   to this file.  */
FILE *original_asm_out_file;

/* Temporary file creation code copied from mips.c.  */

/* Name for a temporary file storing assembler code with UNIX
   line endings (before the conversion).  */
char *temp_filename;

/* On MSDOS, write temp files in current dir
   because there's no place else we can expect to use.  */
#if __MSDOS__
#ifndef P_tmpdir
#define P_tmpdir "./"
#endif
#endif

FILE *
make_temp_file ()
{
  FILE *stream;
  char *base = getenv ("TMPDIR");
  int len;

  if (base == (char *)0)
    {
#ifdef P_tmpdir
      if (access (P_tmpdir, R_OK | W_OK) == 0)
   base = P_tmpdir;
      else
#endif
   if (access ("/usr/tmp", R_OK | W_OK) == 0)
     base = "/usr/tmp/";
   else
     base = "/tmp/";
    }

  len = strlen (base);
  /* temp_filename is global, so we must use malloc, not alloca.  */
  temp_filename = (char *) xmalloc (len + sizeof("/ctXXXXXX"));
  strcpy (temp_filename, base);
  if (len > 0 && temp_filename[len-1] != '/')
    temp_filename[len++] = '/';

  strcpy (temp_filename + len, "ctXXXXXX");
  mktemp (temp_filename);

  stream = fopen (temp_filename, "w+");
  if (!stream)
    pfatal_with_name (temp_filename);

#ifndef __MSDOS__
  /* In MSDOS, we cannot unlink the temporary file until we are finished using
     it.  Otherwise, we delete it now, so that it will be gone even if the
     compiler happens to crash.  */
  unlink (temp_filename);
#endif
  return stream;
}

void
init_unix2dos (void)
{
  if (!flag_dos_line_endings)
    return;

  original_asm_out_file = asm_out_file;
  asm_out_file = make_temp_file ();
}

/* Temporary buffer storing the current chunk of output assembler code
   before the conversion - with UNIX line endings.  */
char unix_line_endings_buffer[32768];

/* Temporary buffer storing the current chunk of output assembler code
   after the conversion - with DOS line endings. Each line ending
   conversion from '\n' to '\r\n' doubles the output size, so this buffer
   has to be twice as large (for the worst case).  */
char dos_line_endings_buffer[2 * sizeof (unix_line_endings_buffer)];

void
finish_unix2dos (void)
{
  long len;
  long i, j;

  if (!flag_dos_line_endings)
    return;

  rewind (asm_out_file);
  while ((len = fread (unix_line_endings_buffer, 1, sizeof (unix_line_endings_buffer), asm_out_file)) > 0)
    {
      for (i = 0, j = 0; i < len; i++)
        {
          if (unix_line_endings_buffer[i] == '\n')
            {
              dos_line_endings_buffer[j++] = '\r';
              dos_line_endings_buffer[j++] = '\n';
            }
          else
            dos_line_endings_buffer[j++] = unix_line_endings_buffer[i];
        }
        if (fwrite (dos_line_endings_buffer, 1, j, original_asm_out_file) != j)
          fatal_io_error (asm_file_name);
     }

  if (ferror (asm_out_file) != 0 || fclose (asm_out_file) != 0)
    fatal_io_error (asm_file_name);

  asm_out_file = original_asm_out_file;
}
