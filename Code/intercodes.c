#include "intercodes.h"
#include "intercode.h"
#include "semantics.h"
#include "syntax.tab.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

/* ------------------------------------ *
 *             intercodes               *
 * ------------------------------------ */

static iclist_t intercodes;

void init_intercodes()
{
    init_iclist(&intercodes);
}

void intercodes_push_back(intercode_t *ic)
{
    iclist_push_back(&intercodes, ic);
}

void fprint_intercodes(FILE *fp)
{
    fprint_iclist(fp, &intercodes);
}

/* ------------------------------------ *
 *           translate errors           *
 * ------------------------------------ */

void translate_error(int lineno, const char *msg, ...)
{
    printf("Line %d: ", lineno);
    va_list ap;
    va_start(ap, msg);
    vprintf(msg, ap);
    va_end(ap);
    printf("\n");
}

/* ------------------------------------ *
 *              translate               *
 * ------------------------------------ */

void intercodes_translate_r(treenode_t *node);
void translate_ext_def(treenode_t *ext_def);
void translate_ext_dec_list(treenode_t *ext_dec_list, type_t *spec);

/* Translate statements */
void gen_funcdef(const char *fname, fieldlist_t *params);
void translate_comp_st(treenode_t *comp_st);
void translate_def_list(treenode_t *def_list);
void translate_stmt_list(treenode_t *stmt_list);
void translate_stmt(treenode_t *stmt);
void translate_stmt_if(treenode_t *exp, treenode_t *stmt);
void translate_stmt_if_else(treenode_t *exp, treenode_t *stmt1, treenode_t *stmt2);
void translate_stmt_while(treenode_t *exp, treenode_t *stmt);

/* Translate an expression and return the result in operand_t. */
operand_t translate_exp(treenode_t *exp);
operand_t translate_literal(treenode_t *literal);
operand_t translate_var(treenode_t *id);
operand_t translate_func_call(treenode_t *id, treenode_t *args);
operand_t translate_assign(treenode_t *lexp, treenode_t *rexp);
operand_t translate_unary_minus(treenode_t *exp);
operand_t translate_arithbop(treenode_t *lexp, treenode_t *rexp, int icop);
operand_t translate_boolexp(treenode_t *exp);

void translate_args(treenode_t *args);
operand_t get_first_arg(treenode_t *args);

/* Translate an expression as a condition. */
void translate_cond(treenode_t *exp, int labeltrue, int labelfalse);
void translate_cond_not(treenode_t *exp, int labeltrue, int labelfalse);
void translate_cond_relop(treenode_t *lexp, treenode_t *rexp,
                          int labeltrue, int labelfalse, int icop);
void translate_cond_and(treenode_t *lexp, treenode_t *rexp,
                        int labeltrue, int labelfalse);
void translate_cond_or(treenode_t *lexp, treenode_t *rexp,
                       int labeltrue, int labelfalse);
void translate_cond_otherwise(treenode_t *exp, int labeltrue, int labelfalse);

void intercodes_translate(treenode_t *root)
{
    init_structdef_table();
    init_symbol_table();
    init_intercodes();
    add_builtin_func();

    intercodes_translate_r(root);

    fprint_intercodes(stdout);
}

void intercodes_translate_r(treenode_t *node)
{
    if (!node)
        return;

    if (!strcmp(node->name, "ExtDef")) {
        translate_ext_def(node);
        return;
    }

    for (treenode_t *child = node->child; child != NULL; child = child->next)
        intercodes_translate_r(child);
}

void translate_ext_def(treenode_t *ext_def)
{
    assert(ext_def);
    assert(!strcmp(ext_def->name, "ExtDef"));
    treenode_t *specifer = ext_def->child;
    assert(specifer);
    type_t *spec = analyse_specifier(specifer);
    if (!spec)
        return;

    treenode_t *child2 = specifer->next;
    assert(child2);
    if (!strcmp(child2->name, "ExtDecList")) {
        translate_ext_dec_list(child2, spec);
        return;
    }
    if (!strcmp(child2->name, "FunDec")) {
        symbol_t func;
        fieldlist_t paramlist;
        init_fieldlist(&paramlist);
        analyse_fun_dec(child2, spec, &func, &paramlist);
        assert(child2->next);
        int is_def = !strcmp(child2->next->name, "SEMI") ? 0 : 1;

        if (checked_symbol_table_add_func(&func, is_def) != 0)
            return;
        if (is_def) {
            symbol_table_pushenv();
            symbol_table_add_from_fieldlist(&paramlist);
            gen_funcdef(func.name, &paramlist);
            translate_comp_st(child2->next);
            symbol_table_popenv();
        }
        return;
    }
    assert(!strcmp(child2->name, "SEMI"));
}

void translate_ext_dec_list(treenode_t *ext_dec_list, type_t *spec)
{
    translate_error(ext_dec_list->lineno, "Assumption 4 is violated. "
                                          "Global variables are not allowed.");
}

void gen_funcdef(const char *fname, fieldlist_t *params)
{
    intercodes_push_back(create_ic_funcdef(fname));
    
    symbol_t *symbol;
    for (fieldlistnode_t *param = params->front; param != NULL; param = param->next) {
        if (symbol_table_find_by_name(param->fieldname, &symbol) != 0) {
            assert(0);
            return;
        }
        intercodes_push_back(create_ic_param(symbol->id));
    }
}

void translate_comp_st(treenode_t *comp_st)
{
    assert(comp_st);
    assert(!strcmp(comp_st->name, "CompSt"));
    assert(comp_st->child);

    treenode_t *child2 = comp_st->child->next;
    assert(child2);

    if (!strcmp(child2->name, "DefList")) {
        translate_def_list(child2);
        treenode_t *child3 = child2->next;
        if (!strcmp(child3->name, "StmtList"))
            translate_stmt_list(child3);
        else
            assert(!strcmp(child3->name, "RC"));
    }
    else if (!strcmp(child2->name, "StmtList")) {
        translate_stmt_list(child2);
    }
    else {
        assert(!strcmp(child2->name, "RC"));
    }
}

void translate_def_list(treenode_t *def_list)
{
    // TODO: gen code for array and struct
    // TODO: gen code for init assign
    analyse_def_list(def_list, NULL, CONTEXT_VAR_DEF);
}

void translate_stmt_list(treenode_t *stmt_list)
{
    assert(stmt_list);
    assert(!strcmp(stmt_list->name, "StmtList"));
    treenode_t *stmt = stmt_list->child;
    assert(stmt);

    translate_stmt(stmt);
    if (stmt->next)
        translate_stmt_list(stmt->next);
}

void translate_stmt(treenode_t *stmt)
{
    assert(stmt);
    assert(!strcmp(stmt->name, "Stmt"));
    treenode_t *child = stmt->child;
    assert(child);

    if (!strcmp(child->name, "Exp")) {
        translate_exp(child);
    }
    else if (!strcmp(child->name, "CompSt")) {
        translate_comp_st(child);
    }
    else if (!strcmp(child->name, "RETURN")) {
        assert(child->next);
        operand_t ret = translate_exp(child->next);
        intercodes_push_back(create_ic_return(&ret));
    }
    else if (!strcmp(child->name, "IF")) {
        treenode_t *exp = child->next->next;
        treenode_t *stmt = exp->next->next;
        if (stmt->next)
            translate_stmt_if_else(exp, stmt, stmt->next->next);
        else
            translate_stmt_if(exp, stmt);
    }
    else {
        assert(!strcmp(child->name, "WHILE"));
        treenode_t *exp = child->next->next;
        treenode_t *stmt = exp->next->next;
        translate_stmt_while(exp, stmt);
    }
}

void translate_stmt_if(treenode_t *exp, treenode_t *stmt)
{
    int labeltrue = alloc_labelid();
    int labelfalse = alloc_labelid();
    translate_cond(exp, labeltrue, labelfalse);
    intercodes_push_back(create_ic_label(labeltrue));
    translate_stmt(stmt);
    intercodes_push_back(create_ic_label(labelfalse));
}

void translate_stmt_if_else(treenode_t *exp, treenode_t *stmt1, treenode_t *stmt2)
{
    int labeltrue = alloc_labelid();
    int labelfalse = alloc_labelid();
    int labelexit = alloc_labelid();

    translate_cond(exp, labeltrue, labelfalse);
    intercodes_push_back(create_ic_label(labeltrue));
    translate_stmt(stmt1);
    intercodes_push_back(create_ic_goto(labelexit));
    intercodes_push_back(create_ic_label(labelfalse));
    translate_stmt(stmt2);
    intercodes_push_back(create_ic_label(labelexit));
}

void translate_stmt_while(treenode_t *exp, treenode_t *stmt)
{
    int labelbegin = alloc_labelid();
    int labeltrue = alloc_labelid();
    int labelfalse = alloc_labelid();

    intercodes_push_back(create_ic_label(labelbegin));
    translate_cond(exp, labeltrue, labelfalse);
    intercodes_push_back(create_ic_label(labeltrue));
    translate_stmt(stmt);
    intercodes_push_back(create_ic_goto(labelbegin));
    intercodes_push_back(create_ic_label(labelfalse));
}

operand_t translate_exp(treenode_t *exp)
{
    assert(exp);
    assert(!strcmp(exp->name, "Exp"));
    treenode_t *child = exp->child;
    assert(child);

    if (!strcmp(child->name, "INT") || !strcmp(child->name, "FLOAT"))
        return translate_literal(child);
    if (!strcmp(child->name, "ID")) {
        if (!child->next) {
            return translate_var(child);
        }
        assert(!strcmp(child->next->name, "LP"));
        treenode_t *child3 = child->next->next;
        assert(child3);
        if (!strcmp(child3->name, "Args"))
            return translate_func_call(child, child3);
        assert(!strcmp(child3->name, "RP"));
        return translate_func_call(child, NULL);
    }
    if (!strcmp(child->name, "LP"))
        return translate_exp(child->next);
    if (!strcmp(child->name, "MINUS"))
        return translate_unary_minus(child->next);
    if (!strcmp(child->name, "NOT"))
        return translate_boolexp(exp);

    assert(!strcmp(child->name, "Exp"));
    treenode_t *child2 = child->next;
    assert(child2);
    treenode_t *child3 = child2->next;
    assert(child3);
    if (!strcmp(child2->name, "PLUS"))
        return translate_arithbop(child, child3, ICOP_ADD);
    if (!strcmp(child2->name, "MINUS"))
        return translate_arithbop(child, child3, ICOP_SUB);
    if (!strcmp(child2->name, "STAR"))
        return translate_arithbop(child, child3, ICOP_MUL);
    if (!strcmp(child2->name, "DIV"))
        return translate_arithbop(child, child3, ICOP_DIV);
    if (!strcmp(child2->name, "ASSIGNOP"))
        return translate_assign(child, child3);
    if (!strcmp(child2->name, "RELOP"))
        return translate_boolexp(exp);
    if (!strcmp(child2->name, "AND") || !strcmp(child2->name, "OR"))
        return translate_boolexp(exp);
    if (!strcmp(child2->name, "DOT"))
        assert(0); // TODO:
        // return translate_struct_access(child, child2, child3);
    if (!strcmp(child2->name, "LB"))
        assert(0); // TODO:
        // return translate_array_access(child, child3);
    assert(0);  /* Should not reach here! */
}

operand_t translate_literal(treenode_t *literal)
{
    assert(literal);
    operand_t op;

    if (literal->token != INT) {
        translate_error(literal->lineno, "Assumption 1 is violated. "
                                         "Floats are not allowed.");
        init_const_operand(&op, 0);
        return op;
    }

    init_const_operand(&op, literal->ival);
    return op;
}

operand_t translate_var(treenode_t *id)
{
    assert(id);
    assert(id->token == ID);
    symbol_t *symbol;
    operand_t op;

    if (symbol_table_find_by_name(id->id, &symbol) != 0) {
        assert(0);
        init_const_operand(&op, 0);
        return op;
    }

    init_var_operand(&op, symbol->id);
    return op;
}

operand_t translate_func_call(treenode_t *id, treenode_t *args)
{
    assert(id);
    assert(id->id);
    operand_t ret;
    
    if (!strcmp(id->id, "read")) {
        assert(!args);
        init_temp_var(&ret);
        intercodes_push_back(create_ic_read(ret.varid));
        return ret;
    }

    if (!strcmp(id->id, "write")) {
        assert(args);
        init_const_operand(&ret, 0);
        operand_t arg = get_first_arg(args);
        intercodes_push_back(create_ic_write(&arg));
        return ret;
    }
    
    if (args)
        translate_args(args);
    init_temp_var(&ret);
    intercodes_push_back(create_ic_call(id->id, ret.varid));
    return ret;
}

void translate_args(treenode_t *args)
{
    assert(args);
    assert(!strcmp(args->name, "Args"));
    treenode_t *arg = args->child;
    assert(arg);

    operand_t result = translate_exp(arg);
    if (arg->next)
        translate_args(arg->next->next);
    intercodes_push_back(create_ic_arg(&result));
}

operand_t get_first_arg(treenode_t *args)
{
    assert(args);
    assert(!strcmp(args->name, "Args"));
    return translate_exp(args->child);
}

operand_t translate_assign(treenode_t *lexp, treenode_t *rexp)
{
    assert(lexp);
    assert(rexp);

    operand_t lhs = translate_exp(lexp);
    operand_t rhs = translate_exp(rexp);

    assert(!is_const_operand(&lhs));
    intercodes_push_back(create_ic_assign(&lhs, &rhs));
    return lhs;
}

operand_t translate_unary_minus(treenode_t *exp)
{
    assert(exp);

    operand_t target, zero;
    operand_t subexp = translate_exp(exp);

    if (is_const_operand(&subexp)) {
        init_const_operand(&target, -subexp.val);
        return target;
    }
        
    init_temp_var(&target);
    init_const_operand(&zero, 0);
    intercodes_push_back(create_ic_arithbop(ICOP_SUB, &target, &zero, &subexp));
    return target;
}

operand_t translate_arithbop(treenode_t *lexp, treenode_t *rexp, int icop)
{
    assert(lexp);
    assert(rexp);
    
    operand_t lhs = translate_exp(lexp);
    operand_t rhs = translate_exp(rexp);
    operand_t target;

    if (is_const_operand(&lhs) && is_const_operand(&rhs)) {
        int target_val;
        switch (icop) {
        case ICOP_ADD: target_val = lhs.val + rhs.val; break;
        case ICOP_SUB: target_val = lhs.val - rhs.val; break;
        case ICOP_MUL: target_val = lhs.val * rhs.val; break;
        case ICOP_DIV: target_val = lhs.val / rhs.val; break;
        default: assert(0); break;
        }
        init_const_operand(&target, target_val);
        return target;
    }

    init_temp_var(&target);
    intercodes_push_back(create_ic_arithbop(icop, &target, &lhs, &rhs));
    return target;
}

operand_t translate_boolexp(treenode_t *exp)
{
    int labeltrue = alloc_labelid();
    int labelfalse = alloc_labelid();
    operand_t zero, one, op;
    init_const_operand(&zero, 0);
    init_const_operand(&one, 1);
    init_temp_var(&op);

    intercodes_push_back(create_ic_assign(&op, &zero));
    translate_cond(exp, labeltrue, labelfalse);
    intercodes_push_back(create_ic_label(labeltrue));
    intercodes_push_back(create_ic_assign(&op, &one));
    intercodes_push_back(create_ic_label(labelfalse));
    return op;
}

void translate_cond(treenode_t *exp, int labeltrue, int labelfalse)
{
    assert(exp);
    assert(!strcmp(exp->name, "Exp"));
    treenode_t *child = exp->child;
    assert(child);

    if (!strcmp(child->name, "LP")) {
        translate_cond(child->next, labeltrue, labelfalse);
        return;
    }
    if (!strcmp(child->name, "NOT")) {
        translate_cond_not(exp->next, labeltrue, labelfalse);
        return;
    }

    treenode_t *child2, *child3;
    if ((child2 = child->next) && (child3 = child2->next)) {
        if (!strcmp(child2->name, "RELOP")) {
            translate_cond_relop(child, child3, labeltrue, labelfalse,
                                 str_to_icop(child2->relop));
            return;
        }   
        if (!strcmp(child2->name, "AND")) {
            translate_cond_and(child, child3, labeltrue, labelfalse);
            return;
        }
        if (!strcmp(child2->name, "OR")) {
            translate_cond_or(child, child3, labeltrue, labelfalse);
            return;
        }
    }
    translate_cond_otherwise(exp, labeltrue, labelfalse);
}

void translate_cond_not(treenode_t *exp, int labeltrue, int labelfalse)
{
    translate_cond(exp, labelfalse, labeltrue);
}

void translate_cond_relop(treenode_t *lexp, treenode_t *rexp,
                          int labeltrue, int labelfalse, int icop)
{
    operand_t lhs = translate_exp(lexp);
    operand_t rhs = translate_exp(rexp);

    if (is_const_operand(&lhs) && is_const_operand(&rhs)) {
        int cond;
        switch (icop) {
        case ICOP_EQ:  cond = (lhs.val == rhs.val); break;
        case ICOP_NEQ: cond = (lhs.val != rhs.val); break;
        case ICOP_L:   cond = (lhs.val < rhs.val);  break;
        case ICOP_LE:  cond = (lhs.val <= rhs.val); break;
        case ICOP_G:   cond = (lhs.val > rhs.val);  break;
        case ICOP_GE:  cond = (lhs.val >= rhs.val); break;
        default: assert(0); break;
        }
        intercodes_push_back(create_ic_goto((cond ? labeltrue : labelfalse)));
        return;
    }

    intercodes_push_back(create_ic_condgoto(icop, &lhs, &rhs, labeltrue));
    intercodes_push_back(create_ic_goto(labelfalse));
}

void translate_cond_and(treenode_t *lexp, treenode_t *rexp,
                        int labeltrue, int labelfalse)
{
    int labelid = alloc_labelid();
    translate_cond(lexp, labelid, labelfalse);
    intercodes_push_back(create_ic_label(labelid));
    translate_cond(rexp, labeltrue, labelfalse);
}

void translate_cond_or(treenode_t *lexp, treenode_t *rexp,
                       int labeltrue, int labelfalse)
{
    int labelid = alloc_labelid();
    translate_cond(lexp, labeltrue, labelid);
    intercodes_push_back(create_ic_label(labelid));
    translate_cond(rexp, labeltrue, labelfalse);
}

void translate_cond_otherwise(treenode_t *exp, int labeltrue, int labelfalse)
{
    operand_t op = translate_exp(exp);

    if (is_const_operand(&op)) {
        intercodes_push_back(create_ic_goto((op.val ? labeltrue : labelfalse)));
        return;
    }

    operand_t zero;
    init_const_operand(&zero, 0);
    intercodes_push_back(create_ic_condgoto(ICOP_NEQ, &op, &zero, labeltrue));
    intercodes_push_back(create_ic_goto(labelfalse));
}