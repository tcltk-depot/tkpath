/*
 * tkPathStyle.c --
 *
 *	This file implements style objects used when drawing paths.
 *      See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2005-2008  Mats Bengtsson
 *
 * Note: It would be best to have this in the canvas widget as a special
 *       object, but I see no way of doing this without touching
 *       the canvas code.
 *
 * Note: When a style object is modified or destroyed the corresponding
 *       items are not notified. They will only notice any change when
 *       they need to redisplay.
 *
 */

#include "tkIntPath.h"
#include "tkPathStyle.h"

typedef struct {
    Tcl_HashTable	styleHash;
    Tk_OptionTable 	styleOptionTable;
    int			styleNameUid;
} InterpData;

static const char 	*kStyleNameBase = "tkp::style";
static const char	*kGradientNameBase = "tkp::gradient";

/*
 * Declarationd for functions local to this file.
 */

static int 	StyleObjCmd(ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj* const objv[]);

/*
 * Custom option processing code.
 */

/*
 * The -matrix custom option.
 */

int MatrixSetOption(
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
    char *list;
    Tcl_Size length;
    Tcl_Obj *valuePtr;
    TMatrix *newPtr;

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
            list = Tcl_GetStringFromObj(valuePtr, &length);
            newPtr = (TMatrix *) ckalloc(sizeof(TMatrix));
            if (PathGetTMatrix(interp, list, newPtr) != TCL_OK) {
                ckfree((char *) newPtr);
                return TCL_ERROR;
            }
	} else {
	    newPtr = NULL;
        }
	*((TMatrix **) oldInternalPtr) = *((TMatrix **) internalPtr);
	*((TMatrix **) internalPtr) = newPtr;
    }
    return TCL_OK;
}

Tcl_Obj *
MatrixGetOption(
    ClientData clientData,
    Tk_Window tkwin,
    char *recordPtr,	     /* Pointer to widget record. */
    Tcl_Size internalOffset) /* Offset within *recordPtr containing the
                              * value. */
{
    char 	*internalPtr;
    TMatrix 	*matrixPtr;
    Tcl_Obj 	*listObj;

    /* @@@ An alternative to this could be to have an objOffset in option table. */
    internalPtr = recordPtr + internalOffset;
    matrixPtr = *((TMatrix **) internalPtr);
    PathGetTclObjFromTMatrix(NULL, matrixPtr, &listObj);
    return listObj;
}

void
MatrixRestoreOption(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr,		/* Pointer to storage for value. */
    char *oldInternalPtr)	/* Pointer to old value. */
{
    *(TMatrix **)internalPtr = *(TMatrix **)oldInternalPtr;
}

void
MatrixFreeOption(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr)		/* Pointer to storage for value. */
{
    if (*((char **) internalPtr) != NULL) {
        ckfree(*((char **) internalPtr));
        *((char **) internalPtr) = NULL;
    }
}

/* Return NULL on error and leave error message */

Tk_PathDash *
TkPathDashNew(Tcl_Interp *interp, Tcl_Obj *dashObjPtr)
{
    Tk_PathDash *dashPtr;
    Tcl_Size objc;
    int i;
    double value;
    Tcl_Obj **objv;

    dashPtr = (Tk_PathDash *) ckalloc(sizeof(Tk_PathDash));
    dashPtr->number = 0;
    dashPtr->array = NULL;
    if (Tcl_ListObjGetElements(interp, dashObjPtr, &objc, (Tcl_Obj ***) &objv) != TCL_OK) {
	goto error;
    }
    dashPtr->number = objc;
    dashPtr->array = (float *) ckalloc(objc * sizeof(float));
    for (i = 0; i < objc; i++) {
	if (Tcl_GetDoubleFromObj(interp, objv[i], &value) != TCL_OK) {
	    goto error;
	}
	dashPtr->array[i] = (float) value;
    }
    return dashPtr;

error:
    TkPathDashFree(dashPtr);
    return NULL;
}

void
TkPathDashFree(Tk_PathDash *dashPtr)
{
    if (dashPtr->array) {
	ckfree((char *) dashPtr->array);
    }
    ckfree((char *) dashPtr);
}

/*
 * The -strokedasharray custom option.
 */

/*
 *--------------------------------------------------------------
 *
 * Tk_PathDashOptionSetProc, Tk_PathDashOptionGetProc,
 *	Tk_PathDashOptionRestoreProc, Tk_PathDashOptionRestoreProc --
 *
 *	These functions are invoked during option processing to handle
 *	"-strokedasharray" option for canvas objects.
 *
 * Results:
 *	According to the Tk_ObjCustomOption struct.
 *
 * Side effects:
 *	Memory allocated or freed.
 *
 *--------------------------------------------------------------
 */

int Tk_PathDashOptionSetProc(
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
    Tk_PathDash *newPtr = NULL;

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
	    newPtr = TkPathDashNew(interp, valuePtr);
	    if (newPtr == NULL) {
		return TCL_ERROR;
	    }
        }
	*((Tk_PathDash **) oldInternalPtr) = *((Tk_PathDash **) internalPtr);
	*((Tk_PathDash **) internalPtr) = newPtr;
    }
    return TCL_OK;
}

Tcl_Obj *
Tk_PathDashOptionGetProc(
    ClientData clientData,
    Tk_Window tkwin,
    char *recordPtr,		/* Pointer to widget record. */
    Tcl_Size internalOffset)	/* Offset within *recordPtr containing the
				 * value. */
{
    Tk_PathDash *dashPtr = *((Tk_PathDash **) (recordPtr + internalOffset));
    Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);
    int i;

    if (dashPtr != NULL) {
    for (i = 0; i < dashPtr->number; i++) {
	    Tcl_ListObjAppendElement(NULL, listObj,
				     Tcl_NewDoubleObj(dashPtr->array[i]));
	}
    }
    return listObj;
}

void
Tk_PathDashOptionRestoreProc(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr,		/* Pointer to storage for value. */
    char *oldInternalPtr)	/* Pointer to old value. */
{
    *(Tk_PathDash **)internalPtr = *(Tk_PathDash **)oldInternalPtr;
}

void
Tk_PathDashOptionFreeProc(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr)		/* Pointer to storage for value. */
{
    if (*((char **) internalPtr) != NULL) {
        TkPathDashFree(*(Tk_PathDash **) internalPtr);
        *((char **) internalPtr) = NULL;
    }
}

/*
 * Combined XColor and gradient name in a TkPathColor record.
 */

int PathColorSetOption(
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
    TkPathColor *newPtr = NULL;

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
            newPtr = TkPathNewPathColor(interp, tkwin, valuePtr);
            if (newPtr == NULL) {
                return TCL_ERROR;
            }
        } else {
            newPtr = NULL;
        }
	*((TkPathColor **) oldInternalPtr) = *((TkPathColor **) internalPtr);
	*((TkPathColor **) internalPtr) = newPtr;
    }
    return TCL_OK;
}

Tcl_Obj *
PathColorGetOption(
    ClientData clientData,
    Tk_Window tkwin,
    char *recordPtr,	    	/* Pointer to widget record. */
    Tcl_Size internalOffset) /* Offset within *recordPtr containing the
                              * value. */
{
    char 	*internalPtr;
    Tcl_Obj 	*objPtr = NULL;
    TkPathColor *pathColor = NULL;

    internalPtr = recordPtr + internalOffset;
    pathColor = *((TkPathColor **) internalPtr);
    if (pathColor != NULL) {
        if (pathColor->color) {
            objPtr = Tcl_NewStringObj(Tk_NameOfColor(pathColor->color), -1);
        } else if (pathColor->gradientInstPtr) {
            objPtr = Tcl_NewStringObj(pathColor->gradientInstPtr->masterPtr->name, -1);
        }
    }
    return objPtr;
}

void
PathColorRestoreOption(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr,		/* Pointer to storage for value. */
    char *oldInternalPtr)	/* Pointer to old value. */
{
    *(TkPathColor **)internalPtr = *(TkPathColor **)oldInternalPtr;
}

void
PathColorFreeOption(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr)		/* Pointer to storage for value. */
{
    if (*((char **) internalPtr) != NULL) {
        TkPathFreePathColor(*(TkPathColor **) internalPtr);
        *((char **) internalPtr) = NULL;
    }
}

PATH_STYLE_CUSTOM_OPTION_RECORDS
PATH_OPTION_STRING_TABLES_FILL
PATH_OPTION_STRING_TABLES_STROKE

/*
 * @@@ TODO: BAD I had to duplicate this record here and in tkPathStyle.h.
 *     Else I get problems with offsetof and records.
 */
/* ORLOV
    {TK_OPTION_COLOR, "-stroke", NULL, NULL,
        "black", -1, offsetof(Tk_PathStyle, strokeColor), TK_OPTION_NULL_OK, 0,
	PATH_STYLE_OPTION_STROKE},
*/


static Tk_OptionSpec styleOptionSpecs[] = {
    {TK_OPTION_STRING, "-fill", NULL, NULL,
	"", offsetof(Tk_PathStyle, fillObj), -1,
	TK_OPTION_NULL_OK, 0, PATH_STYLE_OPTION_FILL},
    {TK_OPTION_DOUBLE, "-fillopacity", NULL, NULL,
        "1.0", -1, offsetof(Tk_PathStyle, fillOpacity), 0, 0,
	PATH_STYLE_OPTION_FILL_OPACITY},
    {TK_OPTION_STRING_TABLE, "-fillrule", NULL, NULL,
        "nonzero", -1, offsetof(Tk_PathStyle, fillRule),
	0, (ClientData) fillRuleST, PATH_STYLE_OPTION_FILL_RULE},
    {TK_OPTION_CUSTOM, "-matrix", NULL, NULL,
	NULL, -1, offsetof(Tk_PathStyle, matrixPtr),
	TK_OPTION_NULL_OK, (ClientData) &matrixCO, PATH_STYLE_OPTION_MATRIX},
    {TK_OPTION_STRING, "-stroke", NULL, NULL,
        "", offsetof(Tk_PathStyle, strokeObj), -1,
        TK_OPTION_NULL_OK, 0, PATH_STYLE_OPTION_STROKE},
    {TK_OPTION_CUSTOM, "-strokedasharray", NULL, NULL,
	NULL, -1, offsetof(Tk_PathStyle, dashPtr),
	TK_OPTION_NULL_OK, (ClientData) &dashCO, PATH_STYLE_OPTION_STROKE_DASHARRAY},
    {TK_OPTION_INT, "-strokedashoffset", NULL, NULL,
	"0", -1, offsetof(Tk_PathStyle, offset), 0, 0,
	PATH_STYLE_OPTION_STROKE_DASHOFFSET},
    {TK_OPTION_STRING_TABLE, "-strokelinecap", NULL, NULL,
        "butt", -1, offsetof(Tk_PathStyle, capStyle),
	0, (ClientData) lineCapST, PATH_STYLE_OPTION_STROKE_LINECAP},
    {TK_OPTION_STRING_TABLE, "-strokelinejoin", NULL, NULL,
        "round", -1, offsetof(Tk_PathStyle, joinStyle),
	0, (ClientData) lineJoinST, PATH_STYLE_OPTION_STROKE_LINEJOIN},
    {TK_OPTION_DOUBLE, "-strokemiterlimit", NULL, NULL,
        "4.0", -1, offsetof(Tk_PathStyle, miterLimit), 0, 0,
	PATH_STYLE_OPTION_STROKE_MITERLIMIT},
    {TK_OPTION_DOUBLE, "-strokeopacity", NULL, NULL,
        "1.0", -1, offsetof(Tk_PathStyle, strokeOpacity), 0, 0,
	PATH_STYLE_OPTION_STROKE_OPACITY},
    {TK_OPTION_DOUBLE, "-strokewidth", NULL, NULL,
        "1.0", -1, offsetof(Tk_PathStyle, strokeWidth), 0, 0,
	PATH_STYLE_OPTION_STROKE_WIDTH},
    {TK_OPTION_BOOLEAN, "-filloverstroke", NULL, NULL,
        "0", -1, offsetof(Tk_PathStyle, fillOverStroke), 0, 0,
	PATH_STYLE_OPTION_FILLOVERSTROKE},

    /* @@@ TODO: When this comes into canvas code we should add a -tags option here??? */

    {TK_OPTION_END, NULL, NULL, NULL,
	NULL, 0, -1, 0, (ClientData) NULL, 0}
};

static void
PathStyleInterpDeleted(ClientData clientData)
{
    InterpData *dataPtr = (InterpData *) clientData;

    Tcl_DeleteHashTable(&dataPtr->styleHash);
    ckfree((char *) dataPtr);
}

void
PathStyleInit(Tcl_Interp *interp)
{
    InterpData *dataPtr =
        (InterpData *) Tcl_GetAssocData(interp, kStyleNameBase, NULL);

    if (dataPtr == NULL) {
	dataPtr = (InterpData *) ckalloc(sizeof(InterpData));
	Tcl_InitHashTable(&dataPtr->styleHash, TCL_STRING_KEYS);
	dataPtr->styleOptionTable =
	    Tk_CreateOptionTable(interp, styleOptionSpecs);
	dataPtr->styleNameUid = 0;
	Tcl_SetAssocData(interp, kStyleNameBase,
			 (Tcl_InterpDeleteProc *) PathStyleInterpDeleted,
			 (ClientData) dataPtr);
    }
    Tcl_CreateObjCommand(interp, "tkp::style",
            StyleObjCmd, (ClientData) dataPtr, (Tcl_CmdDeleteProc *) NULL);
}

/*
 * StyleGradientProc: callback to style when gradient changes.
 */

static void
StyleGradientProc(ClientData clientData, int flags)
{
    Tk_PathStyle *stylePtr = (Tk_PathStyle *)clientData;

    if (flags) {
	if (flags & PATH_GRADIENT_FLAG_DELETE) {
	    TkPathFreePathColor(stylePtr->fill);
	    stylePtr->fill = NULL;
	    Tcl_DecrRefCount(stylePtr->fillObj);
	    stylePtr->fillObj = NULL;
/*ORLOV*/
	    TkPathFreePathColor(stylePtr->strokeColor);
	    stylePtr->strokeColor = NULL;
	    Tcl_DecrRefCount(stylePtr->strokeObj);
	    stylePtr->strokeObj = NULL;

	}
	TkPathStyleChanged(stylePtr, flags);
    }
}

static void
PathStyleFree(Tk_PathStyle *stylePtr, Tk_Window tkwin)
{
    if (stylePtr->fill != NULL) {
	TkPathFreePathColor(stylePtr->fill);
    }
    Tk_FreeConfigOptions((char *) stylePtr, stylePtr->optionTable, tkwin);
    ckfree((char *) stylePtr);
}

static int
FindPathStyle(Tcl_Interp *interp, Tcl_Obj *nameObj, Tcl_HashTable *tablePtr, Tk_PathStyle **s)
{
    Tcl_HashEntry   *hPtr;
    char *name = Tcl_GetString(nameObj);
    *s = NULL;
    hPtr = Tcl_FindHashEntry(tablePtr, name);
    if (hPtr == NULL) {
	Tcl_Obj *resultObj;
	resultObj = Tcl_NewStringObj("style \"", -1);
	Tcl_AppendStringsToObj(resultObj, name, "\" doesn't exist", (char *) NULL);
	Tcl_SetObjResult(interp, resultObj);
	return TCL_ERROR;
    }
    *s = (Tk_PathStyle *) Tcl_GetHashValue(hPtr);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * PathStyleCget, Configure, Create, Delete, InUse, Names --
 *
 *	These functions implement style object commands in a generic way.
 *	The Tcl_HashTable defines the style namespace.
 *
 * Results:
 *	Varies: typically a standard tcl result or void.
 *
 * Side effects:
 *	Varies.
 *
 *--------------------------------------------------------------
 */

int
PathStyleCget(Tcl_Interp *interp, Tk_Window tkwin, int objc, Tcl_Obj * const objv[],
    Tcl_HashTable *tablePtr)
{
    Tk_PathStyle    *stylePtr = NULL;
    Tcl_Obj	    *resultObj = NULL;

    if (FindPathStyle(interp, objv[0], tablePtr, &stylePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    resultObj = Tk_GetOptionValue(interp, (char *)stylePtr,
	    stylePtr->optionTable, objv[1], tkwin);
    if (resultObj == NULL) {
	return TCL_ERROR;
    } else {
	Tcl_SetObjResult(interp, resultObj);
    }
    return TCL_OK;
}

int
PathStyleConfigure(Tcl_Interp *interp, Tk_Window tkwin, int objc, Tcl_Obj * const objv[],
    Tcl_HashTable *styleTablePtr, Tcl_HashTable *gradTablePtr)
{
    int		    mask;
    Tk_PathStyle    *stylePtr = NULL;
    Tcl_Obj	    *resultObj = NULL;

    if (FindPathStyle(interp, objv[0], styleTablePtr, &stylePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc <= 2) {
	resultObj = Tk_GetOptionInfo(interp, (char *)stylePtr,
		stylePtr->optionTable,
		(objc == 1) ? (Tcl_Obj *) NULL : objv[1], tkwin);
	if (resultObj == NULL) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, resultObj);
    } else {
	TkPathColor *fillPtr = NULL;

	/* @@@ TODO: loop error to recover using savedOptions! */
	if (Tk_SetOptions(interp, (char *)stylePtr, stylePtr->optionTable,
		objc - 1, objv + 1, tkwin, NULL, &mask) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (stylePtr->fillObj != NULL) {
	    fillPtr = TkPathGetPathColor(interp, tkwin, stylePtr->fillObj,
		    gradTablePtr, StyleGradientProc, (ClientData) stylePtr);
	    if (fillPtr == NULL) {
		return TCL_ERROR;
	    }
	} else {
	    fillPtr = NULL;
	}
	/* Free any old and store the new. */
	if (stylePtr->fill != NULL) {
	    TkPathFreePathColor(stylePtr->fill);
	}
	stylePtr->fill = fillPtr;
/*ORLOV*/
	if (stylePtr->strokeObj != NULL) {
	    fillPtr = TkPathGetPathColor(interp, tkwin, stylePtr->strokeObj,
		    gradTablePtr, StyleGradientProc, (ClientData) stylePtr);
	    if (fillPtr == NULL) {
		return TCL_ERROR;
	    }
	} else {
	    fillPtr = NULL;
	}
	/* Free any old and store the new. */
	if (stylePtr->strokeColor != NULL) {
	    TkPathFreePathColor(stylePtr->strokeColor);
	}
	stylePtr->strokeColor = fillPtr;


	/*
	 * Let mask be the cumalative options set.
	 */
	stylePtr->mask |= mask;
    }
    TkPathStyleChanged(stylePtr, PATH_STYLE_FLAG_CONFIGURE);
    return TCL_OK;
}

int
PathStyleCreate(Tcl_Interp *interp, Tk_Window tkwin, int objc,
		Tcl_Obj * const objv[],
		Tcl_HashTable *styleTablePtr,
		Tcl_HashTable *gradTablePtr, char *tokenName)
{
    int		    isNew;
    int		    mask;
    Tcl_HashEntry   *hPtr;
    Tk_PathStyle    *stylePtr = NULL;
    TkPathColor	    *fillPtr = NULL;
    InterpData      *dataPtr =
        (InterpData *) Tcl_GetAssocData(interp, kStyleNameBase, NULL);

    stylePtr = (Tk_PathStyle *) ckalloc(sizeof(Tk_PathStyle));
    memset(stylePtr, '\0', sizeof(Tk_PathStyle));

    /* Fill in defaults. */
    TkPathInitStyle(stylePtr);

    /*
     * Create the option table for this class.  If it has already
     * been created, the cached pointer will be returned.
     */
    stylePtr->optionTable = dataPtr->styleOptionTable;
    stylePtr->name = Tk_GetUid(tokenName);

    if (Tk_InitOptions(interp, (char *)stylePtr,
	    stylePtr->optionTable, tkwin) != TCL_OK) {
	ckfree((char *)stylePtr);
	return TCL_ERROR;
    }
    if (Tk_SetOptions(interp, (char *)stylePtr, stylePtr->optionTable,
	    objc, objv, tkwin, NULL, &mask) != TCL_OK) {
	Tk_FreeConfigOptions((char *)stylePtr, stylePtr->optionTable, NULL);
	ckfree((char *)stylePtr);
	return TCL_ERROR;
    }
    if (stylePtr->fillObj != NULL) {
	fillPtr = TkPathGetPathColor(interp, tkwin, stylePtr->fillObj,
		gradTablePtr, StyleGradientProc, (ClientData) stylePtr);
	if (fillPtr == NULL) {
	    Tk_FreeConfigOptions((char *)stylePtr, stylePtr->optionTable, NULL);
	    ckfree((char *)stylePtr);
	    return TCL_ERROR;
	}
    } else {
	fillPtr = NULL;
    }
    stylePtr->fill = fillPtr;
/*ORLOV*/
    if (stylePtr->strokeObj != NULL) {
	fillPtr = TkPathGetPathColor(interp, tkwin, stylePtr->strokeObj,
		gradTablePtr, StyleGradientProc, (ClientData) stylePtr);
	if (fillPtr == NULL) {
	    Tk_FreeConfigOptions((char *)stylePtr, stylePtr->optionTable, NULL);
	    ckfree((char *)stylePtr);
	    return TCL_ERROR;
	}
    } else {
	fillPtr = NULL;
    }
    stylePtr->strokeColor = fillPtr;

    /*
     * Let mask be the cumalative options set.
     */
    stylePtr->mask |= mask;
    hPtr = Tcl_CreateHashEntry(styleTablePtr, tokenName, &isNew);
    Tcl_SetHashValue(hPtr, stylePtr);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(tokenName, -1));
    return TCL_OK;
}

int
PathStyleDelete(Tcl_Interp *interp, Tcl_Obj *obj, Tcl_HashTable *tablePtr, Tk_Window tkwin)
{
    Tk_PathStyle    *stylePtr = NULL;

    if (FindPathStyle(interp, obj, tablePtr, &stylePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    TkPathStyleChanged(stylePtr, PATH_STYLE_FLAG_DELETE);
    Tcl_DeleteHashEntry(Tcl_FindHashEntry(tablePtr, Tcl_GetString(obj)));
    PathStyleFree(stylePtr, tkwin);
    return TCL_OK;
}

int
PathStyleInUse(Tcl_Interp *interp, Tcl_Obj *obj, Tcl_HashTable *tablePtr)
{
    Tk_PathStyle    *stylePtr = NULL;

    if (FindPathStyle(interp, obj, tablePtr, &stylePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), stylePtr->instancePtr != NULL);
    return TCL_OK;
}

void
PathStyleNames(Tcl_Interp *interp, Tcl_HashTable *tablePtr)
{
    char	    *name;
    Tcl_HashEntry   *hPtr;
    Tcl_Obj	    *listObj;
    Tcl_HashSearch  search;

    listObj = Tcl_NewListObj(0, NULL);
    hPtr = Tcl_FirstHashEntry(tablePtr, &search);
    while (hPtr != NULL) {
	name = (char *) Tcl_GetHashKey(tablePtr, hPtr);
	Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj(name, -1));
	hPtr = Tcl_NextHashEntry(&search);
    }
    Tcl_SetObjResult(interp, listObj);
}

/*
 *----------------------------------------------------------------------
 *
 * TkPathConfigStyle --
 *
 *	Parses a list of Tcl objects to an already allocated Tk_PathStyle.
 *
 * Results:
 *	Standard Tcl result
 *
 * Side effects:
 *	Options allocated. Use Tk_FreeConfigOptions when finished.
 *
 *----------------------------------------------------------------------
 */

int
TkPathConfigStyle(Tcl_Interp *interp, Tk_PathStyle *stylePtr,
		  int objc, Tcl_Obj * const objv[])
{
    Tk_Window tkwin = Tk_MainWindow(interp);
    InterpData *dataPtr =
        (InterpData *) Tcl_GetAssocData(interp, kStyleNameBase, NULL);

    stylePtr->optionTable = dataPtr->styleOptionTable;
    if (Tk_InitOptions(interp, (char *) stylePtr, dataPtr->styleOptionTable,
		       tkwin) != TCL_OK) {
        return TCL_ERROR;
    }
    if (Tk_SetOptions(interp, (char *) stylePtr, dataPtr->styleOptionTable,
            objc, objv, tkwin, NULL, NULL) != TCL_OK) {
        Tk_FreeConfigOptions((char *) stylePtr, dataPtr->styleOptionTable,
			     NULL);
        return TCL_ERROR;
    }
    return TCL_OK;
}

static const char *styleCmds[] = {
    "cget", "configure", "create", "delete", "inuse", "names",
    (char *) NULL
};

enum {
    kPathStyleCmdCget	= 0L,
    kPathStyleCmdConfigure,
    kPathStyleCmdCreate,
    kPathStyleCmdDelete,
    kPathStyleCmdInUse,
    kPathStyleCmdNames
};

/*
 *----------------------------------------------------------------------
 *
 * StyleObjCmd --
 *
 *	This implements the standalone tkp::style command.
 *
 * Results:
 *	Standard Tcl result
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static int
StyleObjCmd(
        ClientData clientData,
        Tcl_Interp *interp,
        int objc,
      	Tcl_Obj * const objv[] )
{
    InterpData *dataPtr = (InterpData *) clientData;
    int index;
    int result = TCL_OK;
    Tk_Window tkwin = Tk_MainWindow(interp);

    /*
     * objv[1] is the subcommand: cget | configure | create | delete | names
     */
    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "command ?arg arg...?");
        return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], styleCmds, "command", 0,
	    &index) != TCL_OK) {
        return TCL_ERROR;
    }
    switch (index) {

        case kPathStyleCmdCget: {
	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 2, objv, "name option");
		return TCL_ERROR;
	    }
	    result = PathStyleCget(interp, tkwin, objc-2, objv+2,
				   &dataPtr->styleHash);
            break;
        }

        case kPathStyleCmdConfigure: {
	    Tcl_HashTable *gradientHashPtr =
	        (Tcl_HashTable *) Tcl_GetAssocData(interp, kGradientNameBase,
						   NULL);

	    if (objc < 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "name ?option? ?value option value...?");
		return TCL_ERROR;
	    }
	    result = PathStyleConfigure(interp, tkwin, objc-2, objv+2,
					&dataPtr->styleHash, gradientHashPtr);
            break;
        }

        case kPathStyleCmdCreate: {
	    Tcl_HashTable *gradientHashPtr =
	        (Tcl_HashTable *) Tcl_GetAssocData(interp, kGradientNameBase,
						   NULL);
	    char str[255];

	    if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "?option value...?");
		return TCL_ERROR;
	    }
            sprintf(str, "%s%d", kStyleNameBase, dataPtr->styleNameUid++);
	    result = PathStyleCreate(interp, tkwin, objc-2, objv+2,
				     &dataPtr->styleHash,
				     gradientHashPtr, str);
            break;
        }

        case kPathStyleCmdDelete: {
	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "name");
		return TCL_ERROR;
	    }
	    result = PathStyleDelete(interp, objv[2], &dataPtr->styleHash,
				     tkwin);
	    break;
        }

	case kPathStyleCmdInUse: {
	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "name");
		return TCL_ERROR;
	    }
	    result = PathStyleInUse(interp, objv[2], &dataPtr->styleHash);
	    break;
	}

        case kPathStyleCmdNames: {
	    if (objc != 2) {
		Tcl_WrongNumArgs(interp, 2, objv, NULL);
		return TCL_ERROR;
	    }
	    PathStyleNames(interp, &dataPtr->styleHash);
            break;
        }
    }
    return result;
}

void
PathStylesFree(Tk_Window tkwin, Tcl_HashTable *hashTablePtr)
{
    Tcl_HashEntry 	*hPtr;
    Tcl_HashSearch	search;
    char		*recordPtr;

    hPtr = Tcl_FirstHashEntry(hashTablePtr, &search);
    while (hPtr != NULL) {
	recordPtr = (char *) Tcl_GetHashValue(hPtr);
	Tcl_DeleteHashEntry(hPtr);
	PathStyleFree((Tk_PathStyle *)recordPtr, tkwin);
	hPtr = Tcl_NextHashEntry(&search);
    }
}

#if 0
static void
CopyXColor(Tk_Window tkwin, XColor **dstPtrPtr, XColor *srcPtr)
{
    XColor *dstPtr;
    XColor *colorPtr = NULL;

    dstPtr = *dstPtrPtr;
    if ((dstPtr == NULL) && (srcPtr == NULL)) {
        /* empty */
    } else if (dstPtr == NULL) {
        colorPtr = Tk_GetColorByValue(tkwin, srcPtr);
    } else {
        Tk_FreeColor(dstPtr);
        colorPtr = Tk_GetColorByValue(tkwin, srcPtr);
    }
    *dstPtrPtr = colorPtr;
}
#endif

#if 0
static void
CopyTMatrix(TMatrix **dstPtrPtr, TMatrix *srcPtr)
{
    TMatrix *dstPtr;
    TMatrix *matrixPtr = NULL;

    dstPtr = *dstPtrPtr;
    if ((dstPtr == NULL) && (srcPtr == NULL)) {
        /* empty */
    } else if (dstPtr == NULL) {
        matrixPtr = (TMatrix *) ckalloc(sizeof(TMatrix));
        *matrixPtr = *srcPtr;
    } else {
        *matrixPtr = *srcPtr;
    }
    *dstPtrPtr = matrixPtr;
}
#endif

#if 0
static void
CopyTkDash(Tk_PathDash *dstPtr, Tk_PathDash *srcPtr)
{
    int i;
    float *dptr, *sptr;

    if (dstPtr != NULL) {
	TkPathDashFree(dstPtr);
    }
    dstPtr->number = srcPtr->number;
    dptr = dstPtr->array;
    sptr = srcPtr->array;
    for (i = 0; i < srcPtr->number; i++) {
	*dptr++ = *sptr++;
    }
}
#endif

#if 0
static void
CopyPathColor(Tk_Window tkwin, TkPathColor **dstPtrPtr, TkPathColor *srcPtr)
{
    TkPathColor *dstPtr;
    TkPathColor *pathColorPtr = NULL;

    dstPtr = *dstPtrPtr;
    if ((dstPtr == NULL) && (srcPtr == NULL)) {
        /* empty */
    } else {
        if (dstPtr != NULL) {
            TkPathFreePathColor(dstPtr);
        }
        pathColorPtr = (TkPathColor *) ckalloc(sizeof(TkPathColor));
	pathColorPtr->color = NULL;
	pathColorPtr->gradientName = NULL;
        if (srcPtr->color != NULL) {
            pathColorPtr->color = Tk_GetColorByValue(tkwin, srcPtr->color);

	    /* @@@ TODO */
        } else if (srcPtr->gradientName != NULL) {
            pathColorPtr->gradientName = (char *) ckalloc(strlen(srcPtr->gradientName) + 1);
            strcpy(pathColorPtr->gradientName, srcPtr->gradientName);
        }
    }
    *dstPtrPtr = pathColorPtr;
}
#endif

/*
 *--------------------------------------------------------------
 *
 * TkPathStyleMergeStyleStatic --
 *
 *	Looks up the named style in styleObj in the globally defined
 *	style hash table.
 *	Overwrites values in dstStyle if set in styleObj.
 *	This is indicated by the mask of the srcStyle.
 *	This just copy pointers. For short lived style records only!
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 *	Changes *values* in dstStyle. Leaves any error string in interp.
 *
 *--------------------------------------------------------------
 */

int
TkPathStyleMergeStyleStatic(Tcl_Interp* interp, Tcl_Obj *styleObj,
			    Tk_PathStyle *dstStyle, long flags)
{
    Tcl_HashEntry *hPtr;
    Tk_PathStyle *srcStyle;
    InterpData *dataPtr =
        (InterpData *) Tcl_GetAssocData(interp, kStyleNameBase, NULL);

    if (styleObj == NULL) {
	return TCL_OK;
    }
    if (dataPtr == NULL) {
        goto noStyle;
    }
    hPtr = Tcl_FindHashEntry(&dataPtr->styleHash, Tcl_GetString(styleObj));
    if (hPtr == NULL) {
noStyle:
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"the global style \"", Tcl_GetString(styleObj),
		"\" does not exist", NULL);
        return TCL_ERROR;
    }
    srcStyle = (Tk_PathStyle *) Tcl_GetHashValue(hPtr);
    TkPathStyleMergeStyles(srcStyle, dstStyle, flags);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * TkPathStyleMergeStyles --
 *
 *	Overwrites values in dstStyle if set in srcStyle.
 *	This is indicated by the mask of the srcStyle.
 *	This just copy pointers. For short lived style records only!
 *	Be sure to NEVER free any pointers in this style since we
 *	don't own theme!
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes *values* in dstStyle.
 *
 *--------------------------------------------------------------
 */

void
TkPathStyleMergeStyles(
    Tk_PathStyle *srcStyle,
    Tk_PathStyle *dstStyle,
    long flags)
{
    int mask = srcStyle->mask;

    if (mask == 0) {
	return;
    }

    /*
     * Go through all options set in srcStylePtr and merge
     * these into dstStylePtr.
     */
    if (!(flags & kPathMergeStyleNotFill)) {
        if (mask & PATH_STYLE_OPTION_FILL) {
	    dstStyle->fill = srcStyle->fill;
        }
        if (mask & PATH_STYLE_OPTION_FILL_OFFSET) {
            /* @@@ TODO */
        }
        if (mask & PATH_STYLE_OPTION_FILL_OPACITY) {
            dstStyle->fillOpacity = srcStyle->fillOpacity;
        }
        if (mask & PATH_STYLE_OPTION_FILL_RULE) {
            dstStyle->fillRule = srcStyle->fillRule;
        }
        if (mask & PATH_STYLE_OPTION_FILL_STIPPLE) {
            /* @@@ TODO */
        }
    }
    if (mask & PATH_STYLE_OPTION_MATRIX) {
	dstStyle->matrixPtr = srcStyle->matrixPtr;
    }
    if (!(flags & kPathMergeStyleNotStroke)) {
        if (mask & PATH_STYLE_OPTION_STROKE) {
	    dstStyle->strokeColor = srcStyle->strokeColor;
        }
        if (mask & PATH_STYLE_OPTION_STROKE_DASHARRAY) {
	    dstStyle->dashPtr = srcStyle->dashPtr;
        }
        if (mask & PATH_STYLE_OPTION_STROKE_DASHOFFSET) {
	    dstStyle->offset = srcStyle->offset;
        }
        if (mask & PATH_STYLE_OPTION_STROKE_LINECAP) {
            dstStyle->capStyle = srcStyle->capStyle;
        }
        if (mask & PATH_STYLE_OPTION_STROKE_LINEJOIN) {
            dstStyle->joinStyle = srcStyle->joinStyle;
        }
        if (mask & PATH_STYLE_OPTION_STROKE_MITERLIMIT) {
            dstStyle->miterLimit = srcStyle->miterLimit;
        }
        if (mask & PATH_STYLE_OPTION_STROKE_OFFSET) {
            /* @@@ TODO */
        }
        if (mask & PATH_STYLE_OPTION_STROKE_OPACITY) {
            dstStyle->strokeOpacity = srcStyle->strokeOpacity;
        }
        if (mask & PATH_STYLE_OPTION_STROKE_STIPPLE) {
            /* @@@ TODO */
        }
        if (mask & PATH_STYLE_OPTION_STROKE_WIDTH) {
            dstStyle->strokeWidth = srcStyle->strokeWidth;
        }
        if (mask & PATH_STYLE_OPTION_FILLOVERSTROKE) {
            dstStyle->fillOverStroke = srcStyle->fillOverStroke;
        }
    }
    dstStyle->mask |= mask;
}

/*
 *--------------------------------------------------------------
 *
 * TkPathInitStyle
 *
 *	This procedure initializes the Tk_PathStyle structure
 *	with default values.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *
 *--------------------------------------------------------------
 */

void
TkPathInitStyle(Tk_PathStyle *style)
{
    memset(style, '\0', sizeof(Tk_PathStyle));

    style->mask = 0;
    style->strokeObj = NULL;
    style->strokeColor = NULL;
    style->strokeWidth = 1.0;
    style->strokeOpacity = 1.0;
    style->fillOverStroke = 0;
    style->offset = 0;
    style->dashPtr = NULL;
    style->capStyle = CapButt;
    style->joinStyle = JoinRound;

    style->fillOpacity = 1.0;
    style->fillRule = WindingRule;
    style->fillObj = NULL;
    style->fill = NULL;
    style->matrixPtr = NULL;
    style->instancePtr = NULL;
}

/*
 *--------------------------------------------------------------
 *
 * TkPathDeleteStyle
 *
 *	This procedure frees all memory in the Tk_PathStyle structure
 *	that is not freed by Tk_FreeConfigOptions.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Memory freed
 *
 *--------------------------------------------------------------
 */

void
TkPathDeleteStyle(Tk_PathStyle *style)
{
    if (style->fill != NULL) {
	TkPathFreePathColor(style->fill);
    }
}


/*
 * These functions are called by users of styles, typically items,
 * that make instances of styles from a style object (master).
 */

/*
 *----------------------------------------------------------------------
 *
 * TkPathGetStyle --
 *
 *	This function is invoked by an item when it wants to use a particular
 *	style for a particular hash table. Compare Tk_GetImage.
 *
 * Results:
 *	The return value is a token for the style. If there is no style by the
 *	given name, then NULL is returned and an error message is left in the
 *	interp's result.
 *
 * Side effects:
 *	Tk records the fact that the item is using the style, and it will
 *	invoke changeProc later if the item needs redisplay. The caller must
 *	eventually invoke TkPathFreeStyle when it no longer needs the style.
 *
 *----------------------------------------------------------------------
 */

TkPathStyleInst *
TkPathGetStyle(
    Tcl_Interp *interp,
    const char *name,
    Tcl_HashTable *tablePtr,
    TkPathGradientChangedProc *changeProc,
    ClientData clientData)
{
    Tcl_HashEntry *hPtr;
    TkPathStyleInst *stylePtr;
    Tk_PathStyle *masterPtr;

    hPtr = Tcl_FindHashEntry(tablePtr, name);
    if (hPtr == NULL) {
	if (interp != NULL) {
            Tcl_Obj *resultObj;
            resultObj = Tcl_NewStringObj("style \"", -1);
            Tcl_AppendStringsToObj(resultObj, name, "\" doesn't exist", (char *) NULL);
            Tcl_SetObjResult(interp, resultObj);
	}
	return NULL;
    }
    masterPtr = (Tk_PathStyle *) Tcl_GetHashValue(hPtr);
    stylePtr = (TkPathStyleInst *) ckalloc(sizeof(TkPathStyleInst));
    stylePtr->masterPtr = masterPtr;
    stylePtr->changeProc = changeProc;
    stylePtr->clientData = clientData;
    stylePtr->nextPtr = masterPtr->instancePtr;
    masterPtr->instancePtr = stylePtr;
    return stylePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TkPathFreeStyle --
 *
 *	This function is invoked by an item when it no longer needs a gradient
 *	acquired by a previous call to TkPathGetGradient. For each call to
 *	TkPathGetGradient there must be exactly one call to TkPathFreeGradient.
 *	Compare Tk_FreeImage.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The association between the gradient and the item is removed.
 *
 *----------------------------------------------------------------------
 */

void
TkPathFreeStyle(
    TkPathStyleInst *stylePtr)
{
    Tk_PathStyle *masterPtr = stylePtr->masterPtr;
    TkPathStyleInst *walkPtr;

    walkPtr = masterPtr->instancePtr;
    if (walkPtr == stylePtr) {
	masterPtr->instancePtr = stylePtr->nextPtr;
    } else {
	while (walkPtr->nextPtr != stylePtr) {
	    walkPtr = walkPtr->nextPtr;
	}
	walkPtr->nextPtr = stylePtr->nextPtr;
    }
    ckfree((char *)stylePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TkPathStyleChanged --
 *
 *	This function is called by a style manager whenever something has
 *	happened that requires the style to be redrawn or it has been deleted.
 *	Compare Tk_ImageChanged,
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Any items that display the style are notified so that they can
 *	redisplay themselves as appropriate.
 *
 *----------------------------------------------------------------------
 */

void
TkPathStyleChanged(Tk_PathStyle *masterPtr, int flags)
{
    TkPathStyleInst *walkPtr, *nextPtr;

    if (flags) {
	/*
	 * NB: We may implicitly call TkPathFreeGradient if being deleted!
	 *     Therefore cache the nextPtr before invoking changeProc.
	 */
	for (walkPtr = masterPtr->instancePtr; walkPtr != NULL; ) {
	    nextPtr = walkPtr->nextPtr;
	    if (walkPtr->changeProc != NULL) {
		(*walkPtr->changeProc)(walkPtr->clientData, flags);
	    }
	    walkPtr = nextPtr;
	}
    }
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
