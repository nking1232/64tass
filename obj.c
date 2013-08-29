/*
    $Id$

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/
#include <string.h>
#include "values.h"
#include "variables.h"
#include "misc.h"
#include "section.h"
#include "64tass.h"

#include "listobj.h"
#include "boolobj.h"
#include "addressobj.h"
#include "codeobj.h"
#include "floatobj.h"
#include "strobj.h"
#include "bytesobj.h"
#include "bitsobj.h"
#include "intobj.h"

int referenceit = 1;

static struct obj_s macro_obj;
static struct obj_s segment_obj;
static struct obj_s lbl_obj;
static struct obj_s function_obj;
static struct obj_s struct_obj;
static struct obj_s union_obj;
static struct obj_s identref_obj;
static struct obj_s none_obj;
static struct obj_s error_obj;
static struct obj_s gap_obj;
static struct obj_s ident_obj;
static struct obj_s anonident_obj;
static struct obj_s oper_obj;
static struct obj_s default_obj;
static struct obj_s dict_obj;
static struct obj_s pair_obj;

obj_t MACRO_OBJ = &macro_obj;
obj_t SEGMENT_OBJ = &segment_obj;
obj_t LBL_OBJ = &lbl_obj;
obj_t FUNCTION_OBJ = &function_obj;
obj_t STRUCT_OBJ = &struct_obj;
obj_t UNION_OBJ = &union_obj;
obj_t IDENTREF_OBJ = &identref_obj;
obj_t NONE_OBJ = &none_obj;
obj_t ERROR_OBJ = &error_obj;
obj_t GAP_OBJ = &gap_obj;
obj_t IDENT_OBJ = &ident_obj;
obj_t ANONIDENT_OBJ = &anonident_obj;
obj_t OPER_OBJ = &oper_obj;
obj_t DEFAULT_OBJ = &default_obj;
obj_t DICT_OBJ = &dict_obj;
obj_t PAIR_OBJ = &pair_obj;

static void error_copy(const struct value_s *, struct value_s *);

void obj_oper_error(oper_t op) {
    struct value_s *e1, *e2;
    struct value_s *v1 = op->v1, *v2 = op->v2, *v = op->v;
    switch (op->op->u.oper.op) {
    case O_EQ: 
        if (v == v1 || v == v2) obj_destroy(v);
        bool_from_int(v, v1->obj->type == v2->obj->type); return;
    case O_NE:
        if (v == v1 || v == v2) obj_destroy(v);
        bool_from_int(v, v1->obj->type != v2->obj->type); return;
    default: break;
    }
    if (v == v1) {
        e1 = val_alloc();
        v->obj->copy_temp(v1, e1);
    } else e1 = v1 ? val_reference(v1) : NULL;
    if (v == v2) {
        e2 = val_alloc();
        v->obj->copy_temp(v2, e2);
    } else e2 = v2 ? val_reference(v2) : NULL;
    v->obj = ERROR_OBJ;
    v->u.error.num = ERROR__INVALID_OPER;
    v->u.error.u.invoper.op = op->op;
    v->u.error.u.invoper.v1 = e1;
    v->u.error.u.invoper.v2 = e2;
    v->u.error.epoint = op->epoint3;
}

static void invalid_destroy(struct value_s *UNUSED(v1)) {
    return;
}

static void invalid_copy(const struct value_s *v1, struct value_s *v) {
    *v = *v1;
    v->refcount = 1;
}

static int invalid_same(const struct value_s *v1, const struct value_s *v2) {
    return v1->obj == v2->obj;
}

static int invalid_truth(const struct value_s *UNUSED(v1)) {
    return 1;
}

static int invalid_hash(const struct value_s *v1, struct value_s *v, linepos_t epoint) {
    if (v1->obj == ERROR_OBJ) {
        if (v != v1) error_copy(v1, v);
        return -1;
    }
    v->obj = ERROR_OBJ;
    v->u.error.num = ERROR__NOT_HASHABLE;
    v->u.error.epoint = *epoint;
    return -1;
}

static void invalid_repr(const struct value_s *v1, struct value_s *v) {
    uint8_t *s;
    const char *name;
    if (v1->obj == ERROR_OBJ) {
        if (v != v1) error_copy(v1, v);
        return;
    }
    name = v1->obj->name;
    if (v == v1) v->obj->destroy(v);
    v->obj = STR_OBJ;
    v->u.str.len = strlen(name);
    v->u.str.chars = v->u.str.len;
    s = (uint8_t *)malloc(v->u.str.len);
    if (!s) err_msg_out_of_memory();
    memcpy(s, name, v->u.str.len);
    v->u.str.data = s;
}

static void invalid_str(const struct value_s *v1, struct value_s *v) {
    if (v1->obj == ERROR_OBJ) {
        if (v != v1) error_copy(v1, v);
        return;
    }
    return v1->obj->repr(v1, v);
}

static int gap_hash(const struct value_s *UNUSED(v1), struct value_s *UNUSED(v), linepos_t UNUSED(epoint)) {
    return 0; /* whatever, there's only one */
}

static void gap_repr(const struct value_s *UNUSED(v1), struct value_s *v) {
    uint8_t *s;
    v->obj = STR_OBJ;
    v->u.str.len = 1;
    v->u.str.chars = 1;
    s = (uint8_t *)malloc(v->u.str.len);
    if (!s) err_msg_out_of_memory();
    *s = '?';
    v->u.str.data = s;
}

static void invalid_calc1(oper_t op) {
    obj_oper_error(op);
}

static void invalid_calc2(oper_t op) {
    if (op->v2->obj == ERROR_OBJ) {
        ERROR_OBJ->rcalc2(op);
        return;
    }
    obj_oper_error(op);
}

static void invalid_rcalc2(oper_t op) {
    if (op->v1->obj == ERROR_OBJ) {
        ERROR_OBJ->calc2(op);
        return;
    }
    obj_oper_error(op);
}

static void invalid_repeat(oper_t op, uval_t UNUSED(rep)) {
    if (op->v1->obj == ERROR_OBJ) {
        ERROR_OBJ->calc2(op);
        return;
    }
    obj_oper_error(op);
}

static void invalid_iindex(oper_t op) {
    obj_oper_error(op);
}

static void invalid_slice(struct value_s *v1, ival_t UNUSED(offs), ival_t UNUSED(end), ival_t UNUSED(step), struct value_s *v, linepos_t epoint) {
    struct oper_s oper;
    oper.op = &o_SLICE;
    oper.v1 = v1;
    oper.v2 = NULL;
    oper.v = v;
    oper.epoint3 = *epoint;
    obj_oper_error(&oper);
}

static int MUST_CHECK invalid_ival(const struct value_s *UNUSED(v1), struct value_s *v, ival_t *UNUSED(iv), int UNUSED(bits), linepos_t epoint) {
    v->obj = ERROR_OBJ;
    v->u.error.num = ERROR______CANT_INT;
    v->u.error.epoint = *epoint;
    return 1;
}

static int MUST_CHECK invalid_uval(const struct value_s *UNUSED(v1), struct value_s *v, uval_t *UNUSED(uv), int UNUSED(bits), linepos_t epoint) {
    v->obj = ERROR_OBJ;
    v->u.error.num = ERROR______CANT_INT;
    v->u.error.epoint = *epoint;
    return 1;
}

static int MUST_CHECK invalid_real(const struct value_s *UNUSED(v1), struct value_s *v, double *UNUSED(r), linepos_t epoint) {
    v->obj = ERROR_OBJ;
    v->u.error.num = ERROR_____CANT_REAL;
    v->u.error.epoint = *epoint;
    return 1;
}

static int MUST_CHECK invalid_sign(const struct value_s *UNUSED(v1), struct value_s *v, int *UNUSED(s), linepos_t epoint) {
    v->obj = ERROR_OBJ;
    v->u.error.num = ERROR_____CANT_SIGN;
    v->u.error.epoint = *epoint;
    return 1;
}

static void invalid_abs(const struct value_s *v1, struct value_s *v, linepos_t epoint) {
    if (v1 == v) v->obj->destroy(v);
    v->obj = ERROR_OBJ;
    v->u.error.num = ERROR______CANT_ABS;
    v->u.error.epoint = *epoint;
}

static void invalid_integer(const struct value_s *v1, struct value_s *v, linepos_t epoint) {
    if (v1 == v) v->obj->destroy(v);
    v->obj = ERROR_OBJ;
    v->u.error.num = ERROR______CANT_INT;
    v->u.error.epoint = *epoint;
}

static int MUST_CHECK invalid_len(const struct value_s *UNUSED(v1), struct value_s *v, uval_t *UNUSED(uv), linepos_t epoint) {
    v->obj = ERROR_OBJ;
    v->u.error.num = ERROR______CANT_LEN;
    v->u.error.epoint = *epoint;
    return 1;
}

static void pair_destroy(struct value_s *v1) {
    val_destroy(v1->u.pair.key);
    val_destroy(v1->u.pair.data);
}

static void pair_copy(const struct value_s *v1, struct value_s *v) {
    v->obj = PAIR_OBJ;
    v->refcount = 1;
    v->u.pair.key = val_reference(v1->u.pair.key);
    v->u.pair.data = val_reference(v1->u.pair.data);
}

static void pair_copy_temp(const struct value_s *v1, struct value_s *v) {
    v->obj = PAIR_OBJ;
    v->refcount = 1;
    v->u.pair.key = v1->u.pair.key;
    v->u.pair.data = v1->u.pair.data;
}

static int pair_same(const struct value_s *v1, const struct value_s *v2) {
    if (v2->obj != PAIR_OBJ)  return 0;
    if (!obj_same(v1->u.pair.key, v2->u.pair.key)) return 0;
    if (!obj_same(v1->u.pair.data, v2->u.pair.data)) return 0;
    return 1;
}

static void pair_repr(const struct value_s *v1, struct value_s *v) {
    size_t len = 1, chars;
    struct value_s tmp[2];
    uint8_t *s;

    v1->u.pair.key->obj->repr(v1->u.pair.key, &tmp[0]);
    len += tmp[0].u.str.len;
    if (len < tmp[0].u.str.len) err_msg_out_of_memory(); /* overflow */
    v1->u.pair.data->obj->repr(v1->u.pair.data, &tmp[1]);
    len += tmp[1].u.str.len;
    s = (uint8_t *)malloc(len);
    if (!s || len < tmp[1].u.str.len) err_msg_out_of_memory(); /* overflow */
    memcpy(s, tmp[0].u.str.data, tmp[0].u.str.len);
    len = tmp[0].u.str.len;
    chars = tmp[0].u.str.len - tmp[0].u.str.chars;
    tmp[0].obj->destroy(&tmp[0]);
    s[len++] = ':';
    memcpy(s + len, tmp[1].u.str.data, tmp[1].u.str.len);
    len += tmp[1].u.str.len;
    chars += tmp[1].u.str.len - tmp[1].u.str.chars;
    tmp[1].obj->destroy(&tmp[1]);
    if (v1 == v) v->obj->destroy(v);
    v->obj = STR_OBJ;
    v->u.str.data = s;
    v->u.str.len = len;
    v->u.str.chars = len - chars;
}

static void macro_destroy(struct value_s *v1) {
    while (v1->u.macro.argc) {
        --v1->u.macro.argc;
        free((char *)v1->u.macro.param[v1->u.macro.argc].name.data);
        free((char *)v1->u.macro.param[v1->u.macro.argc].init.data);
    }
    free(v1->u.macro.param);
}

static void macro_copy(const struct value_s *v1, struct value_s *v) {
    v->obj = v1->obj;
    v->refcount = 1;
    memcpy(&v->u.macro, &v1->u.macro, sizeof(v->u.macro));
    if (v1->u.macro.argc) {
        size_t i;
        v->u.macro.param = malloc(v1->u.macro.argc * sizeof(v->u.macro.param[0]));
        if (!v->u.macro.param) err_msg_out_of_memory();
        for (i = 0; i < v1->u.macro.argc; i++) {
            str_cpy(&v->u.macro.param[i].name, &v1->u.macro.param[i].name);
            str_cpy(&v->u.macro.param[i].init, &v1->u.macro.param[i].init);
        }
        v->u.macro.argc = i;
    } else v->u.macro.param = NULL;
}

static void macro_copy_temp(const struct value_s *v1, struct value_s *v) {
    v->obj = v1->obj;
    v->refcount = 1;
    memcpy(&v->u.macro, &v1->u.macro, sizeof(v->u.macro));
}

static int macro_same(const struct value_s *v1, const struct value_s *v2) {
    size_t i;
    if (v1->obj != v2->obj || v1->u.macro.p != v2->u.macro.p || v1->u.macro.file_list != v2->u.macro.file_list || v1->u.macro.sline != v2->u.macro.sline || v1->u.macro.size != v2->u.macro.size) return 0;
    for (i = 0; i < v1->u.macro.argc; i++) {
        if (str_cmp(&v1->u.macro.param[i].name, &v2->u.macro.param[i].name)) return 0;
        if (str_cmp(&v1->u.macro.param[i].init, &v2->u.macro.param[i].init)) return 0;
    }
    return 1;
}

static void function_destroy(struct value_s *v1) {
    size_t i = v1->u.func.argc;
    while (i--) {
        free((char *)v1->u.func.param[i].name.data);
        if (v1->u.func.param[i].init) val_destroy(v1->u.func.param[i].init);
    }
    free(v1->u.func.param);
}

static void function_copy(const struct value_s *v1, struct value_s *v) {
    v->obj = FUNCTION_OBJ;
    v->refcount = 1;
    memcpy(&v->u.func, &v1->u.func, sizeof(v->u.func));
    if (v1->u.func.argc) {
        size_t i;
        v->u.func.param = malloc(v1->u.func.argc * sizeof(v->u.func.param[0]));
        if (!v->u.func.param) err_msg_out_of_memory();
        for (i = 0; i < v1->u.func.argc; i++) {
            str_cpy(&v->u.func.param[i].name, &v1->u.func.param[i].name);
            if (v1->u.func.param[i].init) {
                v->u.func.param[i].init = val_reference(v1->u.func.param[i].init);
            } else v->u.func.param[i].init = NULL;
            v->u.func.param[i].epoint = v1->u.func.param[i].epoint;
        }
        v->u.func.argc = i;
    } else v->u.func.param = NULL;
}

static void function_copy_temp(const struct value_s *v1, struct value_s *v) {
    v->obj = FUNCTION_OBJ;
    v->refcount = 1;
    memcpy(&v->u.func, &v1->u.func, sizeof(v->u.func));
}

static int function_same(const struct value_s *v1, const struct value_s *v2) {
    size_t i;
    if (v2->obj != FUNCTION_OBJ || v1->u.func.p != v2->u.func.p || v1->u.func.file_list != v2->u.func.file_list || v1->u.func.sline != v2->u.func.sline || v1->u.func.context != v2->u.func.context) return 0;
    for (i = 0; i < v1->u.func.argc; i++) {
        if (str_cmp(&v1->u.func.param[i].name, &v2->u.func.param[i].name)) return 0;
        if (v1->u.func.param[i].init != v2->u.func.param[i].init && (!v1->u.func.param[i].init || !v2->u.func.param[i].init || !obj_same(v1->u.func.param[i].init, v2->u.func.param[i].init))) return 0;
        if (v1->u.func.param[i].epoint.pos != v2->u.func.param[i].epoint.pos) return 0;
        if (v1->u.func.param[i].epoint.upos != v2->u.func.param[i].epoint.upos) return 0;
    }
    return 1;
}

static void dict_free(struct avltree_node *aa)
{
    struct pair_s *a = avltree_container_of(aa, struct pair_s, node);
    val_destroy(a->key);
    val_destroy(a->data);
    free(a);
}

static void dict_destroy(struct value_s *v1) {
    avltree_destroy(&v1->u.dict.members, dict_free);
}

static void dict_copy(const struct value_s *v1, struct value_s *v) {
    v->obj = DICT_OBJ;
    v->refcount = 1;
    v->u.dict.len = v1->u.dict.len;
    avltree_init(&v->u.dict.members);
    if (v1->u.dict.len) {
        const struct avltree_node *n = avltree_first(&v1->u.dict.members);
        const struct pair_s *p;
        struct pair_s *p2;
        while (n) {
            p = cavltree_container_of(n, struct pair_s, node);
            p2 = (struct pair_s *)malloc(sizeof(struct pair_s));
            if (!p2) err_msg_out_of_memory();
            p2->hash = p->hash;
            p2->key = val_reference(p->key);
            p2->data = val_reference(p->data);
            avltree_insert(&p2->node, &v->u.dict.members, pair_compare);
            n = avltree_next(n);
        }
    }
}

static void dict_copy_temp(const struct value_s *v1, struct value_s *v) {
    v->obj = DICT_OBJ;
    v->refcount = 1;
    v->u.dict.len = v1->u.dict.len;
    v->u.dict.members = v1->u.dict.members;
}

static int dict_same(const struct value_s *v1, const struct value_s *v2) {
    const struct avltree_node *n;
    const struct avltree_node *n2;
    if (v2->obj != DICT_OBJ || v1->u.dict.len != v2->u.dict.len) return 0;
    n = avltree_first(&v1->u.dict.members);
    n2 = avltree_first(&v2->u.dict.members);
    while (n && n2) {
        const struct pair_s *p, *p2;
        if (pair_compare(n, n2)) return 0;
        p = cavltree_container_of(n, struct pair_s, node);
        p2 = cavltree_container_of(n2, struct pair_s, node);
        if (!obj_same(p->data, p2->data)) return 0;
        n = avltree_next(n);
        n2 = avltree_next(n2);
    }
    return n == n2;
}

static int dict_truth(const struct value_s *v1) {
    return !!v1->u.dict.len;
}

static int MUST_CHECK dict_len(const struct value_s *v1, struct value_s *UNUSED(v), uval_t *uv, linepos_t UNUSED(epoint)) {
    *uv = v1->u.dict.len;
    return 0;
}

static void dict_repr(const struct value_s *v1, struct value_s *v) {
    const struct avltree_node *n;
    const struct pair_s *p;
    size_t i, len = 2, chars = 0;
    struct value_s *tmp = NULL;
    uint8_t *s;
    if (v1->u.dict.len) {
        len = v1->u.dict.len + v1->u.dict.len;
        tmp = (struct value_s *)malloc(len * sizeof(struct value_s));
        if (!tmp || len < v1->u.dict.len || len > ((size_t)~0) / sizeof(struct value_s)) err_msg_out_of_memory(); /* overflow */
        len += 1;
        if (len < 1) err_msg_out_of_memory(); /* overflow */

        n = avltree_first(&v1->u.dict.members); i = 0;
        while (n) {
            p = cavltree_container_of(n, struct pair_s, node);
            p->key->obj->repr(p->key, &tmp[i]);
            len += tmp[i].u.str.len;
            if (len < tmp[i].u.str.len) err_msg_out_of_memory(); /* overflow */
            i++;
            p->data->obj->repr(p->data, &tmp[i]);
            n = avltree_next(n);
            len += tmp[i].u.str.len;
            if (len < tmp[i].u.str.len) err_msg_out_of_memory(); /* overflow */
            i++;
        }
    }
    s = (uint8_t *)malloc(len);
    if (!s) err_msg_out_of_memory();
    len = 0;
    s[len++] = '{';
    for (i = 0;i < v1->u.dict.len * 2;) {
        if (i) s[len++] = ',';
        memcpy(s + len, tmp[i].u.str.data, tmp[i].u.str.len);
        len += tmp[i].u.str.len;
        chars += tmp[i].u.str.len - tmp[i].u.str.chars;
        tmp[i].obj->destroy(&tmp[i]);
        i++;
        s[len++] = ':';
        memcpy(s + len, tmp[i].u.str.data, tmp[i].u.str.len);
        len += tmp[i].u.str.len;
        chars += tmp[i].u.str.len - tmp[i].u.str.chars;
        tmp[i].obj->destroy(&tmp[i]);
        i++;
    }
    s[len++] = '}';
    free(tmp);
    if (v1 == v) v->obj->destroy(v);
    v->obj = STR_OBJ;
    v->u.str.data = s;
    v->u.str.len = len;
    v->u.str.chars = len - chars;
}

static void dict_rcalc2(oper_t op) {
    if (op->op == &o_IN) {
        struct pair_s p;
        struct avltree_node *b;

        p.key = op->v1;
        p.hash = obj_hash(p.key, op->v, &op->epoint);
        if (p.hash >= 0) {
            b = avltree_lookup(&p.node, &op->v2->u.dict.members, pair_compare);
            if (op->v == op->v1) obj_destroy(op->v);
            bool_from_int(op->v, b != NULL); return;
        }
        return;
    } else {
        op->v1->obj->calc2(op); return;
    }
    obj_oper_error(op);
}

static void error_destroy(struct value_s *v1) {
    if (v1->u.error.num == ERROR__INVALID_OPER) {
        if (v1->u.error.u.invoper.v1) val_destroy(v1->u.error.u.invoper.v1);
        if (v1->u.error.u.invoper.v2) val_destroy(v1->u.error.u.invoper.v2);
    }
}

static void error_copy(const struct value_s *v1, struct value_s *v) {
    v->obj = ERROR_OBJ;
    v->refcount = 1;
    memcpy(&v->u.error, &v1->u.error, sizeof(v->u.error));
    if (v1->u.error.num == ERROR__INVALID_OPER) {
        if (v1->u.error.u.invoper.v1) v->u.error.u.invoper.v1 = val_reference(v1->u.error.u.invoper.v1);
        if (v1->u.error.u.invoper.v2) v->u.error.u.invoper.v2 = val_reference(v1->u.error.u.invoper.v2);
    }
}

static void error_copy_temp(const struct value_s *v1, struct value_s *v) {
    v->obj = ERROR_OBJ;
    v->refcount = 1;
    memcpy(&v->u.error, &v1->u.error, sizeof(v->u.error));
}

static void error_calc1(oper_t op) {
    if (op->v == op->v1) return;
    error_copy(op->v1, op->v);
}

static void error_calc2(oper_t op) {
    if (op->v == op->v1) return;
    error_copy(op->v1, op->v);
}

static void error_rcalc2(oper_t op) {
    if (op->v == op->v2) return;
    error_copy(op->v2, op->v);
}

static int MUST_CHECK error_ival(const struct value_s *v1, struct value_s *v, ival_t *UNUSED(iv), int UNUSED(bits), linepos_t UNUSED(epoint)) {
    error_copy(v1, v);
    return 1;
}

static int MUST_CHECK error_uval(const struct value_s *v1, struct value_s *v, uval_t *UNUSED(uv), int UNUSED(bits), linepos_t UNUSED(epoint)) {
    error_copy(v1, v);
    return 1;
}

static int MUST_CHECK error_real(const struct value_s *v1, struct value_s *v, double *UNUSED(r), linepos_t UNUSED(epoint)) {
    error_copy(v1, v);
    return 1;
}

static int MUST_CHECK error_sign(const struct value_s *v1, struct value_s *v, int *UNUSED(s), linepos_t UNUSED(epoint)) {
    error_copy(v1, v);
    return 1;
}

static void error_abs(const struct value_s *v1, struct value_s *v, linepos_t UNUSED(epoint)) {
    error_copy(v1, v);
}

static void error_integer(const struct value_s *v1, struct value_s *v, linepos_t UNUSED(epoint)) {
    error_copy(v1, v);
}

static int MUST_CHECK error_len(const struct value_s *v1, struct value_s *v, uval_t *UNUSED(len), linepos_t UNUSED(epoint)) {
    error_copy(v1, v);
    return 1;
}

static int ident_resolv(struct value_s *v1, struct value_s *v) {
    struct label_s *l = find_label(&v1->u.ident.name);
    struct linepos_s epoint;
    if (l) {
        l->shadowcheck = 1;
        v->u.identref.epoint = v1->u.ident.epoint;
        v->obj = IDENTREF_OBJ;
        v->u.identref.label = l;
        return 1;
    }
    epoint = v1->u.ident.epoint;
    v->u.error.u.ident = v1->u.ident.name;
    v->obj = ERROR_OBJ;
    v->u.error.epoint = epoint;
    v->u.error.num = ERROR___NOT_DEFINED;
    return 0;
}

static void ident_calc1(oper_t op) {
    if (op->v1 == op->v) {
        if (ident_resolv(op->v1, op->v)) IDENTREF_OBJ->calc1(op);
    } else {
        struct value_s tmp, *old = op->v1;
        op->v1 = &tmp;
        if (ident_resolv(old, &tmp)) IDENTREF_OBJ->calc1(op);
        else tmp.obj->copy_temp(&tmp, op->v);
        op->v1 = old;
    }
}

static void ident_calc2(oper_t op) {
    if (op->v1 == op->v) {
        if (ident_resolv(op->v1, op->v)) IDENTREF_OBJ->calc2(op);
    } else {
        struct value_s tmp, *old = op->v1;
        op->v1 = &tmp;
        if (ident_resolv(old, &tmp)) IDENTREF_OBJ->calc2(op);
        else tmp.obj->copy_temp(&tmp, op->v);
        op->v1 = old;
    }
}

static void ident_rcalc2(oper_t op) {
    if (op->op == &o_MEMBER) {
        op->v1->obj->calc2(op);return;
    }
    if (op->v2 == op->v) {
        if (ident_resolv(op->v2, op->v)) IDENTREF_OBJ->rcalc2(op);
    } else {
        struct value_s tmp, *old = op->v2; 
        op->v2 = &tmp;
        if (ident_resolv(old, &tmp)) IDENTREF_OBJ->rcalc2(op);
        else tmp.obj->copy_temp(&tmp, op->v);
        op->v2 = old;
    }
}

static int ident_hash(const struct value_s *v1, struct value_s *v, linepos_t epoint) {
    if (v1 == v) {
        if (ident_resolv((struct value_s *)v1, v)) return IDENTREF_OBJ->hash(v, v, epoint);
    } else {
        struct value_s tmp;
        if (ident_resolv((struct value_s *)v1, &tmp)) return IDENTREF_OBJ->hash(&tmp, v, epoint);
        tmp.obj->copy_temp(&tmp, v);
    }
    return -1;
}

static void ident_repr(const struct value_s *v1, struct value_s *v) {
    if (v1 == v) {
        if (ident_resolv((struct value_s *)v1, v)) return IDENTREF_OBJ->repr(v, v);
    } else {
        struct value_s tmp;
        if (ident_resolv((struct value_s *)v1, &tmp)) return IDENTREF_OBJ->repr(&tmp, v);
        tmp.obj->copy_temp(&tmp, v);
    }
}

static int MUST_CHECK ident_ival(const struct value_s *v1, struct value_s *v, ival_t *iv, int bits, linepos_t epoint) {
    if (v1 == v) {
        if (ident_resolv((struct value_s *)v1, v)) return IDENTREF_OBJ->ival(v, v, iv, bits, epoint);
    } else {
        struct value_s tmp;
        if (ident_resolv((struct value_s *)v1, &tmp)) return IDENTREF_OBJ->ival(&tmp, v, iv, bits, epoint);
        tmp.obj->copy_temp(&tmp, v);
    }
    return 1;
}

static int MUST_CHECK ident_uval(const struct value_s *v1, struct value_s *v, uval_t *uv, int bits, linepos_t epoint) {
    if (v1 == v) {
        if (ident_resolv((struct value_s *)v1, v)) return IDENTREF_OBJ->uval(v, v, uv, bits, epoint);
    } else {
        struct value_s tmp;
        if (ident_resolv((struct value_s *)v1, &tmp)) return IDENTREF_OBJ->uval(&tmp, v, uv, bits, epoint);
        tmp.obj->copy_temp(&tmp, v);
    }
    return 1;
}

static void ident_str(const struct value_s *v1, struct value_s *v) {
    if (v1 == v) {
        if (ident_resolv((struct value_s *)v1, v)) return IDENTREF_OBJ->str(v, v);
    } else {
        struct value_s tmp;
        if (ident_resolv((struct value_s *)v1, &tmp)) return IDENTREF_OBJ->str(&tmp, v);
        tmp.obj->copy_temp(&tmp, v);
    }
}

static int MUST_CHECK ident_real(const struct value_s *v1, struct value_s *v, double *r, linepos_t epoint) {
    if (v1 == v) {
        if (ident_resolv((struct value_s *)v1, v)) return IDENTREF_OBJ->real(v, v, r, epoint);
    } else {
        struct value_s tmp;
        if (ident_resolv((struct value_s *)v1, &tmp)) return IDENTREF_OBJ->real(&tmp, v, r, epoint);
        tmp.obj->copy_temp(&tmp, v);
    }
    return 1;
}

static void ident_integer(const struct value_s *v1, struct value_s *v, linepos_t epoint) {
    if (v1 == v) {
        if (ident_resolv((struct value_s *)v1, v)) return IDENTREF_OBJ->integer(v, v, epoint);
    } else {
        struct value_s tmp;
        if (ident_resolv((struct value_s *)v1, &tmp)) return IDENTREF_OBJ->integer(&tmp, v, epoint);
        tmp.obj->copy_temp(&tmp, v);
    }
}

static int anonident_resolv(struct value_s *v1, struct value_s *v) {
    char idents[100];
    str_t ident;
    struct label_s *l;

    sprintf(idents, (v1->u.anonident.count >= 0) ? "+%x+%x" : "-%x-%x" , reffile, ((v1->u.anonident.count >= 0) ? forwr : backr) + v1->u.anonident.count);
    ident.data = (const uint8_t *)idents;
    ident.len = strlen(idents);
    l = find_label(&ident);
    if (l) {
        v->u.identref.epoint = v1->u.anonident.epoint;
        v->obj = IDENTREF_OBJ;
        v->u.identref.label = l;
        return 1;
    }
    v->u.error.epoint = v1->u.anonident.epoint;
    v->obj = ERROR_OBJ;
    v->u.error.num = ERROR___NOT_DEFINED;
    v->u.error.u.ident.len = 1;
    v->u.error.u.ident.data = (const uint8_t *)((v1->u.anonident.count >= 0) ? "+" : "-");
    return 0;
}

static void anonident_calc1(oper_t op) {
    if (op->v1 == op->v) {
        if (anonident_resolv(op->v1, op->v)) IDENTREF_OBJ->calc1(op);
    } else {
        struct value_s tmp, *old = op->v1; 
        op->v1 = &tmp;
        if (anonident_resolv(old, &tmp)) IDENTREF_OBJ->calc1(op);
        else tmp.obj->copy_temp(&tmp, op->v);
        op->v1 = old;
    }
}

static void anonident_calc2(oper_t op) {
    if (op->v1 == op->v) {
        if (anonident_resolv(op->v1, op->v)) IDENTREF_OBJ->calc2(op);
    } else {
        struct value_s tmp, *old = op->v1; 
        op->v1 = &tmp;
        if (anonident_resolv(old, &tmp)) IDENTREF_OBJ->calc2(op);
        else tmp.obj->copy_temp(&tmp, op->v);
        op->v1 = old;
    }
}

static void anonident_rcalc2(oper_t op) {
    if (op->op == &o_MEMBER) {
        op->v1->obj->calc2(op);return;
    }
    if (op->v2 == op->v) {
        if (anonident_resolv(op->v2, op->v)) IDENTREF_OBJ->rcalc2(op);
    } else {
        struct value_s tmp, *old = op->v2; 
        op->v2 = &tmp;
        if (anonident_resolv(old, &tmp)) IDENTREF_OBJ->rcalc2(op);
        else tmp.obj->copy_temp(&tmp, op->v);
        op->v2 = old;
    }
}

static int anonident_hash(const struct value_s *v1, struct value_s *v, linepos_t epoint) {
    if (v1 == v) {
        if (anonident_resolv((struct value_s *)v1, v)) return IDENTREF_OBJ->hash(v, v, epoint);
    } else {
        struct value_s tmp;
        if (anonident_resolv((struct value_s *)v1, &tmp)) return IDENTREF_OBJ->hash(&tmp, v, epoint);
        tmp.obj->copy_temp(&tmp, v);
    }
    return -1;
}

static void anonident_repr(const struct value_s *v1, struct value_s *v) {
    if (v1 == v) {
        if (anonident_resolv((struct value_s *)v1, v)) return IDENTREF_OBJ->repr(v, v);
    } else {
        struct value_s tmp;
        if (anonident_resolv((struct value_s *)v1, &tmp)) return IDENTREF_OBJ->repr(&tmp, v);
        tmp.obj->copy_temp(&tmp, v);
    }
}

static int MUST_CHECK anonident_ival(const struct value_s *v1, struct value_s *v, ival_t *iv, int bits, linepos_t epoint) {
    if (v1 == v) {
        if (anonident_resolv((struct value_s *)v1, v)) return IDENTREF_OBJ->ival(v, v, iv, bits, epoint);
    } else {
        struct value_s tmp;
        if (anonident_resolv((struct value_s *)v1, &tmp)) return IDENTREF_OBJ->ival(&tmp, v, iv, bits, epoint);
        tmp.obj->copy_temp(&tmp, v);
    }
    return 1;
}

static int MUST_CHECK anonident_uval(const struct value_s *v1, struct value_s *v, uval_t *uv, int bits, linepos_t epoint) {
    if (v1 == v) {
        if (anonident_resolv((struct value_s *)v1, v)) return IDENTREF_OBJ->uval(v, v, uv, bits, epoint);
    } else {
        struct value_s tmp;
        if (anonident_resolv((struct value_s *)v1, &tmp)) return IDENTREF_OBJ->uval(&tmp, v, uv, bits, epoint);
        tmp.obj->copy_temp(&tmp, v);
    }
    return 1;
}

static void anonident_str(const struct value_s *v1, struct value_s *v) {
    if (v1 == v) {
        if (anonident_resolv((struct value_s *)v1, v)) return IDENTREF_OBJ->str(v, v);
    } else {
        struct value_s tmp;
        if (anonident_resolv((struct value_s *)v1, &tmp)) return IDENTREF_OBJ->str(&tmp, v);
        tmp.obj->copy_temp(&tmp, v);
    }
}

static int MUST_CHECK anonident_real(const struct value_s *v1, struct value_s *v, double *r, linepos_t epoint) {
    if (v1 == v) {
        if (anonident_resolv((struct value_s *)v1, v)) return IDENTREF_OBJ->real(v, v, r, epoint);
    } else {
        struct value_s tmp;
        if (anonident_resolv((struct value_s *)v1, &tmp)) return IDENTREF_OBJ->real(&tmp, v, r, epoint);
        tmp.obj->copy_temp(&tmp, v);
    }
    return 1;
}

static void anonident_integer(const struct value_s *v1, struct value_s *v, linepos_t epoint) {
    if (v1 == v) {
        if (anonident_resolv((struct value_s *)v1, v)) return IDENTREF_OBJ->integer(v, v, epoint);
    } else {
        struct value_s tmp;
        if (anonident_resolv((struct value_s *)v1, &tmp)) return IDENTREF_OBJ->integer(&tmp, v, epoint);
        tmp.obj->copy_temp(&tmp, v);
    }
}

static int identref_same(const struct value_s *v1, const struct value_s *v2) {
    return v2->obj == IDENTREF_OBJ && v1->u.identref.label == v2->u.identref.label;
}

static struct value_s *identref_resolv(struct value_s *v1, struct value_s *v) {
    struct value_s *vv = v1;
    struct label_s *l;
    struct linepos_s epoint;
    int rec;

    for (rec = 0; rec < 100; rec++) {
        l = vv->u.identref.label;
        if (referenceit) l->ref = 1;
        l->usepass = pass;
        if (l->type == L_VAR && l->defpass != pass) {
            epoint = v1->u.identref.epoint;
            v->u.error.u.ident = v1->u.identref.label->name;
            v->obj = ERROR_OBJ;
            v->u.error.epoint = epoint;
            v->u.error.num = ERROR___NOT_DEFINED;
            return NULL;
        }
        if (l->value->obj != IDENTREF_OBJ) return vv;
        vv = l->value;
    } 
    err_msg2(ERROR__REFRECURSION, NULL, &v1->u.identref.epoint);
    v->obj = NONE_OBJ;
    return NULL;
}

static struct value_s *identref_access_check(struct label_s *l, struct value_s *v, linepos_t epoint) {
    if (pass != 1) {
        if (l->requires & ~current_section->provides) {
            v->u.error.u.ident = l->name;
            v->obj = ERROR_OBJ;
            v->u.error.epoint = *epoint;
            v->u.error.num = ERROR_REQUIREMENTS_;
            return NULL;
        }
        if (l->conflicts & current_section->provides) {
            v->u.error.u.ident = l->name;
            v->obj = ERROR_OBJ;
            v->u.error.epoint = *epoint;
            v->u.error.num = ERROR______CONFLICT;
            return NULL;
        }
    }
    return l->value;
}

static void identref_calc1(oper_t op) {
    struct value_s *v1;
    v1 = identref_resolv(op->v1, op->v);
    if (!v1) return;
    v1 = identref_access_check(v1->u.identref.label, op->v, &op->epoint);
    if (v1) {
        struct value_s *old = op->v1;
        op->v1 = v1;
        v1->obj->calc1(op);
        op->v1 = old;
    }
}

static int identref_hash(const struct value_s *v1, struct value_s *v, linepos_t epoint) {
    struct value_s *vv;
    vv = identref_resolv((struct value_s *)v1, v);
    if (!vv) return -1;
    vv = identref_access_check(vv->u.identref.label, v, epoint);
    if (vv) return vv->obj->hash(vv, v, epoint);
    return -1;
}

static void identref_repr(const struct value_s *v1, struct value_s *v) {
    struct value_s *vv;
    struct linepos_s epoint = {0,0};
    vv = identref_resolv((struct value_s *)v1, v);
    if (!vv) return;
    vv = identref_access_check(vv->u.identref.label, v, &epoint);
    if (vv) return vv->obj->repr(vv, v);
}

static int MUST_CHECK identref_ival(const struct value_s *v1, struct value_s *v, ival_t *iv, int bits, linepos_t epoint) {
    struct value_s *vv;
    vv = identref_resolv((struct value_s *)v1, v);
    if (!vv) return 1;
    vv = identref_access_check(vv->u.identref.label, v, epoint);
    if (vv) return vv->obj->ival(vv, v, iv, bits, epoint);
    return 1;
}

static int MUST_CHECK identref_uval(const struct value_s *v1, struct value_s *v, uval_t *uv, int bits, linepos_t epoint) {
    struct value_s *vv;
    vv = identref_resolv((struct value_s *)v1, v);
    if (!vv) return 1;
    vv = identref_access_check(vv->u.identref.label, v, epoint);
    if (vv) return vv->obj->uval(vv, v, uv, bits, epoint);
    return 1;
}

static void identref_str(const struct value_s *v1, struct value_s *v) {
    struct value_s *vv;
    struct linepos_s epoint = {0,0};
    vv = identref_resolv((struct value_s *)v1, v);
    if (!vv) return;
    vv = identref_access_check(vv->u.identref.label, v, &epoint);
    if (vv) return vv->obj->str(vv, v);
}

static int MUST_CHECK identref_real(const struct value_s *v1, struct value_s *v, double *r, linepos_t epoint) {
    struct value_s *vv;
    vv = identref_resolv((struct value_s *)v1, v);
    if (!vv) return 1;
    vv = identref_access_check(vv->u.identref.label, v, epoint);
    if (vv) return vv->obj->real(vv, v, r, epoint);
    return 1;
}

static void identref_integer(const struct value_s *v1, struct value_s *v, linepos_t epoint) {
    struct value_s *vv;
    vv = identref_resolv((struct value_s *)v1, v);
    if (!vv) return;
    vv = identref_access_check(vv->u.identref.label, v, epoint);
    if (vv) return vv->obj->integer(vv, v, epoint);
}

static void identref_calc2(oper_t op) {
    struct label_s *l;
    struct value_s *v1, *v = op->v, *v2 = op->v2;
    struct linepos_s epoint;

    v1 = identref_resolv(op->v1, v);
    if (!v1) return;

    if (op->op == &o_MEMBER) {
        if (v1->u.identref.label->value->obj == NONE_OBJ) {
            v->obj = NONE_OBJ;
            return;
        }
        switch (v2->obj->type) {
        case T_IDENT:
            l = find_label2(&v2->u.ident.name, v1->u.identref.label);
            if (l) {
                v->u.identref.epoint = v2->u.ident.epoint;
                v->obj = IDENTREF_OBJ;
                v->u.identref.label = l;
                return;
            } 
            epoint = v2->u.ident.epoint;
            v->u.error.u.ident = v2->u.ident.name;
            v->obj = ERROR_OBJ;
            v->u.error.num = ERROR___NOT_DEFINED;
            v->u.error.epoint = epoint;
            return;
        case T_ANONIDENT:
            {
                char idents[100];
                str_t ident;
                sprintf(idents, (v2->u.anonident.count >= 0) ? "+%x+%x" : "-%x-%x" , reffile, ((v2->u.anonident.count >= 0) ? forwr : backr) + v2->u.anonident.count);
                ident.data = (const uint8_t *)idents;
                ident.len = strlen(idents);
                l = find_label2(&ident, v1->u.identref.label);
                if (l) {
                    v->u.identref.epoint = v2->u.anonident.epoint;
                    v->obj = IDENTREF_OBJ;
                    v->u.identref.label = l;
                    return;
                }
                v->u.error.epoint = v2->u.anonident.epoint;
                v->obj = ERROR_OBJ;
                v->u.error.num = ERROR___NOT_DEFINED;
                v->u.error.u.ident.len = 1;
                v->u.error.u.ident.data = (const uint8_t *)((v2->u.anonident.count >= 0) ? "+" : "-");
                return;
            }
        case T_TUPLE:
        case T_LIST: v2->obj->rcalc2(op); return;
        default: break;
        }
    }
    v1 = identref_access_check(v1->u.identref.label, v, &epoint);
    if (v1) {
        struct value_s *old = op->v1;
        op->v1 = v1;
        v1->obj->calc2(op);
        op->v1 = old;
    }
}

static void identref_rcalc2(oper_t op) {
    struct value_s *v2;
    v2 = identref_resolv(op->v2, op->v);
    if (!v2) return;
    v2 = identref_access_check(op->v2->u.identref.label, op->v, &op->epoint2);
    if (v2) {
        struct value_s *old = op->v2;
        op->v2 = v2;
        v2->obj->rcalc2(op);
        op->v2 = old;
    }
}

static int none_hash(const struct value_s *v1, struct value_s *v, linepos_t UNUSED(epoint)) {
    if (v == v1) obj_destroy(v);
    v->obj = NONE_OBJ;
    return -1;
}

static void none_calc1(oper_t op) {
    op->v->obj = NONE_OBJ;
}

static void none_calc2(oper_t op) {
    if (op->v2->obj == ERROR_OBJ) {
        ERROR_OBJ->rcalc2(op);
        return;
    }
    op->v->obj = NONE_OBJ;
}

static void none_rcalc2(oper_t op) {
    if (op->v1->obj == ERROR_OBJ) {
        ERROR_OBJ->calc2(op);
        return;
    }
    op->v->obj = NONE_OBJ;
}

static int MUST_CHECK none_ival(const struct value_s *v1, struct value_s *v, ival_t *iv, int bits, linepos_t epoint) {
    if (v1->obj == ERROR_OBJ) {
        return ERROR_OBJ->ival(v1, v, iv, bits, epoint);
    }
    *iv = 0;
    return 0;
}

static int MUST_CHECK none_uval(const struct value_s *v1, struct value_s *v, uval_t *uv, int bits, linepos_t epoint) {
    if (v1->obj == ERROR_OBJ) {
        return ERROR_OBJ->uval(v1, v, uv, bits, epoint);
    }
    *uv = 0;
    return 0;
}

static int MUST_CHECK none_real(const struct value_s *v1, struct value_s *v, double *r, linepos_t epoint) {
    if (v1->obj == ERROR_OBJ) {
        return ERROR_OBJ->real(v1, v, r, epoint);
    }
    *r = 0.0;
    return 0;
}

static int MUST_CHECK none_sign(const struct value_s *v1, struct value_s *v, int *s, linepos_t epoint) {
    if (v1->obj == ERROR_OBJ) {
        return ERROR_OBJ->sign(v1, v, s, epoint);
    }
    *s = 0;
    return 0;
}

static void none_abs(const struct value_s *v1, struct value_s *v, linepos_t epoint) {
    if (v1->obj == ERROR_OBJ) {
        return ERROR_OBJ->abs(v1, v, epoint);
    }
    v->obj = NONE_OBJ;
}

static void none_integer(const struct value_s *v1, struct value_s *v, linepos_t epoint) {
    if (v1->obj == ERROR_OBJ) {
        return ERROR_OBJ->abs(v1, v, epoint);
    }
    v->obj = NONE_OBJ;
}

static int MUST_CHECK none_len(const struct value_s *v1, struct value_s *v, uval_t *len, linepos_t epoint) {
    if (v1->obj == ERROR_OBJ) {
        return ERROR_OBJ->len(v1, v, len, epoint);
    }
    *len = 0;
    return 0;
}

static int lbl_same(const struct value_s *v1, const struct value_s *v2) {
    return v2->obj == LBL_OBJ && v1->u.lbl.p == v2->u.lbl.p && v1->u.lbl.sline == v2->u.lbl.sline && v1->u.lbl.waitforp == v2->u.lbl.waitforp && v1->u.lbl.file_list == v2->u.lbl.file_list && v1->u.lbl.parent == v2->u.lbl.parent;
}

void obj_init(struct obj_s *obj, enum type_e type, const char *name) {
    obj->type = type;
    obj->name = name;
    obj->destroy = invalid_destroy;
    obj->copy = invalid_copy;
    obj->copy_temp = invalid_copy;
    obj->same = invalid_same;
    obj->truth = invalid_truth;
    obj->hash = invalid_hash;
    obj->repr = invalid_repr;
    obj->str = invalid_str;
    obj->calc1 = invalid_calc1;
    obj->calc2 = invalid_calc2;
    obj->rcalc2 = invalid_rcalc2;
    obj->repeat = invalid_repeat;
    obj->iindex = invalid_iindex;
    obj->slice = invalid_slice;
    obj->ival = invalid_ival;
    obj->uval = invalid_uval;
    obj->real = invalid_real;
    obj->sign = invalid_sign;
    obj->abs = invalid_abs;
    obj->integer = invalid_integer;
    obj->len = invalid_len;
};

void objects_init(void) {
    boolobj_init();
    floatobj_init();
    addressobj_init();
    codeobj_init();
    strobj_init();
    listobj_init();
    bytesobj_init();
    bitsobj_init();
    intobj_init();

    obj_init(&macro_obj, T_MACRO, "<macro>");
    macro_obj.destroy = macro_destroy;
    macro_obj.copy = macro_copy;
    macro_obj.copy_temp = macro_copy_temp;
    macro_obj.same = macro_same;
    obj_init(&segment_obj, T_SEGMENT, "<segment>");
    segment_obj.destroy = macro_destroy;
    segment_obj.copy = macro_copy;
    segment_obj.copy_temp = macro_copy_temp;
    segment_obj.same = macro_same;
    obj_init(&lbl_obj, T_LBL, "<lbl>");
    lbl_obj.same = lbl_same;
    obj_init(&function_obj, T_FUNCTION, "<function>");
    function_obj.destroy = function_destroy;
    function_obj.copy = function_copy;
    function_obj.copy_temp = function_copy_temp;
    function_obj.same = function_same;
    obj_init(&struct_obj, T_STRUCT, "<struct>");
    struct_obj.destroy = macro_destroy;
    struct_obj.copy = macro_copy;
    struct_obj.copy_temp = macro_copy_temp;
    struct_obj.same = macro_same;
    obj_init(&union_obj, T_UNION, "<union>");
    union_obj.destroy = macro_destroy;
    union_obj.copy = macro_copy;
    union_obj.copy_temp = macro_copy_temp;
    union_obj.same = macro_same;
    obj_init(&identref_obj, T_IDENTREF, "<identref>");
    identref_obj.same = identref_same;
    identref_obj.hash = identref_hash;
    identref_obj.repr = identref_repr;
    identref_obj.ival = identref_ival;
    identref_obj.uval = identref_uval;
    identref_obj.str = identref_str;
    identref_obj.real = identref_real;
    identref_obj.integer = identref_integer;
    identref_obj.calc1 = identref_calc1;
    identref_obj.calc2 = identref_calc2;
    identref_obj.rcalc2 = identref_rcalc2;
    obj_init(&none_obj, T_NONE, "<none>");
    none_obj.hash = none_hash;
    none_obj.calc1 = none_calc1;
    none_obj.calc2 = none_calc2;
    none_obj.rcalc2 = none_rcalc2;
    none_obj.ival = none_ival;
    none_obj.uval = none_uval;
    none_obj.real = none_real;
    none_obj.sign = none_sign;
    none_obj.abs = none_abs;
    none_obj.integer = none_integer;
    none_obj.len = none_len;
    obj_init(&error_obj, T_ERROR, "<error>");
    error_obj.destroy = error_destroy;
    error_obj.copy = error_copy;
    error_obj.copy_temp = error_copy_temp;
    error_obj.calc1 = error_calc1;
    error_obj.calc2 = error_calc2;
    error_obj.rcalc2 = error_rcalc2;
    error_obj.ival = error_ival;
    error_obj.uval = error_uval;
    error_obj.real = error_real;
    error_obj.sign = error_sign;
    error_obj.abs = error_abs;
    error_obj.integer = error_integer;
    error_obj.len = error_len;
    obj_init(&gap_obj, T_GAP, "<gap>");
    gap_obj.hash = gap_hash;
    gap_obj.repr = gap_repr;
    obj_init(&ident_obj, T_IDENT, "<ident>");
    ident_obj.hash = ident_hash;
    ident_obj.repr = ident_repr;
    ident_obj.str = ident_str;
    ident_obj.ival = ident_ival;
    ident_obj.uval = ident_uval;
    ident_obj.real = ident_real;
    ident_obj.integer = ident_integer;
    ident_obj.calc1 = ident_calc1;
    ident_obj.calc2 = ident_calc2;
    ident_obj.rcalc2 = ident_rcalc2;
    obj_init(&anonident_obj, T_ANONIDENT, "<anonident>");
    anonident_obj.hash = anonident_hash;
    anonident_obj.repr = anonident_repr;
    anonident_obj.str = anonident_str;
    anonident_obj.ival = anonident_ival;
    anonident_obj.uval = anonident_uval;
    anonident_obj.real = anonident_real;
    anonident_obj.integer = anonident_integer;
    anonident_obj.calc1 = anonident_calc1;
    anonident_obj.calc2 = anonident_calc2;
    anonident_obj.rcalc2 = anonident_rcalc2;
    obj_init(&oper_obj, T_OPER, "<oper>");
    obj_init(&default_obj, T_DEFAULT, "<default>");
    obj_init(&dict_obj, T_DICT, "<dict>");
    dict_obj.destroy = dict_destroy;
    dict_obj.copy = dict_copy;
    dict_obj.copy_temp = dict_copy_temp;
    dict_obj.same = dict_same;
    dict_obj.truth = dict_truth;
    dict_obj.len = dict_len;
    dict_obj.repr = dict_repr;
    dict_obj.rcalc2 = dict_rcalc2;
    obj_init(&pair_obj, T_PAIR, "<pair>");
    pair_obj.destroy = pair_destroy;
    pair_obj.copy = pair_copy;
    pair_obj.copy_temp = pair_copy_temp;
    pair_obj.same = pair_same;
    pair_obj.repr = pair_repr;
};
