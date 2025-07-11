/*
 * tkCanvArrow.h --
 *
 *	Header file for arrow heads
 *
 * Copyright (c) 2014 OpenSim Ltd., author:Zoltan Bojthe
 *
 */

#ifndef INCLUDED_TKCANVARROW_H
#define INCLUDED_TKCANVARROW_H

#include "tkPath.h"

/*
 * For C++ compilers, use extern "C"
 */

#ifdef __cplusplus
extern "C" {
#endif


/* for arrows: */

/* ARROW_BOTH = ARROWS_FIRST | ARROWS_LAST */
typedef enum {
    ARROWS_OFF = 0, ARROWS_ON = 1
} ArrowState;

typedef struct ArrowDescr
{
    ArrowState arrowEnabled;    /* Indicates whether or not to draw arrowheads: off/on */
    double arrowLength;         /* Length of arrowhead. */
    double arrowWidth;          /* width of arrowhead. */
    double arrowFillRatio;      /* filled part of arrow head, relative to arrowLengthRel.
                                 * 0: special case, arrowhead only 2 line, without fill */
    PathPoint *arrowPointsPtr;  /* Points to array of PTS_IN_ARROW points
                                 * describing polygon for arrowhead in line.
                                 * NULL means no arrowhead at current point. */
} ArrowDescr;

MODULE_SCOPE void	TkPathArrowDescrInit(ArrowDescr *descr);
MODULE_SCOPE void	TkPathPreconfigureArrow(PathPoint *pf,
			    ArrowDescr *arrowDescr);
MODULE_SCOPE PathPoint	TkPathConfigureArrow(PathPoint pf, PathPoint pl,
			   ArrowDescr *arrow,
			   Tk_PathStyle *lineStyle, int updateFirstPoint);
MODULE_SCOPE void	TkPathIncludeArrowPoints(Tk_PathItem *itemPtr,
			    ArrowDescr *arrowDescrPtr);
MODULE_SCOPE void	IncludeArrowPointsInRect(PathRect *bbox,
			    ArrowDescr *arrowDescrPtr);
MODULE_SCOPE void	TkPathTranslateArrow(ArrowDescr *arrowDescr,
			    double deltaX, double deltaY);

/*
 * scales only original point, should use TkPathPreconfigureArrow() and
 * TkPathConfigureArrow(), too
 */

MODULE_SCOPE void	TkPathScaleArrow(ArrowDescr *arrowDescr, double originX,
			    double originY, double scaleX, double scaleY);
MODULE_SCOPE void	TkPathFreeArrow(ArrowDescr *arrowDescr);
MODULE_SCOPE int	GetSegmentsFromPathAtomList(PathAtom *firstAtom,
			    PathPoint **firstPt, PathPoint *secondPt,
			    PathPoint *penultPt, PathPoint **lastPt);
MODULE_SCOPE PathAtom *	MakePathAtomsFromArrow(ArrowDescr *arrowDescr);
MODULE_SCOPE void	DisplayArrow(Tk_PathCanvas canvas, ArrowDescr *arrowDescr,
			    Tk_PathStyle *const style, TMatrix *mPtr,
			    PathRect *bboxPtr);
MODULE_SCOPE void	PaintArrow(TkPathContext context,
			    ArrowDescr *arrowDescr,
			    Tk_PathStyle *const style, PathRect *bboxPtr);

#define PATH_OPTION_SPEC_ARROWLENGTH_DEFAULT  "10.0"
#define PATH_OPTION_SPEC_ARROWWIDTH_DEFAULT    "5.0"
#define PATH_OPTION_SPEC_ARROWFILL_DEFAULT     "0.7"

#define PATH_OPTION_SPEC_STARTARROW(Item)				\
    {TK_OPTION_BOOLEAN, "-startarrow", NULL, NULL,			\
	"0", -1, offsetof(Item, startarrow.arrowEnabled),		\
	0, 0, 0}

#define PATH_OPTION_SPEC_STARTARROWLENGTH(Item)				\
    {TK_OPTION_DOUBLE, "-startarrowlength", NULL, NULL,			\
	PATH_OPTION_SPEC_ARROWLENGTH_DEFAULT, -1,			\
	offsetof(Item, startarrow.arrowLength),			\
	0, 0, 0}

#define PATH_OPTION_SPEC_STARTARROWWIDTH(Item)				\
    {TK_OPTION_DOUBLE, "-startarrowwidth", NULL, NULL,			\
	PATH_OPTION_SPEC_ARROWWIDTH_DEFAULT, -1,			\
	offsetof(Item, startarrow.arrowWidth),				\
	0, 0, 0}

#define PATH_OPTION_SPEC_STARTARROWFILL(Item)				\
    {TK_OPTION_DOUBLE, "-startarrowfill", NULL, NULL,			\
	PATH_OPTION_SPEC_ARROWFILL_DEFAULT, -1,				\
	offsetof(Item, startarrow.arrowFillRatio),			\
	0, 0, 0}

#define PATH_OPTION_SPEC_ENDARROW(Item)					\
    {TK_OPTION_BOOLEAN, "-endarrow", NULL, NULL,			\
	"0", -1, offsetof(Item, endarrow.arrowEnabled),		\
	0, 0, 0}

#define PATH_OPTION_SPEC_ENDARROWLENGTH(Item)				\
    {TK_OPTION_DOUBLE, "-endarrowlength", NULL, NULL,			\
	PATH_OPTION_SPEC_ARROWLENGTH_DEFAULT, -1,			\
	offsetof(Item, endarrow.arrowLength),				\
	0, 0, 0}

#define PATH_OPTION_SPEC_ENDARROWWIDTH(Item)				\
    {TK_OPTION_DOUBLE, "-endarrowwidth", NULL, NULL,			\
	PATH_OPTION_SPEC_ARROWWIDTH_DEFAULT, -1,			\
	offsetof(Item, endarrow.arrowWidth),				\
	0, 0, 0}

#define PATH_OPTION_SPEC_ENDARROWFILL(Item)				\
    {TK_OPTION_DOUBLE, "-endarrowfill", NULL, NULL,			\
	PATH_OPTION_SPEC_ARROWFILL_DEFAULT, -1,				\
	offsetof(Item, endarrow.arrowFillRatio),			\
	0, 0, 0}

#define PATH_OPTION_SPEC_STARTARROW_GRP(Item)		\
	PATH_OPTION_SPEC_STARTARROW(Item),		\
	PATH_OPTION_SPEC_STARTARROWLENGTH(Item),	\
	PATH_OPTION_SPEC_STARTARROWWIDTH(Item),		\
	PATH_OPTION_SPEC_STARTARROWFILL(Item)

#define PATH_OPTION_SPEC_ENDARROW_GRP(Item)		\
	PATH_OPTION_SPEC_ENDARROW(Item),		\
	PATH_OPTION_SPEC_ENDARROWLENGTH(Item),		\
	PATH_OPTION_SPEC_ENDARROWWIDTH(Item),		\
	PATH_OPTION_SPEC_ENDARROWFILL(Item)

#ifdef __cplusplus
}
#endif

#endif  /* INCLUDED_TKCANVARROW_H */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */

