/* Collected kpathsea files in the tidied workalike version.

   Copyright 1993, 1994, 1995, 2008, 2009, 2010, 2011 Karl Berry.
   Copyright 1997, 2002, 2005 Olaf Weber.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this library; if not, see <http://www.gnu.org/licenses/>.  */

#include <tectonic/tectonic.h>
#include <tectonic/internals.h>

/* c-pathch.h */

#ifndef IS_DIR_SEP_CH
#define IS_DIR_SEP_CH(ch) IS_DIR_SEP(ch)
#endif
#ifndef IS_DEVICE_SEP /* No `devices' on, e.g., Unix.  */
#define IS_DEVICE_SEP(ch) 0
#endif
#ifndef NAME_BEGINS_WITH_DEVICE
#define NAME_BEGINS_WITH_DEVICE(name) 0
#endif
#ifndef IS_UNC_NAME /* Unc names are in practice found on Win32 only. */
#define IS_UNC_NAME(name) 0
#endif

#define ISLOWER(c) (isascii (c) && islower((unsigned char)c))
#define TOUPPER(c) (ISLOWER (c) ? toupper ((unsigned char)c) : (c))


/* concat3.c */

string
concat3 (const_string s1,  const_string s2,  const_string s3)
{
  int s2l = s2 ? strlen (s2) : 0;
  int s3l = s3 ? strlen (s3) : 0;
  string answer
      = (string) xmalloc (strlen(s1) + s2l + s3l + 1);
  strcpy (answer, s1);
  if (s2) strcat (answer, s2);
  if (s3) strcat (answer, s3);

  return answer;
}


/* line.c */

/* Allocate in increments of this size.  */
#define LINE_C_BLOCK_SIZE 75

char *
read_line (FILE *f)
{
  int c;
  unsigned limit = LINE_C_BLOCK_SIZE;
  unsigned loc = 0;
  char *line = xmalloc (limit);

  flockfile (f);

  while ((c = getc_unlocked (f)) != EOF && c != '\n' && c != '\r') {
    line[loc] = c;
    loc++;

    /* By testing after the assignment, we guarantee that we'll always
       have space for the null we append below.  We know we always
       have room for the first char, since we start with LINE_C_BLOCK_SIZE.  */
    if (loc == limit) {
      limit += LINE_C_BLOCK_SIZE;
      line = xrealloc (line, limit);
    }
  }

  /* If we read anything, return it, even a partial last-line-if-file
     which is not properly terminated.  */
  if (loc == 0 && c == EOF) {
    /* At end of file.  */
    free (line);
    line = NULL;
  } else {
    /* Terminate the string.  We can't represent nulls in the file,
       but this doesn't matter.  */
    line[loc] = 0;
    /* Absorb LF of a CRLF pair. */
    if (c == '\r') {
      c = getc_unlocked (f);
      if (c != '\n') {
        ungetc (c, f);
      }
    }
  }

  funlockfile (f);

  return line;
}

/* x*.c */

/* Return NAME with any leading path stripped off.  This returns a
   pointer into NAME.  For example, `basename ("/foo/bar.baz")'
   returns "bar.baz".  */

const_string
xbasename (const_string name)
{
    const_string base = name;
    const_string p;

    if (NAME_BEGINS_WITH_DEVICE(name))
        base += 2;

    else if (IS_UNC_NAME(name)) {
        unsigned limit;

        for (limit = 2; name[limit] && !IS_DIR_SEP (name[limit]); limit++)
            ;
        if (name[limit++] && name[limit] && !IS_DIR_SEP (name[limit])) {
            for (; name[limit] && !IS_DIR_SEP (name[limit]); limit++)
                ;
        } else
            /* malformed UNC name, backup */
            limit = 0;
        base += limit;
    }

    for (p = base; *p; p++) {
        if (IS_DIR_SEP(*p))
            base = p + 1;
    }

    return base;
}

void *
xcalloc (size_t nelem,  size_t elsize)
{
    void *new_mem = (void*)calloc(nelem ? nelem : 1, elsize ? elsize : 1);

    if (new_mem == NULL)
	_tt_abort ("xcalloc request for %lu elements of size %lu failed",
		   (unsigned long) nelem, (unsigned long) elsize);

    return new_mem;
}

FILE *
xfopen (const_string filename, const_string mode)
{
    FILE *f;

    assert(filename && mode);

    f = fopen(filename, mode);
    if (f == NULL)
        _tt_abort("fopen(%s) failed: %s", filename, strerror(errno));

    return f;
}


void
xfclose (FILE *f, const_string filename)
{
    assert(f);

    if (fclose(f) == EOF)
        _tt_abort("fclose(%s) failed: %s", filename, strerror(errno));
}


void
xfseek (FILE *f, long offset, int wherefrom, const_string filename)
{
    if (fseek (f, offset, wherefrom) < 0) {
	if (filename == NULL)
	    filename = "(unknown file)";
        _tt_abort("fseek(%s, %ld, %d) failed: %s", filename, offset, wherefrom, strerror(errno));
    }
}


void *
xmalloc (size_t size)
{
    void *new_mem = (void *)malloc(size ? size : 1);

    if (new_mem == NULL)
	_tt_abort ("xmalloc request for %lu bytes failed", (unsigned long) size);

    return new_mem;
}


void *
xrealloc (void *old_ptr, size_t size)
{
    void *new_mem;

    if (old_ptr == NULL) {
        new_mem = xmalloc(size);
    } else {
        new_mem = realloc(old_ptr, size ? size : 1);
        if (new_mem == NULL)
	    _tt_abort("xrealloc() to %lu bytes failed", (unsigned long) size);
    }

    return new_mem;
}


string
xstrdup (const_string s)
{
  string new_string = (string)xmalloc(strlen (s) + 1);
  return strcpy(new_string, s);
}


/* zround.c */

integer
zround (double r)
{
  integer i;

  /* R can be outside the range of an integer if glue is stretching or
     shrinking a lot.  We can't do any better than returning the largest
     or smallest integer possible in that case.  It doesn't seem to make
     any practical difference.  Here is a sample input file which
     demonstrates the problem, from phil@cs.arizona.edu:
     	\documentstyle{article}
	\begin{document}
	\begin{flushleft}
	$\hbox{} $\hfill
	\filbreak
	\eject

     djb@silverton.berkeley.edu points out we should testing against
     TeX's largest or smallest integer (32 bits), not the machine's.  So
     we might as well use a floating-point constant, and avoid potential
     compiler bugs (also noted by djb, on BSDI).  */
  if (r > 2147483647.0)
    i = 2147483647;
  /* should be ...8, but atof bugs are too common */
  else if (r < -2147483647.0)
    i = -2147483647;
  /* Admittedly some compilers don't follow the ANSI rules of casting
     meaning truncating toward zero; but it doesn't matter enough to do
     anything more complicated here.  */
  else if (r >= 0.0)
    i = (integer)(r + 0.5);
  else
    i = (integer)(r - 0.5);

  return i;
}


/* trans.c */

void
make_identity(transform* t)
{
    t->a = 1.0;
    t->b = 0.0;
    t->c = 0.0;
    t->d = 1.0;
    t->x = 0.0;
    t->y = 0.0;
}

void
make_scale(transform* t, double xscale, double yscale)
{
    t->a = xscale;
    t->b = 0.0;
    t->c = 0.0;
    t->d = yscale;
    t->x = 0.0;
    t->y = 0.0;
}

void
make_translation(transform* t, double dx, double dy)
{
    t->a = 1.0;
    t->b = 0.0;
    t->c = 0.0;
    t->d = 1.0;
    t->x = dx;
    t->y = dy;
}

void
make_rotation(transform* t, double a)
{
    t->a = cos(a);
    t->b = sin(a);
    t->c = -sin(a);
    t->d = cos(a);
    t->x = 0.0;
    t->y = 0.0;
}

void
transform_point(real_point* p, const transform* t)
{
    real_point r;

    r.x = t->a * p->x + t->c * p->y + t->x;
    r.y = t->b * p->x + t->d * p->y + t->y;

    *p = r;
}

void
transform_concat(transform* t1, const transform* t2)
{
    transform r;

    r.a = t1->a * t2->a + t1->b * t2->c + 0.0 * t2->x;
    r.b = t1->a * t2->b + t1->b * t2->d + 0.0 * t2->y;
    r.c = t1->c * t2->a + t1->d * t2->c + 0.0 * t2->x;
    r.d = t1->c * t2->b + t1->d * t2->d + 0.0 * t2->y;
    r.x = t1->x * t2->a + t1->y * t2->c + 1.0 * t2->x;
    r.y = t1->x * t2->b + t1->y * t2->d + 1.0 * t2->y;

    *t1 = r;
}
