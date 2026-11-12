/* Copyright (C) 2026 Chris Piker
 *
 * This file is part of das3js, das stream utilities for browsers.
 * See the LICENSE file in the repository root for terms.
 */

/* Implementation of the flat FFI layer, see wasm.h for the contract.
 *
 * Everything here is a projection: no logic beyond bounds checks lives in
 * this file, so a fix in the object model is automatically a fix here.
 */

#define _POSIX_C_SOURCE 200112L

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include <das3/core.h>
#include <das3/form.h>

/* Private das2C headers, for _jb_source only.  

   Candidate for upstream: a public sequence accessor, after which
      these two includes can go.

   The value generators (DasGen) have no public face, but a
   sequence's intercept and slopes are part of the snapshot contract 
   and theres no public access for them.  gen_p.h allows access for 
   statically linked programs like this one.  Contact upstream and
   ask for a public interface.  
*/
#include <das3/var_p.h>
#include <das3/gen_p.h>

/* Private too, for the source snapshot only: das2C has a short caption for
   a group (DasSrcGroup_label) but none for a property, and gives an enum
   control's values but not their display labels.  Both are read from the
   node through the property's dict.  Upstream request in das2C's co_notes,
   request_das3js_srcprop_labels.md. */
#include <das3/source_p.h>

#include "wasm.h"

/* ************************************************************************* */
/* Library level */

/* das2C keeps no copy of an error message: daserrf() hands it to
   the log hook and returns.  A foreign caller asks for the text after the
   failing call has returned, so the hook keeps the newest one here.  The wasm
   target is single threaded, so a static is fine. */
static char g_sLastErr[DASLOG_MSG_SZ] = {'\0'};

/* Runs inside das2C's logger: may not call back into das2C except for
   daslog_stderr(), see log.h. */
static void _daswasm_onLog(const DasLogMsg* pMsg, void* pUser)
{
	(void)pUser;
	if(pMsg->errCode != DAS_OKAY)
		snprintf(g_sLastErr, sizeof(g_sLastErr), "%s", pMsg->msg);

	/* Errors reach the console too: emscripten maps stderr there */
	daslog_stderr(pMsg, NULL);
}

void daswasm_init(const char* sProgName)
{
	/* daslib_init copies the hook, so a local is fine */
	DasLogHook hook = {_daswasm_onLog, NULL};
	daslib_init(sProgName, DASERRDISP_RET, DASLOG_WARN, &hook);
}

const char* daswasm_lastError(void){ return g_sLastErr; }

void daswasm_clearError(void){ g_sLastErr[0] = '\0'; }

/* Static result, see daswasm_lastError for why that's fine here */
const char* daswasm_timeIsoC(const char* sTime, int nFrac)
{
	static char sBuf[64];
	DasTime dt;
	sBuf[0] = '\0';
	if((sTime != NULL) && DasTime_fromStr(&dt, sTime))
		DasTime_toIsoC(&dt, nFrac, sBuf, sizeof(sBuf));
	return sBuf;
}

int daswasm_idxRagged(void){ return DASIDX_RAGGED; }

int daswasm_idxUnused(void){ return DASIDX_UNUSED; }

int daswasm_idxBorrow(void){ return DASIDX_BORROW; }

/* ************************************************************************* */
/* Reader context */

#define DASWASM_EXCEPT_SZ 1024

struct das_wasm_rdr {
	DasIO* pIo;
	DasIOProc oobs;            /* progress & exception capture callbacks */
	int nTaskSize;
	int nProgress;
	char sExcept[DASWASM_EXCEPT_SZ];
};

static das_errcode _DasWasmRdr_onComment(
	DasStream* pSd, const DasComment* pCmt, void* pUser
){
	DasWasmRdr* pThis = (DasWasmRdr*)pUser;
	const char* sType = DasComment_get(pCmt, "type");
	const char* sVal  = DasComment_get(pCmt, "value");

	if((sType == NULL)||(sVal == NULL))
		return DAS_OKAY;

	if(strcmp(sType, "taskProgress") == 0)
		sscanf(sVal, "%d", &(pThis->nProgress));
	else if(strcmp(sType, "taskSize") == 0)
		sscanf(sVal, "%d", &(pThis->nTaskSize));

	return DAS_OKAY;
}

/* Exceptions arrive only here, at the end of the stream, since an
   exception ends the stream. 

   Returning DAS_OKAY matters: with no onClose anywhere, das2C's default
   fails the read on any exception other than "no data", but an in-band
   exception is data about the query and the caller reads it from 
   DasWasmRdr_exception(). 
*/
static das_errcode _DasWasmRdr_onClose(
	DasStream* pSd, const DasExcept* pEx, void* pUser
){
	DasWasmRdr* pThis = (DasWasmRdr*)pUser;
	const char* sMsg = (pEx != NULL) ? DasExcept_msg(pEx) : NULL;

	if(sMsg != NULL)
		snprintf(pThis->sExcept, DASWASM_EXCEPT_SZ, "%s", sMsg);
	return DAS_OKAY;
}

DasWasmRdr* new_DasWasmRdr(void)
{
	DasWasmRdr* pThis = (DasWasmRdr*)calloc(1, sizeof(DasWasmRdr));
	if(pThis == NULL){
		daserrf(DASERR_IO, "Couldn't allocate a reader");
		return NULL;
	}
	pThis->nTaskSize = -1;
	pThis->nProgress = -1;

	if((pThis->pIo = new_DasIO_feed("daswasm", "r")) == NULL){
		free(pThis);
		return NULL;
	}
	DasIO_setModel(pThis->pIo, DASIOMODEL_V3);  /* foreign callers only see the das3 model */

	/* DasIO builds the datasets itself; by default it drops each packet's
	   records before decoding the next.  Keep every record, the caller walks
	   the whole stream. */
	DasIO_setAccum(pThis->pIo, true);

	init_DasIOProc(&(pThis->oobs), pThis);
	pThis->oobs.onComment = _DasWasmRdr_onComment;
	pThis->oobs.onClose   = _DasWasmRdr_onClose;
	if(DasIO_addProc(pThis->pIo, &(pThis->oobs)) < 0){
		del_DasIO(pThis->pIo);
		free(pThis);
		return NULL;
	}

	return pThis;
}

int DasWasmRdr_feed(DasWasmRdr* pThis, const uint8_t* pData, int nLen)
{
	if(nLen < 0)
		return daserrf(DASERR_IO, "Negative chunk length");
	return DasIO_feed(pThis->pIo, pData, (size_t)nLen);
}

int DasWasmRdr_end(DasWasmRdr* pThis)
{
	return DasIO_feedEnd(pThis->pIo);
}

DasStream* DasWasmRdr_stream(DasWasmRdr* pThis)
{
	return DasIO_getStream(pThis->pIo);
}

DasStream* DasWasmRdr_take(DasWasmRdr* pThis)
{
	return DasIO_takeStream(pThis->pIo);
}

int DasWasmRdr_progress(const DasWasmRdr* pThis){ return pThis->nProgress; }

int DasWasmRdr_taskSize(const DasWasmRdr* pThis){ return pThis->nTaskSize; }

const char* DasWasmRdr_exception(const DasWasmRdr* pThis){ return pThis->sExcept; }

void del_DasWasmRdr(DasWasmRdr* pThis)
{
	del_DasIO(pThis->pIo);  /* frees the stream too, unless taken */
	free(pThis);
}

int DasWasmRdr_addProcessor(DasWasmRdr* pThis, DasIOProc* pProc)
{
	/* DasIO_addProc returns a slot count or a negated error */
	int nRet = DasIO_addProc(pThis->pIo, pProc);
	return (nRet < 0) ? -nRet : DAS_OKAY;
}

void daswasm_stream_free(DasStream* pSd){ del_DasDesc((DasDesc*)pSd); }

/* ************************************************************************* */
/* Tree walk */

/* Datasets that lost their packet ID to a redefinition stay attached, and
   are counted and visited here, see "Several datasets under one ID" in
   das3/stream.h.  So the index of a dataset never changes once assigned. */
int daswasm_strm_dsCount(DasStream* pSd)
{
	return (int)DasStream_numDesc(pSd, DASDESCTYPE_DS);
}

DasDs* daswasm_strm_dsAt(DasStream* pSd, int i)
{
	int nSets = 0;
	DasDesc* pDesc = NULL;
	while((pDesc = DasStream_nextDesc(pSd, pDesc, DASDESCTYPE_DS)) != NULL){
		if(nSets == i)
			return (DasDs*)pDesc;
		++nSets;
	}
	return NULL;
}

int daswasm_desc_propCount(const DasDesc* pDesc)
{
	return (int)DasDesc_numProps(pDesc);
}

const char* daswasm_desc_propName(const DasDesc* pDesc, int i)
{
	const DasProp* pProp = DasDesc_propAt(pDesc, (size_t)i);
	return (pProp != NULL) ? DasProp_name(pProp) : NULL;
}

const char* daswasm_desc_propValue(const DasDesc* pDesc, int i)
{
	const DasProp* pProp = DasDesc_propAt(pDesc, (size_t)i);
	return (pProp != NULL) ? DasProp_value(pProp) : NULL;
}

const char* daswasm_desc_propType(const DasDesc* pDesc, int i)
{
	const DasProp* pProp = DasDesc_propAt(pDesc, (size_t)i);
	return (pProp != NULL) ? DasProp_typeStr3(pProp) : NULL;
}

const char* daswasm_ds_id(const DasDs* pDs){ return DasDs_name(pDs); }

const char* daswasm_ds_group(const DasDs* pDs){ return DasDs_group(pDs); }

int daswasm_ds_rank(const DasDs* pDs){ return DasDs_rank(pDs); }

int daswasm_ds_shapeAt(const DasDs* pDs, int i)
{
	ptrdiff_t aShape[DASIDX_MAX] = DASIDX_INIT_UNUSED;
	int nRank = DasDs_shape(pDs, aShape);
	if((i < 0)||(i >= nRank))
		return DASIDX_UNUSED;
	return (int)aShape[i];
}

int daswasm_ds_dimCount(const DasDs* pDs, int bCoord)
{
	return (int)DasDs_numDims(pDs, bCoord ? DASDIMTYPE_COORD : DASDIMTYPE_DATA);
}

DasDim* daswasm_ds_dimAt(DasDs* pDs, int i, int bCoord)
{
	if(i < 0) return NULL;
	return DasDs_dimAt(pDs, (size_t)i, bCoord ? DASDIMTYPE_COORD : DASDIMTYPE_DATA);
}

const char* daswasm_dim_id(const DasDim* pDim){ return DasDim_name(pDim); }

int daswasm_dim_isCoord(const DasDim* pDim)
{
	return (DasDim_type(pDim) == DASDIMTYPE_COORD) ? 1 : 0;
}

int daswasm_dim_varCount(const DasDim* pDim){ return (int)DasDim_numVars(pDim); }

DasVar* daswasm_dim_varAt(DasDim* pDim, int i)
{
	if(i < 0) return NULL;
	return DasDim_varAt(pDim, (size_t)i);
}

const char* daswasm_dim_roleAt(DasDim* pDim, int i)
{
	if(i < 0) return NULL;
	return DasDim_roleAt(pDim, (size_t)i);
}

/* ************************************************************************* */
/* Variables */

const char* daswasm_var_units(const DasVar* pVar)
{
	const char* sUnits = (const char*)DasVar_units(pVar);
	return (sUnits != NULL) ? sUnits : "";
}

const char* daswasm_var_valType(const DasVar* pVar)
{
	das_vt vt = DasVar_vt(pVar);

	/* A composite (vector, matrix, complex...) is a run of simple storage
	 * elements plus a formalism; the type a foreign caller must map to a
	 * typed array is the ELEMENT type.  Geometry comes from the intrinsic
	 * shape, meaning from the form -- neither changes the bytes. */
	if(vt == DASVT_COMP)
		vt = (das_vt)DasVar_et(pVar);

	return dasvt_name(vt);
}

int daswasm_var_shapeAt(const DasVar* pVar, int i)
{
	ptrdiff_t aShape[DASIDX_MAX] = DASIDX_INIT_UNUSED;
	int nRank = DasVar_shape(pVar, aShape);
	if((i < 0)||(i >= nRank))
		return DASIDX_UNUSED;
	return (int)aShape[i];
}

int daswasm_var_intrRank(const DasVar* pVar)
{
	ptrdiff_t aShape[DASIDX_MAX - 1] = {0};
	return DasVar_intrShape(pVar, aShape);
}

int daswasm_var_intrShapeAt(const DasVar* pVar, int i)
{
	ptrdiff_t aShape[DASIDX_MAX - 1] = {0};
	int nRank = DasVar_intrShape(pVar, aShape);
	if((i < 0)||(i >= nRank))
		return DASIDX_UNUSED;
	return (int)aShape[i];
}

const char* daswasm_var_compSym(const DasVar* pVar, int i)
{
	return DasVar_compSym(pVar, i);
}

int daswasm_var_isNumeric(const DasVar* pVar)
{
	return DasVar_isNumeric(pVar) ? 1 : 0;
}

DasAry* daswasm_var_ary(DasVar* pVar){ return DasVar_ary(pVar); }

DasAry* daswasm_var_qubeLike(DasVar* pVar, const DasVar* pShapeFrom)
{
	ptrdiff_t aShape[DASIDX_MAX] = DASIDX_INIT_UNUSED;
	int nRank = DasVar_shape(pVar, aShape);
	return new_DasAry_materializeQube(pVar, nRank, NULL, NULL, pShapeFrom);
}

DasAry* daswasm_var_qube(DasVar* pVar)
{
	return daswasm_var_qubeLike(pVar, NULL);
}

/* ************************************************************************* */
/* Arrays */

const char* daswasm_ary_valType(const DasAry* pAry)
{
	return DasAry_vtName(pAry);
}

int daswasm_ary_valSize(const DasAry* pAry){ return (int)DasAry_valSize(pAry); }

int daswasm_ary_rank(const DasAry* pAry)
{
	ptrdiff_t aShape[DASIDX_MAX] = DASIDX_INIT_UNUSED;
	return DasAry_shape(pAry, aShape);
}

int daswasm_ary_shapeAt(const DasAry* pAry, int i)
{
	ptrdiff_t aShape[DASIDX_MAX] = DASIDX_INIT_UNUSED;
	int nRank = DasAry_shape(pAry, aShape);
	if((i < 0)||(i >= nRank))
		return DASIDX_UNUSED;
	return (int)aShape[i];
}

int daswasm_ary_size(const DasAry* pAry){ return (int)DasAry_size(pAry); }

const uint8_t* daswasm_ary_data(DasAry* pAry)
{
	size_t uVals = 0;
	return DasAry_valsIn(pAry, DasAry_vt(pAry), DAS_ADDR0, &uVals);
}

const char* daswasm_ary_textAt(DasAry* pAry, int i)
{
	size_t uVals = 0;
	if(DasAry_vt(pAry) != DASVT_TEXT)
		return NULL;
	char** pStrs = (char**)DasAry_valsIn(pAry, DASVT_TEXT, DAS_ADDR0, &uVals);
	if((pStrs == NULL)||(i < 0)||((size_t)i >= uVals))
		return NULL;
	return pStrs[i];
}

/* The foreign side speaks int; das2C indexes with ptrdiff_t.  Widen into
 * a fixed local, bounded by the maximum array rank das2C supports. */
static int _daswasm_loc(const DasAry* pAry, const int* pLoc, int nLoc, ptrdiff_t* aLoc)
{
	if((nLoc < 0)||(nLoc > DASIDX_MAX)||(nLoc > DasAry_rank(pAry))){
		daserrf(DASERR_ARRAY, "Partial index of length %d out of range for rank %d array",
		          nLoc, DasAry_rank(pAry));
		return -1;
	}
	for(int i = 0; i < nLoc; ++i)
		aLoc[i] = pLoc[i];
	return nLoc;
}

int daswasm_ary_lengthAt(const DasAry* pAry, const int* pLoc, int nLoc)
{
	ptrdiff_t aLoc[DASIDX_MAX];
	if(_daswasm_loc(pAry, pLoc, nLoc, aLoc) < 0)
		return -1;
	/* a complete index has nothing below it */
	if(nLoc == DasAry_rank(pAry))
		return 1;
	return (int)DasAry_lengthIn(pAry, nLoc, aLoc);
}

int daswasm_ary_sizeAt(DasAry* pAry, const int* pLoc, int nLoc)
{
	ptrdiff_t aLoc[DASIDX_MAX];
	size_t uVals = 0;
	if(_daswasm_loc(pAry, pLoc, nLoc, aLoc) < 0)
		return -1;
	if(DasAry_valsIn(pAry, DasAry_vt(pAry), nLoc, aLoc, &uVals) == NULL)
		return 0;
	return (int)uVals;
}

const uint8_t* daswasm_ary_dataAt(DasAry* pAry, const int* pLoc, int nLoc)
{
	ptrdiff_t aLoc[DASIDX_MAX];
	size_t uVals = 0;
	if(_daswasm_loc(pAry, pLoc, nLoc, aLoc) < 0)
		return NULL;
	return DasAry_valsIn(pAry, DasAry_vt(pAry), nLoc, aLoc, &uVals);
}

double* daswasm_ary_times(DasAry* pAry)
{
	size_t uVals = 0;
	if(DasAry_vt(pAry) != DASVT_TIME)
		return NULL;
	const DasTime* pTimes = DasAry_timesIn(pAry, DAS_ADDR0, &uVals);

	/* An empty time array (header landed, no data yet) has no element
	 * pointer, and malloc(0) may legally return NULL too.  Neither is a
	 * failure, so hand back a real buffer of zero useful doubles. */
	if((pTimes == NULL)&&(DasAry_size(pAry) != 0))
		return NULL;
	if(pTimes == NULL)
		uVals = 0;

	double* pOut = (double*)malloc((uVals > 0 ? uVals : 1) * sizeof(double));
	if(pOut == NULL){
		daserrf(DASERR_ARRAY, "Couldn't allocate %zu doubles", uVals);
		return NULL;
	}
	for(size_t u = 0; u < uVals; ++u)
		pOut[u] = DasTime_toSec1958(pTimes + u);
	return pOut;
}

/* One typed pass of daswasm_ary_doubles.  DIFF is spelled per type by the
   caller because the exactness argument differs: integer elements widen to
   int64 and subtract there, so a TT2000 axis re-based on its own first
   sample never meets a double until the residual is small; float elements
   subtract as doubles.  The fill test is a bitwise-equal compare in the
   element type, which is what das2C itself does when it asks "is this
   fill", and is why a NaN fill on a real array falls through: NaN != NaN,
   but the cast then yields NaN anyway. */
#define _DASWASM_TO_DBL(T, DIFF) do{ \
	const T* pIn = (const T*)pData; \
	const T fill = *((const T*)pFill); \
	const T ref = (pRef != NULL) ? *((const T*)pRef) : (T)0; \
	for(size_t u = 0; u < uVals; ++u) \
		pOut[u] = (pIn[u] == fill) ? NAN : ((double)(DIFF)) * rScale; \
}while(0)

double* daswasm_ary_doubles(DasAry* pAry, const uint8_t* pRef, double rScale)
{
	das_vt vt = DasAry_vt(pAry);
	if((vt < DASVT_UBYTE)||(vt > DASVT_DOUBLE)){
		daserrf(DASERR_ARRAY, "No double conversion for arrays of %s elements",
		          dasvt_name(vt));
		return NULL;
	}

	/* Same empty-array contract as daswasm_ary_times: a header with no
	   records yet is not an error, answer with a real (if useless) buffer */
	size_t uVals = 0;
	const uint8_t* pData = DasAry_valsIn(pAry, vt, DAS_ADDR0, &uVals);
	if((pData == NULL)&&(DasAry_size(pAry) != 0))
		return NULL;
	if(pData == NULL)
		uVals = 0;

	double* pOut = (double*)malloc((uVals > 0 ? uVals : 1) * sizeof(double));
	if(pOut == NULL){
		daserrf(DASERR_ARRAY, "Couldn't allocate %zu doubles", uVals);
		return NULL;
	}
	if(uVals == 0)
		return pOut;

	const uint8_t* pFill = DasAry_fill(pAry);

	/* The two 64-bit cases subtract as uint64 and reinterpret: signed
	   overflow is undefined in C, unsigned wrap is defined, and the wrapped
	   bit pattern read as int64 is the right signed difference whenever
	   that difference fits, which for a time axis it always does */
	switch(vt){
	case DASVT_UBYTE:  _DASWASM_TO_DBL(uint8_t,  (int64_t)pIn[u] - (int64_t)ref); break;
	case DASVT_BYTE:   _DASWASM_TO_DBL(int8_t,   (int64_t)pIn[u] - (int64_t)ref); break;
	case DASVT_USHORT: _DASWASM_TO_DBL(uint16_t, (int64_t)pIn[u] - (int64_t)ref); break;
	case DASVT_SHORT:  _DASWASM_TO_DBL(int16_t,  (int64_t)pIn[u] - (int64_t)ref); break;
	case DASVT_UINT:   _DASWASM_TO_DBL(uint32_t, (int64_t)pIn[u] - (int64_t)ref); break;
	case DASVT_INT:    _DASWASM_TO_DBL(int32_t,  (int64_t)pIn[u] - (int64_t)ref); break;
	case DASVT_ULONG:  _DASWASM_TO_DBL(uint64_t, (int64_t)(pIn[u] - ref)); break;
	case DASVT_LONG:   _DASWASM_TO_DBL(int64_t,  (int64_t)((uint64_t)pIn[u] - (uint64_t)ref)); break;
	case DASVT_FLOAT:  _DASWASM_TO_DBL(float,    (double)pIn[u] - (double)ref); break;
	case DASVT_DOUBLE: _DASWASM_TO_DBL(double,   pIn[u] - ref); break;
	default: break;  /* excluded above */
	}
	return pOut;
}

#undef _DASWASM_TO_DBL

void daswasm_ary_free(DasAry* pAry)
{
	if(pAry != NULL)
		dec_DasAry(pAry);
}

void daswasm_free(void* pBuf)
{
	if(pBuf != NULL)
		free(pBuf);
}

/* ************************************************************************* */
/* Metadata snapshot
 *
 * One call per dataset returns its whole structure as JSON.  The structure
 * is fixed once the header packet has been parsed (only the record count
 * grows afterwards), so a foreign caller parses this once and keeps plain
 * objects, instead of making a C call per field per tick.  The key
 * vocabulary follows the das3 XML attribute names where they exist; see the
 * description in wasm.h.  Pointers ride along as "ptr" so the caller can
 * reach the live objects (record counts, value buffers) without re-walking.
 */

typedef struct jbuf {
	char* s;
	size_t len;
	size_t cap;
	int err;
} JBuf;

static int _jb_reserve(JBuf* b, size_t uMore)
{
	if(b->err) return 0;
	if(b->len + uMore + 1 <= b->cap) return 1;
	size_t uCap = (b->cap > 0) ? b->cap : 1024;
	while(uCap < b->len + uMore + 1) uCap *= 2;
	char* sNew = (char*)realloc(b->s, uCap);
	if(sNew == NULL){
		b->err = daserrf(DASERR_DS, "Couldn't allocate %zu bytes for a snapshot", uCap);
		return 0;
	}
	b->s = sNew;
	b->cap = uCap;
	return 1;
}

static void _jb_raw(JBuf* b, const char* s)
{
	size_t n = strlen(s);
	if(!_jb_reserve(b, n)) return;
	memcpy(b->s + b->len, s, n);
	b->len += n;
	b->s[b->len] = '\0';
}

/* JSON needs a comma between siblings but not after an opener or a key.
   Looking at the previous byte is enough to tell the cases apart, which
   saves threading a "first item" flag through every emitter. */
static void _jb_item(JBuf* b)
{
	if(b->len == 0) return;
	char c = b->s[b->len - 1];
	if((c != '{')&&(c != '[')&&(c != ':'))
		_jb_raw(b, ",");
}

/* Quoted and escaped.  UTF-8 passes through untouched, JSON is UTF-8. */
static void _jb_str(JBuf* b, const char* s)
{
	_jb_item(b);
	if(s == NULL) s = "";
	if(!_jb_reserve(b, strlen(s) * 6 + 2)) return;
	b->s[b->len++] = '"';
	for(const unsigned char* p = (const unsigned char*)s; *p != '\0'; ++p){
		switch(*p){
		case '"':  b->s[b->len++] = '\\'; b->s[b->len++] = '"';  break;
		case '\\': b->s[b->len++] = '\\'; b->s[b->len++] = '\\'; break;
		case '\n': b->s[b->len++] = '\\'; b->s[b->len++] = 'n';  break;
		case '\r': b->s[b->len++] = '\\'; b->s[b->len++] = 'r';  break;
		case '\t': b->s[b->len++] = '\\'; b->s[b->len++] = 't';  break;
		default:
			if(*p < 0x20)
				b->len += (size_t)snprintf(b->s + b->len, 7, "\\u%04x", (unsigned)*p);
			else
				b->s[b->len++] = (char)*p;
		}
	}
	b->s[b->len++] = '"';
	b->s[b->len] = '\0';
}

static void _jb_key(JBuf* b, const char* k)
{
	_jb_str(b, k);
	_jb_raw(b, ":");
}

static void _jb_int(JBuf* b, long long n)
{
	char sTmp[32];
	_jb_item(b);
	snprintf(sTmp, sizeof(sTmp), "%lld", n);
	_jb_raw(b, sTmp);
}

/* An int64 as a JSON STRING: JSON numbers are doubles on the far side and
   lose integers past 2^53, where TT2000 lives.  The caller converts to a
   BigInt by the type tag beside it. */
static void _jb_int64str(JBuf* b, long long n)
{
	char sTmp[32];
	snprintf(sTmp, sizeof(sTmp), "%lld", n);
	_jb_str(b, sTmp);
}

static void _jb_uint64str(JBuf* b, unsigned long long n)
{
	char sTmp[32];
	snprintf(sTmp, sizeof(sTmp), "%llu", n);
	_jb_str(b, sTmp);
}

/* JSON has no NaN or infinity; null is the honest substitute */
static void _jb_real(JBuf* b, double d)
{
	char sTmp[32];
	_jb_item(b);
	if(!isfinite(d)){ _jb_raw(b, "null"); return; }
	snprintf(sTmp, sizeof(sTmp), "%.17g", d);
	_jb_raw(b, sTmp);
}

static void _jb_bool(JBuf* b, int bVal)
{
	_jb_item(b);
	_jb_raw(b, bVal ? "true" : "false");
}

static void _jb_null(JBuf* b)
{
	_jb_item(b);
	_jb_raw(b, "null");
}

static void _jb_open(JBuf* b, const char* s){ _jb_item(b); _jb_raw(b, s); }

static void _jb_shape(JBuf* b, const ptrdiff_t* pShape, int nRank)
{
	_jb_open(b, "[");
	for(int i = 0; i < nRank; ++i)
		_jb_int(b, (long long)pShape[i]);
	_jb_raw(b, "]");
}

/* The schema's storage token for an element type.  dasvt_name already
   says "das_time"; the two pointer spellings never describe elements, a
   byte run's elements are ubytes. */
static const char* _storage_tok(das_vt vt)
{
	if((vt == DASVT_TEXT)||(vt == DASVT_BYTESEQ)) return "ubyte";
	const char* s = dasvt_name(vt);
	return (s != NULL) ? s : "";
}

/* One typed value: {storage, units, value}.  The JS type of value follows
   storage: integers up to 32 bits and reals are JSON numbers, 64-bit
   integers are strings (see _jb_int64str), a DasTime is ISO 8601. */
static void _jb_datum(JBuf* b, const uint8_t* pVal, das_vt vt, DasUnits units)
{
	char sTmp[48];
	_jb_open(b, "{");
	_jb_key(b, "storage"); _jb_str(b, _storage_tok(vt));
	_jb_key(b, "units");   _jb_str(b, (units != NULL) ? (const char*)units : "");
	_jb_key(b, "value");
	if(pVal == NULL){ _jb_null(b); }
	else switch(vt){
	case DASVT_UBYTE:  _jb_int(b, *((const uint8_t*)pVal));  break;
	case DASVT_BYTE:   _jb_int(b, *((const int8_t*)pVal));   break;
	case DASVT_USHORT: _jb_int(b, *((const uint16_t*)pVal)); break;
	case DASVT_SHORT:  _jb_int(b, *((const int16_t*)pVal));  break;
	case DASVT_UINT:   _jb_int(b, *((const uint32_t*)pVal)); break;
	case DASVT_INT:    _jb_int(b, *((const int32_t*)pVal));  break;
	case DASVT_ULONG:  _jb_uint64str(b, *((const uint64_t*)pVal)); break;
	case DASVT_LONG:   _jb_int64str(b, *((const int64_t*)pVal));   break;
	case DASVT_FLOAT:  _jb_real(b, *((const float*)pVal));   break;
	case DASVT_DOUBLE: _jb_real(b, *((const double*)pVal));  break;
	case DASVT_TIME:
		DasTime_toIsoC((const DasTime*)pVal, 9, sTmp, sizeof(sTmp));
		_jb_str(b, sTmp);
		break;
	case DASVT_TEXT:   _jb_str(b, (const char*)pVal); break;
	default:       _jb_null(b); break;
	}
	_jb_raw(b, "}");
}

/* Properties of any descriptor (stream, dataset, dimension, variable) as
   {name: {type, value, units, alt}}.  Every entry carries all four keys so
   a reader never tests for a missing one.  Range and array types become
   JSON arrays; integers are strings for the BigInt rule; datetimes stay
   the ISO text the producer wrote. */
static void _jb_props(JBuf* b, const DasDesc* pDesc)
{
	_jb_open(b, "{");
	size_t uProps = DasDesc_numProps(pDesc);
	for(size_t u = 0; u < uProps; ++u){
		const DasProp* pProp = DasDesc_propAt(pDesc, u);
		if((pProp == NULL)||(!DasProp_isValid(pProp))) continue;

		uint8_t uType = DasProp_type(pProp);
		uint8_t uBase = uType & DASPROP_TYPE_MASK;
		uint8_t uMulti = uType & DASPROP_MULTI_MASK;

		/* A single value is used whole, straight from the property, no copy.
		   Sets and ranges go through the item splitter; each item can be as
		   long as the whole value, so size every slot to that */
		const char* sWhole = DasProp_value(pProp);
		int nItems = 1;
		char* pStore = NULL;
		char** psItems = NULL;
		const char* const* psVals = &sWhole;
		if(uMulti != DASPROP_SINGLE){
			nItems = DasProp_numItems(pProp);
			if(nItems < 0) nItems = 0;
			size_t uLenEa = strlen(sWhole) + 1;
			pStore = (char*)calloc((size_t)(nItems > 0 ? nItems : 1), uLenEa);
			psItems = (char**)calloc((size_t)(nItems > 0 ? nItems : 1), sizeof(char*));
			if((pStore == NULL)||(psItems == NULL)){
				free(pStore); free(psItems);
				b->err = daserrf(DASERR_DS, "Couldn't allocate property item buffers");
				return;
			}
			for(int i = 0; i < nItems; ++i) psItems[i] = pStore + (size_t)i * uLenEa;
			int nGot = (nItems > 0) ? DasProp_toStrs(pProp, psItems, (size_t)nItems, uLenEa) : 0;
			if(nGot < nItems) nItems = (nGot > 0) ? nGot : 0;
			psVals = (const char* const*)psItems;
		}

		_jb_key(b, DasProp_name(pProp));
		_jb_open(b, "{");
		_jb_key(b, "type");  _jb_str(b, DasProp_typeStr3(pProp));
		_jb_key(b, "value");
		if(uMulti != DASPROP_SINGLE) _jb_open(b, "[");
		for(int i = 0; i < nItems; ++i){
			switch(uBase){
			case DASPROP_BOOL:
				_jb_bool(b, (strcmp(psVals[i], "true") == 0)||(strcmp(psVals[i], "1") == 0));
				break;
			case DASPROP_INT:
				_jb_str(b, psVals[i]);   /* digits as text, BigInt on the far side */
				break;
			case DASPROP_REAL:
				_jb_real(b, strtod(psVals[i], NULL));
				break;
			default:  /* string, datetime: as written */
				_jb_str(b, psVals[i]);
				break;
			}
		}
		if(uMulti != DASPROP_SINGLE) _jb_raw(b, "]");
		DasUnits units = DasProp_units(pProp);
		_jb_key(b, "units"); _jb_str(b, (units != NULL) ? (const char*)units : "");
		_jb_key(b, "alt");   _jb_open(b, "{"); _jb_raw(b, "}");
		_jb_raw(b, "}");

		free(pStore);
		free(psItems);
	}
	_jb_raw(b, "}");
}

/* One <ops> parameter as a plain JSON value, parsed by the type das2C
   reports.  Unlike _jb_props there is no {type, value, units} wrapper: a
   formalism's parameter set is small and fixed per kind, so JS reads
   ops.sysorder directly.  Integers are plain numbers here, not BigInt text;
   the only integer parameters are component slots (0 to 8).
   
   DasProp's own converters do the splitting, so this never needs to know
   that sysorder's items are separated by ';'. */
#define OPS_MAX_ITEMS 16
static void _jb_opsParam(JBuf* b, const DasProp* pProp)
{
	uint8_t uType  = DasProp_type(pProp);
	uint8_t uBase  = uType & DASPROP_TYPE_MASK;
	bool  bMulti = ((uType & DASPROP_MULTI_MASK) != DASPROP_SINGLE);

	_jb_key(b, DasProp_name(pProp));

	int n = 0;
	switch(uBase){
	case DASPROP_INT: {
		int64_t aVal[OPS_MAX_ITEMS];
		n = DasProp_toInts(pProp, aVal, OPS_MAX_ITEMS);
		if(bMulti) _jb_open(b, "[");
		for(int i = 0; i < n; ++i) _jb_int(b, (long long)aVal[i]);
		if(bMulti) _jb_raw(b, "]");
		break;
	}
	case DASPROP_REAL: {
		double aVal[OPS_MAX_ITEMS];
		n = DasProp_toDoubles(pProp, aVal, OPS_MAX_ITEMS);
		if(bMulti) _jb_open(b, "[");
		for(int i = 0; i < n; ++i) _jb_real(b, aVal[i]);
		if(bMulti) _jb_raw(b, "]");
		break;
	}
	case DASPROP_BOOL: {
		uint8_t aVal[OPS_MAX_ITEMS];
		n = DasProp_toBools(pProp, aVal, OPS_MAX_ITEMS);
		if(bMulti) _jb_open(b, "[");
		for(int i = 0; i < n; ++i) _jb_bool(b, aVal[i]);
		if(bMulti) _jb_raw(b, "]");
		break;
	}
	default:
		/* string, and every parameter of a kind das2C does not know */
		_jb_str(b, DasProp_value(pProp));
		n = 1;
		break;
	}

	/* A scalar that failed to convert left a key with no value, which would
	   be invalid JSON.  daserrf has already said why. */
	if((n < 1)&&(!bMulti)) _jb_null(b);
}

/* {kind, ...parameters}.  Every parameter the form holds, defaults included,
   so JS never has to know that a missing system= means cartesian. */
static void _jb_ops(JBuf* b, const DasForm* pForm)
{
	_jb_open(b, "{");
	_jb_key(b, "kind"); _jb_str(b, DasForm_kind(pForm));

	DasFormParamBuf buf;
	const DasProp* pProp = NULL;
	for(int i = 0; (pProp = DasForm_paramAt(pForm, i, &buf)) != NULL; ++i)
		_jb_opsParam(b, pProp);

	_jb_raw(b, "}");
}

/* {kind} plus, for a sequence, the intercept and slopes as typed values.
   A DasTime sequence keeps its slopes as doubles in seconds (das2C's
   affine rule), so their storage and units differ from the intercept's. */
static void _jb_source(JBuf* b, const DasVar* pVar, const ptrdiff_t* pShape, int nRank)
{
	const DasGen* pGen = DasVar_gen(pVar);
	_jb_open(b, "{");
	_jb_key(b, "kind");

	/* The one variable class with no leaf value source is an operation over
	   two other variables (das2 streams build time = reference + offset this
	   way).  Say what was combined, by role, so a caller can find the
	   operands in the same dimension. */
	if(pGen == NULL){
		const char* sOp = dasop_name(DasVarBin_op(pVar), NULL);
		const DasVar* pLeft = DasVarBin_left(pVar);
		const DasVar* pRight = DasVarBin_right(pVar);
		_jb_str(b, "binaryOp");
		_jb_key(b, "op");    _jb_str(b, (sOp != NULL) ? sOp : "");
		_jb_key(b, "left");  _jb_str(b, (pLeft != NULL) ? DasVar_role(pLeft) : "");
		_jb_key(b, "right"); _jb_str(b, (pRight != NULL) ? DasVar_role(pRight) : "");
		_jb_raw(b, "}");
		return;
	}

	switch(DasGen_type(pGen)){
	case gtArray: _jb_str(b, "array");    break;
	case gtSeq:   _jb_str(b, "sequence"); break;
	case gtConst: _jb_str(b, "const");    break;
	case gtUnop:  _jb_str(b, "unaryOp");  break;
	case gtBinop: _jb_str(b, "binaryOp"); break;
	default:      _jb_str(b, "");         break;
	}

	if(DasGen_type(pGen) == gtSeq){
		const DasGenSeq* pSeq = (const DasGenSeq*)pGen;
		das_vt vt = (das_vt)DasGen_elemType(pGen);
		DasUnits units = DasVar_units(pVar);
		das_vt vtSlope = (vt == DASVT_TIME) ? DASVT_DOUBLE : vt;
		DasUnits uSlope = (vt == DASVT_TIME) ? DASUNITS_SECONDS : units;
		int nComps = (pSeq->nComps > 0) ? pSeq->nComps : 1;

		_jb_key(b, "minval");
		if(nComps > 1) _jb_open(b, "[");
		for(int c = 0; c < nComps; ++c)
			_jb_datum(b, DasGenSeq_intercept(pSeq, c), vt, units);
		if(nComps > 1) _jb_raw(b, "]");

		/* one slot per dataset index, null where the sequence does not run */
		_jb_key(b, "interval");
		if(nComps > 1) _jb_open(b, "[");
		for(int c = 0; c < nComps; ++c){
			_jb_open(b, "[");
			for(int i = 0; i < nRank; ++i){
				if(pShape[i] == DASIDX_UNUSED) _jb_null(b);
				else _jb_datum(b, DasGenSeq_interval(pSeq, c, i), vtSlope, uSlope);
			}
			_jb_raw(b, "]");
		}
		if(nComps > 1) _jb_raw(b, "]");
	}
	_jb_raw(b, "}");
}

static void _jb_var(JBuf* b, DasVar* pVar, int nDsRank)
{
	das_vt vt = DasVar_vt(pVar);
	das_vt et = (das_vt)DasVar_et(pVar);
	DasUnits units = DasVar_units(pVar);
	bool bBytes = (vt == DASVT_TEXT)||(vt == DASVT_BYTESEQ);

	ptrdiff_t aShape[DASIDX_MAX] = DASIDX_INIT_UNUSED;
	DasVar_shape(pVar, aShape);
	ptrdiff_t aInner[DASIDX_MAX] = {0};
	int nInner = DasVar_intrShape(pVar, aInner);
	if(nInner < 0) nInner = 0;

	_jb_open(b, "{");
	_jb_key(b, "ptr");      _jb_int(b, (long long)(uintptr_t)pVar);
	_jb_key(b, "role");     _jb_str(b, DasVar_role(pVar));
	_jb_key(b, "units");    _jb_str(b, (units != NULL) ? (const char*)units : "");

	/* semantic is derived the way the das3 writer derives it: spent at parse
	   time, re-stated for the reader.  A byte run's meaning is its class. */
	_jb_key(b, "semantic");
	if(vt == DASVT_TEXT) _jb_str(b, "string");
	else if(vt == DASVT_BYTESEQ) _jb_str(b, "blob");
	else{
		const char* sSem = dassem_default(et, units);
		_jb_str(b, (sSem != NULL) ? sSem : "");
	}
	_jb_key(b, "storage");  _jb_str(b, _storage_tok(bBytes ? vt : et));
	_jb_key(b, "innerShape"); _jb_shape(b, aInner, nInner);
	_jb_key(b, "isNumeric"); _jb_bool(b, DasVar_isNumeric(pVar));
	_jb_key(b, "props");    _jb_props(b, (const DasDesc*)pVar);
	_jb_key(b, "source");   _jb_source(b, pVar, aShape, nDsRank);

	/* a byte run has no formalism by class, so no ops key at all */
	const DasForm* pForm = DasVar_form(pVar);
	if(pForm != NULL){
		_jb_key(b, "ops");  _jb_ops(b, pForm);
	}

	/* component symbols in storage order, null where the kind has none */
	_jb_key(b, "symbols"); _jb_open(b, "[");
	if(!bBytes && (nInner > 0)){
		long long nSyms = 1;
		for(int i = 0; i < nInner; ++i)
			nSyms = (aInner[i] > 0) ? nSyms * aInner[i] : 0;
		for(long long i = 0; i < nSyms; ++i){
			const char* sSym = DasVar_compSym(pVar, (int)i);
			if(sSym != NULL) _jb_str(b, sSym); else _jb_null(b);
		}
	}
	_jb_raw(b, "]");
	_jb_raw(b, "}");
}

static void _jb_dim(JBuf* b, DasDim* pDim, int nDsRank)
{
	_jb_open(b, "{");
	_jb_key(b, "ptr");     _jb_int(b, (long long)(uintptr_t)pDim);
	_jb_key(b, "name");    _jb_str(b, DasDim_name(pDim));
	_jb_key(b, "physDim"); _jb_str(b, DasDim_physDim(pDim));
	_jb_key(b, "axes");    _jb_open(b, "[");
	for(int i = 0; i < DASDIM_NAXES; ++i){
		const char* sAxis = DasDim_axisSymAt(pDim, i);
		if(sAxis == NULL) break;
		_jb_str(b, sAxis);
	}
	_jb_raw(b, "]");
	_jb_key(b, "kind");    _jb_str(b, DasDim_typeName(pDim));
	_jb_key(b, "props");   _jb_props(b, (const DasDesc*)pDim);
	_jb_key(b, "vars");    _jb_open(b, "{");
	size_t uVars = DasDim_numVars(pDim);
	for(size_t u = 0; u < uVars; ++u){
		_jb_key(b, DasDim_roleAt(pDim, u));
		_jb_var(b, DasDim_varAt(pDim, u), nDsRank);
	}
	_jb_raw(b, "}");
	_jb_raw(b, "}");
}

static void _jb_dims(JBuf* b, DasDs* pDs, das_dimtype dt, int nDsRank)
{
	_jb_open(b, "[");
	size_t uDims = DasDs_numDims(pDs, dt);
	for(size_t u = 0; u < uDims; ++u)
		_jb_dim(b, DasDs_dimAt(pDs, u, dt), nDsRank);
	_jb_raw(b, "]");
}

char* daswasm_ds_json(DasDs* pDs)
{
	JBuf b = {NULL, 0, 0, 0};
	ptrdiff_t aShape[DASIDX_MAX] = DASIDX_INIT_UNUSED;
	int nRank = DasDs_shape(pDs, aShape);
	const char* sGroup = DasDs_group(pDs);

	_jb_open(&b, "{");
	_jb_key(&b, "ptr");    _jb_int(&b, (long long)(uintptr_t)pDs);
	_jb_key(&b, "name");   _jb_str(&b, DasDs_name(pDs));
	_jb_key(&b, "group");  _jb_str(&b, (sGroup != NULL) ? sGroup : "");
	_jb_key(&b, "rank");   _jb_int(&b, nRank);
	_jb_key(&b, "shape");  _jb_shape(&b, aShape, nRank);
	_jb_key(&b, "props");  _jb_props(&b, (const DasDesc*)pDs);
	_jb_key(&b, "coords"); _jb_dims(&b, pDs, DASDIMTYPE_COORD, nRank);
	_jb_key(&b, "data");   _jb_dims(&b, pDs, DASDIMTYPE_DATA, nRank);
	_jb_raw(&b, "}");

	if(b.err){ free(b.s); return NULL; }
	return b.s;
}

char* daswasm_desc_json(const DasDesc* pDesc)
{
	JBuf b = {NULL, 0, 0, 0};
	_jb_props(&b, pDesc);
	if(b.err){ free(b.s); return NULL; }
	return b.s;
}

/* ************************************************************************* */
/* Catalog node snapshot */

static const char* _node_kind_tok(das_nodetype nt)
{
	switch(nt){
	case DASNODETYPE_CATALOG:    return "catalog";
	case DASNODETYPE_COLLECTION: return "collection";
	case DASNODETYPE_SRCSET:     return "srcset";
	case DASNODETYPE_STREAM_SRC: return "stream_src";
	case DASNODETYPE_FILE_AGG:   return "file_agg";
	case DASNODETYPE_SPDF_CAT:   return "spdf_cat";
	case DASNODETYPE_SPASE_CAT:  return "spase_cat";
	default:                return "inv";
	}
}

/* _jb_str writes "" for NULL, but a node field that is absent and one that
   is empty mean different things to a caller */
static void _jb_strOrNull(JBuf* b, const char* s)
{
	if(s == NULL) _jb_null(b);
	else _jb_str(b, s);
}

/* The stub goes through as the server wrote it.  Fields beyond type and
   urls (provides, validRange, purpose ...) vary by server and age, and a
   tree view wants whichever ones are there. */
static void _jb_stub(JBuf* b, const DasJdo* pStub)
{
	size_t uLen = 0;
	char* sStub = (pStub == NULL) ? NULL : (char*)DasJdo_newMiniStr(pStub, &uLen);
	if(sStub == NULL){ _jb_null(b); return; }
	_jb_item(b);
	_jb_raw(b, sStub);
	free(sStub);
}

char* daswasm_node_json(const DasNode* pNode)
{
	if(pNode == NULL) return NULL;
	JBuf b = {NULL, 0, 0, 0};

	const DasJdo* pType = DasNode_jdo(pNode, DASNODE_FRAG_TYPE);

	_jb_open(&b, "{");
	_jb_key(&b, "ptr");         _jb_int(&b, (long long)(uintptr_t)pNode);
	_jb_key(&b, "kind");        _jb_str(&b, _node_kind_tok(DasNode_type(pNode)));
	_jb_key(&b, "type");        _jb_strOrNull(&b, pType ? DasJdo_str(pType) : NULL);
	_jb_key(&b, "version");     _jb_strOrNull(&b, DasNode_version(pNode));
	_jb_key(&b, "name");        _jb_strOrNull(&b, DasNode_name(pNode));
	_jb_key(&b, "label");       _jb_strOrNull(&b, DasNode_label(pNode));
	_jb_key(&b, "title");       _jb_strOrNull(&b, DasNode_title(pNode));
	_jb_key(&b, "description"); _jb_strOrNull(&b, DasNode_summary(pNode));
	_jb_key(&b, "uri");         _jb_strOrNull(&b, DasNode_pathUri(pNode));
	_jb_key(&b, "url");         _jb_str(&b, DasNode_srcUrl(pNode));
	_jb_key(&b, "separator");   _jb_strOrNull(&b, DasNode_pathSep(pNode));

	_jb_key(&b, "children");
	_jb_open(&b, "[");
	size_t uKids = DasNode_numSubs(pNode);
	for(size_t u = 0; u < uKids; ++u){
		_jb_open(&b, "{");
		_jb_key(&b, "name"); _jb_str(&b, DasNode_subNameAt(pNode, u));
		_jb_key(&b, "kind"); _jb_str(&b, _node_kind_tok(DasNode_subTypeAt(pNode, u)));
		_jb_key(&b, "stub"); _jb_stub(&b, DasNode_subStubAt(pNode, u));
		_jb_raw(&b, "}");
	}
	_jb_raw(&b, "]");

	/* Counted first: the names are borrowed, so only the pointer array is
	   allocated here */
	_jb_key(&b, "loadable");
	_jb_open(&b, "[");
	size_t uLoad = DasSrc_loadable(pNode, NULL, 0);
	if(uLoad > 0){
		const char** aNames = (const char**)calloc(uLoad, sizeof(const char*));
		if(aNames == NULL){
			b.err = daserrf(DASERR_DS, "Couldn't allocate a loadable list");
		}
		else{
			DasSrc_loadable(pNode, aNames, uLoad);
			for(size_t u = 0; u < uLoad; ++u) _jb_str(&b, aNames[u]);
			free(aNames);
		}
	}
	_jb_raw(&b, "]");

	_jb_raw(&b, "}");

	if(b.err){ free(b.s); return NULL; }
	return b.s;
}

/* ************************************************************************* */
/* Data source snapshot */

static const char* _srcsec_tok(das_srcsec sec)
{
	const char* s = dassrcsec_name(sec);
	return (s != NULL) ? s : "";
}

static const char* _srcproptype_tok(das_srcproptype t)
{
	switch(t){
	case DASSRCPROPTYPE_BOOL: return "boolean";
	case DASSRCPROPTYPE_INT:  return "integer";
	case DASSRCPROPTYPE_REAL: return "real";
	case DASSRCPROPTYPE_TIME: return "time";
	case DASSRCPROPTYPE_ENUM: return "enum";
	default:                  return "text";
	}
}

/* A string member of a node dict, or NULL */
static const char* _jdo_mstr(const DasJdo* pDict, const char* sName)
{
	return (pDict == NULL) ? NULL : DasJdo_str(DasJdo_subtree(pDict, sName));
}

/* A property's short caption, the same rule DasSrcGroup_label() applies to
   groups: the node's label, else its name.  NULL when it has neither, so
   the caller can tell "no caption" from an empty one. */
static const char* _srcprop_label(const DasSrcProp* p)
{
	const char* s = _jdo_mstr(p->pJdo, "label");
	return (s != NULL) ? s : _jdo_mstr(p->pJdo, "name");
}

/* The display label of one enum value, NULL when the node gives none.
   Matched by value rather than position: das2C skips enum items it can't
   read, so its indices need not line up with the node's. */
static const char* _srcctrl_enumLabel(const DasSrcCtrl* pC, const char* sVal)
{
	const DasJdo* pEnum = DasJdo_subtree(pC->pSet, "enum");
	char sItem[256];
	for(const DasJsonAryEl* pEl = DasJdo_aryFirst(pEnum); pEl; pEl = pEl->next){
		if(pEl->value->type != DASJSONTYPE_DICT) continue;
		const DasJdo* pItem = DasJdo_subtree(pEl->value, "value");
		if(!_das_json_scalar(pItem, sItem, sizeof(sItem))) continue;
		if(strcmp(sItem, sVal) == 0) return _jdo_mstr(pEl->value, "label");
	}
	return NULL;
}

static void _jb_srcCtrl(JBuf* b, const DasSrcCtrl* pC)
{
	if(pC == NULL){ _jb_null(b); return; }
	_jb_open(b, "{");
	_jb_key(b, "param");    _jb_str(b, DasSrcCtrl_param(pC));
	_jb_key(b, "required"); _jb_bool(b, DasSrcCtrl_isRequired(pC));

	_jb_key(b, "choices");  _jb_open(b, "[");
	size_t uEnum = DasSrcCtrl_numEnums(pC);
	for(size_t u = 0; u < uEnum; ++u){
		const char* sVal = DasSrcCtrl_enumAt(pC, u);
		_jb_open(b, "{");
		_jb_key(b, "value"); _jb_str(b, sVal);
		_jb_key(b, "label"); _jb_strOrNull(b, _srcctrl_enumLabel(pC, sVal));
		_jb_raw(b, "}");
	}
	_jb_raw(b, "]");

	/* either end may be absent, and "now" is a legal high end */
	const char* sLo = NULL;
	const char* sHi = NULL;
	_jb_key(b, "range");
	if(DasSrcCtrl_range(pC, &sLo, &sHi)){
		_jb_open(b, "[");
		_jb_strOrNull(b, sLo);
		_jb_strOrNull(b, sHi);
		_jb_raw(b, "]");
	}
	else
		_jb_null(b);
	_jb_raw(b, "}");
}

static void _jb_srcProp(JBuf* b, const DasSrcProp* p)
{
	char sTitle[128];
	_jb_open(b, "{");
	_jb_key(b, "key");      _jb_str(b, DasSrcProp_key(p));
	_jb_key(b, "name");     _jb_str(b, DasSrcProp_name(p));
	_jb_key(b, "label");    _jb_strOrNull(b, _srcprop_label(p));
	_jb_key(b, "title");    _jb_str(b, DasSrcProp_title(p, sTitle, sizeof(sTitle)));
	_jb_key(b, "type");     _jb_str(b, _srcproptype_tok(DasSrcProp_type(p)));
	_jb_key(b, "typeName"); _jb_str(b, DasSrcProp_typeName(p));
	_jb_key(b, "units");    _jb_str(b, DasSrcProp_units(p));
	_jb_key(b, "value");    _jb_strOrNull(b, DasSrcProp_value(p));
	_jb_key(b, "ctrl");     _jb_srcCtrl(b, DasSrcProp_ctrl(p));
	_jb_raw(b, "}");
}

static void _jb_srcGroup(JBuf* b, const DasSrcGroup* pG)
{
	const DasSrcProp* pOn = DasSrcGroup_enableProp(pG);
	_jb_open(b, "{");
	_jb_key(b, "name");     _jb_str(b, DasSrcGroup_name(pG));
	_jb_key(b, "label");    _jb_str(b, DasSrcGroup_label(pG));
	_jb_key(b, "title");    _jb_str(b, DasSrcGroup_title(pG));
	_jb_key(b, "enable");   _jb_strOrNull(b, pOn ? DasSrcProp_key(pOn) : NULL);
	_jb_key(b, "xorGroup"); _jb_strOrNull(b, DasSrcGroup_xorGroup(pG));
	_jb_key(b, "mimes");    _jb_open(b, "[");
	for(size_t u = 0; u < DasSrcGroup_numMimes(pG); ++u) _jb_str(b, DasSrcGroup_mimeAt(pG, u));
	_jb_raw(b, "]");
	_jb_key(b, "props");    _jb_open(b, "[");
	for(size_t u = 0; u < DasSrcGroup_numProps(pG); ++u) _jb_srcProp(b, DasSrcGroup_propAt(pG, u));
	_jb_raw(b, "]");
	_jb_raw(b, "}");
}

char* daswasm_src_json(const DasSrc* pSrc)
{
	if(pSrc == NULL) return NULL;
	JBuf b = {NULL, 0, 0, 0};

	_jb_open(&b, "{");
	_jb_key(&b, "ptr");       _jb_int(&b, (long long)(uintptr_t)pSrc);
	_jb_key(&b, "kind");      _jb_str(&b, (DasSrc_kind(pSrc) == DASSRCKIND_STREAM) ? "stream" : "");
	_jb_key(&b, "path");      _jb_strOrNull(&b, DasSrc_path(pSrc));
	_jb_key(&b, "url");       _jb_strOrNull(&b, DasSrc_url(pSrc));
	_jb_key(&b, "name");      _jb_str(&b, DasSrc_name(pSrc));
	_jb_key(&b, "title");     _jb_str(&b, DasSrc_title(pSrc));
	_jb_key(&b, "version");   _jb_str(&b, DasSrc_version(pSrc));
	_jb_key(&b, "needsAuth"); _jb_bool(&b, DasSrc_needsAuth(pSrc));
	_jb_key(&b, "authRealm"); _jb_str(&b, DasSrc_authRealm(pSrc));

	_jb_key(&b, "contacts");  _jb_open(&b, "[");
	for(size_t u = 0; u < DasSrc_numContacts(pSrc); ++u){
		const char* sEmail = NULL;
		const char* sName = DasSrc_contactAt(pSrc, u, &sEmail);
		_jb_open(&b, "{");
		_jb_key(&b, "name");  _jb_str(&b, sName);
		_jb_key(&b, "email"); _jb_strOrNull(&b, sEmail);
		_jb_raw(&b, "}");
	}
	_jb_raw(&b, "]");

	_jb_key(&b, "examples");  _jb_open(&b, "[");
	for(size_t u = 0; u < DasSrc_numExamples(pSrc); ++u) _jb_str(&b, DasSrc_exampleLblAt(pSrc, u));
	_jb_raw(&b, "]");

	/* Presentation order, empty sections left out */
	_jb_key(&b, "sections");  _jb_open(&b, "[");
	for(int sec = 0; sec < DASSRCSEC_SIZE; ++sec){
		size_t uGroups = DasSrc_numGroups(pSrc, (das_srcsec)sec);
		if(uGroups == 0) continue;
		_jb_open(&b, "{");
		_jb_key(&b, "name");   _jb_str(&b, _srcsec_tok((das_srcsec)sec));
		_jb_key(&b, "groups"); _jb_open(&b, "[");
		for(size_t u = 0; u < uGroups; ++u)
			_jb_srcGroup(&b, DasSrc_groupAt(pSrc, (das_srcsec)sec, u));
		_jb_raw(&b, "]");
		_jb_raw(&b, "}");
	}
	_jb_raw(&b, "]");
	_jb_raw(&b, "}");

	if(b.err){ free(b.s); return NULL; }
	return b.s;
}

static const char* _reqverdict_tok(das_reqverdict v)
{
	switch(v){
	case DASREQVERDICT_OK:  return "ok";
	case DASREQVERDICT_BAD: return "bad";
	default:                return "unchecked";
	}
}

char* daswasm_req_json(const DasReq* pReq)
{
	if(pReq == NULL) return NULL;
	JBuf b = {NULL, 0, 0, 0};

	_jb_open(&b, "{");
	_jb_key(&b, "label");   _jb_str(&b, DasReq_label(pReq));
	_jb_key(&b, "entries"); _jb_open(&b, "[");
	for(size_t u = 0; u < DasReq_numKeys(pReq); ++u){
		_jb_open(&b, "{");
		_jb_key(&b, "key");     _jb_str(&b, DasReq_keyAt(pReq, u));
		_jb_key(&b, "value");   _jb_str(&b, DasReq_valAt(pReq, u));
		_jb_key(&b, "verdict"); _jb_str(&b, _reqverdict_tok(DasReq_verdictAt(pReq, u)));
		_jb_key(&b, "why");     _jb_str(&b, DasReq_whyAt(pReq, u));
		_jb_key(&b, "derived"); _jb_bool(&b, DasReq_isDerivedAt(pReq, u));
		_jb_raw(&b, "}");
	}
	_jb_raw(&b, "]");
	_jb_key(&b, "raw");     _jb_open(&b, "[");
	for(size_t u = 0; u < DasReq_numRaw(pReq); ++u){
		const char* sVal = NULL;
		const char* sName = DasReq_rawAt(pReq, u, &sVal);
		_jb_open(&b, "{");
		_jb_key(&b, "name");  _jb_str(&b, sName);
		_jb_key(&b, "value"); _jb_str(&b, sVal);
		_jb_raw(&b, "}");
	}
	_jb_raw(&b, "]");
	_jb_raw(&b, "}");

	if(b.err){ free(b.s); return NULL; }
	return b.s;
}
