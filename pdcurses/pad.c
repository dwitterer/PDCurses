/************************************************************************ 
 * This file is part of PDCurses. PDCurses is public domain software;	*
 * you may use it for any purpose. This software is provided AS IS with	*
 * NO WARRANTY whatsoever.						*
 *									*
 * If you use PDCurses in an application, an acknowledgement would be	*
 * appreciated, but is not mandatory. If you make corrections or	*
 * enhancements to PDCurses, please forward them to the current		*
 * maintainer for the benefit of other users.				*
 *									*
 * No distribution of modified PDCurses code may be made under the name	*
 * "PDCurses", except by the current maintainer. (Although PDCurses is	*
 * public domain, the name is a trademark.)				*
 *									*
 * See the file maintain.er for details of the current maintainer.	*
 ************************************************************************/

#define CURSES_LIBRARY 1
#include <curses.h>
#include <stdlib.h>
#include <string.h>

/* undefine any macros for functions defined in this module */
#undef newpad
#undef subpad
#undef prefresh
#undef pnoutrefresh
#undef pechochar

/* undefine any macros for functions called by this module if in debug mode */
#ifdef PDCDEBUG
# undef doupdate
#endif

RCSID("$Id: pad.c,v 1.27 2006/07/15 15:38:24 wmcbrine Exp $");

/* save values for pechochar() */

static int save_pminrow, save_pmincol;
static int save_sminrow, save_smincol, save_smaxrow, save_smaxcol;

/*man-start**************************************************************

  Name:                                                           pad

  Synopsis:
	WINDOW *newpad(int nlines, int ncols);
	WINDOW *subpad(WINDOW *orig, int nlines, int ncols,
		       int begin_y, int begin_x);
	int prefresh(WINDOW *win, int py, int px, int sy1, int sx1,
		     int sy2, int sx2);
	int pnoutrefresh(WINDOW *w, int py, int px, int sy1, int sx1,
			 int sy2, int sx2);
	int pechochar(WINDOW *pad, chtype ch);

  X/Open Description:
	newpad() creates a new pad data structure.  A pad is a special 
	case of a window, which is not restricted by the screen size, 
	and is not necessarily associated with a particular part of the 
	screen.  A pad can be used when a large window is needed, and 
	only a part of the window will be on the screen at one tme.  
	Automatic refreshes of pads (e.g., from scrolling or echoing of 
	input) do not occur.  It is not legal to call refresh() with a 
	pad as an argument; the routines prefresh() or pnoutrefresh() 
	should be called instead.  Note that these routines require 
	additional parameters to specify the part of the pad to be 
	displayed and the location on the screen to be used for display.

	The subpad() routine creates a new sub-pad within a pad.  The 
	dimensions of the sub-pad are nlines lines and ncols columns.  
	The sub-pad is at position (begin_y, begin_x) in the the parent 
	pad.  This position is relative to the pad, and not to the 
	screen like with subwin. The sub-pad is made in the middle of 
	the pad orig, so that changes made to either pad will affect 
	both.  When using this routine, it will often be necessary to 
	call touchwin before calling prefresh.

	The prefresh() routine copies the specified pad to the physical 
	terminal screen.  It takes account of what is already displayed 
	on the screen to optimize cursor movement. The pnoutrefresh() 
	routine copies the named pad to the virtual screen. It then 
	compares the virtual screen with the physical screen and 
	performs the actual update. These routines are analogous to the 
	routines wrefresh() and wnoutrefresh() except that pads, instead 
	of windows, are involved.  Additional parameters are also needed 
	to indicate what part of the pad and screen are involved. The 
	upper left corner of the part of the pad to be displayed is 
	specified by py and px.  The coordinates sy1, sx1, sy2, and sx2 
	specify the edges of the screen rectangle that will contain the 
	selected part of the pad.

	The lower right corner of the pad rectangle to be displayed is 
	calculated from the screen co-ordinates.  This ensures that the 
	screen rectangle and the pad rectangle are the same size. Both 
	rectangles must be entirely contained within their respective 
	structures.

	The pechochar() is functionally equivalent to adch() followed by 
	a call to refresh().

  X/Open Return Value:
	All functions return OK on success and ERR on error.

  Portability				     X/Open    BSD    SYS V
					     Dec '88
	newpad					Y	-	Y
	subpad					Y	-	Y
	prefresh				Y	-	Y
	pnoutrefresh				Y	-	Y
	pechochar				-	-      3.0

**man-end****************************************************************/

WINDOW *newpad(int nlines, int ncols)
{
	WINDOW *win;
	chtype *ptr;
	int i, j;

	PDC_LOG(("newpad() - called: lines=%d cols=%d\n", nlines, ncols));

	if ((win = PDC_makenew( nlines, ncols, -1, -1 )) == (WINDOW *)NULL)
		return (WINDOW *)NULL;

	for (i = 0; i < nlines; i++)
	{
		/* make and clear the lines */

		if ((win->_y[i] = calloc(ncols, sizeof(chtype))) == NULL)
		{
			/* if error, free all the data */

			for (j = 0; j < i; j++)
				free(win->_y[j]);

			free(win->_firstch);
			free(win->_lastch);
			free(win->_y);
			free(win);

			return (WINDOW *)NULL;
		}
		else	/* retain the original screen attributes */

			for (ptr = win->_y[i]; ptr < win->_y[i] + ncols; ptr++)
				*ptr = SP->blank;
	}

	win->_flags = _PAD;

	/* save default values in case pechochar() is the first call
	   to prefresh(). */

	save_pminrow = 0;
	save_pmincol = 0;
	save_sminrow = 0;
	save_smincol = 0;
	save_smaxrow = min(LINES, nlines) - 1;
	save_smaxcol = min(COLS, ncols) - 1;

	return win;
}

WINDOW *subpad(WINDOW *orig, int nlines, int ncols, int begin_y, int begin_x)
{
	WINDOW *win;
	int i;
	int j = begin_y;
	int k = begin_x;

	PDC_LOG(("subpad() - called: lines=%d cols=%d begy=%d begx=%d\n",
		nlines, ncols, begin_y, begin_x));

	if (!orig || !(orig->_flags & _PAD))
		return (WINDOW *)NULL;

	/* make sure window fits inside the original one */

	if ((begin_y < orig->_begy) || (begin_x < orig->_begx) ||
	    (begin_y + nlines) > (orig->_begy + orig->_maxy) ||
	    (begin_x + ncols)  > (orig->_begx + orig->_maxx))
		return (WINDOW *)NULL;

	if (!nlines) 
		nlines = orig->_maxy - 1 - j;

	if (!ncols) 
		ncols = orig->_maxx - 1 - k;

	if ((win = PDC_makenew(nlines, ncols, begin_y, begin_x))
	    == (WINDOW *) NULL)
		return (WINDOW *)NULL;

	/* initialize window variables */

	win->_attrs = orig->_attrs;
	win->_leaveit = orig->_leaveit;
	win->_scroll = orig->_scroll;
	win->_nodelay = orig->_nodelay;
	win->_use_keypad = orig->_use_keypad;
	win->_parent = orig;

	for (i = 0; i < nlines; i++)
		win->_y[i] = (orig->_y[j++]) + k;

	win->_flags = _SUBPAD;

	/* save default values in case pechochar() is the first call
	   to prefresh(). */

	save_pminrow = 0;
	save_pmincol = 0;
	save_sminrow = 0;
	save_smincol = 0;
	save_smaxrow = min(LINES, nlines) - 1;
	save_smaxcol = min(COLS, ncols) - 1;

	return win;
}

int prefresh(WINDOW *win, int py, int px, int sy1, int sx1, int sy2, int sx2)
{
	PDC_LOG(("prefresh() - called\n"));

	if (win == (WINDOW *)NULL)
		return ERR;

	if (pnoutrefresh(win, py, px, sy1, sx1, sy2, sx2) == ERR)
		return ERR;

	doupdate();
	return OK;
}

int pnoutrefresh(WINDOW *w, int py, int px, int sy1, int sx1, int sy2, int sx2)
{
	int num_cols;
	int sline = sy1;
	int pline = py;

	PDC_LOG(("pnoutrefresh() - called\n"));

	if (w == (WINDOW *)NULL)
		return ERR;

	num_cols = min((sx2 - sx1 + 1), (w->_maxx - px));

	if (sy2 < sy1 || sx2 < sx1)
		return ERR;

	if (!(w->_flags == _PAD) && !(w->_flags == _SUBPAD))
		return ERR;

	while (sline <= sy2)
	{
		if (pline < w->_maxy)
		{
			memcpy(&(curscr->_y[sline][sx1]), &(w->_y[pline][px]),
				(num_cols) * sizeof(chtype));

			if ((curscr->_firstch[sline] == _NO_CHANGE) 
			    || (curscr->_firstch[sline] > sx1))
				curscr->_firstch[sline] = sx1;

			if (sx2 > curscr->_lastch[sline])
				curscr->_lastch[sline] = sx2;

			w->_firstch[pline] = _NO_CHANGE;  /* updated now */
			w->_lastch[pline] = _NO_CHANGE;  /* updated now */
		}

		sline++;
		pline++;
	}

	w->_lastpy = py;
	w->_lastpx = px;
	w->_lastsy1 = sy1;
	w->_lastsx1 = sx1;
	w->_lastsy2 = sy2;
	w->_lastsx2 = sx2;

	if (w->_clear)
	{
		w->_clear = FALSE;
		curscr->_clear = TRUE;
	}

	/* position the cursor to the pad's current position if
	   possible -- is the pad current position going to end up 
	   displayed? if not, then don't move the cursor; if so, move it 
	   to the correct place */

	if (!w->_leaveit && w->_cury >= py && w->_curx >= px &&
	     w->_cury <= py + (sy2 - sy1 + 1) &&
	     w->_curx <= px + (sx2 - sx1 + 1))
	{
		curscr->_cury = (w->_cury - py) + sy1;
		curscr->_curx = (w->_curx - px) + sx1;
	}

	return OK;
}

int pechochar(WINDOW *pad, chtype ch)
{
	PDC_LOG(("pechochar() - called\n"));

	if (waddch(pad, ch) == ERR)
		return ERR;

	return prefresh(pad, save_pminrow, save_pmincol, save_sminrow, 
		save_smincol, save_smaxrow, save_smaxcol);
}

#ifdef PDC_WIDE
int pecho_wchar(WINDOW *pad, const cchar_t *wch)
{
	PDC_LOG(("pecho_wchar() - called\n"));

	if (!wch || (waddch(pad, *wch) == ERR))
		return ERR;

	return prefresh(pad, save_pminrow, save_pmincol, save_sminrow, 
		save_smincol, save_smaxrow, save_smaxcol);
}
#endif
