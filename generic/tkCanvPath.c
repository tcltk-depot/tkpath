/*
 * tkCanvPath.c --
 *
 *  This file implements a path canvas item modelled after its
 *  SVG counterpart. See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2005-2008  Mats Bengtsson
 *
 */

#include "tkIntPath.h"
#include "tkCanvPathUtil.h"
#include "tkCanvArrow.h"
#include "tkpCanvas.h"
#include "tkPathStyle.h"

/* Values for the PathItem's flag. */

enum {
    kPathItemNeedNewNormalizedPath                     = (1L << 0)
};

/*
 * The structure below defines the record for each path item.
 */

typedef struct PathItem  {
    Tk_PathItemEx headerEx; /* Generic stuff that's the same for all
                             * path types.  MUST BE FIRST IN STRUCTURE. */
    Tcl_Obj *pathObjPtr;    /* The object containing the path definition. */
    int pathLen;
    Tcl_Obj *normPathObjPtr;/* The object containing the normalized path. */
    PathAtom *atomPtr;
    int maxNumSegments;     /* Max number of straight segments (for subpath)
                             * needed for Area and Point functions. */
    ArrowDescr startarrow;
    ArrowDescr endarrow;
    long flags;             /* Various flags, see enum. */
} PathItem;


/*
 * Prototypes for procedures defined in this file:
 */

static void	ComputePathBbox(Tk_PathCanvas canvas, PathItem *pathPtr);
static int	ConfigurePath(Tcl_Interp *interp, Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, Tcl_Size objc,
                        Tcl_Obj *const objv[], int flags);
static int	CreatePath(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, struct Tk_PathItem *itemPtr,
                        Tcl_Size objc, Tcl_Obj *const objv[]);
static int	ProcessPath(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
                        Tcl_Size objc, Tcl_Obj *const objv[]);
static int	PathCoords(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
                        Tcl_Size objc, Tcl_Obj *const objv[]);
static void	DeletePath(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, Display *display);
static void	DisplayPath(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, Display *display, Drawable dst,
                        int x, int y, int width, int height);
static void	PathBbox(Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
		    int mask);
static int	PathToArea(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double *areaPtr);
static int	PathToPdf(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, Tk_PathItem *itemPtr, Tcl_Size objc,
			Tcl_Obj *const objv[], int prepass);
static double	PathToPoint(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double *coordPtr);
#ifndef TKP_NO_POSTSCRIPT
static int	PathToPostscript(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
			int prepass);
#endif
static void	ScalePath(Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
			int compensate, double originX, double originY,
			double scaleX, double scaleY);
static void	TranslatePath(Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
			int compensate, double deltaX, double deltaY);
static int      ConfigureArrows(Tk_PathCanvas canvas, PathItem *pathPtr);

/* Support functions. */

static int		GetSubpathMaxNumSegments(PathAtom *atomPtr);


PATH_STYLE_CUSTOM_OPTION_RECORDS
PATH_CUSTOM_OPTION_TAGS
PATH_OPTION_STRING_TABLES_FILL
PATH_OPTION_STRING_TABLES_STROKE
PATH_OPTION_STRING_TABLES_STATE

static Tk_OptionSpec optionSpecs[] = {
    PATH_OPTION_SPEC_CORE(Tk_PathItemEx),
    PATH_OPTION_SPEC_PARENT,
    PATH_OPTION_SPEC_STYLE_FILL(Tk_PathItemEx, ""),
    PATH_OPTION_SPEC_STYLE_MATRIX(Tk_PathItemEx),
    PATH_OPTION_SPEC_STYLE_STROKE(Tk_PathItemEx, "black"),
    PATH_OPTION_SPEC_STARTARROW_GRP(PathItem),
    PATH_OPTION_SPEC_ENDARROW_GRP(PathItem),
    PATH_OPTION_SPEC_END
};

/*
 * The structures below defines the 'path' item type by means
 * of procedures that can be invoked by generic item code.
 */

Tk_PathItemType tkpPathType = {
    "path",			/* name */
    sizeof(PathItem),		/* itemSize */
    CreatePath,			/* createProc */
    optionSpecs,		/* optionSpecs */
    ConfigurePath,		/* configureProc */
    PathCoords,			/* coordProc */
    DeletePath,			/* deleteProc */
    DisplayPath,		/* displayProc */
    0,                          /* flags */
    PathBbox,                   /* bboxProc */
    PathToPoint,		/* pointProc */
    PathToArea,			/* areaProc */
#ifndef TKP_NO_POSTSCRIPT
    PathToPostscript,		/* postscriptProc */
#endif
    PathToPdf,			/* pdfProc */
    ScalePath,			/* scaleProc */
    TranslatePath,		/* translateProc */
    (Tk_PathItemIndexProc *) NULL,      /* indexProc */
    (Tk_PathItemCursorProc *) NULL,	/* icursorProc */
    (Tk_PathItemSelectionProc *) NULL,	/* selectionProc */
    (Tk_PathItemInsertProc *) NULL,	/* insertProc */
    (Tk_PathItemDCharsProc *) NULL,	/* dTextProc */
    (Tk_PathItemType *) NULL,		/* nextPtr */
    1,				/* isPathType */
};

/* Be sure rect is not empty (see above) before doing this. */
static void
NormalizePathRect(PathRect *r)
{
    double min, max;

    min = MIN(r->x1, r->x2);
    max = MAX(r->x1, r->x2);
    r->x1 = min;
    r->x2 = max;
    min = MIN(r->y1, r->y2);
    max = MAX(r->y1, r->y2);
    r->y1 = min;
    r->y2 = max;
}

/*
 +++ This starts the canvas item part +++
 */

/*
 *--------------------------------------------------------------
 *
 * CreatePath --
 *
 *	This procedure is invoked to create a new line item in
 *	a canvas.
 *
 * Results:
 *	A standard Tcl return value.  If an error occurred in
 *	creating the item, then an error message is left in
 *	the interp's result;  in this case itemPtr is left uninitialized,
 *	so it can be safely freed by the caller.
 *
 * Side effects:
 *	A new line item is created.
 *
 *--------------------------------------------------------------
 */

static int
CreatePath(
    Tcl_Interp *interp, 	/* Used for error reporting. */
    Tk_PathCanvas canvas, 	/* Canvas containing item. */
    Tk_PathItem *itemPtr, 	/* Item to create. */
    Tcl_Size objc,                   /* Number of elements in objv.  */
    Tcl_Obj *const objv[])	/* Arguments describing the item. */
{
    PathItem *pathPtr = (PathItem *) itemPtr;
    Tk_PathItemEx *itemExPtr = &pathPtr->headerEx;
    Tk_OptionTable optionTable;

    if (objc == 0) {
        Tcl_Panic("canvas did not pass any coords\n");
    }

    /*
     * Carry out initialization that is needed to set defaults and to
     * allow proper cleanup after errors during the the remainder of
     * this procedure.
     */
    TkPathInitStyle(&itemExPtr->style);
    itemExPtr->canvas = canvas;
    itemExPtr->styleObj = NULL;
    itemExPtr->styleInst = NULL;
    pathPtr->pathObjPtr = NULL;
    pathPtr->pathLen = 0;
    pathPtr->normPathObjPtr = NULL;
    pathPtr->atomPtr = NULL;
    itemPtr->bbox = NewEmptyPathRect();
    itemPtr->totalBbox = NewEmptyPathRect();
    pathPtr->maxNumSegments = 0;
    TkPathArrowDescrInit(&pathPtr->startarrow);
    TkPathArrowDescrInit(&pathPtr->endarrow);
    pathPtr->flags = 0L;

    /* Forces a computation of the normalized path in PathCoords. */
    pathPtr->flags |= kPathItemNeedNewNormalizedPath;

    optionTable = Tk_CreateOptionTable(interp, optionSpecs);
    itemPtr->optionTable = optionTable;
    if (Tk_InitOptions(interp, (char *) pathPtr, optionTable,
	    Tk_PathCanvasTkwin(canvas)) != TCL_OK) {
        goto error;
    }

    /*
     * The first argument must be the path definition list.
     */

    if (ProcessPath(interp, canvas, itemPtr, 1, objv) != TCL_OK) {
        goto error;
    }
    if (ConfigurePath(interp, canvas, itemPtr, objc-1, objv+1, 0) == TCL_OK) {
        return TCL_OK;
    }

    error:
    /*
     * NB: We must unlink the item here since the TkPathCanvasItemExConfigure()
     *     link it to the root by default.
     */
    TkPathCanvasItemDetach(itemPtr);
    DeletePath(canvas, itemPtr, Tk_Display(Tk_PathCanvasTkwin(canvas)));
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * ProcessPath --
 *
 *	Does the main job of processing the drawing path in 'PathCoords'
 *      but doesn't do the bbox calculation since this cannot be done
 *      before we have callaed 'ConfigurePath' because we need
 *      the inherited style.
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
ProcessPath(
    Tcl_Interp *interp,     /* Used for error reporting. */
    Tk_PathCanvas canvas,   /* Canvas containing item. */
    Tk_PathItem *itemPtr,   /* Item whose coordinates are to be
                             * read or modified. */
    Tcl_Size objc,
    Tcl_Obj *const objv[])  /*  */
{
    PathItem *pathPtr = (PathItem *) itemPtr;
    PathAtom *atomPtr = NULL;
    Tcl_Size len;
    int result;

    if (objc == 0) {
        /* @@@ We have an option here if to return the normalized or original path. */
#if 0
        Tcl_SetObjResult(interp, pathPtr->pathObjPtr);
#endif

        /* We may need to recompute the normalized path from the atoms. */
        if (pathPtr->flags & kPathItemNeedNewNormalizedPath) {
            if (pathPtr->normPathObjPtr != NULL) {
                Tcl_DecrRefCount(pathPtr->normPathObjPtr);
            }
            TkPathNormalize(interp, pathPtr->atomPtr, &(pathPtr->normPathObjPtr));
            Tcl_IncrRefCount(pathPtr->normPathObjPtr);
            pathPtr->flags &= ~kPathItemNeedNewNormalizedPath;
        }
        Tcl_SetObjResult(interp, pathPtr->normPathObjPtr);
        return TCL_OK;
    } else if (objc == 1) {
        result = TkPathParseToAtoms(interp, objv[0], &atomPtr, &len);
        if (result == TCL_OK) {

            /* Free any old atoms. */
            if (pathPtr->atomPtr != NULL) {
                TkPathFreeAtoms(pathPtr->atomPtr);
            }
            pathPtr->atomPtr = atomPtr;
            pathPtr->pathLen = len;
            if (pathPtr->pathObjPtr != NULL) {
		Tcl_DecrRefCount(pathPtr->pathObjPtr);
	    }
            pathPtr->pathObjPtr = objv[0];
            pathPtr->maxNumSegments = GetSubpathMaxNumSegments(atomPtr);
            Tcl_IncrRefCount(pathPtr->pathObjPtr);
            pathPtr->flags |= kPathItemNeedNewNormalizedPath;
        }
        return result;
    } else {
        Tcl_WrongNumArgs(interp, 0, objv, "pathName coords id ?pathSpec?");
        return TCL_ERROR;
    }
}

/*
 *--------------------------------------------------------------
 *
 * PathCoords --
 *
 *	This procedure is invoked to process the "coords" widget
 *	command on lines.  See the user documentation for details
 *	on what it does.
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
PathCoords(
    Tcl_Interp *interp,     /* Used for error reporting. */
    Tk_PathCanvas canvas,   /* Canvas containing item. */
    Tk_PathItem *itemPtr,   /* Item whose coordinates are to be
                             * read or modified. */
    Tcl_Size objc,
    Tcl_Obj *const objv[])  /*  */
{
    PathItem *pathPtr = (PathItem *) itemPtr;
    int result;

    result = ProcessPath(interp, canvas, itemPtr, objc, objv);
    if ((result == TCL_OK) && (objc == 1)) {
        ConfigureArrows(canvas, pathPtr);
        ComputePathBbox(canvas, pathPtr);
    }
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * ComputePathBbox --
 *
 *	This procedure is invoked to compute the bounding box of
 *	all the pixels that may be drawn as part of a path.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The fields x1, y1, x2, and y2 are updated in the header
 *	for itemPtr.
 *
 *--------------------------------------------------------------
 */

static void
ComputePathBbox(
    Tk_PathCanvas canvas,   /* Canvas that contains item. */
    PathItem *pathPtr)      /* Item whose bbox is to be
                             * recomputed. */
{
    Tk_PathItemEx *itemExPtr = &pathPtr->headerEx;
    Tk_PathItem *itemPtr = &itemExPtr->header;
    Tk_PathStyle style;
    Tk_PathState state = itemExPtr->header.state;

    if (state == TK_PATHSTATE_NULL) {
        state = TkPathCanvasState(canvas);
    }
    if (pathPtr->pathObjPtr == NULL || (pathPtr->pathLen < 4) || (state == TK_PATHSTATE_HIDDEN)) {
        itemExPtr->header.x1 = itemExPtr->header.x2 =
        itemExPtr->header.y1 = itemExPtr->header.y2 = -1;
        return;
    }
    style = TkPathCanvasInheritStyle(itemPtr, kPathMergeStyleNotFill);

    /*
     * Get an approximation of the path's bounding box
     * assuming zero stroke width.
     */
    itemPtr->bbox = GetGenericBarePathBbox(pathPtr->atomPtr);
    IncludeArrowPointsInRect(&itemPtr->bbox, &pathPtr->startarrow);
    IncludeArrowPointsInRect(&itemPtr->bbox, &pathPtr->endarrow);
    itemPtr->totalBbox = GetGenericPathTotalBboxFromBare(pathPtr->atomPtr,
            &style, &itemPtr->bbox);
    SetGenericPathHeaderBbox(&itemExPtr->header, style.matrixPtr, &itemPtr->totalBbox);
    TkPathCanvasFreeInheritedStyle(&style);
}

static int
ConfigureArrows(
        Tk_PathCanvas canvas, PathItem *pathPtr)
{
    PathPoint *pfirstp;
    PathPoint psecond;
    PathPoint ppenult;
    PathPoint *plastp;

    int error = GetSegmentsFromPathAtomList(pathPtr->atomPtr, &pfirstp,
			&psecond, &ppenult, &plastp);

    if (error == TCL_OK) {
        PathPoint pfirst = *pfirstp;
        PathPoint plast = *plastp;
        Tk_PathStyle *lineStyle = &pathPtr->headerEx.style;
        int isOpen = lineStyle->fill==NULL &&
		((pfirst.x != plast.x) || (pfirst.y != plast.y));

        TkPathPreconfigureArrow(&pfirst, &pathPtr->startarrow);
        TkPathPreconfigureArrow(&plast, &pathPtr->endarrow);

        *pfirstp = TkPathConfigureArrow(pfirst, psecond,
			&pathPtr->startarrow, lineStyle, isOpen);
        *plastp = TkPathConfigureArrow(plast, ppenult, &pathPtr->endarrow,
			lineStyle, isOpen);
    } else {
        TkPathFreeArrow(&pathPtr->startarrow);
        TkPathFreeArrow(&pathPtr->endarrow);
    }

    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * ConfigurePath --
 *
 *	This procedure is invoked to configure various aspects
 *	of a line item such as its background color.
 *
 * Results:
 *	A standard Tcl result code.  If an error occurs, then
 *	an error message is left in the interp's result.
 *
 * Side effects:
 *	Configuration information, such as colors and stipple
 *	patterns, may be set for itemPtr.
 *
 *--------------------------------------------------------------
 */

static int
ConfigurePath(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tk_PathCanvas canvas,	/* Canvas containing itemPtr. */
    Tk_PathItem *itemPtr,	/* Line item to reconfigure. */
    Tcl_Size objc,		/* Number of elements in objv.  */
    Tcl_Obj *const objv[],	/* Arguments describing things to configure. */
    int flags)			/* Flags to pass to Tk_ConfigureWidget. */
{
    PathItem *pathPtr = (PathItem *) itemPtr;
    Tk_PathItemEx *itemExPtr = &pathPtr->headerEx;
    Tk_PathStyle *stylePtr = &itemExPtr->style;
    Tk_Window tkwin;
    Tk_SavedOptions savedOptions;
    Tcl_Obj *errorResult = NULL;
    int mask, error;

    tkwin = Tk_PathCanvasTkwin(canvas);
    for (error = 0; error <= 1; error++) {
	if (!error) {
	    if (Tk_SetOptions(interp, (char *) pathPtr, itemPtr->optionTable,
		    objc, objv, tkwin, &savedOptions, &mask) != TCL_OK) {
		continue;
	    }
	} else {
	    errorResult = Tcl_GetObjResult(interp);
	    Tcl_IncrRefCount(errorResult);
	    Tk_RestoreSavedOptions(&savedOptions);
	}
	if (TkPathCanvasItemExConfigure(interp, canvas, itemExPtr, mask) != TCL_OK) {
	    continue;
	}

	/*
	 * If we reach this on the first pass we are OK and continue below.
	 */
	break;
    }
    if (!error) {
	Tk_FreeSavedOptions(&savedOptions);
	stylePtr->mask |= mask;
    }

    stylePtr->strokeOpacity = MAX(0.0, MIN(1.0, stylePtr->strokeOpacity));
    stylePtr->fillOpacity   = MAX(0.0, MIN(1.0, stylePtr->fillOpacity));

    ConfigureArrows(canvas, pathPtr);

    /*
     * Recompute bounding box for path.
     */
    if (error) {
	Tcl_SetObjResult(interp, errorResult);
	Tcl_DecrRefCount(errorResult);
	return TCL_ERROR;
    } else {
        ComputePathBbox(canvas, pathPtr);
	return TCL_OK;
    }
}

/*
 *--------------------------------------------------------------
 *
 * DeletePath --
 *
 *	This procedure is called to clean up the data structure
 *	associated with a line item.
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
DeletePath(
    Tk_PathCanvas canvas,   /* Info about overall canvas widget. */
    Tk_PathItem *itemPtr,   /* Item that is being deleted. */
    Display *display)       /* Display containing window for
                             * canvas. */
{
    PathItem *pathPtr = (PathItem *) itemPtr;
    Tk_PathItemEx *itemExPtr = &pathPtr->headerEx;
    Tk_PathStyle *stylePtr = &itemExPtr->style;

    if (stylePtr->fill != NULL) {
	TkPathFreePathColor(stylePtr->fill);
    }
    if (itemExPtr->styleInst != NULL) {
	TkPathFreeStyle(itemExPtr->styleInst);
    }
    if (pathPtr->pathObjPtr != NULL) {
        Tcl_DecrRefCount(pathPtr->pathObjPtr);
    }
    if (pathPtr->normPathObjPtr != NULL) {
        Tcl_DecrRefCount(pathPtr->normPathObjPtr);
    }
    if (pathPtr->atomPtr != NULL) {
        TkPathFreeAtoms(pathPtr->atomPtr);
        pathPtr->atomPtr = NULL;
    }
    TkPathFreeArrow(&pathPtr->startarrow);
    TkPathFreeArrow(&pathPtr->endarrow);
    Tk_FreeConfigOptions((char *) pathPtr, itemPtr->optionTable,
			 Tk_PathCanvasTkwin(canvas));
}

/*
 *--------------------------------------------------------------
 *
 * DisplayPath --
 *
 *	This procedure is invoked to draw a line item in a given
 *	drawable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	ItemPtr is drawn in drawable using the transformation
 *	information in canvas.
 *
 *--------------------------------------------------------------
 */

static void
DisplayPath(
    Tk_PathCanvas canvas,   /* Canvas that contains item. */
    Tk_PathItem *itemPtr,   /* Item to be displayed. */
    Display *display,       /* Display on which to draw item. */
    Drawable drawable,      /* Pixmap or window in which to draw
                             * item. */
    int x, int y,           /* Describes region of canvas that */
    int width, int height)  /* must be redisplayed (not used). */
{
    PathItem *pathPtr = (PathItem *) itemPtr;
    TMatrix m = GetCanvasTMatrix(canvas);
    Tk_PathStyle style;

    if (pathPtr->pathLen > 2) {
        style = TkPathCanvasInheritStyle(itemPtr, 0);
        TkPathDrawPath(ContextOfCanvas(canvas), pathPtr->atomPtr, &style,
		&m, &itemPtr->bbox);
        /*
         * Display arrowheads, if they are wanted.
         */
        DisplayArrow(canvas, &pathPtr->startarrow, &style, &m, &itemPtr->bbox);
        DisplayArrow(canvas, &pathPtr->endarrow, &style, &m, &itemPtr->bbox);

        TkPathCanvasFreeInheritedStyle(&style);
    }
}

static void
PathBbox(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, int mask)
{
    PathItem *pathPtr = (PathItem *) itemPtr;
    /*
     * Try to be economical here.
     */
    if ((mask & PATH_STYLE_OPTION_MATRIX) ||
            (mask & PATH_STYLE_OPTION_STROKE) ||
            (mask & PATH_STYLE_OPTION_STROKE_WIDTH) ||
            (mask & PATH_CORE_OPTION_PARENT) ||
            (mask & PATH_CORE_OPTION_STYLENAME)) {
        ComputePathBbox(canvas, pathPtr);
    }
}

/*
 *--------------------------------------------------------------
 *
 * PathToPoint --
 *
 *	Computes the distance from a given point to a given
 *	line, in canvas units.
 *
 * Results:
 *	The return value is 0 if the point whose x and y coordinates
 *	are pointPtr[0] and pointPtr[1] is inside the line.  If the
 *	point isn't inside the line then the return value is the
 *	distance from the point to the line.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static double
PathToPoint(
    Tk_PathCanvas canvas,	/* Canvas containing item. */
    Tk_PathItem *itemPtr,	/* Item to check against point. */
    double *pointPtr)		/* Pointer to x and y coordinates. */
{
    PathItem        *pathPtr = (PathItem *) itemPtr;
    PathAtom        *atomPtr = pathPtr->atomPtr;
    Tk_PathStyle style;
    double dist;

    style = TkPathCanvasInheritStyle(itemPtr, 0);
    dist = GenericPathToPoint(canvas, itemPtr, &style, atomPtr,
            pathPtr->maxNumSegments, pointPtr);
    TkPathCanvasFreeInheritedStyle(&style);
    return dist;
}

/**********************************/

#ifdef NOWHERE_USED
double
TkLineToPoint2(
    double end1Ptr[2],		/* Coordinates of first end-point of line. */
    double end2Ptr[2],		/* Coordinates of second end-point of line. */
    double pointPtr[2])		/* Points to coords for point. */
{
    double dx, dy, a2, b2, c2;

    /*
     * Compute the point on the line that is closest to the
     * point. Use Pythagoras!
     * Notation:
     *	a = distance between end1 and end2
     * 	b = distance between end1 and point
     *	c = distance between end2 and point
     *
     *   point
     *    |\
     *    | \
     *  b |  \ c
     *    |   \
     *    |----\
     * end1  a  end2
     *
     * If angle between a and b is 90 degrees: c2 = a2 + b2
     * If larger then c2 > a2 + b2 and end1 is closest to point
     * Similar for end2 with b and c interchanged.
     */

    dx = end1Ptr[0] - end2Ptr[0];
    dy = end1Ptr[1] - end2Ptr[1];
    a2 = dx*dx + dy*dy;

    dx = end1Ptr[0] - pointPtr[0];
    dy = end1Ptr[1] - pointPtr[1];
    b2 = dx*dx + dy*dy;

    dx = end2Ptr[0] - pointPtr[0];
    dy = end2Ptr[1] - pointPtr[1];
    c2 = dx*dx + dy*dy;

    if (c2 >= a2 + b2) {
        return sqrt(b2);
    } else if (b2 >= a2 + c2) {
        return sqrt(c2);
    } else {
        double delta;

        /*
         * The closest point is found at the point between end1 and end2
         * that is perp to point. delta is the distance from end1 along
         * that line which is closest to point.
         */
        delta = (a2 + b2 - c2)/(2.0*sqrt(a2));
        return sqrt(MAX(0.0, b2 - delta*delta));
    }
}
#endif

/*
 * Get maximum number of segments needed to describe path.
 * Needed to see if we can use static space or need to allocate more.
 */

static int
GetArcNumSegments(double currentX, double currentY, ArcAtom *arc)
{
    int result;
    int ntheta, nlength;
    int numSteps;			/* Number of curve points to
					 * generate.  */
    double cx, cy, rx, ry;
    double theta1, dtheta;

    result = EndpointToCentralArcParameters(
            currentX, currentY,
            arc->x, arc->y, arc->radX, arc->radY,
            DEGREES_TO_RADIANS * arc->angle,
            arc->largeArcFlag, arc->sweepFlag,
            &cx, &cy, &rx, &ry,
            &theta1, &dtheta);
    if (result == kPathArcLine) {
        return 2;
    } else if (result == kPathArcSkip) {
        return 0;
    }

    /* Estimate the number of steps needed.
     * Max 10 degrees or length 50.
     */
    ntheta = (int) (dtheta/5.0 + 0.5);
    nlength = (int) (0.5*(rx + ry)*dtheta/50 + 0.5);
    numSteps = MAX(4, MAX(ntheta, nlength));;
    return numSteps;
}

static int
GetSubpathMaxNumSegments(PathAtom *atomPtr)
{
    int			num;
    int 		maxNumSegments;
    double 		currentX = 0.0, currentY = 0.0;
    double 		startX = 0.0, startY = 0.0;
    MoveToAtom 	*move;
    LineToAtom 	*line;
    ArcAtom 	*arc;
    QuadBezierAtom *quad;
    CurveToAtom *curve;

    num = 0;
    maxNumSegments = 0;

    while (atomPtr != NULL) {

        switch (atomPtr->type) {
            case PATH_ATOM_M: {
                move = (MoveToAtom *) atomPtr;
                num = 1;
                currentX = move->x;
                currentY = move->y;
                startX = currentX;
                startY = currentY;
                break;
            }
            case PATH_ATOM_L: {
                line = (LineToAtom *) atomPtr;
                num++;
                currentX = line->x;
                currentY = line->y;
                break;
            }
            case PATH_ATOM_A: {
                arc = (ArcAtom *) atomPtr;
                num += GetArcNumSegments(currentX, currentY, arc);
                currentX = arc->x;
                currentY = arc->y;
                break;
            }
            case PATH_ATOM_Q: {
                quad = (QuadBezierAtom *) atomPtr;
                num += kPathNumSegmentsQuadBezier;
                currentX = quad->anchorX;
                currentY = quad->anchorY;
                break;
            }
            case PATH_ATOM_C: {
                curve = (CurveToAtom *) atomPtr;
                num += kPathNumSegmentsCurveTo;
                currentX = curve->anchorX;
                currentY = curve->anchorY;
                break;
            }
            case PATH_ATOM_Z: {
                num++;
                currentX = startX;
                currentY = startY;
                break;
            }
            case PATH_ATOM_ELLIPSE:
            case PATH_ATOM_RECT: {
                /* Empty. */
                break;
            }
        }
        if (num > maxNumSegments) {
            maxNumSegments = num;
        }
        atomPtr = atomPtr->nextPtr;
    }
    return maxNumSegments;
}

/*
 *--------------------------------------------------------------
 *
 * PathToArea --
 *
 *	This procedure is called to determine whether an item
 *	lies entirely inside, entirely outside, or overlapping
 *	a given rectangular area.
 *
 *	Each subpath is treated in turn. Generate straight line
 *	segments for each subpath and treat it as a polygon.
 *
 * Results:
 *	-1 is returned if the item is entirely outside the
 *	area, 0 if it overlaps, and 1 if it is entirely
 *	inside the given area.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
PathToArea(
    Tk_PathCanvas canvas,   /* Canvas containing item. */
    Tk_PathItem *itemPtr,   /* Item to check against line. */
    double *areaPtr)        /* Pointer to array of four coordinates
                             * (x1, y1, x2, y2) describing rectangular
                             * area.  */
{
    PathItem *pathPtr = (PathItem *) itemPtr;
    Tk_PathStyle style;
    int area;

    style = TkPathCanvasInheritStyle(itemPtr, 0);
    area = GenericPathToArea(canvas, itemPtr, &style,
            pathPtr->atomPtr, pathPtr->maxNumSegments, areaPtr);
    TkPathCanvasFreeInheritedStyle(&style);
    return area;
}

/*
 *--------------------------------------------------------------
 *
 * ScalePath --
 *
 *	This procedure is invoked to rescale a path item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The line referred to by itemPtr is rescaled so that the
 *	following transformation is applied to all point
 *	coordinates:
 *	x' = originX + scaleX*(x-originX)
 *	y' = originY + scaleY*(y-originY)
 *
 *--------------------------------------------------------------
 */

static void
ScalePath(
    Tk_PathCanvas canvas,           /* Canvas containing line. */
    Tk_PathItem *itemPtr,           /* Path to be scaled. */
    int compensate,		    /* Compensate matrix. */
    double originX, double originY, /* Origin about which to scale rect. */
    double scaleX,                  /* Amount to scale in X direction. */
    double scaleY)                  /* Amount to scale in Y direction. */
{
    PathItem *pathPtr = (PathItem *) itemPtr;
    PathAtom *atomPtr = pathPtr->atomPtr;

    CompensateScale(itemPtr, compensate, &originX, &originY, &scaleX, &scaleY);

    /* @@@ TODO: Arc atoms with nonzero rotation angle is WRONG! */

    ScalePathAtoms(atomPtr, originX, originY, scaleX, scaleY);

    /*
     * Set flags bit so we know that PathCoords need to update the
     * normalized path before being used.
     */
    pathPtr->flags |= kPathItemNeedNewNormalizedPath;

    /* Just scale the bbox'es as well. */
    ScalePathRect(&itemPtr->bbox, originX, originY, scaleX, scaleY);
    NormalizePathRect(&itemPtr->bbox);

    TkPathScaleArrow(&pathPtr->startarrow, originX, originY, scaleX, scaleY);
    TkPathScaleArrow(&pathPtr->endarrow, originX, originY, scaleX, scaleY);
    ConfigureArrows(canvas, pathPtr);
    ScaleItemHeader(itemPtr, originX, originY, scaleX, scaleY);
}

/*
 *--------------------------------------------------------------
 *
 * TranslatePath --
 *
 *	This procedure is called to move a path by a given amount.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The position of the line is offset by (xDelta, yDelta), and
 *	the bounding box is updated in the generic part of the item
 *	structure.
 *
 *--------------------------------------------------------------
 */

static void
TranslatePath(
    Tk_PathCanvas canvas,       /* Canvas containing item. */
    Tk_PathItem *itemPtr, 	/* Item that is being moved. */
    int compensate,		/* Compensate matrix. */
    double deltaX,		/* Amount by which item is to be */
    double deltaY)              /* moved. */
{
    PathItem *pathPtr = (PathItem *) itemPtr;
    PathAtom *atomPtr = pathPtr->atomPtr;

    CompensateTranslate(itemPtr, compensate, &deltaX, &deltaY);

    TranslatePathAtoms(atomPtr, deltaX, deltaY);

    /*
     * Set flags bit so we know that PathCoords need to update the
     * normalized path before being used.
     */
    pathPtr->flags |= kPathItemNeedNewNormalizedPath;

    /* Just translate the bbox'es as well. */
    TranslatePathRect(&itemPtr->bbox, deltaX, deltaY);
    TkPathTranslateArrow(&pathPtr->startarrow, deltaX, deltaY);
    TkPathTranslateArrow(&pathPtr->endarrow, deltaX, deltaY);
    TranslateItemHeader(itemPtr, deltaX, deltaY);
}

/*
 *--------------------------------------------------------------
 *
 * PathToPdf --
 *
 *	This procedure is called to generate Pdf for
 *	path items.
 *
 * Results:
 *	The return value is a standard Tcl result.  If an error
 *	occurs in generating Pdf then an error message is
 *	left in the interp's result, replacing whatever used
 *	to be there.  If no error occurs, then Pdf for the
 *	item is appended to the result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
PathToPdf(
    Tcl_Interp *interp,     /* Leave Pdf or error message
                             * here. */
    Tk_PathCanvas canvas,   /* Information about overall canvas. */
    Tk_PathItem *itemPtr,   /* Item for which Pdf is
                             * wanted. */
    Tcl_Size objc,          /* Number of arguments. */
    Tcl_Obj *const objv[],  /* Argument list. */
    int prepass)            /* 1 means this is a prepass to
                             * collect font information;  0 means
                             * final Pdf is being created. */
{
    Tk_PathStyle style;
    PathItem *pathPtr = (PathItem *) itemPtr;
    Tk_PathState state = itemPtr->state;
    int result = TCL_OK;

    if (state == TK_PATHSTATE_NULL) {
        state = TkPathCanvasState(canvas);
    }
    if ((pathPtr->pathObjPtr == NULL) || (state == TK_PATHSTATE_HIDDEN)) {
	return result;
    }
    if (pathPtr->pathLen > 2) {
	style = TkPathCanvasInheritStyle(itemPtr, 0);
	result = TkPathPdf(interp, pathPtr->atomPtr, &style, &itemPtr->bbox,
			   objc, objv);
	if (result == TCL_OK) {
	    result = TkPathPdfArrow(interp, &pathPtr->startarrow, &style);
	    if (result == TCL_OK) {
		result = TkPathPdfArrow(interp, &pathPtr->endarrow, &style);
	    }
	}
	TkPathCanvasFreeInheritedStyle(&style);
    }
    return result;
}

#ifndef TKP_NO_POSTSCRIPT
/*
 *--------------------------------------------------------------
 *
 * PathToPostscript --
 *
 *	This procedure is called to generate Postscript for
 *	path items.
 *
 * Results:
 *	The return value is a standard Tcl result.  If an error
 *	occurs in generating Postscript then an error message is
 *	left in the interp's result, replacing whatever used
 *	to be there.  If no error occurs, then Postscript for the
 *	item is appended to the result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
PathToPostscript(
    Tcl_Interp *interp,     /* Leave Postscript or error message
                             * here. */
    Tk_PathCanvas canvas,   /* Information about overall canvas. */
    Tk_PathItem *itemPtr,   /* Item for which Postscript is
                             * wanted. */
    int prepass)            /* 1 means this is a prepass to
                             * collect font information;  0 means
                             * final Postscript is being created. */
{
    return TCL_ERROR;
}
#endif

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
