/*
 * smap.c
 *
 * Map handling.
 *
 * Copyright (c) 2015 F. Aragon. All rights reserved.
 */ 

#include "smap.h"
#include "scommon.h"

/*
 * Internal functions
 */

static int cmp_i(const struct SMapii *a, const struct SMapii *b)
{
	return a->k - b->k;
}

static int cmp_u(const struct SMapuu *a, const struct SMapuu *b)
{
	return a->k > b->k ? 1 : a->k < b->k ? -1 : 0;
}

static int cmp_I(const struct SMapIx *a, const struct SMapIx *b)
{
	sint_t r = a->k - b->k;
	return r > 0 ? 1 : r < 0 ? -1 : 0;
}

static int cmp_s(const struct SMapSx *a, const struct SMapSx *b)
{
	return ss_cmp(a->k, b->k);
}

static int rw_inc_SM_I32I32(const st_t *t, stn_t *node, const stn_t *new_data)
{
	return ((struct SMapii *)node)->v += ((struct SMapii *)new_data)->v;
}

static int rw_inc_SM_U32U32(const st_t *t, stn_t *node, const stn_t *new_data)
{
	return ((struct SMapuu *)node)->v += ((struct SMapuu *)new_data)->v;
}

static int rw_inc_SM_IntInt(const st_t *t, stn_t *node, const stn_t *new_data)
{
	return ((struct SMapII *)node)->v += ((struct SMapII *)new_data)->v;
}

static int rw_inc_SM_StrInt(const st_t *t, stn_t *node, const stn_t *new_data)
{
	return ((struct SMapSI *)node)->v += ((struct SMapSI *)new_data)->v;
}

static void aux_is_delete(void *node)
{
	ss_free(&((struct SMapIS *)node)->v);
}

static void aux_si_delete(void *node)
{
	ss_free(&((struct SMapSI *)node)->x.k);
}

static void aux_ss_delete(void *node)
{
	ss_free(&((struct SMapSS *)node)->x.k, &((struct SMapIS *)node)->v);
}

static void aux_sp_delete(void *node)
{
	ss_free(&((struct SMapSP *)node)->x.k);
}

struct SV2X { sv_t *kv, *vv; };

static int aux_ii32_sort(struct STraverseParams *tp)
{
	const struct SMapii *cn = (const struct SMapii *)tp->cn;
	if (cn) {
		struct SV2X *v2x = (struct SV2X *)tp->context;
		sv_push_i(&v2x->kv, cn->k);
		sv_push_i(&v2x->vv, cn->v);
	}
	return 0;
}

static int aux_uu32_sort(struct STraverseParams *tp)
{
	const struct SMapuu *cn = (const struct SMapuu *)tp->cn;
	if (cn) {
		struct SV2X *v2x = (struct SV2X *)tp->context;
		sv_push_u(&v2x->kv, cn->k);
		sv_push_u(&v2x->vv, cn->v);
	}
	return 0;
}
static int aux_ii_sort(struct STraverseParams *tp)
{
	const struct SMapII *cn = (const struct SMapII *)tp->cn;
	if (cn) {
		struct SV2X *v2x = (struct SV2X *)tp->context;
		sv_push_i(&v2x->kv, cn->x.k);
		sv_push_i(&v2x->vv, cn->v);
	}
	return 0;
}

static int aux_is_ip_sort(struct STraverseParams *tp)
{
	const struct SMapIP *cn = (const struct SMapIP *)tp->cn;
	if (cn) {
		struct SV2X *v2x = (struct SV2X *)tp->context;
		sv_push_i(&v2x->kv, cn->x.k);
		sv_push(&v2x->vv, &cn->v);
	}
	return 0;
}

static int aux_si_sort(struct STraverseParams *tp)
{
	const struct SMapII *cn = (const struct SMapII *)tp->cn;
	if (cn) {
		struct SV2X *v2x = (struct SV2X *)tp->context;
		sv_push(&v2x->kv, &cn->x.k);
		sv_push_i(&v2x->vv, cn->v);
	}
	return 0;
}

static int aux_sp_ss_sort(struct STraverseParams *tp)
{
	const struct SMapII *cn = (const struct SMapII *)tp->cn;
	if (cn) {
		struct SV2X *v2x = (struct SV2X *)tp->context;
		sv_push(&v2x->kv, &cn->x.k);
		sv_push(&v2x->vv, &cn->v);
	}
	return 0;
}

static void smf_setup(const enum eSM_Type t, struct STConf *f)
{
	memset(f, 0, sizeof(*f));
	switch (t)
	{
	case SM_U32U32:
		f->cmp = (st_cmp_t)cmp_u;
		break;
	case SM_I32I32:
		f->cmp = (st_cmp_t)cmp_i;
		break;
	case SM_IntInt: case SM_IntStr: case SM_IntPtr:
		f->cmp = (st_cmp_t)cmp_I;
		break;
	case SM_StrInt: case SM_StrStr: case SM_StrPtr:
		f->cmp = (st_cmp_t)cmp_s;
		break;
	default:
		break;
	}
	f->type = (unsigned)t;
	f->node_size = sm_elem_size(t);
	f->iaux1 = SINT_MIN;
	f->paux1 = (const void *)ss_empty();
}

/*
* Allocation
*/

sm_t *sm_alloc_raw(const enum eSM_Type t, const sbool_t ext_buf, void *buffer,
		   const size_t buffer_size)
{
	struct STConf f;
	smf_setup(t, &f);
	return (sm_t *)st_alloc_raw(&f, ext_buf, buffer, buffer_size);
}

sm_t *sm_alloc(const enum eSM_Type t, const size_t n)
{
	struct STConf f;
	smf_setup(t, &f);
	return (sm_t *)st_alloc(&f, n);
}

void sm_free_aux(const size_t nargs, sm_t **m, ...)
{
	va_list ap;
	va_start(ap, m);
	if (m)
		sm_reset(*m);
	if (nargs > 1) {
		size_t i = 1;
		for (; i < nargs; i++)
			sm_reset(*va_arg(ap, sm_t **));
	}
	va_start(ap, m);
	sd_free_va(nargs, (sd_t **)m, ap);
	va_end(ap);
}

sm_t *sm_shrink(sm_t **m)
{
	return (sm_t *)st_shrink((st_t **)m);
}

size_t sm_elem_size(const enum eSM_Type t)
{
	switch (t) {
	case SM_I32I32:	return sizeof(struct SMapii);
	case SM_U32U32:	return sizeof(struct SMapuu);
	case SM_IntInt:	return sizeof(struct SMapII);
	case SM_IntStr:	return sizeof(struct SMapIS);
	case SM_IntPtr: return sizeof(struct SMapIP);
	case SM_StrInt:	return sizeof(struct SMapSI);
	case SM_StrStr:	return sizeof(struct SMapSS);
	case SM_StrPtr: return sizeof(struct SMapSP);
	default:
		break;
	}
	return 0;
}

sm_t *sm_dup(const sm_t *src)
{
	return st_dup(src);
}

sbool_t sm_reset(sm_t *m)
{
	RETURN_IF(!m, S_FALSE);
	RETURN_IF(!m->df.size, S_TRUE);
	stn_callback_t delete_callback = NULL;
	switch (m->f.type) {
	case SM_IntStr: delete_callback = aux_is_delete; break;
	case SM_StrInt: delete_callback = aux_si_delete; break;
	case SM_StrStr: delete_callback = aux_ss_delete; break;
	case SM_StrPtr: delete_callback = aux_sp_delete; break;
	}
	if (delete_callback) {	/* deletion of dynamic memory elems */
		stndx_t i = 0;
		for (; i < (stndx_t)m->df.size; i++) {
			stn_t *n = st_enum(m, i);
			delete_callback(n);
		}
	}
	st_set_size((st_t *)m, 0);
	return S_TRUE;
}

void sm_set_defaults(sm_t *m, const sint_t i_def_v, const ss_t *s_def_v)
{
	if (m) {
		m->f.iaux1 = i_def_v;
		m->f.paux1 = (const void *)s_def_v;
	}
}

/*
 * Accessors
 */

size_t sm_size(const sm_t *m)
{
	return st_len(m);
}

/*
 * Random access
 */

sint32_t sm_ii32_at(const sm_t *m, const sint32_t k)
{
	ASSERT_RETURN_IF(!m, SINT32_MIN);
	struct SMapii n;
	n.k = k;
	const struct SMapii *nr =
			(const struct SMapii *)st_locate(m, (const stn_t *)&n);
	return nr ? nr->v : (sint32_t)m->f.iaux1;
}

suint32_t sm_uu32_at(const sm_t *m, const suint32_t k)
{
	ASSERT_RETURN_IF(!m, 0);
	struct SMapuu n;
	n.k = k;
	const struct SMapuu *nr =
			(const struct SMapuu *)st_locate(m, (const stn_t *)&n);
	return nr ? nr->v : (suint32_t)m->f.iaux1;
}

sint_t sm_ii_at(const sm_t *m, const sint_t k)
{
	ASSERT_RETURN_IF(!m, SINT_MIN);
	struct SMapII n;
	n.x.k = k;
	const struct SMapII *nr =
			(const struct SMapII *)st_locate(m, (const stn_t *)&n);
	return nr ? nr->v : m->f.iaux1;
}

const ss_t *sm_is_at(const sm_t *m, const sint_t k)
{
	ASSERT_RETURN_IF(!m, ss_empty());
	struct SMapIS n;
	n.x.k = k;
	const struct SMapIS *nr =
			(const struct SMapIS *)st_locate(m, (const stn_t *)&n);
	return nr ? nr->v : (const ss_t *)m->f.paux1;
}

const void *sm_ip_at(const sm_t *m, const sint_t k)
{
	ASSERT_RETURN_IF(!m, NULL);
	struct SMapIP n;
	n.x.k = k;
	const struct SMapIP *nr =
			(const struct SMapIP *)st_locate(m, (const stn_t *)&n);
	return nr ? nr->v : NULL;
}

sint_t sm_si_at(const sm_t *m, const ss_t *k)
{
	ASSERT_RETURN_IF(!m, SINT_MIN);
	struct SMapSI n;
	n.x.k = (ss_t *)k;	/* not going to be overwritten */
	const struct SMapSI *nr =
			(const struct SMapSI *)st_locate(m, (const stn_t *)&n);
	return nr ? nr->v : m->f.iaux1;
}

const ss_t *sm_ss_at(const sm_t *m, const ss_t *k)
{
	ASSERT_RETURN_IF(!m, ss_empty());
	struct SMapSS n;
	n.x.k = (ss_t *)k;	/* not going to be overwritten */
	const struct SMapSS *nr =
			(const struct SMapSS *)st_locate(m, (const stn_t *)&n);
	return nr ? nr->v : (ss_t *)m->f.paux1;
}

const void *sm_sp_at(const sm_t *m, const ss_t *k)
{
	ASSERT_RETURN_IF(!m, NULL);
	struct SMapSP n;
	n.x.k = (ss_t *)k;	/* not going to be overwritten */
	const struct SMapSP *nr =
			(const struct SMapSP *)st_locate(m, (const stn_t *)&n);
	return nr ? nr->v : NULL;
}

/*
 * Existence check
 */

sbool_t sm_u_count(const sm_t *m, const suint32_t k)
{
	ASSERT_RETURN_IF(!m, S_FALSE);
	struct SMapuu n;
	n.k = k;
	return st_locate(m, (const stn_t *)&n) ? S_TRUE : S_FALSE;
}

sbool_t sm_i_count(const sm_t *m, const sint_t k)
{
	ASSERT_RETURN_IF(!m, S_FALSE);
	struct SMapIx n;
	n.k = k;
	return st_locate(m, (const stn_t *)&n) ? S_TRUE : S_FALSE;
}

sbool_t sm_s_count(const sm_t *m, const ss_t *k)
{
	ASSERT_RETURN_IF(!m, S_FALSE);
	struct SMapSX n;
	n.k = k;
	return st_locate(m, (const stn_t *)&n) ? S_TRUE : S_FALSE;
}

/*
 * Insert
 */

S_INLINE sbool_t sm_ii32_insert_aux(sm_t **m, const sint32_t k,
				    const sint32_t v, const st_rewrite_t rw_f)
{
	ASSERT_RETURN_IF(!m, S_FALSE);
	struct SMapii n;
	n.k = k;
	n.v = v;
	return st_insert_rw((st_t **)m, (const stn_t *)&n, rw_f);
}

sbool_t sm_ii32_insert(sm_t **m, const sint32_t k, const sint32_t v)
{
	return sm_ii32_insert_aux(m, k, v, NULL);
}

sbool_t sm_ii32_inc(sm_t **m, const sint32_t k, const sint32_t v)
{
	return sm_ii32_insert_aux(m, k, v, rw_inc_SM_I32I32);
}

S_INLINE sbool_t sm_uu32_insert_aux(sm_t **m, const suint32_t k,
				    const suint32_t v, const st_rewrite_t rw_f)
{
	ASSERT_RETURN_IF(!m, S_FALSE);
	struct SMapuu n;
	n.k = k;
	n.v = v;
	return st_insert_rw((st_t **)m, (const stn_t *)&n, rw_f);
}

sbool_t sm_uu32_insert(sm_t **m, const suint32_t k, const suint32_t v)
{
	return sm_uu32_insert_aux(m, k, v, NULL);
}

sbool_t sm_uu32_inc(sm_t **m, const suint32_t k, const suint32_t v)
{
	return sm_uu32_insert_aux(m, k, v, rw_inc_SM_U32U32);
}

S_INLINE sbool_t sm_ii_insert_aux(sm_t **m, const sint_t k,
			          const sint_t v, const st_rewrite_t rw_f)
{
	ASSERT_RETURN_IF(!m, S_FALSE);
	struct SMapII n;
	n.x.k = k;
	n.v = v;
	return st_insert_rw((st_t **)m, (const stn_t *)&n, rw_f);
}

sbool_t sm_ii_insert(sm_t **m, const sint_t k, const sint_t v)
{
	return sm_ii_insert_aux(m, k, v, NULL);
}

sbool_t sm_ii_inc(sm_t **m, const sint_t k, const sint_t v)
{
	return sm_ii_insert_aux(m, k, v, rw_inc_SM_IntInt);
}

sbool_t sm_is_insert(sm_t **m, const sint_t k, const ss_t *v)
{
	ASSERT_RETURN_IF(!m, S_FALSE);
	struct SMapIS n;
	n.x.k = k;
	n.v = ss_dup(v);
	return st_insert((st_t **)m, (const stn_t *)&n);
}

sbool_t sm_ip_insert(sm_t **m, const sint_t k, const void *v)
{
	ASSERT_RETURN_IF(!m, S_FALSE);
	struct SMapIP n;
	n.x.k = k;
	n.v = v;
	return st_insert((st_t **)m, (const stn_t *)&n);
}

S_INLINE sbool_t sm_si_insert_aux(sm_t **m, const ss_t *k,
				  const sint_t v, const st_rewrite_t rw_f)
{
	ASSERT_RETURN_IF(!m, S_FALSE);
	struct SMapSI n;
	n.x.k = NULL;
	ss_cpy(&n.x.k, k);
	n.v = v;
	return st_insert_rw((st_t **)m, (const stn_t *)&n, rw_f);
}

sbool_t sm_si_insert(sm_t **m, const ss_t *k, const sint_t v)
{
	return sm_si_insert_aux(m, k, v, NULL);
}

sbool_t sm_si_inc(sm_t **m, const ss_t *k, const sint_t v)
{
	return sm_si_insert_aux(m, k, v, rw_inc_SM_StrInt);
}

sbool_t sm_ss_insert(sm_t **m, const ss_t *k, const ss_t *v)
{
	ASSERT_RETURN_IF(!m, S_FALSE);
	struct SMapSS n;
	n.x.k = n.v = NULL;
	ss_cpy(&n.x.k, k);
	ss_cpy(&n.v, v);
	return st_insert((st_t **)m, (const stn_t *)&n);
}

sbool_t sm_sp_insert(sm_t **m, const ss_t *k, const void *v)
{
	ASSERT_RETURN_IF(!m, S_FALSE);
	struct SMapSP n;
	n.x.k = NULL;
	ss_cpy(&n.x.k, k);
	n.v = v;
	return st_insert((st_t **)m, (const stn_t *)&n);
}

/*
 * Delete
 */

sbool_t sm_i_delete(sm_t *m, const sint_t k)
{
	struct SMapIx ix;
	struct SMapii ii;
	struct SMapuu uu;
	const stn_t *n;
	stn_callback_t callback = NULL;
	switch (m->f.type) {
	case SM_I32I32:
		RETURN_IF(k > SINT32_MAX || k < SINT32_MIN, S_FALSE);
		ii.k = (sint32_t)k;
		n = (const stn_t *)&ii;
		break;
	case SM_U32U32:
		RETURN_IF(k > SUINT32_MAX, S_FALSE);
		uu.k = (suint32_t)k;
		n = (const stn_t *)&uu;
		break;
	case SM_IntStr:
		callback = aux_is_delete;
		/* don't break */
	case SM_IntInt: case SM_IntPtr:
		ix.k = k;
		n = (const stn_t *)&ix;
		break;
	default:
		return S_FALSE;
	}
	return st_delete(m, n, callback);
}

sbool_t sm_s_delete(sm_t *m, const ss_t *k)
{
	stn_callback_t callback = NULL;
	struct SMapSx sx;
	sx.k = (ss_t *)k;	/* not going to be overwritten */
	switch (m->f.type) {
		case SM_StrInt: callback = aux_si_delete; break;
		case SM_StrStr: callback = aux_ss_delete; break;
		case SM_StrPtr: callback = aux_sp_delete; break;
	}
	return st_delete(m, (const stn_t *)&sx, callback);
}

/*
 * Enumeration / export data
 */

stn_t *sm_enum(sm_t *m, const stndx_t i)
{
	return st_enum(m, i);
}

const stn_t *sm_enum_r(const sm_t *m, const stndx_t i)
{
	return st_enum_r(m, i);
}

ssize_t sm_inorder_enum(const sm_t *m, st_traverse f, void *context)
{
	return st_traverse_inorder((const st_t *)m, f, context);
}

ssize_t sm_sort_to_vectors(const sm_t *m, sv_t **kv, sv_t **vv)
{
	RETURN_IF(!kv || !vv, 0);
	struct SV2X v2x = { *kv, *vv };
	st_traverse traverse_f = NULL;
	enum eSV_Type kt, vt;
	switch (m->f.type) {
	case SM_I32I32:
		kt = vt = SV_I32;
		break;
	case SM_U32U32:
		kt = vt = SV_U32;
		break;
	case SM_IntInt: case SM_IntStr: case SM_IntPtr:
		kt = SV_I64;
		vt = m->f.type == SM_IntInt ? SV_I64 : SV_GEN;
		break;
	case SM_StrInt: case SM_StrStr: case SM_StrPtr:
		kt = SV_GEN;
		vt = m->f.type == SM_StrInt ? SV_I64 : SV_GEN;
		break;
	default: return 0;
	}
	if (v2x.kv) {
		if (v2x.kv->sv_type != (char)kt)
			sv_free(&v2x.kv);
		else
			sv_reserve(&v2x.kv, m->df.size);
	}
	if (v2x.vv) {
		if (v2x.vv->sv_type != (char)vt)
			sv_free(&v2x.vv);
		else
			sv_reserve(&v2x.vv, m->df.size);
	}
	if (!v2x.kv)
		v2x.kv = sv_alloc_t(kt, m->df.size);
	if (!v2x.vv)
		v2x.vv = sv_alloc_t(vt, m->df.size);
	switch (m->f.type) {
	case SM_I32I32: traverse_f = aux_ii32_sort; break;
	case SM_U32U32: traverse_f = aux_uu32_sort; break;
	case SM_IntInt: traverse_f = aux_ii_sort; break;
	case SM_IntStr: case SM_IntPtr: traverse_f = aux_is_ip_sort; break;
	case SM_StrInt: traverse_f = aux_si_sort; break;
	case SM_StrStr: case SM_StrPtr: traverse_f = aux_sp_ss_sort; break;
	}
	ssize_t r = st_traverse_inorder((const st_t *)m, traverse_f,
					(void *)&v2x);
	*kv = v2x.kv;
	*vv = v2x.vv;
	return r;
}

