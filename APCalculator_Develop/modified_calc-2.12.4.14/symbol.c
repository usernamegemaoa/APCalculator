/*
 * symbol - global and local symbol routines
 *
 * Copyright (C) 1999-2007  David I. Bell and Ernest Bowen
 *
 * Primary author:  David I. Bell
 *
 * Calc is open software; you can redistribute it and/or modify it under
 * the terms of the version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * Calc is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU Lesser General
 * Public License for more details.
 *
 * A copy of version 2.1 of the GNU Lesser General Public License is
 * distributed with calc under the filename COPYING-LGPL.  You should have
 * received a copy with calc; if not, write to Free Software Foundation, Inc.
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * @(#) $Revision: 30.2 $
 * @(#) $Id: symbol.c,v 30.2 2013/08/11 08:41:38 chongo Exp $
 * @(#) $Source: /usr/local/src/bin/calc/RCS/symbol.c,v $
 *
 * Under source code control:	1990/02/15 01:48:23
 * File existed as early as:	before 1990
 *
 * Share and enjoy!  :-)	http://www.isthe.com/chongo/tech/comp/calc/
 */


#include <stdio.h>
#include "calc.h"
#include "token.h"
#include "symbol.h"
#include "str.h"
#include "opcodes.h"
#include "func.h"

#include "user.h"

#define HASHSIZE	37	/* size of hash table */

STATIC int filescope;		/* file scope level for static variables */
STATIC int funcscope;		/* function scope level for static variables */
STATIC STRINGHEAD localnames;	/* list of local variable names */
STATIC STRINGHEAD globalnames;	/* list of global variable names */
STATIC STRINGHEAD paramnames;	/* list of parameter variable names */
STATIC GLOBAL *globalhash[HASHSIZE];	/* hash table for globals */

S_FUNC void printtype(VALUE *);
S_FUNC void unscope(void);
S_FUNC void addstatic(GLOBAL *);
STATIC long staticcount = 0;
STATIC long staticavail = 0;
STATIC GLOBAL **statictable;


/*
 * Hash a symbol name so we can find it in the hash table.
 * Args are the symbol name and the symbol name size.
 */
#define HASHSYM(n, s) ((unsigned)((n)[0]*123 + (n)[s-1]*135 + (s)*157) % \
		       HASHSIZE)


/*
 * Initialize the global symbol table.
 */
void
initglobals(void)
{
	int i;		/* index counter */

	for (i = 0; i < HASHSIZE; i++)
		globalhash[i] = NULL;
	initstr(&globalnames);
	filescope = SCOPE_STATIC;
	funcscope = 0;
}

/*
 * Define a possibly new global variable which may or may not be static.
 * If it did not already exist, it is created with a value of zero.
 * The address of the global symbol structure is returned.
 *
 * given:
 *	name		name of global variable
 *	isstatic	TRUE if symbol is static
 */
GLOBAL *
addglobal(char *name, BOOL isstatic)
{
	GLOBAL *sp;		/* current symbol pointer */
	GLOBAL **hp;		/* hash table head address */
	size_t len;		/* length of string */
	int newfilescope;	/* file scope being looked for */
	int newfuncscope;	/* function scope being looked for */

	newfilescope = SCOPE_GLOBAL;
	newfuncscope = 0;
	if (isstatic) {
		newfilescope = filescope;
		newfuncscope = funcscope;
	}
	len = strlen(name);
	if (len <= 0)
		return NULL;
	hp = &globalhash[HASHSYM(name, len)];
	for (sp = *hp; sp; sp = sp->g_next) {
		if ((sp->g_len == len) && (strcmp(sp->g_name, name) == 0)
			&& (sp->g_filescope == newfilescope)
			&& (sp->g_funcscope == newfuncscope))
				return sp;
	}
	sp = (GLOBAL *) malloc(sizeof(GLOBAL));
	if (sp == NULL)
		return sp;
	sp->g_name = addstr(&globalnames, name);
	sp->g_len = len;
	sp->g_filescope = newfilescope;
	sp->g_funcscope = newfuncscope;
	sp->g_value.v_num = qlink(&_qzero_);
	sp->g_value.v_type = V_NUM;
	sp->g_value.v_subtype = V_NOSUBTYPE;
	sp->g_next = *hp;
	*hp = sp;
	return sp;
}


/*
 * Look for the highest-scope global variable with a specified name.
 * Returns the address of the variable or NULL according as the search
 * succeeds or fails.
 */
GLOBAL *
findglobal(char *name)
{
	GLOBAL *sp;		/* current symbol pointer */
	GLOBAL *bestsp;		/* found symbol with highest scope */
	size_t len;		/* length of string */

	bestsp = NULL;
	len = strlen(name);
	for (sp = globalhash[HASHSYM(name, len)]; sp; sp = sp->g_next) {
		if ((sp->g_len == len) && !strcmp(sp->g_name, name)) {
			if ((bestsp == NULL) ||
				(sp->g_filescope > bestsp->g_filescope) ||
				 (sp->g_funcscope > bestsp->g_funcscope))
				bestsp = sp;
		}
	}
	return bestsp;
}


/*
 * Return the name of a global variable given its address.
 *
 * given:
 *	sp		address of global pointer
 */
char *
globalname(GLOBAL *sp)
{
	if (sp)
		return sp->g_name;
	return "";
}


/*
 * Show the value of all real-number valued global variables, displaying
 * only the head and tail of very large numerators and denominators.
 * Static variables are not included.
 */
void
showglobals(void)
{
	GLOBAL **hp;			/* hash table head address */
	register GLOBAL *sp;		/* current global symbol pointer */
	long count;			/* number of global variables shown */

	count = 0;
	for (hp = &globalhash[HASHSIZE-1]; hp >= globalhash; hp--) {
		for (sp = *hp; sp; sp = sp->g_next) {
			if (sp->g_value.v_type != V_NUM)
				continue;
			if (count++ == 0) {
				out_str("\nName	  Digits	   Value\n");
				out_str(	 "----	  ------	   -----\n");
			}
			out_fmt("%-8s", sp->g_name);
			if (sp->g_filescope != SCOPE_GLOBAL)
				out_str(" (s)");
			fitprint(sp->g_value.v_num, 50);
			out_str("\n");
		}
	}
	if (count == 0) {
		out_str("No real-valued global variables\n");
	}
	putchar('\n');
}


void
showallglobals(void)
{
	GLOBAL **hp;			/* hash table head address */
	register GLOBAL *sp;		/* current global symbol pointer */
	long count;			/* number of global variables shown */

	count = 0;
	for (hp = &globalhash[HASHSIZE-1]; hp >= globalhash; hp--) {
		for (sp = *hp; sp; sp = sp->g_next) {
			if (count++ == 0) {
				out_str("\nName	  Level	   Type\n");
				out_str(	 "----	  -----	   -----\n");
			}
			out_fmt("%-8s%4d	    ", sp->g_name, sp->g_filescope);
			printtype(&sp->g_value);
			out_str("\n");
		}
	}
	if (count > 0)
		out_fmt("\nNumber: %ld\n", count);
	else
		out_str("No global variables\n");
}

S_FUNC void
printtype(VALUE *vp)
{
	int	type;
	char	*s;

	type = vp->v_type;
	if (type < 0) {
		out_fmt("Error %d", -type);
		return;
	}
	switch (type) {
	case V_NUM:
		out_str("real = ");
		fitprint(vp->v_num, 32);
		return;
	case V_COM:
		out_str("complex = ");
		fitprint(vp->v_com->real, 8);
		if (!vp->v_com->imag->num.sign)
			out_str("+");
		fitprint(vp->v_com->imag, 8);
		out_str("i");
		return;
	case V_STR:
		out_str("string = \"");
		fitstring(vp->v_str->s_str, vp->v_str->s_len, 50);
		out_str("\"");
		return;
	case V_NULL:
		s = "null";
		break;
	case V_MAT:
		s = "matrix";
		break;
	case V_LIST:
		s = "list";
		break;
	case V_ASSOC:
		s = "association";
		break;
	case V_OBJ:
		out_fmt("%s ", objtypename(
			vp->v_obj->o_actions->oa_index));
		s = "object";
		break;
	case V_FILE:
		s = "file id";
		break;
	case V_RAND:
		s = "additive 55 random state";
		break;
	case V_RANDOM:
		s = "Blum random state";
		break;
	case V_CONFIG:
		s = "config state";
		break;
	case V_HASH:
		s = "hash state";
		break;
	case V_BLOCK:
		s = "unnamed block";
		break;
	case V_NBLOCK:
		s = "named block";
		break;
	case V_VPTR:
		s = "value pointer";
		break;
	case V_OPTR:
		s = "octet pointer";
		break;
	case V_SPTR:
		s = "string pointer";
		break;
	case V_NPTR:
		s = "number pointer";
		break;
	default:
		s = "???";
		break;
	}
	out_fmt("%s", s);
}

/*
 * Free all non-null global and visible static variables
 */
void
freeglobals(void)
{
	GLOBAL **hp;		/* hash table head address */
	GLOBAL *sp;		/* current global symbol pointer */
	long count;		/* number of global variables freed */

	/*
	 * We prevent the hp pointer from walking behind globalhash
	 * by stopping one short of the end and running the loop one
	 * more time.
	 *
	 * We could stop the loop with just hp >= globalhash, but stopping
	 * short and running the loop one last time manually helps make
	 * code checkers such as insure happy.
	 */
	count = 0;
	for (hp = &globalhash[HASHSIZE-1]; hp > globalhash; hp--) {
		for (sp = *hp; sp; sp = sp->g_next) {
			if (sp->g_value.v_type != V_NULL) {
				freevalue(&sp->g_value);
				count++;
			}
		}
	}
	/* run the loop manually one last time */
	for (sp = *hp; sp; sp = sp->g_next) {
		if (sp->g_value.v_type != V_NULL) {
			freevalue(&sp->g_value);
			count++;
		}
	}
}

/*
 * Free all invisible static variables
 */
void
freestatics(void)
{
	GLOBAL **stp;
	GLOBAL *sp;
	long count;

	stp = statictable;
	count = staticcount;
	while (count-- > 0) {
		sp = *stp++;
		freevalue(&sp->g_value);
	}
}


/*
 * Reset the file and function scope levels back to the original values.
 * This is called on errors to forget any static variables which were being
 * defined.
 */
void
resetscopes(void)
{
	filescope = SCOPE_STATIC;
	funcscope = 0;
	unscope();
}


/*
 * Enter a new file scope level so that newly defined static variables
 * will have the appropriate scope, and so that previously defined static
 * variables will temporarily be unaccessible.	This should only be called
 * when the function scope level is zero.
 */
void
enterfilescope(void)
{
	filescope++;
	funcscope = 0;
}


/*
 * Exit from a file scope level.  This deletes from the global symbol table
 * all of the static variables that were defined within this file scope level.
 * The function scope level is also reset to zero.
 */
void
exitfilescope(void)
{
	if (filescope > SCOPE_STATIC)
		filescope--;
	funcscope = 0;
	unscope();
}


/*
 * Enter a new function scope level within the current file scope level.
 * This allows newly defined static variables to override previously defined
 * static variables in the same file scope level.
 */
void
enterfuncscope(void)
{
	funcscope++;
}


/*
 * Exit from a function scope level.  This deletes static symbols which were
 * defined within the current function scope level, and makes previously
 * defined symbols with the same name within the same file scope level
 * accessible again.
 */
void
exitfuncscope(void)
{
	if (funcscope > 0)
		funcscope--;
	unscope();
}


/*
 * To end the scope of any static variable with identifier id when
 * id is being declared as global, or when id is declared as static and the
 * variable is at the same file and function level.
 */
void
endscope(char *name, BOOL isglobal)
{
	GLOBAL *sp;
	GLOBAL *prevsp;
	GLOBAL **hp;
	size_t len;

	len = strlen(name);
	prevsp = NULL;
	hp = &globalhash[HASHSYM(name, len)];
	for (sp = *hp; sp; sp = sp->g_next) {
		if (sp->g_len == len && !strcmp(sp->g_name, name) &&
				sp->g_filescope > SCOPE_GLOBAL) {
			if (isglobal || (sp->g_filescope == filescope &&
					sp->g_funcscope == funcscope)) {
				addstatic(sp);
				if (prevsp)
					prevsp->g_next = sp->g_next;
				else
					*hp = sp->g_next;
				continue;
			}
		}
		prevsp = sp;
	}
}

/*
 * To store in a table a static variable whose scope is being ended
 */
void
addstatic(GLOBAL *sp)
{
	GLOBAL **stp;

	if (staticavail <= 0) {
		if (staticcount <= 0)
			stp = (GLOBAL **) malloc(20 * sizeof(GLOBAL *));
		else
			stp = (GLOBAL **) realloc(statictable,
				 (20 + staticcount) * sizeof(GLOBAL *));
		if (stp == NULL) {
			math_error("Cannot allocate static-variable table");
			/*NOTREACHED*/
		}
		statictable = stp;
		staticavail = 20;
	}
	statictable[staticcount++] = sp;
	staticavail--;
}

/*
 * To display all static variables whose scope has been ended
 */
void
showstatics(void)
{
	long count;
	GLOBAL **stp;
	GLOBAL *sp;

	for (count = 0, stp = statictable; count < staticcount; count++) {
		sp = *stp++;
		if (count == 0) {
			out_str("\nName	  Scopes    Type\n");
			out_str(	 "----	  ------    -----\n");
		}
		out_fmt("%-8s", sp->g_name);
		out_fmt("%3d", sp->g_filescope);
		out_fmt("%3d    ", sp->g_funcscope);
		printtype(&sp->g_value);
		out_str("\n");
	}
	if (count > 0)
		out_fmt("\nNumber: %ld\n", count);
	else
		out_str("No unscoped static variables\n");
}

/*
 * Remove all the symbols from the global symbol table which have file or
 * function scopes larger than the current scope levels.  Their memory
 * remains allocated since their values still actually exist.
 */
S_FUNC void
unscope(void)
{
	GLOBAL **hp;			/* hash table head address */
	register GLOBAL *sp;		/* current global symbol pointer */
	GLOBAL *prevsp;			/* previous kept symbol pointer */

	/*
	 * We prevent the hp pointer from walking behind globalhash
	 * by stopping one short of the end and running the loop one
	 * more time.
	 *
	 * We could stop the loop with just hp >= globalhash, but stopping
	 * short and running the loop one last time manually helps make
	 * code checkers such as insure happy.
	 */
	for (hp = &globalhash[HASHSIZE-1]; hp > globalhash; hp--) {
		prevsp = NULL;
		for (sp = *hp; sp; sp = sp->g_next) {
			if ((sp->g_filescope == SCOPE_GLOBAL) ||
				(sp->g_filescope < filescope) ||
				((sp->g_filescope == filescope) &&
					(sp->g_funcscope <= funcscope))) {
				prevsp = sp;
				continue;
			}

			/*
			 * This symbol needs removing.
			 */
			addstatic(sp);
			if (prevsp)
				prevsp->g_next = sp->g_next;
			else
				*hp = sp->g_next;
		}
	}
	/* run the loop manually one last time */
	prevsp = NULL;
	for (sp = *hp; sp; sp = sp->g_next) {
		if ((sp->g_filescope == SCOPE_GLOBAL) ||
			(sp->g_filescope < filescope) ||
			((sp->g_filescope == filescope) &&
				(sp->g_funcscope <= funcscope))) {
			prevsp = sp;
			continue;
		}

		/*
		 * This symbol needs removing.
		 */
		addstatic(sp);
		if (prevsp)
			prevsp->g_next = sp->g_next;
		else
			*hp = sp->g_next;
	}
}


/*
 * Initialize the local and parameter symbol table information.
 */
void
initlocals(void)
{
	initstr(&localnames);
	initstr(&paramnames);
	curfunc->f_localcount = 0;
	curfunc->f_paramcount = 0;
}


/*
 * Add a possibly new local variable definition.
 * Returns the index of the variable into the local symbol table.
 * Minus one indicates the symbol could not be added.
 *
 * given:
 *	name		name of local variable
 */
long
addlocal(char *name)
{
	long index;		/* current symbol index */

	index = findstr(&localnames, name);
	if (index >= 0)
		return index;
	index = localnames.h_count;
	(void) addstr(&localnames, name);
	curfunc->f_localcount++;
	return index;
}


/*
 * Find a local variable name and return its index.
 * Returns minus one if the variable name is not defined.
 *
 * given:
 *	name		name of local variable
 */
long
findlocal(char *name)
{
	return findstr(&localnames, name);
}


/*
 * Return the name of a local variable.
 */
char *
localname(long n)
{
	return namestr(&localnames, n);
}


/*
 * Add a possibly new parameter variable definition.
 * Returns the index of the variable into the parameter symbol table.
 * Minus one indicates the symbol could not be added.
 *
 * given:
 *	name		name of parameter variable
 */
long
addparam(char *name)
{
	long index;		/* current symbol index */

	index = findstr(&paramnames, name);
	if (index >= 0)
		return index;
	index = paramnames.h_count;
	(void) addstr(&paramnames, name);
	curfunc->f_paramcount++;
	return index;
}


/*
 * Find a parameter variable name and return its index.
 * Returns minus one if the variable name is not defined.
 *
 * given:
 *	name		name of parameter variable
 */
long
findparam(char *name)
{
	return findstr(&paramnames, name);
}


/*
 * Return the name of a parameter variable.
 */
char *
paramname(long n)
{
	return namestr(&paramnames, n);
}


/*
 * Return the type of a variable name.
 * This is either local, parameter, global, static, or undefined.
 *
 * given:
 *	name		variable name to find
 */
int
symboltype(char *name)
{
	GLOBAL *sp;

	if (findparam(name) >= 0)
		return SYM_PARAM;
	if (findlocal(name) >= 0)
		return SYM_LOCAL;
	sp = findglobal(name);
	if (sp) {
		if (sp->g_filescope == SCOPE_GLOBAL)
			return SYM_GLOBAL;
		return SYM_STATIC;
	}
	return SYM_UNDEFINED;
}

/* END CODE */
