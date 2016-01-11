// Tutorial on recursive descent parsing:   http://matt.might.net/articles/parsing-regex-with-recursive-descent/
// Inspiration for code to build NFA:       https://swtch.com/~rsc/regexp/regexp1.html

#include "regex.h"
#include "mempool.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SPLIT 256
#define MATCH 257
#define NONE -1
#define CHRCLS -2
#define ANY -3
#define FIRST -4
#define LAST -5

struct char_class_node {
    int a;
    int b;
    struct char_class_node *next;
    int negate;
};

typedef enum {
    RAN_OR,
    RAN_SEQUENCE,
    RAN_BASE,
    RAN_REPETION,
    RAN_AT_LEAST_ONE,
    RAN_ONE_OR_NONE,
    RAN_CLASS,
    RAN_BLANK
} rgx_ast_type;

typedef struct {
    rgx_ast_type type;
} rgx_ast_node;

typedef struct {
    rgx_ast_type type;

    rgx_ast_node *first;
    rgx_ast_node *second;
} rgx_ast_or_seq;

typedef struct {
    rgx_ast_type type;

    rgx_ast_node *loop;
} rgx_ast_repetition;

typedef struct {
    rgx_ast_type type;

    int c;
} rgx_ast_base;

typedef struct {
    rgx_ast_type type;

    struct char_class_node *node;
} rgx_ast_class;

struct state {
    int c;
    struct char_class_node *chrcls;
    struct state *out;
    struct state *out_alt;
    int listit;
};

union dangling_pointers {
    struct state *state;
    union dangling_pointers *next;
};

typedef struct {
    char *input;
    char *error;

    mempool mempool;
    rgx_regex *regex;
} rgx_compiler;

typedef struct state rgx_state;
typedef struct char_class_node rgx_charclass;
typedef union dangling_pointers dangling_pointers;
typedef struct {
    rgx_state *start;
    dangling_pointers *out;
} rgx_fragment;

typedef struct {
    rgx_state **mem;
    int ct;
} rgxr_list;

struct regex {
    rgx_state *start;
    rgx_state **l1;
    rgx_state **l2;

    rgxr_list *curr_list;
    rgxr_list *next_list;

    int listgen;
    int matched;

    int state_count;
    mempool state_mempool;
    mempool class_mempool;

    unsigned flags;
};

static rgx_ast_node rgx_ast_blank = {RAN_BLANK};

rgx_ast_node *rgxc_base(rgx_compiler *compiler);
rgx_ast_node *rgxc_factor(rgx_compiler *compiler);
rgx_ast_node *rgxc_term(rgx_compiler *compiler);
rgx_ast_node *rgxc_regex(rgx_compiler *compiler);
rgx_fragment rgxb_build(rgx_ast_node *head, rgx_regex *regex);
rgx_state *rgxb_build_state(int c, rgx_state *outa, rgx_state *outb, rgx_regex *regex);
void rgxb_patch(dangling_pointers *p, rgx_state *s);

void rgx_set_flags(rgx_regex *regex, unsigned flags)
{
    regex->flags = flags;
}

unsigned rgx_get_flags(rgx_regex *regex)
{
    return regex->flags;
}

rgx_compiler rgxc_make_compiler(char *input)
{
    rgx_compiler compiler;
    compiler.input = malloc(strlen(input) + 1);
    compiler.error = NULL;
    strcpy(compiler.input, input);

    compiler.mempool = pool_create();
    pool_add(&compiler.mempool, compiler.input);
    compiler.regex = NULL;

    return compiler;
}

char rgxc_peek(rgx_compiler *compiler)
{
    return compiler->input[0];
}

void rgxc_eat(rgx_compiler *compiler, char expect)
{
    if(rgxc_peek(compiler) == expect)
        compiler->input++;
    else
        compiler->error = (char *)"Unexpected Input.";
}

char rgxc_next(rgx_compiler *compiler)
{
    char expect = rgxc_peek(compiler);
    rgxc_eat(compiler, expect);
    return expect;
}

char rgxc_has_more(rgx_compiler *compiler)
{
    return strlen(compiler->input) > 0;
}

void *rgxc_malloc(rgx_compiler *compiler, size_t size)
{
    void *ptr = malloc(size);
    pool_add(&compiler->mempool, ptr);

    return ptr;
}

rgx_regex *rgx_compile(char *input)
{
    rgx_compiler compiler = rgxc_make_compiler(input);
    rgx_regex *regex = malloc(sizeof(*regex));
    regex->state_count = 0;
    regex->listgen = 0;
    regex->flags = 0;
    regex->state_mempool = pool_create();
    regex->class_mempool = pool_create();
    compiler.regex = regex;

    rgx_ast_node *head = rgxc_regex(&compiler);

    rgx_fragment frag = rgxb_build(head, regex);
    pool_drain(&compiler.mempool);

    regex->start = frag.start;

    rgx_state *match = rgxb_build_state(MATCH, NULL, NULL, regex);
    rgxb_patch(frag.out, match);

    return regex;
}

rgx_ast_node *rgxc_regex(rgx_compiler *compiler)
{
    rgx_ast_node *term = rgxc_term(compiler);

    if(rgxc_has_more(compiler) && rgxc_peek(compiler) == '|')
    {
        rgxc_eat(compiler, '|');
        rgx_ast_node *rgx = rgxc_regex(compiler);

        rgx_ast_or_seq *or = rgxc_malloc(compiler, sizeof(*or));
        or->type = RAN_OR;
        or->first = term;
        or->second = rgx;

        return (rgx_ast_node *)or;
    }

    return term;
}

rgx_ast_node *rgxc_term(rgx_compiler *compiler)
{
    rgx_ast_node *factor = &rgx_ast_blank;

    while(rgxc_has_more(compiler) && rgxc_peek(compiler) != '|' && rgxc_peek(compiler) != ')')
    {
        rgx_ast_node *next = rgxc_factor(compiler);

        rgx_ast_or_seq *seq = rgxc_malloc(compiler, sizeof(*seq));
        seq->type = RAN_SEQUENCE;
        seq->first = factor;
        seq->second = next;

        factor = (rgx_ast_node *)seq;
    }

    return factor;
}

rgx_ast_node *rgxc_factor(rgx_compiler *compiler)
{
    rgx_ast_node *base = rgxc_base(compiler);
    while(rgxc_has_more(compiler) && (rgxc_peek(compiler) == '*' || rgxc_peek(compiler) == '+' || rgxc_peek(compiler) == '?'))
    {
        char next = rgxc_peek(compiler);
        rgxc_eat(compiler, next);
        rgx_ast_repetition *node = rgxc_malloc(compiler, sizeof(*node));

        switch(next)
        {
            case '*':
                node->type = RAN_REPETION;
                break;
            case '+':
                node->type = RAN_AT_LEAST_ONE;
                break;
            case '?':
                node->type = RAN_ONE_OR_NONE;
                break;
            default:
                compiler->error = (char *)"Wut?";
                break;
        }

        node->loop = base;

        base = (rgx_ast_node *)node;
    }

    return base;
}

rgx_charclass *rgxc_class_make(rgx_compiler *compiler, int a, int b)
{
    rgx_charclass *cls = malloc(sizeof(*cls));
    cls->negate = 0;
    cls->a = a;
    cls->b = b;
    cls->next = NULL;

    pool_add(&compiler->regex->class_mempool, cls);

    return cls;
}

rgx_ast_node *rgxc_predefined_class(rgx_compiler *compiler, int c)
{
    if(c != 'd' && c != 'D' && c != 's' && c != 'S' && c != 'w' && c != 'W')
        return NULL;

    rgx_charclass *head = rgxc_class_make(compiler, -1, -1);
    switch(c)
    {
        case 'D':
            head->negate = 1;
        case 'd':
            head->next = rgxc_class_make(compiler, '0', '9');
            break;

        case 'S':
            head->negate = 1;
        case 's': {
            rgx_charclass *cur = head;
            const char *bad = " \t\n\x0B\f\r";
            int i;
            for(i = 0; i < 6; i++)
            {
                cur->next = rgxc_class_make(compiler, bad[i], -1);
                cur = cur->next;
            }
            break;
        }

        case 'W':
            head->negate = 1;
        case 'w':
            head->next = rgxc_class_make(compiler, 'a', 'z');
            head->next->next = rgxc_class_make(compiler, 'A', 'Z');
            head->next->next->next = rgxc_class_make(compiler, '0', '9');
            head->next->next->next->next = rgxc_class_make(compiler, '_', -1);
            break;

        default:
            break;
    }

    rgx_ast_class *cls = rgxc_malloc(compiler, sizeof(*cls));
    cls->type = RAN_CLASS;
    cls->node = head;

    return (rgx_ast_node *)cls;
}

char rgxc_class_next(rgx_compiler *compiler, int *escape)
{
    *escape = 0;
    char c = rgxc_next(compiler);
    if(c != '\\')
        return c;

    *escape = 1;
    return rgxc_next(compiler);
}

rgx_ast_node *rgxc_class(rgx_compiler *compiler)
{
    int negate = 0;
    char c = rgxc_next(compiler);
    if(c == '^')
    {
        negate = 1;
        c = rgxc_next(compiler);
    }

    rgx_charclass *head = rgxc_class_make(compiler, -1, -1);
    head->negate = negate;

    rgx_charclass *cur = head;

    char n;
    while(rgxc_has_more(compiler) && rgxc_peek(compiler) != ']')
    {
        int escape;
        n = rgxc_class_next(compiler, &escape);
        if(n == '-' && !escape)
        {
            n = rgxc_next(compiler);
            cur->next = rgxc_class_make(compiler, c, n);
            cur = cur->next;
            if(rgxc_peek(compiler) == ']')
            {
                c = 0;
                break;
            }
            c = rgxc_class_next(compiler, &escape);
            continue;
        }

        cur->next = rgxc_class_make(compiler, c, -1);
        cur = cur->next;
        c = n;
    }

    if(c)
        cur->next = rgxc_class_make(compiler, c, -1);

    rgx_ast_class *cls = rgxc_malloc(compiler, sizeof(*cls));
    cls->type = RAN_CLASS;
    cls->node = head;

    return (rgx_ast_node *)cls;
}

rgx_ast_node *rgxc_base(rgx_compiler *compiler)
{
    int c;
    switch(rgxc_peek(compiler))
    {
        case '(': {
            rgxc_eat(compiler, '(');
            rgx_ast_node *r = rgxc_regex(compiler);
            rgxc_eat(compiler, ')');
            return r;
        }
        case '\\': {
            rgxc_eat(compiler, '\\');
            c = rgxc_next(compiler);
            rgx_ast_node *r = rgxc_predefined_class(compiler, c);
            if(r)
                return r;

            break;
        }
        case '[': {
            rgxc_eat(compiler, '[');
            rgx_ast_node *cls = rgxc_class(compiler);
            rgxc_eat(compiler, ']');
            return cls;
        }

        default:
            c = rgxc_next(compiler);
            if(c == '.')
                c = ANY;
//            else if(c == '$')
//                c = LAST;
//            else if(c == '^')
//                c = FIRST;
            break;
    }

    rgx_ast_base *node = rgxc_malloc(compiler, sizeof(*node));
    node->type = RAN_BASE;
    node->c = c;

    return (rgx_ast_node *)node;
}

dangling_pointers *rgxb_singleton(rgx_state **state)
{
    dangling_pointers *p = (dangling_pointers *)state;
    p->next = NULL;

    return p;
}

dangling_pointers *rgxb_joined(dangling_pointers *a, dangling_pointers *b)
{
    dangling_pointers *head = a;
    for(; a->next; a = a->next);
    a->next = b;
    return head;
}

void rgxb_patch(dangling_pointers *p, rgx_state *s)
{
    while(p)
    {
        dangling_pointers *n = p->next;
        p->state = s;
        p = n;
    }
}

rgx_fragment rgxb_build_fragment(rgx_state *start, dangling_pointers *out)
{
    rgx_fragment frag;
    frag.out = out;
    frag.start = start;

    return frag;
}

rgx_state *rgxb_build_state(int c, rgx_state *outa, rgx_state *outb, rgx_regex *regex)
{
    rgx_state *state = malloc(sizeof(*state));
    state->c = c;
    state->chrcls = NULL;
    state->out = outa;
    state->out_alt = outb;
    state->listit = -1;

    regex->state_count++;
    pool_add(&regex->state_mempool, state);

    return state;
}

rgx_fragment rgxb_or(rgx_ast_node *node, rgx_regex *regex)
{
    rgx_ast_or_seq *or = (rgx_ast_or_seq *)node;
    rgx_fragment left = rgxb_build(or->first, regex);
    rgx_fragment right = rgxb_build(or->second, regex);

    rgx_state *ns = rgxb_build_state(SPLIT, left.start, right.start, regex);
    return rgxb_build_fragment(ns, rgxb_joined(left.out, right.out));
}

rgx_fragment rgxb_sequence(rgx_ast_node *node, rgx_regex *regex)
{
    rgx_ast_or_seq *or = (rgx_ast_or_seq *)node;
    rgx_fragment left = rgxb_build(or->first, regex);
    rgx_fragment right = rgxb_build(or->second, regex);

    rgxb_patch(left.out, right.start);
    return rgxb_build_fragment(left.start, right.out);
}

rgx_fragment rgxb_base(rgx_ast_node *node, rgx_regex *regex)
{
    rgx_ast_base *base = (rgx_ast_base *)node;
    rgx_state *s = rgxb_build_state(base->c, NULL, NULL, regex);
    return rgxb_build_fragment(s, rgxb_singleton(&s->out));
}

rgx_fragment rgxb_class(rgx_ast_node *node, rgx_regex *regex)
{
    rgx_ast_class *cls = (rgx_ast_class *)node;
    rgx_state *s = rgxb_build_state(CHRCLS, NULL, NULL, regex);
    s->chrcls = cls->node;
    return rgxb_build_fragment(s, rgxb_singleton(&s->out));
}

rgx_fragment rgxb_repetition(rgx_ast_node *node, rgx_regex *regex)
{
    rgx_ast_repetition *rep = (rgx_ast_repetition *)node;
    rgx_fragment e = rgxb_build(rep->loop, regex);
    rgx_state *s = rgxb_build_state(SPLIT, e.start, NULL, regex);
    rgxb_patch(e.out, s);
    return rgxb_build_fragment(s, rgxb_singleton(&s->out_alt));
}

rgx_fragment rgxb_at_least_one(rgx_ast_node *node, rgx_regex *regex)
{
    rgx_ast_repetition *rep = (rgx_ast_repetition *)node;
    rgx_fragment e = rgxb_build(rep->loop, regex);
    rgx_state *s = rgxb_build_state(SPLIT, e.start, NULL, regex);
    rgxb_patch(e.out, s);
    return rgxb_build_fragment(e.start, rgxb_singleton(&s->out_alt));
}

rgx_fragment rgxb_one_or_none(rgx_ast_node *node, rgx_regex *regex)
{
    rgx_ast_repetition *rep = (rgx_ast_repetition *)node;
    rgx_fragment e = rgxb_build(rep->loop, regex);
    rgx_state *s = rgxb_build_state(SPLIT, e.start, NULL, regex);
    return rgxb_build_fragment(s, rgxb_joined(e.out, rgxb_singleton(&s->out_alt)));
}

rgx_fragment rgxb_blank(rgx_ast_node *node, rgx_regex *regex)
{
    rgx_state *s = rgxb_build_state(NONE, NULL, NULL, regex);
    return rgxb_build_fragment(s, rgxb_singleton(&s->out));
}

rgx_fragment rgxb_build(rgx_ast_node *head, rgx_regex *regex)
{
    switch(head->type)
    {
        case RAN_OR:
            return rgxb_or(head, regex);
        case RAN_SEQUENCE:
            return rgxb_sequence(head, regex);
        case RAN_BASE:
            return rgxb_base(head, regex);
        case RAN_CLASS:
            return rgxb_class(head, regex);
        case RAN_REPETION:
            return rgxb_repetition(head, regex);
        case RAN_AT_LEAST_ONE:
            return rgxb_at_least_one(head, regex);
        case RAN_ONE_OR_NONE:
            return rgxb_one_or_none(head, regex);
        case RAN_BLANK:
            return rgxb_blank(head, regex);
        default:
            return rgxb_blank(head, regex);
    }
}

void rgx_add_state(rgx_regex *regex, rgxr_list *list, rgx_state *state)
{
    if(state->listit == regex->listgen)
        return;

    state->listit = regex->listgen;
    if(state->c == SPLIT)
    {
        rgx_add_state(regex, list, state->out);
        rgx_add_state(regex, list, state->out_alt);
        return;
    }
    else if(state->c == NONE)
    {
        rgx_add_state(regex, list, state->out);
        return;
    }

    list->mem[list->ct++] = state;
    if(state->c == MATCH)
        regex->matched = 1;
}

int rgx_test_class(rgx_charclass *cls, char c)
{
    int ret;
    int negate = cls->negate;
    for(cls = cls->next; cls; cls = cls->next)
    {
        if(cls->b < 0)
            ret = c == cls->a;
        else
            ret = c >= cls->a && c <= cls->b;

        if(ret && !negate) return 1;
        if(ret &&  negate) return 0;
    }

    return negate;
}

char rgx_lower(char c)
{
    if(c >= 'a' && c <= 'z')
        return c;
    return c - 'A' + 'a';
}

int rgx_test(rgx_state *s, char c, unsigned flags)
{
    if(s->c == ANY)
        return 1;
    if(s->c != CHRCLS)
    {
        if((flags & RGX_IGNORE_CASE) && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
            return rgx_lower(s->c) == rgx_lower(c);

        return s->c == c;
    }

    return rgx_test_class(s->chrcls, c);
}

void rgx_step(rgx_regex *regex, char c)
{
    regex->listgen++;
    regex->matched = 0;
    rgxr_list *cur = regex->curr_list;
    rgxr_list *nex = regex->next_list;

    nex->ct = 0;
    int i;
    for(i = 0; i < cur->ct; i++)
    {
        rgx_state *s = cur->mem[i];
        if(rgx_test(s, c, regex->flags))
            rgx_add_state(regex, nex, s->out);
    }
}

void rgx_reset_all(mempool *state_pool)
{
    struct poolnode *node = state_pool->head;
    for(; node; node = node->next)
        ((rgx_state *)node->data)->listit = 0;
}

rgx_result_wrapper rgx_wrapper_make()
{
    rgx_result_wrapper wrapper;
    wrapper.indices = malloc(sizeof(int) * 10);
    wrapper.ct = 0;
    wrapper.alloced = 10;

    return wrapper;
}

void rgx_wrapper_append(rgx_result_wrapper *w, int c)
{
    if(w->ct == w->alloced)
    { 
        w->alloced += 10;
        w->indices = realloc(w->indices, sizeof(int) * w->alloced);
    }

    w->indices[w->ct++] = c;
}

int *rgx_wrapper_finalize(rgx_result_wrapper *w)
{
    rgx_wrapper_append(w, -1);
    return w->indices;
}

int *rgx_collect_matches(rgx_regex *regex, char *input)
{
    rgx_state *l1[regex->state_count];
    rgx_state *l2[regex->state_count];

    rgxr_list lista;
    rgxr_list listb;

    lista.ct = 0;
    lista.mem = l1;

    listb.ct = 0;
    listb.mem = l2;

    regex->l1 = l1;
    regex->l2 = l2;

    regex->curr_list = &lista;
    regex->next_list = &listb;

    rgx_add_state(regex, regex->curr_list, regex->start);

    rgx_result_wrapper res = rgx_wrapper_make();

    int idx;
    for(idx = 0; *input; input++, idx++)
    {
        char *start = input;
        rgx_reset_all(&regex->state_mempool);
        rgx_add_state(regex, regex->curr_list, regex->start);
        int len;
        int lasti, lastlen;
        lasti = lastlen = -1;
        for(len = 0; *start; start++, len++)
        {
            char c = start[0];
            rgx_step(regex, c);
            rgxr_list *temp = regex->curr_list; regex->curr_list = regex->next_list; regex->next_list = temp;

            if(regex->matched)
            {
                lasti = idx;
                lastlen = len;
                /*input += len;
                rgx_wrapper_append(&res, idx);
                rgx_wrapper_append(&res, len);
                idx += len;
                break;*/
            }
            if(regex->curr_list->ct == 0)
            {
                if(lasti < 0)
                    break;
                input += len - 1;
                rgx_wrapper_append(&res, idx);
                rgx_wrapper_append(&res, len);
                idx += len - 1;
                lasti = -1;

                if(!(regex->flags & RGX_GLOBAL))
                    return rgx_wrapper_finalize(&res);

                break;
            }
        }

        if(lasti >= 0)
        {
            input += len - 1;
            rgx_wrapper_append(&res, idx);
            rgx_wrapper_append(&res, len);
            idx += len;
        }
    }

    return rgx_wrapper_finalize(&res);
}

int rgx_search(rgx_regex *regex, char *input)
{
    rgx_state *l1[regex->state_count];
    rgx_state *l2[regex->state_count];

    rgxr_list lista;
    rgxr_list listb;

    lista.ct = 0;
    lista.mem = l1;

    listb.ct = 0;
    listb.mem = l2;

    regex->l1 = l1;
    regex->l2 = l2;

    regex->curr_list = &lista;
    regex->next_list = &listb;

    rgx_add_state(regex, regex->curr_list, regex->start);

    int idx;
    for(idx = 0; *input; input++, idx++)
    {
        char *start = input;
        rgx_add_state(regex, regex->curr_list, regex->start);
        for(; *start; start++)
        {
            char c = start[0];
            rgx_step(regex, c);
            rgxr_list *temp = regex->curr_list; regex->curr_list = regex->next_list; regex->next_list = temp;

            if(regex->matched)
            {
                rgx_reset_all(&regex->state_mempool);
                return idx;
            }
            if(regex->curr_list->ct == 0)
                break;
        }
    }

    rgx_reset_all(&regex->state_mempool);

//    int res = rgx_list_contains_final(regex->curr_list);
//    regex->curr_list = regex->next_list = NULL;
//    regex->l1 = regex->l2 = NULL;

    return -1;
}

int rgx_matches(rgx_regex *regex, char *input)
{
    rgx_state *l1[regex->state_count];
    rgx_state *l2[regex->state_count];

    rgxr_list lista;
    rgxr_list listb;

    lista.ct = 0;
    lista.mem = l1;

    listb.ct = 0;
    listb.mem = l2;

    regex->l1 = l1;
    regex->l2 = l2;

    regex->curr_list = &lista;
    regex->next_list = &listb;

    rgx_add_state(regex, regex->curr_list, regex->start);
    regex->matched = 0;

    for(; *input; input++)
    {
        char c = input[0];
        rgx_step(regex, c);
        rgxr_list *temp = regex->curr_list; regex->curr_list = regex->next_list; regex->next_list = temp;
    }

    int res = regex->matched;
    regex->curr_list = regex->next_list = NULL;
    regex->l1 = regex->l2 = NULL;

    rgx_reset_all(&regex->state_mempool);

    return res;
}

void rgx_free(rgx_regex *regex)
{
    pool_drain(&regex->state_mempool);
    pool_drain(&regex->class_mempool);
    free(regex);
}
