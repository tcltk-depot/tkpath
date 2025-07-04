/*
 * tkPath.h --
 *
 *	This file implements a path drawing model
 *      SVG counterpart. See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2005-2008  Mats Bengtsson
 *
 */

#ifndef INCLUDED_TKPATH_H
#define INCLUDED_TKPATH_H

#include <tkInt.h>
#include "tkPort.h"
#include "tkp.h"

/*
 * For C++ compilers, use extern "C"
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The enum below defines the valid types for the PathAtom's.
 */

typedef enum {
    PATH_ATOM_M = 'M',
    PATH_ATOM_L = 'L',
    PATH_ATOM_A = 'A',
    PATH_ATOM_Q = 'Q',
    PATH_ATOM_C = 'C',
    PATH_ATOM_Z = 'Z',
    PATH_ATOM_ELLIPSE = '1',	/* These are not a standard atoms
                                 * since they are more complex (molecule).
                                 * Not all features supported for these! */
    PATH_ATOM_RECT = '2'
} PathAtomType;

enum {
    PATH_NEXT_ERROR,
    PATH_NEXT_INSTRUCTION,
    PATH_NEXT_OTHER
};

typedef struct PathPoint {
    double x;
    double y;
} PathPoint;

/*
 * The transformation matrix:
 *		| a  b  0 |
 *		| c  d  0 |
 *		| tx ty 1 |
 */

typedef struct TMatrix {
    double a, b, c, d;
    double tx, ty;
} TMatrix;

/*
 * Records used for parsing path to a linked list of primitive
 * drawing instructions.
 *
 * PathAtom: vaguely modelled after Tk_PathItem. Each atom has a PathAtom record
 * in its first position, padded with type specific data.
 */

typedef struct PathAtom {
    PathAtomType type;		/* Type of PathAtom. */
    struct PathAtom *nextPtr;	/* Next PathAtom along the path. */
} PathAtom;

typedef void (TkPathGradientChangedProc)(ClientData clientData, int flags);
typedef void (TkPathStyleChangedProc)(ClientData clientData, int flags);

/*
 * Records for gradient fills.
 * We need a separate GradientStopArray to simplify option parsing.
 */

typedef struct GradientStop {
    double offset;
    XColor *color;
    double opacity;
} GradientStop;

typedef struct GradientStopArray {
    int nstops;
    GradientStop **stops;	/* Array of pointers to GradientStop. */
} GradientStopArray;

typedef struct LinearGradientFill {
    PathRect *transitionPtr;	/* Actually not a proper rect but a vector. */
    int method;
    int fillRule;		/* Not yet used. */
    int units;
    GradientStopArray *stopArrPtr;
} LinearGradientFill;

typedef struct RadialTransition {
    double centerX;
    double centerY;
    double radius;
    double focalX;
    double focalY;
} RadialTransition;

typedef struct RadialGradientFill {
    RadialTransition *radialPtr;
    int method;
    int fillRule;		/* Not yet used. */
    int units;
    GradientStopArray *stopArrPtr;
} RadialGradientFill;

enum {
    kPathGradientTypeLinear =	0L,
    kPathGradientTypeRadial
};

/*
 * This is the main record for a gradient object.
 */
typedef struct TkPathGradientMaster {
    int type;			/* Any of kPathGradientTypeLinear or kPathGradientTypeRadial */
    Tk_OptionTable optionTable;
    Tk_Uid name;
    Tcl_Obj *transObj;
    Tcl_Obj *stopsObj;
    TMatrix *matrixPtr;		/*  a  b   default (NULL): 1 0
                                    c  d		   0 1
                                    tx ty 		   0 0 */

    struct TkPathGradientInst *instancePtr;
				/* Pointer to first in list of instances
				 * derived from this gradient name. */
    union {			/* Depending on the 'type'. */
        LinearGradientFill linearFill;
        RadialGradientFill radialFill;
    };
} TkPathGradientMaster;

/*
 * This defines an instance of a gradient with specified name and hash table.
 */
typedef struct TkPathGradientInst {
    struct TkPathGradientMaster *masterPtr;
				/* Each instance also points to the actual
				 * TkPathGradientMaster in order to remove itself
				 * from its linked list. */
    TkPathGradientChangedProc *changeProc;
				/* Code in item to call when gradient changes
				 * in a way that affects redisplay. */
    ClientData clientData;	/* Argument to pass to changeProc. */
    struct TkPathGradientInst *nextPtr;
				/* Next in list of all gradient instances
				 * associated with the same gradient name. */
} TkPathGradientInst;

/*
 * Only one of color and gradientInstPtr must be non NULL!
 */
typedef struct TkPathColor {
    XColor *color;	    /* Foreground color for filling. */
    TkPathGradientInst *gradientInstPtr;
			    /* This is an instance of a gradient.
			     * It points to the actual gradient object, the master. */
} TkPathColor;

/*
 * Opaque platform dependent struct.
 */

typedef ClientData TkPathContext;

/*
 * Information used for parsing configuration options.
 * Mask bits for options changed.
 */

enum {
    PATH_STYLE_OPTION_FILL		    = (1L << 0),
    PATH_STYLE_OPTION_FILL_OFFSET	    = (1L << 1),
    PATH_STYLE_OPTION_FILL_OPACITY	    = (1L << 2),
    PATH_STYLE_OPTION_FILL_RULE		    = (1L << 3),
    PATH_STYLE_OPTION_FILL_STIPPLE	    = (1L << 4),
    PATH_STYLE_OPTION_MATRIX		    = (1L << 5),
    PATH_STYLE_OPTION_STROKE		    = (1L << 6),
    PATH_STYLE_OPTION_STROKE_DASHARRAY	    = (1L << 7),
    PATH_STYLE_OPTION_STROKE_DASHOFFSET	    = (1L << 8),
    PATH_STYLE_OPTION_STROKE_LINECAP        = (1L << 9),
    PATH_STYLE_OPTION_STROKE_LINEJOIN       = (1L << 10),
    PATH_STYLE_OPTION_STROKE_MITERLIMIT     = (1L << 11),
    PATH_STYLE_OPTION_STROKE_OFFSET	    = (1L << 12),
    PATH_STYLE_OPTION_STROKE_OPACITY	    = (1L << 13),
    PATH_STYLE_OPTION_STROKE_STIPPLE	    = (1L << 14),
    PATH_STYLE_OPTION_STROKE_WIDTH	    = (1L << 15),
    PATH_CORE_OPTION_PARENT		    = (1L << 16),
    PATH_CORE_OPTION_STYLENAME		    = (1L << 17),
    PATH_CORE_OPTION_TAGS		    = (1L << 18),
};

#define PATH_STYLE_OPTION_INDEX_END 17	/* Use this for item specific flags */

typedef struct Tk_PathStyle {
    Tk_OptionTable optionTable;	/* Not used for canvas. */
    Tk_Uid name;		/* Not used for canvas. */
    int mask;			/* Bits set for actual options modified. */
    XColor *strokeColor;	/* Stroke color. */
    double strokeWidth;		/* Width of stroke. */
    double strokeOpacity;
    int offset;			/* Dash offset */
    Tk_PathDash *dashPtr;	/* Dash pattern. */
    int capStyle;		/* Cap style for stroke. */
    int joinStyle;		/* Join style for stroke. */
    double miterLimit;
    Tcl_Obj *fillObj;		/* This is just used for option parsing. */
    TkPathColor *fill;		/* Record XColor + TkPathGradientInst. */
    double fillOpacity;
    int fillRule;		/* WindingRule or EvenOddRule. */
    TMatrix *matrixPtr;		/*  a  b   default (NULL): 1 0
				    c  d		   0 1
				    tx ty 		   0 0 */
    struct TkPathStyleInst *instancePtr;
				/* Pointer to first in list of instances
				 * derived from this style name. */
} Tk_PathStyle;

/*
 * This defines an instance of a style with specified name and hash table.
 */
typedef struct TkPathStyleInst {
    struct Tk_PathStyle *masterPtr;
				/* Each instance also points to the actual
				 * Tk_PathStyle in order to remove itself
				 * from its linked list. */
    TkPathStyleChangedProc *changeProc;
				/* Code in item to call when style changes
				 * in a way that affects redisplay. */
    ClientData clientData;	/* Argument to pass to changeProc. */
    struct TkPathStyleInst *nextPtr;
				/* Next in list of all style instances
				 * associated with the same style name. */
} TkPathStyleInst;

/* @@@ TODO: Much more to be added here! */

enum FontWeight {
    PATH_TEXT_WEIGHT_NORMAL,
    PATH_TEXT_WEIGHT_BOLD
};

enum FontSlant {
    PATH_TEXT_SLANT_NORMAL,
    PATH_TEXT_SLANT_ITALIC,
    PATH_TEXT_SLANT_OBLIQUE
};

typedef struct Tk_PathTextStyle {
    char *fontFamily;
    double fontSize;
    enum FontWeight fontWeight;
    enum FontSlant fontSlant;
} Tk_PathTextStyle;

/*
 * Functions to create path atoms.
 */

MODULE_SCOPE PathAtom *NewMoveToAtom(double x, double y);
MODULE_SCOPE PathAtom *NewLineToAtom(double x, double y);
MODULE_SCOPE PathAtom *NewArcAtom(double radX, double radY,
			double angle, char largeArcFlag, char sweepFlag,
			double x, double y);
MODULE_SCOPE PathAtom *NewQuadBezierAtom(double ctrlX, double ctrlY,
			double anchorX, double anchorY);
MODULE_SCOPE PathAtom *NewCurveToAtom(double ctrlX1, double ctrlY1,
			double ctrlX2, double ctrlY2,
			double anchorX, double anchorY);
MODULE_SCOPE PathAtom *NewRectAtom(double pointsPtr[]);
MODULE_SCOPE PathAtom *NewCloseAtom(double x, double y);

/*
 * Functions that process lists and atoms.
 */

MODULE_SCOPE int    TkPathParseToAtoms(Tcl_Interp *interp, Tcl_Obj *listObjPtr,
			PathAtom **atomPtrPtr, Tcl_Size *lenPtr);
MODULE_SCOPE void   TkPathFreeAtoms(PathAtom *pathAtomPtr);
MODULE_SCOPE int    TkPathNormalize(Tcl_Interp *interp, PathAtom *atomPtr,
			Tcl_Obj **listObjPtrPtr);
MODULE_SCOPE int    TkPathMakePath(TkPathContext ctx, PathAtom *atomPtr,
			Tk_PathStyle *stylePtr);

/*
 * Stroke, fill, clip etc.
 */

MODULE_SCOPE void   TkPathClipToPath(TkPathContext ctx, int fillRule);
MODULE_SCOPE void   TkPathReleaseClipToPath(TkPathContext ctx);
MODULE_SCOPE void   TkPathStroke(TkPathContext ctx, Tk_PathStyle *style);
MODULE_SCOPE void   TkPathFill(TkPathContext ctx, Tk_PathStyle *style);
MODULE_SCOPE void   TkPathFillAndStroke(TkPathContext ctx, Tk_PathStyle *style);
MODULE_SCOPE int    TkPathGetCurrentPosition(TkPathContext ctx,
			PathPoint *ptPtr);
MODULE_SCOPE int    TkPathBoundingBox(TkPathContext ctx, PathRect *rPtr);
MODULE_SCOPE void   TkPathPaintLinearGradient(TkPathContext ctx,
			PathRect *bbox, LinearGradientFill *fillPtr,
			int fillRule, double fillOpacity, TMatrix *matrixPtr);
MODULE_SCOPE void   TkPathPaintRadialGradient(TkPathContext ctx,
			PathRect *bbox, RadialGradientFill *fillPtr,
			int fillRule, double fillOpacity, TMatrix *mPtr);
MODULE_SCOPE void   TkPathFree(TkPathContext ctx);
MODULE_SCOPE int    TkPathDrawingDestroysPath(void);
MODULE_SCOPE int    TkPathPixelAlign(void);
MODULE_SCOPE void   TkPathPushTMatrix(TkPathContext ctx, TMatrix *mPtr);
MODULE_SCOPE void   TkPathResetTMatrix(TkPathContext ctx);
MODULE_SCOPE void   TkPathSaveState(TkPathContext ctx);
MODULE_SCOPE void   TkPathRestoreState(TkPathContext ctx);

/*
 * Utilities for creating and deleting Tk_PathStyles.
 */

MODULE_SCOPE void   TkPathInitStyle(Tk_PathStyle *style);
MODULE_SCOPE void   TkPathDeleteStyle(Tk_PathStyle *style);
MODULE_SCOPE int    TkPathConfigStyle(Tcl_Interp* interp,
			Tk_PathStyle *stylePtr, int objc,
			Tcl_Obj* const objv[]);

/*
 * end block for C++
 */

#ifdef __cplusplus
}
#endif

#endif      /* INCLUDED_TKPATH_H */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
