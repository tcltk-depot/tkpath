/*
 * tkpCanvLine.c --
 *
 *	This file implements line items for canvas widgets.
 *
 * Copyright (c) 1991-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#include <stdio.h>
#include "tkInt.h"
#include "tkIntPath.h"
#include "tkpCanvas.h"

/*
 * The structure below defines the record for each line item.
 */

typedef enum {
    ARROWS_NONE, ARROWS_FIRST, ARROWS_LAST, ARROWS_BOTH
} Arrows;

typedef struct LineItem  {
    Tk_PathItem header;		/* Generic stuff that's the same for all
				 * types. MUST BE FIRST IN STRUCTURE. */
    Tk_PathOutline outline;	/* Outline structure */
    Tk_PathCanvas canvas;	/* Canvas containing item. Needed for parsing
				 * arrow shapes. */
    int numPoints;		/* Number of points in line (always >= 0). */
    double *coordPtr;		/* Pointer to malloc-ed array containing x-
				 * and y-coords of all points in line.
				 * X-coords are even-valued indices, y-coords
				 * are corresponding odd-valued indices. If
				 * the line has arrowheads then the first and
				 * last points have been adjusted to refer to
				 * the necks of the arrowheads rather than
				 * their tips. The actual endpoints are stored
				 * in the *firstArrowPtr and *lastArrowPtr, if
				 * they exist. */
    int capStyle;		/* Cap style for line. */
    int joinStyle;		/* Join style for line. */
    GC arrowGC;			/* Graphics context for drawing arrowheads. */
    Arrows arrow;		/* Indicates whether or not to draw arrowheads:
				 * "none", "first", "last", or "both". */
    Tcl_Obj *arrowShapeObj;	/* NULL or list of three canvas lengths. */
    double *firstArrowPtr;	/* Points to array of PTS_IN_ARROW points
				 * describing polygon for arrowhead at first
				 * point in line. First point of arrowhead is
				 * tip. Malloc'ed. NULL means no arrowhead at
				 * first point. */
    double *lastArrowPtr;	/* Points to polygon for arrowhead at last
				 * point in line (PTS_IN_ARROW points, first
				 * of which is tip). Malloc'ed. NULL means no
				 * arrowhead at last point. */
    Tk_PathSmoothMethod *smooth;/* Non-zero means draw line smoothed (i.e.
				 * with Bezier splines). */
    int splineSteps;		/* Number of steps in each spline segment. */
} LineItem;

/*
 * Number of points in an arrowHead:
 */

#define PTS_IN_ARROW 6

/*
 * Prototypes for functions defined in this file:
 */

#ifndef TKP_NO_POSTSCRIPT
static int		ArrowheadPostscript(Tcl_Interp *interp,
			    Tk_PathCanvas canvas, LineItem *linePtr,
			    double *arrowPtr);
#endif
static Tcl_Obj *	ArrowShapeOptionGetProc(ClientData clientData,
			    Tk_Window tkwin, char *recordPtr,
			    Tcl_Size internalOffset);
static int		ArrowShapeOptionSetProc(ClientData clientData,
			    Tcl_Interp *interp, Tk_Window tkwin,
			    Tcl_Obj **value, char *recordPtr,
			    Tcl_Size internalOffset, char *oldInternalPtr,
			    int flags);
static void		ComputeLineBbox(Tk_PathCanvas canvas,
			    LineItem *linePtr);
static int		ConfigureLine(Tcl_Interp *interp,
			    Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
			    Tcl_Size objc, Tcl_Obj *const objv[], int flags);
static int		ConfigureArrows(Tk_PathCanvas canvas,
			    LineItem *linePtr);
static int		CreateLine(Tcl_Interp *interp,
			    Tk_PathCanvas canvas, struct Tk_PathItem *itemPtr,
			    Tcl_Size objc, Tcl_Obj *const objv[]);
static void		DeleteLine(Tk_PathCanvas canvas,
			    Tk_PathItem *itemPtr, Display *display);
static void		DisplayLine(Tk_PathCanvas canvas,
			    Tk_PathItem *itemPtr, Display *display,
			    Drawable dst,
			    int x, int y, int width, int height);
static int		GetLineIndex(Tcl_Interp *interp,
			    Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
			    Tcl_Obj *obj, int *indexPtr);
static int		LineCoords(Tcl_Interp *interp,
			    Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
			    Tcl_Size objc, Tcl_Obj *const objv[]);
static void		LineDeleteCoords(Tk_PathCanvas canvas,
			    Tk_PathItem *itemPtr, int first, int last);
static void		LineInsert(Tk_PathCanvas canvas,
			    Tk_PathItem *itemPtr, int beforeThis, Tcl_Obj *obj);
static int		LineToArea(Tk_PathCanvas canvas,
			    Tk_PathItem *itemPtr, double *rectPtr);
static int		LineToPdf(Tcl_Interp *interp,
			    Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
			    Tcl_Size objc, Tcl_Obj *const objv[], int prepass);
static double		LineToPoint(Tk_PathCanvas canvas,
			    Tk_PathItem *itemPtr, double *coordPtr);
#ifndef TKP_NO_POSTSCRIPT
static int		LineToPostscript(Tcl_Interp *interp,
			    Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
			    int prepass);
#endif
static void		ScaleLine(Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
			    int compensate, double originX, double originY,
			    double scaleX, double scaleY);
static void		TranslateLine(Tk_PathCanvas canvas,
			    Tk_PathItem *itemPtr, int compensate,
			    double deltaX, double deltaY);

/*
 * Information used for parsing configuration specs. If you change any of the
 * default strings, be sure to change the corresponding default values in
 * CreateLine.
 */

#define PATH_DEF_STATE "normal"

/* These MUST be kept in sync with enums! X.h */

static const char *arrowStrings[] = {
    "none", "first", "last", "both", NULL
};

static const char *stateStrings[] = {
    "active", "disabled", "normal", "hidden", NULL
};

static const char *lineCapStrings[] = {
    "notlast", "butt", "round", "projecting", NULL
};

static const char *joinStyleStrings[] = {
    "miter", "round", "bevel", NULL
};

static Tk_ObjCustomOption arrowShapeCO = {
    "arrowshape",
    ArrowShapeOptionSetProc,
    ArrowShapeOptionGetProc,
    NULL,
    NULL,
    (ClientData) NULL
};

static Tk_ObjCustomOption dashCO = {
    "dash",
    Tk_DashOptionSetProc,
    Tk_DashOptionGetProc,
    Tk_DashOptionRestoreProc,
    Tk_DashOptionFreeProc,
    (ClientData) NULL
};

static Tk_ObjCustomOption offsetCO = {
    "offset",
    TkPathOffsetOptionSetProc,
    TkPathOffsetOptionGetProc,
    TkPathOffsetOptionRestoreProc,
    TkPathOffsetOptionFreeProc,
    (ClientData) (TK_OFFSET_RELATIVE|TK_OFFSET_INDEX)
};

static Tk_ObjCustomOption pixelCO = {
    "pixel",
    Tk_PathPixelOptionSetProc,
    Tk_PathPixelOptionGetProc,
    Tk_PathPixelOptionRestoreProc,
    NULL,
    (ClientData) NULL
};

static Tk_ObjCustomOption smoothCO = {
    "smooth",
    TkPathSmoothOptionSetProc,
    TkPathSmoothOptionGetProc,
    TkPathSmoothOptionRestoreProc,
    NULL,
    (ClientData) NULL
};

static Tk_ObjCustomOption tagsCO = {
    "tags",
    Tk_PathCanvasTagsOptionSetProc,
    Tk_PathCanvasTagsOptionGetProc,
    Tk_PathCanvasTagsOptionRestoreProc,
    Tk_PathCanvasTagsOptionFreeProc,
    (ClientData) NULL
};

static Tk_OptionSpec optionSpecs[] = {
    {TK_OPTION_CUSTOM, "-activedash", NULL, NULL,
	NULL, -1, offsetof(LineItem, outline.activeDashPtr),
	TK_OPTION_NULL_OK, &dashCO, 0},
    {TK_OPTION_COLOR, "-activefill", NULL, NULL,
	NULL, -1, offsetof(LineItem, outline.activeColor),
	TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_BITMAP, "-activestipple", NULL, NULL,
        NULL, -1, offsetof(LineItem, outline.activeStipple),
	TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_CUSTOM, "-activewidth", NULL, NULL,
	"0.0", -1, offsetof(LineItem, outline.activeWidth),
	0, &pixelCO, 0},
    {TK_OPTION_STRING_TABLE, "-arrow", NULL, NULL,
        "none", -1, offsetof(LineItem, arrow),
        0, (ClientData) arrowStrings, 0},
    {TK_OPTION_CUSTOM, "-arrowshape", NULL, NULL,
	"8 10 3", offsetof(LineItem, arrowShapeObj), -1,
	0, &arrowShapeCO, 0},
    {TK_OPTION_STRING_TABLE, "-capstyle", NULL, NULL,
        "butt", -1, offsetof(LineItem, capStyle),
        0, (ClientData) lineCapStrings, 0},
    {TK_OPTION_CUSTOM, "-dash", NULL, NULL,
	NULL, -1, offsetof(LineItem, outline.dashPtr),
	TK_OPTION_NULL_OK, &dashCO, 0},
    {TK_OPTION_PIXELS, "-dashoffset", NULL, NULL,
	"0", -1, offsetof(LineItem, outline.offset),
	0, 0, 0},
    {TK_OPTION_CUSTOM, "-disableddash", NULL, NULL,
	NULL, -1, offsetof(LineItem, outline.disabledDashPtr),
	TK_OPTION_NULL_OK, &dashCO, 0},
    {TK_OPTION_COLOR, "-disabledfill", NULL, NULL,
	NULL, -1, offsetof(LineItem, outline.disabledColor),
	TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_BITMAP, "-disabledstipple", NULL, NULL,
        NULL, -1, offsetof(LineItem, outline.disabledStipple),
	TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_CUSTOM, "-disabledwidth", NULL, NULL,
	"0.0", -1, offsetof(LineItem, outline.disabledWidth),
	0, &pixelCO, 0},
    {TK_OPTION_COLOR, "-fill", NULL, NULL,
	"black", -1, offsetof(LineItem, outline.color),
	TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_STRING_TABLE, "-joinstyle", NULL, NULL,
        "round", -1, offsetof(LineItem, joinStyle),
        0, (ClientData) joinStyleStrings, 0},
    {TK_OPTION_CUSTOM, "-offset", NULL, NULL,
	"0,0", -1, offsetof(LineItem, outline.tsoffsetPtr),
	0, &offsetCO, 0},
    {TK_OPTION_CUSTOM, "-smooth", NULL, NULL,
	"0", -1, offsetof(LineItem, smooth),
	0, &smoothCO, 0},
    {TK_OPTION_INT, "-splinesteps", NULL, NULL,
        "12", -1, offsetof(LineItem, splineSteps), 0, 0, 0},
    {TK_OPTION_STRING_TABLE, "-state", NULL, NULL,
        PATH_DEF_STATE, -1, offsetof(Tk_PathItem, state),
        0, (ClientData) stateStrings, 0},
    {TK_OPTION_BITMAP, "-stipple", NULL, NULL,
        NULL, -1, offsetof(LineItem, outline.stipple),
	TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_CUSTOM, "-tags", NULL, NULL,
	NULL, -1, offsetof(Tk_PathItem, pathTagsPtr),
	TK_OPTION_NULL_OK, (ClientData) &tagsCO, 0},
    {TK_OPTION_CUSTOM, "-width", NULL, NULL,
        "1.0", -1, offsetof(LineItem, outline.width), 0, &pixelCO, 0},
    {TK_OPTION_END, NULL, NULL, NULL,
	NULL, 0, -1, 0, (ClientData) NULL, 0}
};

/*
 * The structures below defines the line item type by means of functions that
 * can be invoked by generic item code.
 */

Tk_PathItemType tkpLineType = {
    "line",			/* name */
    sizeof(LineItem),		/* itemSize */
    CreateLine,			/* createProc */
    optionSpecs,		/* optionSpecs */
    ConfigureLine,		/* configureProc */
    LineCoords,			/* coordProc */
    DeleteLine,			/* deleteProc */
    DisplayLine,		/* displayProc */
    TK_MOVABLE_POINTS,		/* flags */
    NULL,			/* bboxProc */
    LineToPoint,		/* pointProc */
    LineToArea,			/* areaProc */
#ifndef TKP_NO_POSTSCRIPT
    LineToPostscript,		/* postscriptProc */
#endif
    LineToPdf,			/* pdfProc */
    ScaleLine,			/* scaleProc */
    TranslateLine,		/* translateProc */
    (Tk_PathItemIndexProc *) GetLineIndex,/* indexProc */
    NULL,			/* icursorProc */
    NULL,			/* selectionProc */
    (Tk_PathItemInsertProc *) LineInsert,/* insertProc */
    LineDeleteCoords,		/* dTextProc */
    NULL,			/* nextPtr */
    0,				/* isPathType */
};

/*
 * The definition below determines how large are static arrays used to hold
 * spline points (splines larger than this have to have their arrays
 * malloc-ed).
 */

#define MAX_STATIC_POINTS 200

/*
 *--------------------------------------------------------------
 *
 * CreateLine --
 *
 *	This function is invoked to create a new line item in a canvas.
 *
 * Results:
 *	A standard Tcl return value. If an error occurred in creating the
 *	item, then an error message is left in the interp's result; in this
 *	case itemPtr is left uninitialized, so it can be safely freed by the
 *	caller.
 *
 * Side effects:
 *	A new line item is created.
 *
 *--------------------------------------------------------------
 */

static int
CreateLine(
    Tcl_Interp *interp,		/* Interpreter for error reporting. */
    Tk_PathCanvas canvas,	/* Canvas to hold new item. */
    Tk_PathItem *itemPtr,	/* Record to hold new item; header has been
				 * initialized by caller. */
    Tcl_Size objc,		/* Number of arguments in objv. */
    Tcl_Obj *const objv[])	/* Arguments describing line. */
{
    LineItem *linePtr = (LineItem *) itemPtr;
    Tcl_Size i;
    Tk_OptionTable optionTable;

    if (objc == 0) {
	Tcl_Panic("canvas did not pass any coords\n");
    }

    /*
     * Carry out initialization that is needed to set defaults and to allow
     * proper cleanup after errors during the the remainder of this function.
     */

    Tk_PathCreateOutline(&(linePtr->outline));
    linePtr->canvas = canvas;
    linePtr->numPoints = 0;
    linePtr->coordPtr = NULL;
    linePtr->capStyle = CapButt;
    linePtr->joinStyle = JoinRound;
    linePtr->arrowGC = NULL;
    linePtr->arrow = ARROWS_NONE;
    linePtr->arrowShapeObj = NULL;
    linePtr->firstArrowPtr = NULL;
    linePtr->lastArrowPtr = NULL;
    linePtr->smooth = NULL;
    linePtr->splineSteps = 12;

    optionTable = Tk_CreateOptionTable(interp, optionSpecs);
    itemPtr->optionTable = optionTable;
    if (Tk_InitOptions(interp, (char *) linePtr, optionTable,
	    Tk_PathCanvasTkwin(canvas)) != TCL_OK) {
        goto error;
    }

    /*
     * Count the number of points and then parse them into a point array.
     * Leading arguments are assumed to be points if they start with a digit
     * or a minus sign followed by a digit.
     */

    for (i = 1; i < objc; i++) {
	char *arg = Tcl_GetString(objv[i]);

	if ((arg[0] == '-') && (arg[1] >= 'a') && (arg[1] <= 'z')) {
	    break;
	}
    }
    if (LineCoords(interp, canvas, itemPtr, i, objv) != TCL_OK) {
	goto error;
    }
    if (ConfigureLine(interp, canvas, itemPtr, objc-i, objv+i, 0) == TCL_OK) {
	return TCL_OK;
    }

  error:
    DeleteLine(canvas, itemPtr, Tk_Display(Tk_PathCanvasTkwin(canvas)));
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * LineCoords --
 *
 *	This function is invoked to process the "coords" widget command on
 *	lines. See the user documentation for details on what it does.
 *
 * Results:
 *	Returns TCL_OK or TCL_ERROR, and sets the interp's result.
 *
 * Side effects:
 *	The coordinates for the given item may be changed.
 *
 *--------------------------------------------------------------
 */

static int
LineCoords(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tk_PathCanvas canvas,	/* Canvas containing item. */
    Tk_PathItem *itemPtr,	/* Item whose coordinates are to be read or
				 * modified. */
    Tcl_Size objc,		/* Number of coordinates supplied in objv. */
    Tcl_Obj *const objv[])	/* Array of coordinates: x1, y1, x2, y2, ... */
{
    LineItem *linePtr = (LineItem *) itemPtr;
    int i, numPoints;
    double *coordPtr;
    Tcl_Size myobjc = objc;

    if (myobjc == 0) {
	int numCoords;
	Tcl_Obj *subobj, *obj = Tcl_NewObj();

	numCoords = 2*linePtr->numPoints;
	if (linePtr->firstArrowPtr != NULL) {
	    coordPtr = linePtr->firstArrowPtr;
	} else {
	    coordPtr = linePtr->coordPtr;
	}
	for (i = 0; i < numCoords; i++, coordPtr++) {
	    if (i == 2) {
		coordPtr = linePtr->coordPtr+2;
	    }
	    if ((linePtr->lastArrowPtr != NULL) && (i == (numCoords-2))) {
		coordPtr = linePtr->lastArrowPtr;
	    }
	    subobj = Tcl_NewDoubleObj(*coordPtr);
	    Tcl_ListObjAppendElement(interp, obj, subobj);
	}
	Tcl_SetObjResult(interp, obj);
	return TCL_OK;
    }
    if (myobjc == 1) {
	if (Tcl_ListObjGetElements(interp, objv[0], &myobjc,
		(Tcl_Obj ***) &objv) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (myobjc & 1) {
        Tcl_SetObjResult(interp,
            Tcl_ObjPrintf("wrong # coordinates: expected an even number, got %"
                          TCL_SIZE_MODIFIER "d", myobjc));
	return TCL_ERROR;
    } else if (myobjc < 4) {
        Tcl_SetObjResult(interp,
                         Tcl_ObjPrintf("wrong # coordinates: expected at least 4, got %"
                                       TCL_SIZE_MODIFIER "d", myobjc));
	return TCL_ERROR;
    } else {
	numPoints = myobjc/2;
	if (linePtr->numPoints != numPoints) {
	    coordPtr = (double *)
		    ckalloc((unsigned) (sizeof(double) * myobjc));
	    if (linePtr->coordPtr != NULL) {
		ckfree((char *) linePtr->coordPtr);
	    }
	    linePtr->coordPtr = coordPtr;
	    linePtr->numPoints = numPoints;
	}
	coordPtr = linePtr->coordPtr;
	for (i = 0; i <myobjc; i++) {
	    if (Tk_PathCanvasGetCoordFromObj(interp, canvas, objv[i],
		    coordPtr++) != TCL_OK) {
  		return TCL_ERROR;
  	    }
  	}

	/*
	 * Update arrowheads by throwing away any existing arrow-head
	 * information and calling ConfigureArrows to recompute it.
	 */

	if (linePtr->firstArrowPtr != NULL) {
	    ckfree((char *) linePtr->firstArrowPtr);
	    linePtr->firstArrowPtr = NULL;
	}
	if (linePtr->lastArrowPtr != NULL) {
	    ckfree((char *) linePtr->lastArrowPtr);
	    linePtr->lastArrowPtr = NULL;
	}
	if (linePtr->arrow != ARROWS_NONE) {
	    ConfigureArrows(canvas, linePtr);
	}
	ComputeLineBbox(canvas, linePtr);
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * ConfigureLine --
 *
 *	This function is invoked to configure various aspects of a line item
 *	such as its background color.
 *
 * Results:
 *	A standard Tcl result code. If an error occurs, then an error message
 *	is left in the interp's result.
 *
 * Side effects:
 *	Configuration information, such as colors and stipple patterns, may be
 *	set for itemPtr.
 *
 *--------------------------------------------------------------
 */

static int
ConfigureLine(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tk_PathCanvas canvas,	/* Canvas containing itemPtr. */
    Tk_PathItem *itemPtr,	/* Line item to reconfigure. */
    Tcl_Size objc,		/* Number of elements in objv.  */
    Tcl_Obj *const objv[],	/* Arguments describing things to configure. */
    int flags)			/* Flags to pass to Tk_ConfigureWidget. */
{
    LineItem *linePtr = (LineItem *) itemPtr;
    XGCValues gcValues;
    GC newGC, arrowGC;
    unsigned long mask;
    Tk_Window tkwin;
    Tk_PathState state;

    tkwin = Tk_PathCanvasTkwin(canvas);
    if (TCL_OK != Tk_SetOptions(interp, (char *) linePtr, itemPtr->optionTable,
	    objc, objv, tkwin, NULL, NULL)) {
	return TCL_ERROR;
    }

    /*
     * A few of the options require additional processing, such as graphics
     * contexts.
     */

    state = itemPtr->state;

    if (state == TK_PATHSTATE_NULL) {
	state = TkPathCanvasState(canvas);
    }

    if (linePtr->outline.activeWidth > linePtr->outline.width ||
	    (linePtr->outline.activeDashPtr != NULL
		    && linePtr->outline.activeDashPtr->number != 0) ||
	    linePtr->outline.activeColor != NULL ||
	    linePtr->outline.activeStipple != None) {
	itemPtr->redraw_flags |= TK_ITEM_STATE_DEPENDANT;
    } else {
	itemPtr->redraw_flags &= ~TK_ITEM_STATE_DEPENDANT;
    }
    mask = Tk_PathConfigOutlineGC(&gcValues, canvas, itemPtr,
	    &(linePtr->outline));
    if (mask) {
	if (linePtr->arrow == ARROWS_NONE) {
	    gcValues.cap_style = linePtr->capStyle;
	    mask |= GCCapStyle;
	}
	gcValues.join_style = linePtr->joinStyle;
	mask |= GCJoinStyle;
	newGC = Tk_GetGC(tkwin, mask, &gcValues);
#ifdef MAC_OSX_TK
	/*
	 * Mac OS X CG drawing needs access to linewidth even for
	 * arrow fills (as linewidth controls antialiasing).
	 */
	mask |= GCLineWidth;
#else
	gcValues.line_width = 0;
#endif
	arrowGC = Tk_GetGC(tkwin, mask, &gcValues);
    } else {
	newGC = arrowGC = NULL;
    }
    if (linePtr->outline.gc != NULL) {
	Tk_FreeGC(Tk_Display(tkwin), linePtr->outline.gc);
    }
    if (linePtr->arrowGC != NULL) {
	Tk_FreeGC(Tk_Display(tkwin), linePtr->arrowGC);
    }
    linePtr->outline.gc = newGC;
    linePtr->arrowGC = arrowGC;

    /*
     * Keep spline parameters within reasonable limits.
     */

    if (linePtr->splineSteps < 1) {
	linePtr->splineSteps = 1;
    } else if (linePtr->splineSteps > 100) {
	linePtr->splineSteps = 100;
    }

    if ((!linePtr->numPoints) || (state == TK_PATHSTATE_HIDDEN)) {
	ComputeLineBbox(canvas, linePtr);
	return TCL_OK;
    }

    /*
     * Setup arrowheads, if needed. If arrowheads are turned off, restore the
     * line's endpoints (they were shortened when the arrowheads were added).
     */

    if ((linePtr->firstArrowPtr != NULL) && (linePtr->arrow != ARROWS_FIRST)
	    && (linePtr->arrow != ARROWS_BOTH)) {
	linePtr->coordPtr[0] = linePtr->firstArrowPtr[0];
	linePtr->coordPtr[1] = linePtr->firstArrowPtr[1];
	ckfree((char *) linePtr->firstArrowPtr);
	linePtr->firstArrowPtr = NULL;
    }
    if ((linePtr->lastArrowPtr != NULL) && (linePtr->arrow != ARROWS_LAST)
	    && (linePtr->arrow != ARROWS_BOTH)) {
	int i;

	i = 2*(linePtr->numPoints-1);
	linePtr->coordPtr[i] = linePtr->lastArrowPtr[0];
	linePtr->coordPtr[i+1] = linePtr->lastArrowPtr[1];
	ckfree((char *) linePtr->lastArrowPtr);
	linePtr->lastArrowPtr = NULL;
    }
    if (linePtr->arrow != ARROWS_NONE) {
	ConfigureArrows(canvas, linePtr);
    }

    /*
     * Recompute bounding box for line.
     */

    ComputeLineBbox(canvas, linePtr);

    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * DeleteLine --
 *
 *	This function is called to clean up the data structure associated with
 *	a line item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Resources associated with itemPtr are released.
 *
 *--------------------------------------------------------------
 */

static void
DeleteLine(
    Tk_PathCanvas canvas,	/* Info about overall canvas widget. */
    Tk_PathItem *itemPtr,	/* Item that is being deleted. */
    Display *display)		/* Display containing window for canvas. */
{
    LineItem *linePtr = (LineItem *) itemPtr;

    if (linePtr->coordPtr != NULL) {
	ckfree((char *) linePtr->coordPtr);
    }
    if (linePtr->arrowGC != NULL) {
	Tk_FreeGC(display, linePtr->arrowGC);
    }
    if (linePtr->firstArrowPtr != NULL) {
	ckfree((char *) linePtr->firstArrowPtr);
    }
    if (linePtr->lastArrowPtr != NULL) {
	ckfree((char *) linePtr->lastArrowPtr);
    }
    Tk_FreeConfigOptions((char *) linePtr, itemPtr->optionTable,
			 Tk_PathCanvasTkwin(canvas));
}

/*
 *--------------------------------------------------------------
 *
 * ComputeLineBbox --
 *
 *	This function is invoked to compute the bounding box of all the pixels
 *	that may be drawn as part of a line.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The fields x1, y1, x2, and y2 are updated in the header for itemPtr.
 *
 *--------------------------------------------------------------
 */

static void
ComputeLineBbox(
    Tk_PathCanvas canvas,		/* Canvas that contains item. */
    LineItem *linePtr)		/* Item whose bbos is to be recomputed. */
{
    double *coordPtr;
    int i, intWidth;
    double width;
    Tk_PathState state = linePtr->header.state;
    Tk_TSOffset *tsoffset;

    if (state == TK_PATHSTATE_NULL) {
	state = TkPathCanvasState(canvas);
    }

    if (!(linePtr->numPoints) || (state == TK_PATHSTATE_HIDDEN)) {
	linePtr->header.x1 = -1;
	linePtr->header.x2 = -1;
	linePtr->header.y1 = -1;
	linePtr->header.y2 = -1;
	return;
    }

    width = linePtr->outline.width;
    if (((TkPathCanvas *)canvas)->currentItemPtr == (Tk_PathItem *)linePtr) {
	if (linePtr->outline.activeWidth>width) {
	    width = linePtr->outline.activeWidth;
	}
    } else if (state == TK_PATHSTATE_DISABLED) {
	if (linePtr->outline.disabledWidth>0) {
	    width = linePtr->outline.disabledWidth;
	}
    }

    coordPtr = linePtr->coordPtr;
    linePtr->header.x1 = linePtr->header.x2 = (int) *coordPtr;
    linePtr->header.y1 = linePtr->header.y2 = (int) coordPtr[1];

    /*
     * Compute the bounding box of all the points in the line, then expand in
     * all directions by the line's width to take care of butting or rounded
     * corners and projecting or rounded caps. This expansion is an
     * overestimate (worst-case is square root of two over two) but it's
     * simple. eDon't do anything special for curves. This causes an
     * additional overestimate in the bounding box, but is faster.
     */

    for (i = 1, coordPtr = linePtr->coordPtr+2; i < linePtr->numPoints;
	    i++, coordPtr += 2) {
	TkPathIncludePoint((Tk_PathItem *) linePtr, coordPtr);
    }
    width = linePtr->outline.width;
    if (width < 1.0) {
	width = 1.0;
    }
    if (linePtr->arrow != ARROWS_NONE) {
	if ((linePtr->arrow != ARROWS_LAST) &&
		(linePtr->firstArrowPtr != NULL)) {
	    TkPathIncludePoint((Tk_PathItem *) linePtr, linePtr->firstArrowPtr);
	}
	if ((linePtr->arrow != ARROWS_FIRST) &&
		(linePtr->lastArrowPtr != NULL)) {
	    TkPathIncludePoint((Tk_PathItem *) linePtr, linePtr->lastArrowPtr);
	}
    }

    tsoffset = linePtr->outline.tsoffsetPtr;
    if (tsoffset != NULL) {
	if (tsoffset->flags & TK_OFFSET_INDEX) {
	    double *coordPtr = linePtr->coordPtr + (tsoffset->flags & ~TK_OFFSET_INDEX);
	    if (tsoffset->flags <= 0) {
		coordPtr = linePtr->coordPtr;
		if ((linePtr->arrow == ARROWS_FIRST) || (linePtr->arrow == ARROWS_BOTH)) {
		    coordPtr = linePtr->firstArrowPtr;
		}
	    }
	    if (tsoffset->flags > (linePtr->numPoints * 2)) {
		coordPtr = linePtr->coordPtr + (linePtr->numPoints * 2);
		if ((linePtr->arrow == ARROWS_LAST) || (linePtr->arrow == ARROWS_BOTH)) {
		    coordPtr = linePtr->lastArrowPtr;
		}
	    }
	    tsoffset->xoffset = (int) (coordPtr[0] + 0.5);
	    tsoffset->yoffset = (int) (coordPtr[1] + 0.5);
	} else {
	    if (tsoffset->flags & TK_OFFSET_LEFT) {
		tsoffset->xoffset = linePtr->header.x1;
	    } else if (tsoffset->flags & TK_OFFSET_CENTER) {
		tsoffset->xoffset = (linePtr->header.x1 + linePtr->header.x2)/2;
	    } else if (tsoffset->flags & TK_OFFSET_RIGHT) {
		tsoffset->xoffset = linePtr->header.x2;
	    }
	    if (tsoffset->flags & TK_OFFSET_TOP) {
		tsoffset->yoffset = linePtr->header.y1;
	    } else if (tsoffset->flags & TK_OFFSET_MIDDLE) {
		tsoffset->yoffset = (linePtr->header.y1 + linePtr->header.y2)/2;
	    } else if (tsoffset->flags & TK_OFFSET_BOTTOM) {
		tsoffset->yoffset = linePtr->header.y2;
	    }
	}
    }

    intWidth = (int) (width + 0.5);
    linePtr->header.x1 -= intWidth;
    linePtr->header.x2 += intWidth;
    linePtr->header.y1 -= intWidth;
    linePtr->header.y2 += intWidth;

    if (linePtr->numPoints == 1) {
	linePtr->header.x1 -= 1;
	linePtr->header.x2 += 1;
	linePtr->header.y1 -= 1;
	linePtr->header.y2 += 1;
	return;
    }

    /*
     * For mitered lines, make a second pass through all the points. Compute
     * the locations of the two miter vertex points and add those into the
     * bounding box.
     */

    if (linePtr->joinStyle == JoinMiter) {
	for (i = linePtr->numPoints, coordPtr = linePtr->coordPtr; i >= 3;
		i--, coordPtr += 2) {
	    double miter[4];
	    int j;

	    if (TkGetMiterPoints(coordPtr, coordPtr+2, coordPtr+4,
		    width, miter, miter+2)) {
		for (j = 0; j < 4; j += 2) {
		    TkPathIncludePoint((Tk_PathItem *) linePtr, miter+j);
		}
	    }
	}
    }

    /*
     * Add in the sizes of arrowheads, if any.
     */

    if (linePtr->arrow != ARROWS_NONE) {
	if (linePtr->arrow != ARROWS_LAST) {
	    for (i = 0, coordPtr = linePtr->firstArrowPtr; i < PTS_IN_ARROW;
		    i++, coordPtr += 2) {
		TkPathIncludePoint((Tk_PathItem *) linePtr, coordPtr);
	    }
	}
	if (linePtr->arrow != ARROWS_FIRST) {
	    for (i = 0, coordPtr = linePtr->lastArrowPtr; i < PTS_IN_ARROW;
		    i++, coordPtr += 2) {
		TkPathIncludePoint((Tk_PathItem *) linePtr, coordPtr);
	    }
	}
    }

    /*
     * Add one more pixel of fudge factor just to be safe (e.g. X may round
     * differently than we do).
     */

    linePtr->header.x1 -= 1;
    linePtr->header.x2 += 1;
    linePtr->header.y1 -= 1;
    linePtr->header.y2 += 1;
}

/*
 *--------------------------------------------------------------
 *
 * DisplayLine --
 *
 *	This function is invoked to draw a line item in a given drawable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	ItemPtr is drawn in drawable using the transformation information in
 *	canvas.
 *
 *--------------------------------------------------------------
 */

static void
DisplayLine(
    Tk_PathCanvas canvas,		/* Canvas that contains item. */
    Tk_PathItem *itemPtr,		/* Item to be displayed. */
    Display *display,		/* Display on which to draw item. */
    Drawable drawable,		/* Pixmap or window in which to draw item. */
    int x, int y, int width, int height)
				/* Describes region of canvas that must be
				 * redisplayed (not used). */
{
    LineItem *linePtr = (LineItem *) itemPtr;
    XPoint staticPoints[MAX_STATIC_POINTS*3];
    XPoint *pointPtr;
    double linewidth;
    int numPoints;
    Tk_PathState state = itemPtr->state;

    if ((!linePtr->numPoints)||(linePtr->outline.gc==NULL)) {
	return;
    }

    if (state == TK_PATHSTATE_NULL) {
	state = TkPathCanvasState(canvas);
    }
    linewidth = linePtr->outline.width;
    if (((TkPathCanvas *)canvas)->currentItemPtr == itemPtr) {
	if (linePtr->outline.activeWidth != linewidth) {
	    linewidth = linePtr->outline.activeWidth;
	}
    } else if (state == TK_PATHSTATE_DISABLED) {
	if (linePtr->outline.disabledWidth != linewidth) {
	    linewidth = linePtr->outline.disabledWidth;
	}
    }
    /*
     * Build up an array of points in screen coordinates. Use a static array
     * unless the line has an enormous number of points; in this case,
     * dynamically allocate an array. For smoothed lines, generate the curve
     * points on each redisplay.
     */

    if ((linePtr->smooth) && (linePtr->numPoints > 2)) {
	numPoints = linePtr->smooth->coordProc(canvas, NULL,
		linePtr->numPoints, linePtr->splineSteps, NULL, NULL);
    } else {
	numPoints = linePtr->numPoints;
    }

    if (numPoints <= MAX_STATIC_POINTS) {
	pointPtr = staticPoints;
    } else {
	pointPtr = (XPoint *)ckalloc((unsigned)(numPoints * 3*sizeof(XPoint)));
    }

    if ((linePtr->smooth) && (linePtr->numPoints > 2)) {
	numPoints = linePtr->smooth->coordProc(canvas, linePtr->coordPtr,
		linePtr->numPoints, linePtr->splineSteps, pointPtr, NULL);
    } else {
	numPoints = TkPathCanvTranslatePath((TkPathCanvas*)canvas, numPoints,
		linePtr->coordPtr, 0, pointPtr);
    }

    /*
     * Display line, the free up line storage if it was dynamically allocated.
     * If we're stippling, then modify the stipple offset in the GC. Be sure
     * to reset the offset when done, since the GC is supposed to be
     * read-only.
     */

    if (Tk_PathChangeOutlineGC(canvas, itemPtr, &(linePtr->outline))) {
	Tk_PathCanvasSetOffset(canvas, linePtr->arrowGC, linePtr->outline.tsoffsetPtr);
    }
    if (numPoints>1) {
	XDrawLines(display, drawable, linePtr->outline.gc, pointPtr, numPoints,
	    CoordModeOrigin);
    } else {
	int intwidth = (int) (linewidth + 0.5);
        if (intwidth<1) {
	    intwidth=1;
	}
	XFillArc(display, drawable, linePtr->outline.gc,
		pointPtr->x - intwidth/2, pointPtr->y - intwidth/2,
		(unsigned int)intwidth+1, (unsigned int)intwidth+1, 0, 64*360);
    }
    if (pointPtr != staticPoints) {
	ckfree((char *) pointPtr);
    }

    /*
     * Display arrowheads, if they are wanted.
     */

    if (linePtr->firstArrowPtr != NULL) {
	TkPathFillPolygon(canvas, linePtr->firstArrowPtr, PTS_IN_ARROW,
		display, drawable, linePtr->arrowGC, NULL);
    }
    if (linePtr->lastArrowPtr != NULL) {
	TkPathFillPolygon(canvas, linePtr->lastArrowPtr, PTS_IN_ARROW,
		display, drawable, linePtr->arrowGC, NULL);
    }
    if (Tk_PathResetOutlineGC(canvas, itemPtr, &(linePtr->outline))) {
	XSetTSOrigin(display, linePtr->arrowGC, 0, 0);
    }
}

/*
 *--------------------------------------------------------------
 *
 * LineInsert --
 *
 *	Insert coords into a line item at a given index.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The coords in the given item is modified.
 *
 *--------------------------------------------------------------
 */

static void
LineInsert(
    Tk_PathCanvas canvas,		/* Canvas containing text item. */
    Tk_PathItem *itemPtr,		/* Line item to be modified. */
    int beforeThis,		/* Index before which new coordinates are to
				 * be inserted. */
    Tcl_Obj *obj)		/* New coordinates to be inserted. */
{
    LineItem *linePtr = (LineItem *) itemPtr;
    Tcl_Size objc;
    int length, i;
    double *newCoordPtr, *coordPtr;
    Tk_PathState state = itemPtr->state;
    Tcl_Obj **objv;

    if (state == TK_PATHSTATE_NULL) {
	state = TkPathCanvasState(canvas);
    }

    if (!obj || (Tcl_ListObjGetElements(NULL, obj, &objc, &objv) != TCL_OK)
	    || !objc || objc&1) {
	return;
    }
    length = 2*linePtr->numPoints;
    if (beforeThis < 0) {
	beforeThis = 0;
    }
    if (beforeThis > length) {
	beforeThis = length;
    }
    if (linePtr->firstArrowPtr != NULL) {
	linePtr->coordPtr[0] = linePtr->firstArrowPtr[0];
	linePtr->coordPtr[1] = linePtr->firstArrowPtr[1];
    }
    if (linePtr->lastArrowPtr != NULL) {
	linePtr->coordPtr[length-2] = linePtr->lastArrowPtr[0];
	linePtr->coordPtr[length-1] = linePtr->lastArrowPtr[1];
    }
    newCoordPtr = (double *)
	    ckalloc(sizeof(double) * (unsigned)(length + objc));
    for (i=0; i<beforeThis; i++) {
	newCoordPtr[i] = linePtr->coordPtr[i];
    }
    for (i=0; i<objc; i++) {
	if (Tcl_GetDoubleFromObj(NULL, objv[i],
		&newCoordPtr[i + beforeThis]) != TCL_OK) {
	    Tcl_ResetResult(((TkPathCanvas *)canvas)->interp);
	    ckfree((char *) newCoordPtr);
	    return;
	}
    }

    for (i=beforeThis; i<length; i++) {
	newCoordPtr[i+objc] = linePtr->coordPtr[i];
    }
    if (linePtr->coordPtr) {
	ckfree((char *)linePtr->coordPtr);
    }
    linePtr->coordPtr = newCoordPtr;
    linePtr->numPoints = (length + objc)/2;

    if ((length>3) && (state != TK_PATHSTATE_HIDDEN)) {
	/*
	 * This is some optimizing code that will result that only the part of
	 * the polygon that changed (and the objects that are overlapping with
	 * that part) need to be redrawn. A special flag is set that instructs
	 * the general canvas code not to redraw the whole object. If this
	 * flag is not set, the canvas will do the redrawing, otherwise I have
	 * to do it here.
	 */

	itemPtr->redraw_flags |= TK_ITEM_DONT_REDRAW;

	if (beforeThis>0) {beforeThis -= 2; objc+=2; }
	if ((beforeThis+objc)<length) objc+=2;
	if (linePtr->smooth) {
	    if (beforeThis>0) {
		beforeThis-=2; objc+=2;
	    }
	    if ((beforeThis+objc+2)<length) {
		objc+=2;
	    }
	}
	itemPtr->x1 = itemPtr->x2 = (int) linePtr->coordPtr[beforeThis];
	itemPtr->y1 = itemPtr->y2 = (int) linePtr->coordPtr[beforeThis+1];
	if ((linePtr->firstArrowPtr != NULL) && (beforeThis<1)) {
	    /*
	     * Include old first arrow.
	     */

	    for (i = 0, coordPtr = linePtr->firstArrowPtr; i < PTS_IN_ARROW;
		    i++, coordPtr += 2) {
		TkPathIncludePoint(itemPtr, coordPtr);
	    }
	}
	if ((linePtr->lastArrowPtr != NULL) && ((beforeThis+objc)>=length)) {
	    /*
	     * Include old last arrow.
	     */

	    for (i = 0, coordPtr = linePtr->lastArrowPtr; i < PTS_IN_ARROW;
		    i++, coordPtr += 2) {
		TkPathIncludePoint(itemPtr, coordPtr);
	    }
	}
	coordPtr = linePtr->coordPtr+beforeThis+2;
	for (i=2; i<objc; i+=2) {
	    TkPathIncludePoint(itemPtr, coordPtr);
	    coordPtr+=2;
	}
    }
    if (linePtr->firstArrowPtr != NULL) {
	ckfree((char *) linePtr->firstArrowPtr);
	linePtr->firstArrowPtr = NULL;
    }
    if (linePtr->lastArrowPtr != NULL) {
	ckfree((char *) linePtr->lastArrowPtr);
	linePtr->lastArrowPtr = NULL;
    }
    if (linePtr->arrow != ARROWS_NONE) {
	ConfigureArrows(canvas, linePtr);
    }

    if (itemPtr->redraw_flags & TK_ITEM_DONT_REDRAW) {
	double width;
	int intWidth;

	if ((linePtr->firstArrowPtr != NULL) && (beforeThis>2)) {
	    /*
	     * Include new first arrow.
	     */

	    for (i = 0, coordPtr = linePtr->firstArrowPtr; i < PTS_IN_ARROW;
		    i++, coordPtr += 2) {
		TkPathIncludePoint(itemPtr, coordPtr);
	    }
	}
	if ((linePtr->lastArrowPtr != NULL) && (beforeThis+objc < length-2)) {
	    /*
	     * Include new right arrow.
	     */

	    for (i = 0, coordPtr = linePtr->lastArrowPtr; i < PTS_IN_ARROW;
		    i++, coordPtr += 2) {
		TkPathIncludePoint(itemPtr, coordPtr);
	    }
	}
	width = linePtr->outline.width;
	if (((TkPathCanvas *)canvas)->currentItemPtr == itemPtr) {
	    if (linePtr->outline.activeWidth>width) {
		width = linePtr->outline.activeWidth;
	    }
	} else if (state == TK_PATHSTATE_DISABLED) {
	    if (linePtr->outline.disabledWidth>0) {
		width = linePtr->outline.disabledWidth;
	    }
	}
	intWidth = (int) (width + 0.5);
	if (intWidth < 1) {
	    intWidth = 1;
	}
	itemPtr->x1 -= intWidth; itemPtr->y1 -= intWidth;
	itemPtr->x2 += intWidth; itemPtr->y2 += intWidth;
	Tk_PathCanvasEventuallyRedraw(canvas, itemPtr->x1, itemPtr->y1,
		itemPtr->x2, itemPtr->y2);
    }

    ComputeLineBbox(canvas, linePtr);
}

/*
 *--------------------------------------------------------------
 *
 * LineDeleteCoords --
 *
 *	Delete one or more coordinates from a line item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Characters between "first" and "last", inclusive, get deleted from
 *	itemPtr.
 *
 *--------------------------------------------------------------
 */

static void
LineDeleteCoords(
    Tk_PathCanvas canvas,		/* Canvas containing itemPtr. */
    Tk_PathItem *itemPtr,		/* Item in which to delete characters. */
    int first,			/* Index of first character to delete. */
    int last)			/* Index of last character to delete. */
{
    LineItem *linePtr = (LineItem *) itemPtr;
    int count, i, first1, last1;
    int length = 2*linePtr->numPoints;
    double *coordPtr;
    Tk_PathState state = itemPtr->state;

    if (state == TK_PATHSTATE_NULL) {
	state = TkPathCanvasState(canvas);
    }

    first &= -2;
    last &= -2;

    if (first < 0) {
	first = 0;
    }
    if (last >= length) {
	last = length-2;
    }
    if (first > last) {
	return;
    }
    if (linePtr->firstArrowPtr != NULL) {
	linePtr->coordPtr[0] = linePtr->firstArrowPtr[0];
	linePtr->coordPtr[1] = linePtr->firstArrowPtr[1];
    }
    if (linePtr->lastArrowPtr != NULL) {
	linePtr->coordPtr[length-2] = linePtr->lastArrowPtr[0];
	linePtr->coordPtr[length-1] = linePtr->lastArrowPtr[1];
    }
    first1 = first; last1 = last;
    if (first1>0) first1 -= 2;
    if (last1<length-2) last1 += 2;
    if (linePtr->smooth) {
	if (first1>0) first1 -= 2;
	if (last1<length-2) last1 += 2;
    }

    if ((first1<2) && (last1 >= length-2)) {
	/*
	 * This is some optimizing code that will result that only the part of
	 * the line that changed (and the objects that are overlapping with
	 * that part) need to be redrawn. A special flag is set that instructs
	 * the general canvas code not to redraw the whole object. If this
	 * flag is set, the redrawing has to be done here, otherwise the
	 * general Canvas code will take care of it.
	 */

	itemPtr->redraw_flags |= TK_ITEM_DONT_REDRAW;
	itemPtr->x1 = itemPtr->x2 = (int) linePtr->coordPtr[first1];
	itemPtr->y1 = itemPtr->y2 = (int) linePtr->coordPtr[first1+1];
	if ((linePtr->firstArrowPtr != NULL) && (first1<2)) {
	    /*
	     * Include old first arrow.
	     */

	    for (i = 0, coordPtr = linePtr->firstArrowPtr; i < PTS_IN_ARROW;
		    i++, coordPtr += 2) {
		TkPathIncludePoint(itemPtr, coordPtr);
	    }
	}
	if ((linePtr->lastArrowPtr != NULL) && (last1>=length-2)) {
	    /*
	     * Include old last arrow.
	     */

	    for (i = 0, coordPtr = linePtr->lastArrowPtr; i < PTS_IN_ARROW;
		    i++, coordPtr += 2) {
		TkPathIncludePoint(itemPtr, coordPtr);
	    }
	}
	coordPtr = linePtr->coordPtr+first1+2;
	for (i=first1+2; i<=last1; i+=2) {
	    TkPathIncludePoint(itemPtr, coordPtr);
	    coordPtr+=2;
	}
    }

    count = last + 2 - first;
    for (i=last+2; i<length; i++) {
	linePtr->coordPtr[i-count] = linePtr->coordPtr[i];
    }
    linePtr->numPoints -= count/2;
    if (linePtr->firstArrowPtr != NULL) {
	ckfree((char *) linePtr->firstArrowPtr);
	linePtr->firstArrowPtr = NULL;
    }
    if (linePtr->lastArrowPtr != NULL) {
	ckfree((char *) linePtr->lastArrowPtr);
	linePtr->lastArrowPtr = NULL;
    }
    if (linePtr->arrow != ARROWS_NONE) {
	    ConfigureArrows(canvas, linePtr);
    }
    if (itemPtr->redraw_flags & TK_ITEM_DONT_REDRAW) {
	double width;
	int intWidth;

	if ((linePtr->firstArrowPtr != NULL) && (first1<4)) {
	    /*
	     * Include new first arrow.
	     */

	    for (i = 0, coordPtr = linePtr->firstArrowPtr; i < PTS_IN_ARROW;
		    i++, coordPtr += 2) {
		TkPathIncludePoint(itemPtr, coordPtr);
	    }
	}
	if ((linePtr->lastArrowPtr != NULL) && (last1>(length-4))) {
	    /*
	     * Include new right arrow.
	     */

	    for (i = 0, coordPtr = linePtr->lastArrowPtr; i < PTS_IN_ARROW;
		    i++, coordPtr += 2) {
		TkPathIncludePoint(itemPtr, coordPtr);
	    }
	}
	width = linePtr->outline.width;
	if (((TkPathCanvas *)canvas)->currentItemPtr == itemPtr) {
		if (linePtr->outline.activeWidth>width) {
		    width = linePtr->outline.activeWidth;
		}
	} else if (state == TK_PATHSTATE_DISABLED) {
		if (linePtr->outline.disabledWidth>0) {
		    width = linePtr->outline.disabledWidth;
		}
	}
	intWidth = (int) (width + 0.5);
	if (intWidth < 1) {
	    intWidth = 1;
	}
	itemPtr->x1 -= intWidth; itemPtr->y1 -= intWidth;
	itemPtr->x2 += intWidth; itemPtr->y2 += intWidth;
	Tk_PathCanvasEventuallyRedraw(canvas, itemPtr->x1, itemPtr->y1,
		itemPtr->x2, itemPtr->y2);
    }
    ComputeLineBbox(canvas, linePtr);
}

/*
 *--------------------------------------------------------------
 *
 * LineToPoint --
 *
 *	Computes the distance from a given point to a given line, in canvas
 *	units.
 *
 * Results:
 *	The return value is 0 if the point whose x and y coordinates are
 *	pointPtr[0] and pointPtr[1] is inside the line. If the point isn't
 *	inside the line then the return value is the distance from the point
 *	to the line.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static double
LineToPoint(
    Tk_PathCanvas canvas,		/* Canvas containing item. */
    Tk_PathItem *itemPtr,		/* Item to check against point. */
    double *pointPtr)		/* Pointer to x and y coordinates. */
{
    Tk_PathState state = itemPtr->state;
    LineItem *linePtr = (LineItem *) itemPtr;
    double *coordPtr, *linePoints;
    double staticSpace[2*MAX_STATIC_POINTS];
    double poly[10];
    double bestDist, dist, width;
    int numPoints, count;
    int changedMiterToBevel;	/* Non-zero means that a mitered corner had to
				 * be treated as beveled after all because the
				 * angle was < 11 degrees. */

    bestDist = 1.0e36;

    /*
     * Handle smoothed lines by generating an expanded set of points against
     * which to do the check.
     */

    if (state == TK_PATHSTATE_NULL) {
	state = TkPathCanvasState(canvas);
    }

    width = linePtr->outline.width;
    if (((TkPathCanvas *)canvas)->currentItemPtr == itemPtr) {
	if (linePtr->outline.activeWidth>width) {
	    width = linePtr->outline.activeWidth;
	}
    } else if (state == TK_PATHSTATE_DISABLED) {
	if (linePtr->outline.disabledWidth>0) {
	    width = linePtr->outline.disabledWidth;
	}
    }

    if ((linePtr->smooth) && (linePtr->numPoints > 2)) {
	numPoints = linePtr->smooth->coordProc(canvas, NULL,
		linePtr->numPoints, linePtr->splineSteps, NULL, NULL);
	if (numPoints <= MAX_STATIC_POINTS) {
	    linePoints = staticSpace;
	} else {
	    linePoints = (double *) ckalloc((unsigned)
		    (2*numPoints*sizeof(double)));
	}
	numPoints = linePtr->smooth->coordProc(canvas, linePtr->coordPtr,
		linePtr->numPoints, linePtr->splineSteps, NULL, linePoints);
    } else {
	numPoints = linePtr->numPoints;
	linePoints = linePtr->coordPtr;
    }

    if (width < 1.0) {
	width = 1.0;
    }

    if (!numPoints || itemPtr->state == TK_PATHSTATE_HIDDEN) {
	return bestDist;
    } else if (numPoints == 1) {
	bestDist = hypot(linePoints[0]-pointPtr[0], linePoints[1]-pointPtr[1])
		- width/2.0;
	if (bestDist < 0) bestDist = 0;
	return bestDist;
    }

    /*
     * The overall idea is to iterate through all of the edges of the line,
     * computing a polygon for each edge and testing the point against that
     * polygon. In addition, there are additional tests to deal with rounded
     * joints and caps.
     */

    changedMiterToBevel = 0;
    for (count = numPoints, coordPtr = linePoints; count >= 2;
	    count--, coordPtr += 2) {
	/*
	 * If rounding is done around the first point then compute the
	 * distance between the point and the point.
	 */

	if (((linePtr->capStyle == CapRound) && (count == numPoints))
		|| ((linePtr->joinStyle == JoinRound)
			&& (count != numPoints))) {
	    dist = hypot(coordPtr[0] - pointPtr[0], coordPtr[1] - pointPtr[1])
		    - width/2.0;
	    if (dist <= 0.0) {
		bestDist = 0.0;
		goto done;
	    } else if (dist < bestDist) {
		bestDist = dist;
	    }
	}

	/*
	 * Compute the polygonal shape corresponding to this edge, consisting
	 * of two points for the first point of the edge and two points for
	 * the last point of the edge.
	 */

	if (count == numPoints) {
	    TkGetButtPoints(coordPtr+2, coordPtr, width,
		    linePtr->capStyle == CapProjecting, poly, poly+2);
	} else if ((linePtr->joinStyle == JoinMiter) && !changedMiterToBevel) {
	    poly[0] = poly[6];
	    poly[1] = poly[7];
	    poly[2] = poly[4];
	    poly[3] = poly[5];
	} else {
	    TkGetButtPoints(coordPtr+2, coordPtr, width, 0, poly, poly+2);

	    /*
	     * If this line uses beveled joints, then check the distance to a
	     * polygon comprising the last two points of the previous polygon
	     * and the first two from this polygon; this checks the wedges
	     * that fill the mitered joint.
	     */

	    if ((linePtr->joinStyle == JoinBevel) || changedMiterToBevel) {
		poly[8] = poly[0];
		poly[9] = poly[1];
		dist = TkPolygonToPoint(poly, 5, pointPtr);
		if (dist <= 0.0) {
		    bestDist = 0.0;
		    goto done;
		} else if (dist < bestDist) {
		    bestDist = dist;
		}
		changedMiterToBevel = 0;
	    }
	}
	if (count == 2) {
	    TkGetButtPoints(coordPtr, coordPtr+2, width,
		    linePtr->capStyle == CapProjecting, poly+4, poly+6);
	} else if (linePtr->joinStyle == JoinMiter) {
	    if (TkGetMiterPoints(coordPtr, coordPtr+2, coordPtr+4,
		    width, poly+4, poly+6) == 0) {
		changedMiterToBevel = 1;
		TkGetButtPoints(coordPtr, coordPtr+2, width, 0,
			poly+4, poly+6);
	    }
	} else {
	    TkGetButtPoints(coordPtr, coordPtr+2, width, 0,
		    poly+4, poly+6);
	}
	poly[8] = poly[0];
	poly[9] = poly[1];
	dist = TkPolygonToPoint(poly, 5, pointPtr);
	if (dist <= 0.0) {
	    bestDist = 0.0;
	    goto done;
	} else if (dist < bestDist) {
	    bestDist = dist;
	}
    }

    /*
     * If caps are rounded, check the distance to the cap around the final end
     * point of the line.
     */

    if (linePtr->capStyle == CapRound) {
	dist = hypot(coordPtr[0] - pointPtr[0], coordPtr[1] - pointPtr[1])
		- width/2.0;
	if (dist <= 0.0) {
	    bestDist = 0.0;
	    goto done;
	} else if (dist < bestDist) {
	    bestDist = dist;
	}
    }

    /*
     * If there are arrowheads, check the distance to the arrowheads.
     */

    if (linePtr->arrow != ARROWS_NONE) {
	if (linePtr->arrow != ARROWS_LAST) {
	    dist = TkPolygonToPoint(linePtr->firstArrowPtr, PTS_IN_ARROW,
		    pointPtr);
	    if (dist <= 0.0) {
		bestDist = 0.0;
		goto done;
	    } else if (dist < bestDist) {
		bestDist = dist;
	    }
	}
	if (linePtr->arrow != ARROWS_FIRST) {
	    dist = TkPolygonToPoint(linePtr->lastArrowPtr, PTS_IN_ARROW,
		    pointPtr);
	    if (dist <= 0.0) {
		bestDist = 0.0;
		goto done;
	    } else if (dist < bestDist) {
		bestDist = dist;
	    }
	}
    }

  done:
    if ((linePoints != staticSpace) && (linePoints != linePtr->coordPtr)) {
	ckfree((char *) linePoints);
    }
    return bestDist;
}

/*
 *--------------------------------------------------------------
 *
 * LineToArea --
 *
 *	This function is called to determine whether an item lies entirely
 *	inside, entirely outside, or overlapping a given rectangular area.
 *
 * Results:
 *	-1 is returned if the item is entirely outside the area, 0 if it
 *	overlaps, and 1 if it is entirely inside the given area.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
LineToArea(
    Tk_PathCanvas canvas,		/* Canvas containing item. */
    Tk_PathItem *itemPtr,		/* Item to check against line. */
    double *rectPtr)
{
    LineItem *linePtr = (LineItem *) itemPtr;
    double staticSpace[2*MAX_STATIC_POINTS];
    double *linePoints;
    int numPoints, result;
    double radius, width;
    Tk_PathState state = itemPtr->state;

    if (state == TK_PATHSTATE_NULL) {
	state = TkPathCanvasState(canvas);
    }
    width = linePtr->outline.width;
    if (((TkPathCanvas *)canvas)->currentItemPtr == itemPtr) {
	if (linePtr->outline.activeWidth>width) {
	    width = linePtr->outline.activeWidth;
	}
    } else if (state == TK_PATHSTATE_DISABLED) {
	if (linePtr->outline.disabledWidth>0) {
	    width = linePtr->outline.disabledWidth;
	}
    }

    radius = (width+1.0)/2.0;

    if ((state == TK_PATHSTATE_HIDDEN) || !linePtr->numPoints) {
	return -1;
    } else if (linePtr->numPoints == 1) {
	double oval[4];

	oval[0] = linePtr->coordPtr[0]-radius;
	oval[1] = linePtr->coordPtr[1]-radius;
	oval[2] = linePtr->coordPtr[0]+radius;
	oval[3] = linePtr->coordPtr[1]+radius;
	return TkOvalToArea(oval, rectPtr);
    }

    /*
     * Handle smoothed lines by generating an expanded set of points against
     * which to do the check.
     */

    if ((linePtr->smooth) && (linePtr->numPoints > 2)) {
	numPoints = linePtr->smooth->coordProc(canvas, NULL,
		linePtr->numPoints, linePtr->splineSteps, NULL, NULL);
	if (numPoints <= MAX_STATIC_POINTS) {
	    linePoints = staticSpace;
	} else {
	    linePoints = (double *) ckalloc((unsigned)
		    (2*numPoints*sizeof(double)));
	}
	numPoints = linePtr->smooth->coordProc(canvas, linePtr->coordPtr,
		linePtr->numPoints, linePtr->splineSteps, NULL, linePoints);
    } else {
	numPoints = linePtr->numPoints;
	linePoints = linePtr->coordPtr;
    }

    /*
     * Check the segments of the line.
     */

     if (width < 1.0) {
	width = 1.0;
    }

    result = TkThickPolyLineToArea(linePoints, numPoints,
	    width, linePtr->capStyle, linePtr->joinStyle,
	    rectPtr);
    if (result == 0) {
	goto done;
    }

    /*
     * Check arrowheads, if any.
     */

    if (linePtr->arrow != ARROWS_NONE) {
	if (linePtr->arrow != ARROWS_LAST) {
	    if (TkPolygonToArea(linePtr->firstArrowPtr, PTS_IN_ARROW,
		    rectPtr) != result) {
		result = 0;
		goto done;
	    }
	}
	if (linePtr->arrow != ARROWS_FIRST) {
	    if (TkPolygonToArea(linePtr->lastArrowPtr, PTS_IN_ARROW,
		    rectPtr) != result) {
		result = 0;
		goto done;
	    }
	}
    }

  done:
    if ((linePoints != staticSpace) && (linePoints != linePtr->coordPtr)) {
	ckfree((char *) linePoints);
    }
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * ScaleLine --
 *
 *	This function is invoked to rescale a line item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The line referred to by itemPtr is rescaled so that the following
 *	transformation is applied to all point coordinates:
 *		x' = originX + scaleX*(x-originX)
 *		y' = originY + scaleY*(y-originY)
 *
 *--------------------------------------------------------------
 */

static void
ScaleLine(
    Tk_PathCanvas canvas,		/* Canvas containing line. */
    Tk_PathItem *itemPtr,		/* Line to be scaled. */
    int compensate,			/* Unused. */
    double originX, double originY,
				/* Origin about which to scale rect. */
    double scaleX,		/* Amount to scale in X direction. */
    double scaleY)		/* Amount to scale in Y direction. */
{
    LineItem *linePtr = (LineItem *) itemPtr;
    double *coordPtr;
    int i;

    /*
     * Delete any arrowheads before scaling all the points (so that the
     * end-points of the line get restored).
     */

    if (linePtr->firstArrowPtr != NULL) {
	linePtr->coordPtr[0] = linePtr->firstArrowPtr[0];
	linePtr->coordPtr[1] = linePtr->firstArrowPtr[1];
	ckfree((char *) linePtr->firstArrowPtr);
	linePtr->firstArrowPtr = NULL;
    }
    if (linePtr->lastArrowPtr != NULL) {
	int i;

	i = 2*(linePtr->numPoints-1);
	linePtr->coordPtr[i] = linePtr->lastArrowPtr[0];
	linePtr->coordPtr[i+1] = linePtr->lastArrowPtr[1];
	ckfree((char *) linePtr->lastArrowPtr);
	linePtr->lastArrowPtr = NULL;
    }
    for (i = 0, coordPtr = linePtr->coordPtr; i < linePtr->numPoints;
	    i++, coordPtr += 2) {
	coordPtr[0] = originX + scaleX*(*coordPtr - originX);
	coordPtr[1] = originY + scaleY*(coordPtr[1] - originY);
    }
    if (linePtr->arrow != ARROWS_NONE) {
	ConfigureArrows(canvas, linePtr);
    }
    ComputeLineBbox(canvas, linePtr);
}

/*
 *--------------------------------------------------------------
 *
 * GetLineIndex --
 *
 *	Parse an index into a line item and return either its value or an
 *	error.
 *
 * Results:
 *	A standard Tcl result. If all went well, then *indexPtr is filled in
 *	with the index (into itemPtr) corresponding to string. Otherwise an
 *	error message is left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
GetLineIndex(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tk_PathCanvas canvas,		/* Canvas containing item. */
    Tk_PathItem *itemPtr,		/* Item for which the index is being
				 * specified. */
    Tcl_Obj *obj,		/* Specification of a particular coord in
				 * itemPtr's line. */
    int *indexPtr)		/* Where to store converted index. */
{
    LineItem *linePtr = (LineItem *) itemPtr;
    Tcl_Size length;
    char *string = Tcl_GetStringFromObj(obj, &length);

    if (string[0] == 'e') {
	if (strncmp(string, "end", (unsigned) length) == 0) {
	    *indexPtr = 2*linePtr->numPoints;
	} else {
	    /*
	     * Some of the paths here leave messages in interp->result, so we
	     * have to clear it out before storing our own message.
	     */

	badIndex:
	    Tcl_SetResult(interp, NULL, TCL_STATIC);
	    Tcl_AppendResult(interp, "bad index \"", string, "\"", NULL);
	    return TCL_ERROR;
	}
    } else if (string[0] == '@') {
	int i;
	double x ,y, bestDist, dist, *coordPtr;
	char *end, *p;

	p = string+1;
	x = strtod(p, &end);
	if ((end == p) || (*end != ',')) {
	    goto badIndex;
	}
	p = end+1;
	y = strtod(p, &end);
	if ((end == p) || (*end != 0)) {
	    goto badIndex;
	}
	bestDist = 1.0e36;
	coordPtr = linePtr->coordPtr;
	*indexPtr = 0;
	for (i=0; i<linePtr->numPoints; i++) {
	    dist = hypot(coordPtr[0] - x, coordPtr[1] - y);
	    if (dist<bestDist) {
		bestDist = dist;
		*indexPtr = 2*i;
	    }
	    coordPtr += 2;
	}
    } else {
	if (Tcl_GetIntFromObj(interp, obj, indexPtr) != TCL_OK) {
	    goto badIndex;
	}
	*indexPtr &= -2;		/* if index is odd, make it even */
	if (*indexPtr < 0){
	    *indexPtr = 0;
	} else if (*indexPtr > (2*linePtr->numPoints)) {
	    *indexPtr = (2*linePtr->numPoints);
	}
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * TranslateLine --
 *
 *	This function is called to move a line by a given amount.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The position of the line is offset by (xDelta, yDelta), and the
 *	bounding box is updated in the generic part of the item structure.
 *
 *--------------------------------------------------------------
 */

static void
TranslateLine(
    Tk_PathCanvas canvas,		/* Canvas containing item. */
    Tk_PathItem *itemPtr,		/* Item that is being moved. */
    int compensate,			/* Unused. */
    double deltaX, double deltaY)
				/* Amount by which item is to be moved. */
{
    LineItem *linePtr = (LineItem *) itemPtr;
    double *coordPtr;
    int i;

    for (i = 0, coordPtr = linePtr->coordPtr; i < linePtr->numPoints;
	    i++, coordPtr += 2) {
	coordPtr[0] += deltaX;
	coordPtr[1] += deltaY;
    }
    if (linePtr->firstArrowPtr != NULL) {
	for (i = 0, coordPtr = linePtr->firstArrowPtr; i < PTS_IN_ARROW;
		i++, coordPtr += 2) {
	    coordPtr[0] += deltaX;
	    coordPtr[1] += deltaY;
	}
    }
    if (linePtr->lastArrowPtr != NULL) {
	for (i = 0, coordPtr = linePtr->lastArrowPtr; i < PTS_IN_ARROW;
		i++, coordPtr += 2) {
	    coordPtr[0] += deltaX;
	    coordPtr[1] += deltaY;
	}
    }
    ComputeLineBbox(canvas, linePtr);
}

/*
 *--------------------------------------------------------------
 *
 * ArrowShapeOptionSetProc, ... --
 *
 *	This function is invoked during option processing to handle "-arrowshape"
 *	options for canvas items.
 *
 * Results:
 *	A standard Tcl return value.
 *
 * Side effects:
 *	.
 *
 *--------------------------------------------------------------
 */

static int
ArrowShapeOptionSetProc(
    ClientData clientData,
    Tcl_Interp *interp,	    /* Current interp; may be used for errors. */
    Tk_Window tkwin,	    /* Window for which option is being set. */
    Tcl_Obj **value,	    /* Pointer to the pointer to the value object.
                             * We use a pointer to the pointer because
                             * we may need to return a value (NULL). */
    char *recordPtr,	    /* Pointer to storage for the widget record. */
    Tcl_Size internalOffset,/* Offset within *recordPtr at which the
                               internal value is to be stored. */
    char *oldInternalPtr,   /* Pointer to storage for the old value. */
    int flags)		    /* Flags for the option, set Tk_SetOptions. */
{
    char *internalPtr;	    /* Points to location in record where
                             * internal representation of value should
                             * be stored, or NULL. */
    Tcl_Obj *valuePtr;
    Tcl_Obj **objv;
    Tcl_Size objc;
    double dvalue;

    valuePtr = *value;
    if (internalOffset >= 0) {
        internalPtr = recordPtr + internalOffset;
    } else {
        internalPtr = NULL;
    }
    if ((flags & TK_OPTION_NULL_OK) && ObjectIsEmpty(valuePtr)) {
	valuePtr = NULL;
    }
    if (internalPtr != NULL) {
	if (valuePtr != NULL) {
	    if (Tcl_ListObjGetElements(interp, valuePtr, &objc,
		    (Tcl_Obj ***) &objv) != TCL_OK) {
		goto error;
	    }
	    if (objc != 3) {
		goto error;
	    }
	    /*
	     * We just verify that they are OK.
	     * No internal storage!
	     */
	    if ((Tk_GetMMFromObj(interp, tkwin, objv[0], &dvalue) != TCL_OK)
		    || (Tk_GetMMFromObj(interp, tkwin, objv[1], &dvalue)
			!= TCL_OK)
		    || (Tk_GetMMFromObj(interp, tkwin, objv[2], &dvalue)
			!= TCL_OK)) {
		goto error;
	    }
        }
    }
    return TCL_OK;

error:
    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
	    "bad arrow shape \"", Tcl_GetString(valuePtr),
	    "\": must be list with three numbers", NULL);
    return TCL_ERROR;
}

static Tcl_Obj *
ArrowShapeOptionGetProc(
    ClientData clientData,
    Tk_Window tkwin,
    char *recordPtr,		/* Pointer to widget record. */
    Tcl_Size internalOffset)	/* Offset within *recordPtr containing the
				 * value. */
{
    return (Tcl_Obj *)(recordPtr + internalOffset);
}

/*
 *--------------------------------------------------------------
 *
 * ConfigureArrows --
 *
 *	If arrowheads have been requested for a line, this function makes
 *	arrangements for the arrowheads.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 * Side effects:
 *	Information in linePtr is set up for one or two arrowheads. The
 *	firstArrowPtr and lastArrowPtr polygons are allocated and initialized,
 *	if need be, and the end points of the line are adjusted so that a
 *	thick line doesn't stick out past the arrowheads.
 *
 *--------------------------------------------------------------
 */

static int
ConfigureArrows(
    Tk_PathCanvas canvas,	/* Canvas in which arrows will be displayed
				 * (interp and tkwin fields are needed). */
    LineItem *linePtr)		/* Item to configure for arrows. */
{
    double *poly, *coordPtr;
    double dx, dy, length, sinTheta, cosTheta, temp;
    double fracHeight;		/* Line width as fraction of arrowhead
				 * width. */
    double backup;		/* Distance to backup end points so the line
				 * ends in the middle of the arrowhead. */
    double vertX, vertY;	/* Position of arrowhead vertex. */
    double shapeA, shapeB, shapeC;
				/* Adjusted coordinates (see explanation
				 * below). */
    double width;
    Tk_PathState state = linePtr->header.state;
    Tk_Window tkwin;
    Tcl_Obj *objPtr;

    tkwin = Tk_PathCanvasTkwin(canvas);

    if (linePtr->numPoints <2) {
	return TCL_OK;
    }

    if (state == TK_PATHSTATE_NULL) {
	state = TkPathCanvasState(canvas);
    }

    width = linePtr->outline.width;
    if (((TkPathCanvas *)canvas)->currentItemPtr == (Tk_PathItem *)linePtr) {
	if (linePtr->outline.activeWidth>width) {
	    width = linePtr->outline.activeWidth;
	}
    } else if (state == TK_PATHSTATE_DISABLED) {
	if (linePtr->outline.disabledWidth>0) {
	    width = linePtr->outline.disabledWidth;
	}
    }

    /*
     * The code below makes a tiny increase in the shape parameters for the
     * line. This is a bit of a hack, but it seems to result in displays that
     * more closely approximate the specified parameters. Without the
     * adjustment, the arrows come out smaller than expected.
     */

    Tcl_ListObjIndex(NULL, linePtr->arrowShapeObj, 0, &objPtr);
    Tk_GetMMFromObj(NULL, tkwin, objPtr, &shapeA);
    shapeA *= ((TkPathCanvas *)canvas)->pixelsPerMM;
    shapeA += 0.001;
    Tcl_ListObjIndex(NULL, linePtr->arrowShapeObj, 1, &objPtr);
    Tk_GetMMFromObj(NULL, tkwin, objPtr, &shapeB);
    shapeB *= ((TkPathCanvas *)canvas)->pixelsPerMM;
    shapeB += 0.001;
    Tcl_ListObjIndex(NULL, linePtr->arrowShapeObj, 2, &objPtr);
    Tk_GetMMFromObj(NULL, tkwin, objPtr, &shapeC);
    shapeC *= ((TkPathCanvas *)canvas)->pixelsPerMM;
    shapeC += 0.001 + width/2.0;

    /*
     * If there's an arrowhead on the first point of the line, compute its
     * polygon and adjust the first point of the line so that the line doesn't
     * stick out past the leading edge of the arrowhead.
     */

    fracHeight = (width/2.0)/shapeC;
    backup = fracHeight*shapeB + shapeA*(1.0 - fracHeight)/2.0;
    if (linePtr->arrow != ARROWS_LAST) {
	poly = linePtr->firstArrowPtr;
	if (poly == NULL) {
	    poly = (double *) ckalloc((unsigned)
		    (2*PTS_IN_ARROW*sizeof(double)));
	    poly[0] = poly[10] = linePtr->coordPtr[0];
	    poly[1] = poly[11] = linePtr->coordPtr[1];
	    linePtr->firstArrowPtr = poly;
	}
	dx = poly[0] - linePtr->coordPtr[2];
	dy = poly[1] - linePtr->coordPtr[3];
	length = hypot(dx, dy);
	if (length == 0) {
	    sinTheta = cosTheta = 0.0;
	} else {
	    sinTheta = dy/length;
	    cosTheta = dx/length;
	}
	vertX = poly[0] - shapeA*cosTheta;
	vertY = poly[1] - shapeA*sinTheta;
	temp = shapeC*sinTheta;
	poly[2] = poly[0] - shapeB*cosTheta + temp;
	poly[8] = poly[2] - 2*temp;
	temp = shapeC*cosTheta;
	poly[3] = poly[1] - shapeB*sinTheta - temp;
	poly[9] = poly[3] + 2*temp;
	poly[4] = poly[2]*fracHeight + vertX*(1.0-fracHeight);
	poly[5] = poly[3]*fracHeight + vertY*(1.0-fracHeight);
	poly[6] = poly[8]*fracHeight + vertX*(1.0-fracHeight);
	poly[7] = poly[9]*fracHeight + vertY*(1.0-fracHeight);

	/*
	 * Polygon done. Now move the first point towards the second so that
	 * the corners at the end of the line are inside the arrowhead.
	 */

	linePtr->coordPtr[0] = poly[0] - backup*cosTheta;
	linePtr->coordPtr[1] = poly[1] - backup*sinTheta;
    }

    /*
     * Similar arrowhead calculation for the last point of the line.
     */

    if (linePtr->arrow != ARROWS_FIRST) {
	coordPtr = linePtr->coordPtr + 2*(linePtr->numPoints-2);
	poly = linePtr->lastArrowPtr;
	if (poly == NULL) {
	    poly = (double *)
		    ckalloc((unsigned) (2*PTS_IN_ARROW*sizeof(double)));
	    poly[0] = poly[10] = coordPtr[2];
	    poly[1] = poly[11] = coordPtr[3];
	    linePtr->lastArrowPtr = poly;
	}
	dx = poly[0] - coordPtr[0];
	dy = poly[1] - coordPtr[1];
	length = hypot(dx, dy);
	if (length == 0) {
	    sinTheta = cosTheta = 0.0;
	} else {
	    sinTheta = dy/length;
	    cosTheta = dx/length;
	}
	vertX = poly[0] - shapeA*cosTheta;
	vertY = poly[1] - shapeA*sinTheta;
	temp = shapeC * sinTheta;
	poly[2] = poly[0] - shapeB*cosTheta + temp;
	poly[8] = poly[2] - 2*temp;
	temp = shapeC * cosTheta;
	poly[3] = poly[1] - shapeB*sinTheta - temp;
	poly[9] = poly[3] + 2*temp;
	poly[4] = poly[2]*fracHeight + vertX*(1.0-fracHeight);
	poly[5] = poly[3]*fracHeight + vertY*(1.0-fracHeight);
	poly[6] = poly[8]*fracHeight + vertX*(1.0-fracHeight);
	poly[7] = poly[9]*fracHeight + vertY*(1.0-fracHeight);
	coordPtr[2] = poly[0] - backup*cosTheta;
	coordPtr[3] = poly[1] - backup*sinTheta;
    }

    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * LineToPdf --
 *
 *	This function is called to generate Pdf for line items.
 *
 * Results:
 *	The return value is a standard Tcl result. If an error occurs in
 *	generating Pdf then an error message is left in the interp's
 *	result, replacing whatever used to be there. If no error occurs, then
 *	Pdf for the item is appended to the result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
LineToPdf(
    Tcl_Interp *interp,		/* Leave Pdf or error message here. */
    Tk_PathCanvas canvas,	/* Information about overall canvas. */
    Tk_PathItem *itemPtr,	/* Item for which Pdf is wanted. */
    Tcl_Size objc,              /* Number of arguments. */
    Tcl_Obj *const objv[],      /* Argument list. */
    int prepass)		/* 1 means this is a prepass to collect font
				 * information; 0 means final Pdf is
				 * being created. */
{
    return TCL_OK;
}

#ifndef TKP_NO_POSTSCRIPT
/*
 *--------------------------------------------------------------
 *
 * LineToPostscript --
 *
 *	This function is called to generate Postscript for line items.
 *
 * Results:
 *	The return value is a standard Tcl result. If an error occurs in
 *	generating Postscript then an error message is left in the interp's
 *	result, replacing whatever used to be there. If no error occurs, then
 *	Postscript for the item is appended to the result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
LineToPostscript(
    Tcl_Interp *interp,		/* Leave Postscript or error message here. */
    Tk_PathCanvas canvas,	/* Information about overall canvas. */
    Tk_PathItem *itemPtr,	/* Item for which Postscript is wanted. */
    int prepass)		/* 1 means this is a prepass to collect font
				 * information; 0 means final Postscript is
				 * being created. */
{
    LineItem *linePtr = (LineItem *) itemPtr;
    char buffer[200 + TCL_INTEGER_SPACE];
    const char *style;

    double width;
    XColor *color;
    Pixmap stipple;
    Tk_PathState state = itemPtr->state;

    if (state == TK_PATHSTATE_NULL) {
	state = TkPathCanvasState(canvas);
    }

    width = linePtr->outline.width;
    color = linePtr->outline.color;
    stipple = linePtr->outline.stipple;
    if (((TkPathCanvas *)canvas)->currentItemPtr == itemPtr) {
	if (linePtr->outline.activeWidth>width) {
	    width = linePtr->outline.activeWidth;
	}
	if (linePtr->outline.activeColor!=NULL) {
	    color = linePtr->outline.activeColor;
	}
	if (linePtr->outline.activeStipple!=None) {
	    stipple = linePtr->outline.activeStipple;
	}
    } else if (state == TK_PATHSTATE_DISABLED) {
	if (linePtr->outline.disabledWidth>0) {
	    width = linePtr->outline.disabledWidth;
	}
	if (linePtr->outline.disabledColor!=NULL) {
	    color = linePtr->outline.disabledColor;
	}
	if (linePtr->outline.disabledStipple!=None) {
	    stipple = linePtr->outline.disabledStipple;
	}
    }

    if (color == NULL || linePtr->numPoints<1 || linePtr->coordPtr==NULL) {
	return TCL_OK;
    }

    if (linePtr->numPoints==1) {
	sprintf(buffer, "%.15g %.15g translate %.15g %.15g",
		linePtr->coordPtr[0], Tk_PathCanvasPsY(canvas, linePtr->coordPtr[1]),
		width/2.0, width/2.0);
	Tcl_AppendResult(interp, "matrix currentmatrix\n",buffer,
		" scale 1 0 moveto 0 0 1 0 360 arc\nsetmatrix\n", NULL);
	if (Tk_PathCanvasPsColor(interp, canvas, color) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (stipple != None) {
	    Tcl_AppendResult(interp, "clip ", NULL);
	    if (Tk_PathCanvasPsStipple(interp, canvas, stipple) != TCL_OK) {
		return TCL_ERROR;
	    }
	} else {
	    Tcl_AppendResult(interp, "fill\n", NULL);
	}
	return TCL_OK;
    }
    /*
     * Generate a path for the line's center-line (do this differently for
     * straight lines and smoothed lines).
     */

    if ((!linePtr->smooth) || (linePtr->numPoints < 3)) {
	Tk_PathCanvasPsPath(interp, canvas, linePtr->coordPtr, linePtr->numPoints);
    } else {
	if ((stipple == None) && linePtr->smooth->postscriptProc) {
	    linePtr->smooth->postscriptProc(interp, canvas,
		    linePtr->coordPtr, linePtr->numPoints, linePtr->splineSteps);
	} else {
	    /*
	     * Special hack: Postscript printers don't appear to be able to
	     * turn a path drawn with "curveto"s into a clipping path without
	     * exceeding resource limits, so TkPathMakeBezierPostscript won't work
	     * for stippled curves. Instead, generate all of the intermediate
	     * points here and output them into the Postscript file with
	     * "lineto"s instead.
	     */

	    double staticPoints[2*MAX_STATIC_POINTS];
	    double *pointPtr;
	    int numPoints;

	    numPoints = linePtr->smooth->coordProc(canvas, NULL,
		    linePtr->numPoints, linePtr->splineSteps, NULL, NULL);
	    pointPtr = staticPoints;
	    if (numPoints > MAX_STATIC_POINTS) {
		pointPtr = (double *) ckalloc((unsigned)
			(numPoints * 2 * sizeof(double)));
	    }
	    numPoints = linePtr->smooth->coordProc(canvas, linePtr->coordPtr,
		    linePtr->numPoints, linePtr->splineSteps, NULL, pointPtr);
	    Tk_PathCanvasPsPath(interp, canvas, pointPtr, numPoints);
	    if (pointPtr != staticPoints) {
		ckfree((char *) pointPtr);
	    }
	}
    }

    /*
     * Set other line-drawing parameters and stroke out the line.
     */

    style = "0 setlinecap\n";
    if (linePtr->capStyle == CapRound) {
	style = "1 setlinecap\n";
    } else if (linePtr->capStyle == CapProjecting) {
	style = "2 setlinecap\n";
    }
    Tcl_AppendResult(interp, style, NULL);
    style = "0 setlinejoin\n";
    if (linePtr->joinStyle == JoinRound) {
	style = "1 setlinejoin\n";
    } else if (linePtr->joinStyle == JoinBevel) {
	style = "2 setlinejoin\n";
    }
    Tcl_AppendResult(interp, style, NULL);

    if (Tk_PathCanvasPsOutline(canvas, itemPtr, &(linePtr->outline)) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Output polygons for the arrowheads, if there are any.
     */

    if (linePtr->firstArrowPtr != NULL) {
	if (stipple != None) {
	    Tcl_AppendResult(interp, "grestore gsave\n", NULL);
	}
	if (ArrowheadPostscript(interp, canvas, linePtr,
		linePtr->firstArrowPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (linePtr->lastArrowPtr != NULL) {
	if (stipple != None) {
	    Tcl_AppendResult(interp, "grestore gsave\n", NULL);
	}
	if (ArrowheadPostscript(interp, canvas, linePtr,
		linePtr->lastArrowPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * ArrowheadPostscript --
 *
 *	This function is called to generate Postscript for an arrowhead for a
 *	line item.
 *
 * Results:
 *	The return value is a standard Tcl result. If an error occurs in
 *	generating Postscript then an error message is left in the interp's
 *	result, replacing whatever used to be there. If no error occurs, then
 *	Postscript for the arrowhead is appended to the result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
ArrowheadPostscript(
    Tcl_Interp *interp,		/* Leave Postscript or error message here. */
    Tk_PathCanvas canvas,		/* Information about overall canvas. */
    LineItem *linePtr,		/* Line item for which Postscript is being
				 * generated. */
    double *arrowPtr)		/* Pointer to first of five points describing
				 * arrowhead polygon. */
{
    Pixmap stipple;
    Tk_PathState state = linePtr->header.state;

    if (state == TK_PATHSTATE_NULL) {
	state = TkPathCanvasState(canvas);
    }

    stipple = linePtr->outline.stipple;
    if (((TkPathCanvas *)canvas)->currentItemPtr == (Tk_PathItem *)linePtr) {
	if (linePtr->outline.activeStipple!=None) {
	    stipple = linePtr->outline.activeStipple;
	}
    } else if (state == TK_PATHSTATE_DISABLED) {
	if (linePtr->outline.activeStipple!=None) {
	    stipple = linePtr->outline.disabledStipple;
	}
    }

    Tk_PathCanvasPsPath(interp, canvas, arrowPtr, PTS_IN_ARROW);
    if (stipple != None) {
	Tcl_AppendResult(interp, "clip ", NULL);
	if (Tk_PathCanvasPsStipple(interp, canvas, stipple) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	Tcl_AppendResult(interp, "fill\n", NULL);
    }
    return TCL_OK;
}
#endif

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
