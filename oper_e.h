/*
    $Id: oper_e.h 1943 2019-08-31 09:05:47Z soci $

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
#ifndef OPER_TYPES_E_H
#define OPER_TYPES_E_H

typedef enum Oper_types {
    O_FUNC,          /* a(    */
    O_INDEX,         /* a[    */
    O_BRACE,         /* {a}   */
    O_BRACKET,       /* [a]   */
    O_PARENT,        /* (a)   */
    O_COMMA,         /* ,     */
    O_ASSIGN,        /* =     */
    O_COLON_ASSIGN,  /* :=    */
    O_MIN_ASSIGN,    /* <?=   */
    O_MAX_ASSIGN,    /* >?=   */
    O_OR_ASSIGN,     /* |=    */
    O_XOR_ASSIGN,    /* ^=    */
    O_AND_ASSIGN,    /* &=    */
    O_BLS_ASSIGN,    /* <<=   */
    O_BRS_ASSIGN,    /* >>=   */
    O_ADD_ASSIGN,    /* +=    */
    O_SUB_ASSIGN,    /* -=    */
    O_MUL_ASSIGN,    /* *=    */
    O_DIV_ASSIGN,    /* /=    */
    O_MOD_ASSIGN,    /* %=    */
    O_EXP_ASSIGN,    /* **=   */
    O_CONCAT_ASSIGN, /* ..=   */
    O_X_ASSIGN,      /* x=    */
    O_MEMBER_ASSIGN, /* .=    */
    O_LOR_ASSIGN,    /* ||=   */
    O_LAND_ASSIGN,   /* &&=   */
    O_COND_ASSIGN,   /* :?=   */
    O_COND,          /* ?     */
    O_COLON2,        /* :     */
    O_LOR,           /* ||    */
    O_LXOR,          /* ^^    */
    O_LAND,          /* &&    */

    O_HASH,          /* #     */ /*                      */
    O_HASH_SIGNED,   /* #+    */ /* NOTE:                */
    O_COMMAX,        /* ,x    */ /* there's a check for  */
    O_COMMAY,        /* ,y    */ /* O_COMMAX to O_COMMAK */
    O_COMMAZ,        /* ,z    */ /* in eval.c            */
    O_COMMAR,        /* ,r    */
    O_COMMAS,        /* ,s    */
    O_COMMAD,        /* ,d    */
    O_COMMAB,        /* ,b    */
    O_COMMAK,        /* ,k    */
    O_WORD,          /* <>    */
    O_HWORD,         /* >`    */
    O_BSWORD,        /* ><    */
    O_LOWER,         /* <     */
    O_HIGHER,        /* >     */
    O_BANK,          /* `     */
    O_STRING,        /* ^     */
    O_SPLAT,         /* *     */
    O_NEG,           /* -     */
    O_POS,           /* +     */
    O_INV,           /* ~     */
    O_LNOT,          /* !     */

    O_CMP,           /* <=>   */
    O_EQ,            /* =     */
    O_NE,            /* !=    */
    O_LT,            /* <     */
    O_GT,            /* >     */
    O_GE,            /* >=    */
    O_LE,            /* <=    */
    O_MIN,           /* <?    */
    O_MAX,           /* >?    */
    O_OR,            /* |     */
    O_XOR,           /* ^     */
    O_AND,           /* &     */
    O_LSHIFT,        /* <<    */
    O_RSHIFT,        /* >>    */
    O_ADD,           /* +     */
    O_SUB,           /* -     */
    O_MUL,           /* *     */
    O_DIV,           /* /     */
    O_MOD,           /* %     */
    O_EXP,           /* **    */
    O_MEMBER,        /* .     */
    O_CONCAT,        /* ..    */

    O_X,             /* x     */
    O_IN,            /* in    */

    O_TUPLE,         /* )     */
    O_LIST,          /* ]     */
    O_DICT,          /* }     */
    O_RPARENT,       /* )     */
    O_RBRACKET,      /* ]     */
    O_RBRACE,        /* }     */
    O_QUEST,         /* ?     */
    O_COLON          /* :     */
} Oper_types;

#endif
