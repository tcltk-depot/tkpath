/*
 * tkCanvPrect.c --
 *
 *	This file implements a rectangle canvas item modelled after its
 *	SVG counterpart. See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2007-2008  Mats Bengtsson
 *
 */

#include "tkIntPath.h"
#include "tkpCanvas.h"
#include "tkCanvPathUtil.h"
#include "tkPathStyle.h"

/*
 * The structure below defines the record for each path item.
 */

typedef struct PrectItem  {
    Tk_PathItemEx headerEx; /* Generic stuff that's the same for all
                             * path types.  MUST BE FIRST IN STRUCTURE. */
    double rx;		    /* Radius of corners. */
    double ry;
    int maxNumSegments;	    /* Max number of straight segments (for subpath)
                             * needed for Area and Point functions. */
} PrectItem;

/*
 * Prototypes for procedures defined in this file:
 */

static void	ComputePrectBbox(Tk_PathCanvas canvas, PrectItem *prectPtr);
static int	ConfigurePrect(Tcl_Interp *interp, Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, Tcl_Size objc,
                        Tcl_Obj *const objv[], int flags);
static int	CreatePrect(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, struct Tk_PathItem *itemPtr,
                        Tcl_Size objc, Tcl_Obj *const objv[]);
static void	DeletePrect(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, Display *display);
static void	DisplayPrect(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, Display *display, Drawable drawable,
                        int x, int y, int width, int height);
static void	PrectBbox(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, int mask);
static int	PrectCoords(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
                        Tcl_Size objc, Tcl_Obj *const objv[]);
static int	PrectToArea(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double *rectPtr);
static int	PrectToPdf(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
			Tcl_Size objc, Tcl_Obj *const objv[], int prepass);
static double	PrectToPoint(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double *coordPtr);
#ifndef TKP_NO_POSTSCRIPT
static int	PrectToPostscript(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
			int prepass);
#endif
static void	ScalePrect(Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
			int compensate, double originX, double originY,
			double scaleX, double scaleY);
static void	TranslatePrect(Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
			int compensate, double deltaX, double deltaY);
static PathAtom * MakePathAtoms(PrectItem *prectPtr, int needPath);


enum {
    PRECT_OPTION_INDEX_RX   = (1L << (PATH_STYLE_OPTION_INDEX_END + 0)),
    PRECT_OPTION_INDEX_RY   = (1L << (PATH_STYLE_OPTION_INDEX_END + 1)),
};

PATH_STYLE_CUSTOM_OPTION_RECORDS
PATH_CUSTOM_OPTION_TAGS
PATH_OPTION_STRING_TABLES_FILL
PATH_OPTION_STRING_TABLES_STROKE
PATH_OPTION_STRING_TABLES_STATE

#define PATH_OPTION_SPEC_RX(typeName)		    \
    {TK_OPTION_DOUBLE, "-rx", NULL, NULL,	    \
        "0.0", -1, offsetof(typeName, rx),	    \
	0, 0, PRECT_OPTION_INDEX_RX}

#define PATH_OPTION_SPEC_RY(typeName)		    \
    {TK_OPTION_DOUBLE, "-ry", NULL, NULL,	    \
        "0.0", -1, offsetof(typeName, ry),	    \
	0, 0, PRECT_OPTION_INDEX_RY}

static Tk_OptionSpec optionSpecs[] = {
    PATH_OPTION_SPEC_CORE(Tk_PathItemEx),
    PATH_OPTION_SPEC_PARENT,
    PATH_OPTION_SPEC_STYLE_FILL(Tk_PathItemEx, ""),
    PATH_OPTION_SPEC_STYLE_MATRIX(Tk_PathItemEx),
    PATH_OPTION_SPEC_STYLE_STROKE(Tk_PathItemEx, "black"),
    PATH_OPTION_SPEC_RX(PrectItem),
    PATH_OPTION_SPEC_RY(PrectItem),
    PATH_OPTION_SPEC_END
};

/*
 * The structures below defines the 'prect' item type by means
 * of procedures that can be invoked by generic item code.
 */

Tk_PathItemType tkpPrectType = {
    "prect",				/* name */
    sizeof(PrectItem),			/* itemSize */
    CreatePrect,			/* createProc */
    optionSpecs,			/* optionSpecs OBSOLTE !!! ??? */
    ConfigurePrect,			/* configureProc */
    PrectCoords,			/* coordProc */
    DeletePrect,			/* deleteProc */
    DisplayPrect,			/* displayProc */
    0,					/* flags */
    PrectBbox,				/* bboxProc */
    PrectToPoint,			/* pointProc */
    PrectToArea,			/* areaProc */
#ifndef TKP_NO_POSTSCRIPT
    PrectToPostscript,			/* postscriptProc */
#endif
    PrectToPdf,				/* pdfProc */
    ScalePrect,				/* scaleProc */
    TranslatePrect,			/* translateProc */
    (Tk_PathItemIndexProc *) NULL,	/* indexProc */
    (Tk_PathItemCursorProc *) NULL,	/* icursorProc */
    (Tk_PathItemSelectionProc *) NULL,	/* selectionProc */
    (Tk_PathItemInsertProc *) NULL,	/* insertProc */
    (Tk_PathItemDCharsProc *) NULL,	/* dTextProc */
    (Tk_PathItemType *) NULL,		/* nextPtr */
    1,					/* isPathType */
};


static int
CreatePrect(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
        Tcl_Size objc, Tcl_Obj *const objv[])
{
    PrectItem *prectPtr = (PrectItem *) itemPtr;
    Tk_PathItemEx *itemExPtr = &prectPtr->headerEx;
    Tcl_Size	i;
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
    itemPtr->bbox = NewEmptyPathRect();
    itemPtr->totalBbox = NewEmptyPathRect();
    prectPtr->maxNumSegments = 100;		/* Crude overestimate. */

    optionTable = Tk_CreateOptionTable(interp, optionSpecs);
    itemPtr->optionTable = optionTable;
    if (Tk_InitOptions(interp, (char *) prectPtr, optionTable,
	    Tk_PathCanvasTkwin(canvas)) != TCL_OK) {
        goto error;
    }

    for (i = 1; i < objc; i++) {
        char *arg = Tcl_GetString(objv[i]);
        if ((arg[0] == '-') && (arg[1] >= 'a') && (arg[1] <= 'z')) {
            break;
        }
    }
    if (CoordsForRectangularItems(interp, canvas, &itemPtr->bbox, i, objv) != TCL_OK) {
        goto error;
    }
    if (ConfigurePrect(interp, canvas, itemPtr, objc-i, objv+i, 0) == TCL_OK) {
        return TCL_OK;
    }

    error:
    /*
     * NB: We must unlink the item here since the TkPathCanvasItemExConfigure()
     *     link it to the root by default.
     */
    TkPathCanvasItemDetach(itemPtr);
    DeletePrect(canvas, itemPtr, Tk_Display(Tk_PathCanvasTkwin(canvas)));
    return TCL_ERROR;
}

static int
PrectCoords(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
        Tcl_Size objc, Tcl_Obj *const objv[])
{
    PrectItem *prectPtr = (PrectItem *) itemPtr;
    int result;

    result = CoordsForRectangularItems(interp, canvas, &itemPtr->bbox, objc, objv);
    if ((result == TCL_OK) && ((objc == 1) || (objc == 4))) {
	ComputePrectBbox(canvas, prectPtr);
    }
    return result;
}

void
ComputePrectBbox(Tk_PathCanvas canvas, PrectItem *prectPtr)
{
    Tk_PathItemEx *itemExPtr = &prectPtr->headerEx;
    Tk_PathItem *itemPtr = &itemExPtr->header;
    Tk_PathStyle style;
    Tk_PathState state = itemExPtr->header.state;

    if (state == TK_PATHSTATE_NULL) {
	state = TkPathCanvasState(canvas);
    }
    if (state == TK_PATHSTATE_HIDDEN) {
        itemExPtr->header.x1 = itemExPtr->header.x2 =
        itemExPtr->header.y1 = itemExPtr->header.y2 = -1;
        return;
    }
    style = TkPathCanvasInheritStyle(itemPtr, kPathMergeStyleNotFill);
    itemPtr->totalBbox = GetGenericPathTotalBboxFromBare(NULL, &style, &itemPtr->bbox);
    SetGenericPathHeaderBbox(&itemExPtr->header, style.matrixPtr, &itemPtr->totalBbox);
    TkPathCanvasFreeInheritedStyle(&style);
}

static int
ConfigurePrect(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
        Tcl_Size objc, Tcl_Obj *const objv[], int flags)
{
    PrectItem *prectPtr = (PrectItem *) itemPtr;
    Tk_PathItemEx *itemExPtr = &prectPtr->headerEx;
    Tk_PathStyle *stylePtr = &itemExPtr->style;
    Tk_Window tkwin;
    Tk_SavedOptions savedOptions;
    Tcl_Obj *errorResult = NULL;
    int error, mask;

    tkwin = Tk_PathCanvasTkwin(canvas);
    for (error = 0; error <= 1; error++) {
	if (!error) {
	    if (Tk_SetOptions(interp, (char *) prectPtr, itemPtr->optionTable,
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
    prectPtr->rx = MAX(0.0, prectPtr->rx);
    prectPtr->ry = MAX(0.0, prectPtr->ry);

    /*
     * Recompute bounding box for path.
     */
    if (error) {
	Tcl_SetObjResult(interp, errorResult);
	Tcl_DecrRefCount(errorResult);
	return TCL_ERROR;
    } else {
	ComputePrectBbox(canvas, prectPtr);
	return TCL_OK;
    }
}

static PathAtom *
MakePathAtoms(PrectItem *prectPtr, int needPath)
{
    Tk_PathItem *itemPtr = (Tk_PathItem *) prectPtr;
    PathAtom *atomPtr;
    double points[4];

    points[0] = itemPtr->bbox.x1;
    points[1] = itemPtr->bbox.y1;
    points[2] = itemPtr->bbox.x2;
    points[3] = itemPtr->bbox.y2;
    TkPathMakePrectAtoms(points, prectPtr->rx, prectPtr->ry, needPath, &atomPtr);
    return atomPtr;
}

static void
DeletePrect(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, Display *display)
{
    PrectItem *prectPtr = (PrectItem *) itemPtr;
    Tk_PathItemEx *itemExPtr = &prectPtr->headerEx;
    Tk_PathStyle *stylePtr = &itemExPtr->style;

    if (stylePtr->fill != NULL) {
	TkPathFreePathColor(stylePtr->fill);
    }
    if (itemExPtr->styleInst != NULL) {
	TkPathFreeStyle(itemExPtr->styleInst);
    }
    Tk_FreeConfigOptions((char *) itemPtr, itemPtr->optionTable,
			 Tk_PathCanvasTkwin(canvas));
}

static void
DisplayPrect(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, Display *display, Drawable drawable,
        int x, int y, int width, int height)
{
    PrectItem *prectPtr = (PrectItem *) itemPtr;
    TMatrix m = GetCanvasTMatrix(canvas);
    PathAtom *atomPtr;
    Tk_PathStyle style;

    style = TkPathCanvasInheritStyle(itemPtr, 0);
    atomPtr = MakePathAtoms(prectPtr, 0);
    TkPathDrawPath(ContextOfCanvas(canvas), atomPtr, &style,
	    &m, &itemPtr->bbox);
    TkPathFreeAtoms(atomPtr);
    TkPathCanvasFreeInheritedStyle(&style);
}

static void
PrectBbox(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, int mask)
{
    PrectItem *prectPtr = (PrectItem *) itemPtr;
    ComputePrectBbox(canvas, prectPtr);
}

static double
PrectToPoint(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double *pointPtr)
{
    PrectItem *prectPtr = (PrectItem *) itemPtr;
    Tk_PathStyle style;
    TMatrix *mPtr;
    PathRect *rectPtr = &itemPtr->bbox;
    double bareRect[4];
    double width, dist;
    int rectiLinear = 0;
    int filled;

    style = TkPathCanvasInheritStyle(itemPtr, 0);
    filled = HaveAnyFillFromPathColor(style.fill);
    width = 0.0;
    if (style.strokeColor != NULL) {
        width = style.strokeWidth;
    }
    mPtr = style.matrixPtr;

    /* Try to be economical about this for pure rectangles. */
    if ((prectPtr->rx <= 1.0) && (prectPtr->ry <= 1.0)) {
        if (mPtr == NULL) {
            rectiLinear = 1;
            bareRect[0] = rectPtr->x1;
            bareRect[1] = rectPtr->y1;
            bareRect[2] = rectPtr->x2;
            bareRect[3] = rectPtr->y2;
        } else if (TMATRIX_IS_RECTILINEAR(mPtr)) {

            /* This is a situation we can treat in a simplified way. Apply the transform here. */
            rectiLinear = 1;
            bareRect[0] = mPtr->a * rectPtr->x1 + mPtr->tx;
            bareRect[1] = mPtr->d * rectPtr->y1 + mPtr->ty;
            bareRect[2] = mPtr->a * rectPtr->x2 + mPtr->tx;
            bareRect[3] = mPtr->d * rectPtr->y2 + mPtr->ty;
        }
    }
    if (rectiLinear) {
        dist = PathRectToPoint(bareRect, width, filled, pointPtr);
    } else {
	PathAtom *atomPtr = MakePathAtoms(prectPtr, 1);
        dist = GenericPathToPoint(canvas, itemPtr, &style, atomPtr,
            prectPtr->maxNumSegments, pointPtr);
	TkPathFreeAtoms(atomPtr);
    }
    TkPathCanvasFreeInheritedStyle(&style);
    return dist;
}

static int
PrectToArea(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double *areaPtr)
{
    PrectItem *prectPtr = (PrectItem *) itemPtr;
    Tk_PathItemEx *itemExPtr = &prectPtr->headerEx;
    Tk_PathStyle style = itemExPtr->style;  /* NB: We *copy* the style for temp usage. */
    TMatrix *mPtr;
    PathRect *rectPtr = &(itemPtr->bbox);
    double bareRect[4];
    double width;
    int rectiLinear = 0;
    int filled, area;

    style = TkPathCanvasInheritStyle(itemPtr, 0);
    filled = HaveAnyFillFromPathColor(style.fill);
    width = 0.0;
    if (style.strokeColor != NULL) {
        width = style.strokeWidth;
    }
    mPtr = style.matrixPtr;

    /* Try to be economical about this for pure rectangles. */
    if ((prectPtr->rx <= 1.0) && (prectPtr->ry <= 1.0)) {
        if (mPtr == NULL) {
            rectiLinear = 1;
            bareRect[0] = rectPtr->x1;
            bareRect[1] = rectPtr->y1;
            bareRect[2] = rectPtr->x2;
            bareRect[3] = rectPtr->y2;
        } else if (TMATRIX_IS_RECTILINEAR(mPtr)) {

            /* This is a situation we can treat in a simplified way. Apply the transform here. */
            rectiLinear = 1;
            bareRect[0] = mPtr->a * rectPtr->x1 + mPtr->tx;
            bareRect[1] = mPtr->d * rectPtr->y1 + mPtr->ty;
            bareRect[2] = mPtr->a * rectPtr->x2 + mPtr->tx;
            bareRect[3] = mPtr->d * rectPtr->y2 + mPtr->ty;
        }
    }
    if (rectiLinear) {
        area = PathRectToArea(bareRect, width, filled, areaPtr);
    } else {
	PathAtom *atomPtr = MakePathAtoms(prectPtr, 1);
        area = GenericPathToArea(canvas, itemPtr, &style,
                atomPtr, prectPtr->maxNumSegments, areaPtr);
	TkPathFreeAtoms(atomPtr);
    }
    TkPathCanvasFreeInheritedStyle(&style);
    return area;
}

static int
PrectToPdf(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
    Tcl_Size objc, Tcl_Obj *const objv[], int prepass)
{
    Tk_PathStyle style;
    PathAtom *atomPtr;
    PrectItem *prectPtr = (PrectItem *) itemPtr;
    double points[4];
    Tk_PathState state = itemPtr->state;
    int result;

    if (state == TK_PATHSTATE_NULL) {
	state = TkPathCanvasState(canvas);
    }
    if (state == TK_PATHSTATE_HIDDEN) {
	return TCL_OK;
    }
    style = TkPathCanvasInheritStyle(itemPtr, 0);
    points[0] = itemPtr->bbox.x1;
    points[1] = itemPtr->bbox.y1;
    points[2] = itemPtr->bbox.x2;
    points[3] = itemPtr->bbox.y2;
    TkPathMakePrectAtoms(points, prectPtr->rx, prectPtr->ry, 0, &atomPtr);
    result = TkPathPdf(interp, atomPtr, &style, &itemPtr->bbox, objc, objv);
    TkPathFreeAtoms(atomPtr);
    TkPathCanvasFreeInheritedStyle(&style);
    return result;
}

#ifndef TKP_NO_POSTSCRIPT
static int
PrectToPostscript(Tcl_Interp *interp, Tk_PathCanvas canvas,
    Tk_PathItem *itemPtr, int prepass)
{
    return TCL_ERROR;	/* @@@ Anyone? */
}
#endif

static void
ScalePrect(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, int compensate,
    double originX, double originY, double scaleX, double scaleY)
{
    CompensateScale(itemPtr, compensate, &originX, &originY, &scaleX, &scaleY);

    ScalePathRect(&itemPtr->bbox, originX, originY, scaleX, scaleY);
    ScaleItemHeader(itemPtr, originX, originY, scaleX, scaleY);
}

static void
TranslatePrect(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, int compensate,
    double deltaX, double deltaY)
{
    CompensateTranslate(itemPtr, compensate, &deltaX, &deltaY);

    /* Just translate the bbox'es as well. */
    TranslatePathRect(&itemPtr->bbox, deltaX, deltaY);
    TranslateItemHeader(itemPtr, deltaX, deltaY);
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
