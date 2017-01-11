#include "acirc.h"
#include "utils.h"
#include "commands/outputs.h"
#include "commands/test.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int yyparse(acirc *);
extern FILE *yyin;

size_t acirc_nrefs(const acirc *c)
{
    return c->ninputs + c->gates.n + c->consts.n;
}

static void acirc_init_extgates(acirc_extgates_t *gates)
{
    gates->n = 0;
    gates->gates = NULL;
}

static void acirc_clear_extgates(acirc_extgates_t *gates)
{
    if (gates->gates)
        free(gates->gates);
}

static void acirc_init_commands(acirc_commands_t *cmds)
{
    cmds->n = 2;
    cmds->commands = acirc_calloc(cmds->n, sizeof(acirc_command_t));
    cmds->commands[0] = command_test;
    cmds->commands[1] = command_outputs;
}

void acirc_verbose(uint32_t verbose)
{
    g_verbose = verbose;
}

static void acirc_init_gates(acirc_gates_t *g)
{
    g->_alloc = 2;
    g->gates = acirc_calloc(g->_alloc, sizeof(acirc_gate_t));
    g->n = 0;
}

static void acirc_clear_gates(acirc_gates_t *g)
{
    for (size_t i = 0; i < g->n; ++i) {
        free(g->gates[i].args);
        if (g->gates[i].external)
            free(g->gates[i].external);
    }
    free(g->gates);
}

static void acirc_init_tests(acirc_tests_t *t)
{
    t->_alloc = 2;
    t->inps = acirc_calloc(t->_alloc, sizeof(int *));
    t->outs = acirc_calloc(t->_alloc, sizeof(int *));
    t->n = 0;
}

static void acirc_clear_tests(acirc_tests_t *t)
{
    for (size_t i = 0; i < t->n; ++i) {
        free(t->inps[i]);
        free(t->outs[i]);
    }
    free(t->inps);
    free(t->outs);
}

static void acirc_init_outputs(acirc_outputs_t *o)
{
    o->_alloc = 2;
    o->buf = acirc_calloc(o->_alloc, sizeof(acircref));
    o->n = 0;
}

static void acirc_clear_outputs(acirc_outputs_t *o)
{
    free(o->buf);
}

static void acirc_init_consts(acirc_consts_t *c)
{
    c->_alloc = 2;
    c->buf = acirc_calloc(c->_alloc, sizeof(int));
    c->n = 0;
}

static void acirc_clear_consts(acirc_consts_t *c)
{
    free(c->buf);
}

void acirc_init(acirc *c)
{
    c->ninputs  = 0;
    acirc_init_gates(&c->gates);
    acirc_init_consts(&c->consts);
    acirc_init_outputs(&c->outputs);
    acirc_init_tests(&c->tests);
    acirc_init_commands(&c->commands);
    acirc_init_extgates(&c->extgates);
    c->_degree_memo_allocated = false;
}

static void degree_memo_allocate(acirc *c)
{
    c->_degree_memo   = acirc_calloc(c->ninputs+1, sizeof(size_t *));
    c->_degree_memoed = acirc_calloc(c->ninputs+1, sizeof(bool *));
    for (size_t i = 0; i < c->ninputs+1; i++) {
        c->_degree_memo[i]   = acirc_calloc(acirc_nrefs(c), sizeof(size_t));
        c->_degree_memoed[i] = acirc_calloc(acirc_nrefs(c), sizeof(bool));
    }
    c->_degree_memo_allocated = true;
}

void acirc_clear(acirc *c)
{
    acirc_clear_gates(&c->gates);
    acirc_clear_outputs(&c->outputs);
    acirc_clear_tests(&c->tests);
    acirc_clear_consts(&c->consts);
    acirc_clear_extgates(&c->extgates);
    if (c->_degree_memo_allocated) {
        for (size_t i = 0; i < c->ninputs + 1; i++) {
            free(c->_degree_memo[i]);
            free(c->_degree_memoed[i]);
        }
        free(c->_degree_memo);
        free(c->_degree_memoed);
    }
}

int acirc_parse(acirc *c, const char *filename)
{
    yyin = fopen(filename, "r");
    if (yyin == NULL) {
        fprintf(stderr, "error: could not open file \"%s\"\n", filename);
        return ACIRC_ERR;
    }
    int ret = yyparse(c);
    fclose(yyin);
    return ret == 0 ? ACIRC_OK : ACIRC_ERR;
}

acirc * acirc_from_file(const char *filename)
{
    acirc *c = acirc_malloc(sizeof(acirc));
    acirc_init(c);
    int ret = acirc_parse(c, filename);
    if (ret == ACIRC_ERR) {
        acirc_clear(c);
        free(c);
        return NULL;
    }
    return c;
}

bool acirc_to_file(const acirc *c, const char *fname)
{
    bool ret = true;
    FILE *f;

    f = fopen(fname, "w");
    if (f == NULL)
        return false;
    acirc_add_tests_to_file(&c->tests, c->ninputs, c->outputs.n, f);
    for (size_t i = 0; i < acirc_nrefs(c); ++i) {
        const acirc_gate_t *gate = &c->gates.gates[i];
        switch (gate->op) {
        case OP_INPUT:
            fprintf(f, "%ld input %ld\n", i, gate->args[0]);
            break;
        case OP_CONST:
            fprintf(f, "%ld const %ld\n", i, gate->args[1]);
            break;
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_SET:
            fprintf(f, "%ld %s", i, acirc_op2str(gate->op));
            for (size_t j = 0; j < gate->nargs; ++j) {
                fprintf(f, " %ld", gate->args[j]);
            }
            fprintf(f, "\n");
            break;
        case OP_EXTERNAL:
            abort();
        }
    }
    acirc_add_outputs_to_file(&c->outputs, f);
    fclose(f);
    return ret;
}

////////////////////////////////////////////////////////////////////////////////
// acirc evaluation

int acirc_eval(acirc *c, acircref root, int *xs)
{
    acircref topo[acirc_nrefs(c)];
    acircref vals[acirc_nrefs(c)];
    const size_t n = acirc_topological_order(topo, c, root);

    for (size_t i = 0; i < n; i++) {
        const acircref ref = topo[i];
        const acirc_gate_t *gate = &c->gates.gates[ref];
        switch (gate->op) {
        case OP_INPUT:
            vals[ref] = xs[gate->args[0]];
            break;
        case OP_CONST:
            vals[ref] = gate->args[1];
            break;
        case OP_ADD:
            vals[ref] = 0;
            for (size_t j = 0; j < gate->nargs; ++j) {
                vals[ref] += vals[gate->args[j]];
            }
            break;
        case OP_SUB:
            assert(gate->nargs >= 1);
            vals[ref] = vals[gate->args[0]];
            for (size_t j = 1; j < gate->nargs; ++j) {
                vals[ref] -= vals[gate->args[j]];
            }
            break;
        case OP_MUL:
            vals[ref] = 1;
            for (size_t j = 0; j < gate->nargs; ++j) {
                vals[ref] *= vals[gate->args[j]];
            }
            break;
        case OP_SET:
            vals[ref] = vals[gate->args[0]];
            break;
        }
    }
    return vals[root];
}

bool acirc_ensure(acirc *c)
{
    const acirc_tests_t *tests = &c->tests;
    int res[c->outputs.n];
    bool ok  = true;

    if (g_verbose)
        printf("running acirc tests...\n");

    for (size_t test_num = 0; test_num < tests->n; test_num++) {
        bool test_ok = true;
        for (size_t i = 0; i < c->outputs.n; i++) {
            res[i] = acirc_eval(c, c->outputs.buf[i], tests->inps[test_num]);
            test_ok = test_ok && (res[i] == tests->outs[test_num][i]);
        }

        if (g_verbose) {
            if (!test_ok)
                printf("\033[1;41m");
            printf("test %lu input=", test_num);
            array_printstring_rev(tests->inps[test_num], c->ninputs);
            printf(" expected=");
            array_printstring_rev(tests->outs[test_num], c->outputs.n);
            printf(" got=");
            array_printstring_rev(res, c->outputs.n);
            if (!test_ok)
                printf("\033[0m");
            puts("");
        }

        ok = ok && test_ok;
    }
    return ok;
}

////////////////////////////////////////////////////////////////////////////////
// acirc info calculations

static size_t acirc_depth_helper(const acirc *c, acircref ref, size_t *memo, bool *seen)
{
    if (seen[ref])
        return memo[ref];

    const acirc_gate_t *gate = &c->gates.gates[ref];
    size_t ret = 0;

    switch (gate->op) {
    case OP_INPUT: case OP_CONST:
        ret = 0;
        break;
    case OP_ADD: case OP_SUB: case OP_MUL:
        for (size_t i = 0; i < gate->nargs; ++i) {
            size_t tmp = acirc_depth_helper(c, gate->args[i], memo, seen);
            ret = ret > tmp ? ret : tmp;
        }
        break;
    case OP_SET:
        ret = acirc_depth_helper(c, gate->args[0], memo, seen);
        break;
    default:
        abort();
    }

    seen[ref] = true;
    memo[ref] = ret;
    return ret;
}

size_t acirc_depth(const acirc *c, acircref ref)
{
    size_t memo[acirc_nrefs(c)];
    bool   seen[acirc_nrefs(c)];
    memset(seen, '\0', sizeof seen);
    return acirc_depth_helper(c, ref, memo, seen);
}

static size_t acirc_degree_helper(const acirc *c, acircref ref, size_t *memo, bool *seen)
{
    if (seen[ref])
        return memo[ref];

    size_t ret = 0;
    const acirc_gate_t *gate = &c->gates.gates[ref];
    switch (gate->op) {
    case OP_INPUT: case OP_CONST:
        ret = 1;
        break;
    case OP_ADD: case OP_SUB: case OP_MUL:
        for (size_t i = 0; i < gate->nargs; ++i) {
            size_t tmp = acirc_degree_helper(c, gate->args[i], memo, seen);
            if (gate->op == OP_MUL)
                ret += tmp;
            else
                ret = (ret > tmp) ? ret : tmp;
        }
        break;
    case OP_SET:
        ret = acirc_degree_helper(c, gate->args[0], memo, seen);
        break;
    }

    seen[ref] = true;
    memo[ref] = ret;
    return ret;
}

size_t acirc_degree(const acirc *c, acircref ref)
{
    size_t memo[acirc_nrefs(c)];
    bool   seen[acirc_nrefs(c)];
    memset(seen, '\0', sizeof seen);
    return acirc_degree_helper(c, ref, memo, seen);
}

size_t acirc_max_degree(const acirc *c)
{
    size_t memo[acirc_nrefs(c)];
    bool   seen[acirc_nrefs(c)];
    memset(seen, '\0', sizeof seen);
    size_t ret = 0;
    for (size_t i = 0; i < c->outputs.n; i++) {
        size_t tmp = acirc_degree_helper(c, c->outputs.buf[i], memo, seen);
        if (tmp > ret)
            ret = tmp;
    }

    return ret;
}

size_t acirc_var_degree(acirc *c, acircref ref, acircref id)
{
    if (!c->_degree_memo_allocated)
        degree_memo_allocate(c);
    if (c->_degree_memoed[id][ref])
        return c->_degree_memo[id][ref];

    const acirc_gate_t *gate = &c->gates.gates[ref];
    switch (gate->op) {
    case OP_INPUT:
        return (gate->args[0] == id) ? 1 : 0;
    case OP_CONST:
        return 0;
    case OP_ADD: case OP_SUB: case OP_MUL: {
        size_t res = 0;
        for (size_t i = 0; i < gate->nargs; ++i) {
            size_t tmp = acirc_var_degree(c, gate->args[i], id);
            if (gate->op == OP_MUL)
                res += tmp;
            else
                res = res > tmp ? res : tmp;
        }
        if (gate->op != OP_MUL) {
            c->_degree_memo[id][ref] = res;
            c->_degree_memoed[id][ref] = true;
        }
        return res;
    }
    case OP_SET: {
        size_t res = acirc_var_degree(c, gate->args[0], id);
        c->_degree_memo[id][ref] = res;
        c->_degree_memoed[id][ref] = true;
        return res;
    }
    default:
        abort();
    }
}

size_t acirc_const_degree(acirc *c, acircref ref)
{
    if (!c->_degree_memo_allocated)
        degree_memo_allocate(c);
    if (c->_degree_memoed[c->ninputs][ref])
        return c->_degree_memo[c->ninputs][ref];

    const acirc_gate_t *gate = &c->gates.gates[ref];
    switch (gate->op) {
    case OP_INPUT:
        return 0;
    case OP_CONST:
        return 1;
    case OP_ADD: case OP_SUB: case OP_MUL: {
        size_t res = 0;
        for (size_t i = 0; i < gate->nargs; ++i) {
            size_t tmp = acirc_const_degree(c, gate->args[i]);
            if (gate->op == OP_MUL)
                res += tmp;
            else
                res = res > tmp ? res : tmp;
        }
        if (gate->op != OP_MUL) {
            c->_degree_memo[c->ninputs][ref] = res;
            c->_degree_memoed[c->ninputs][ref] = true;
        }
        return res;
    }
    case OP_SET:
        return acirc_const_degree(c, gate->args[0]);
    default:
        abort();
    }
}

size_t acirc_max_var_degree(acirc *c, acircref id)
{
    size_t ret = 0;
    for (size_t i = 0; i < c->outputs.n; i++) {
        size_t tmp = acirc_var_degree(c, c->outputs.buf[i], id);
        if (tmp > ret)
            ret = tmp;
    }
    return ret;
}

size_t acirc_max_const_degree(acirc *c)
{
    size_t ret = 0;
    for (size_t i = 0; i < c->outputs.n; i++) {
        size_t tmp = acirc_const_degree(c, c->outputs.buf[i]);
        if (tmp > ret)
            ret = tmp;
    }
    return ret;
}

size_t acirc_delta(acirc *c)
{
    size_t delta = acirc_max_const_degree(c);
    for (size_t i = 0; i < c->ninputs; i++) {
        delta += acirc_max_var_degree(c, i);
    }
    return delta;
}

static size_t acirc_total_degree_helper(acirc *c, acircref ref)
{
    if (c->_degree_memoed[c->ninputs][ref])
        return c->_degree_memo[c->ninputs][ref];

    const acirc_gate_t *gate = &c->gates.gates[ref];
    switch (gate->op) {
    case OP_INPUT: case OP_CONST:
        return 1;
    case OP_ADD: case OP_SUB: case OP_MUL: {
        size_t res = 0;
        for (size_t i = 0; i < gate->nargs; ++i) {
            res += acirc_total_degree_helper(c, gate->args[i]);
        }
        c->_degree_memo[c->ninputs][ref] = res;
        c->_degree_memoed[c->ninputs][ref] = true;
        return res;
    }
    case OP_SET:
        return acirc_total_degree_helper(c, gate->args[0]);
    default:
        abort();
    }
}

size_t acirc_max_total_degree(acirc *c)
{
    if (!c->_degree_memo_allocated)
        degree_memo_allocate(c);
    /* reset memoed info */
    memset(c->_degree_memoed[c->ninputs], '\0', acirc_nrefs(c) * sizeof(bool));

    size_t ret = 0;
    for (size_t i = 0; i < c->outputs.n; ++i) {
        size_t tmp = acirc_total_degree_helper(c, c->outputs.buf[i]);
        if (tmp > ret)
            ret = tmp;
    }
    return ret;
}

////////////////////////////////////////////////////////////////////////////////
// helpers

acirc_operation acirc_str2op(char *s)
{
    if (strcmp(s, "ADD") == 0) {
        return OP_ADD;
    } else if (strcmp(s, "SUB") == 0) {
        return OP_SUB;
    } else if (strcmp(s, "MUL") == 0) {
        return OP_MUL;
    } else if (strcmp(s, "SET") == 0) {
        return OP_SET;
    } else {
        fprintf(stderr, "unknown op '%s'\n", s);
        abort();
    }
}

char * acirc_op2str(acirc_operation op)
{
    switch (op) {
    case OP_ADD:
        return "ADD";
    case OP_SUB:
        return "SUB";
    case OP_MUL:
        return "MUL";
    case OP_SET:
        return "SET";
    default:
        return NULL;
    }
}
