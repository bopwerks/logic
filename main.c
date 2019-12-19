#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>

#ifndef NDEBUG
struct call {
    struct call *prev;
    char *name;
};
static struct call *lastfn = NULL;
static int nindent = 0;

#define PRINT(...) do { \
    int _i; \
    for (_i = 0; _i < nindent; ++_i) \
        fprintf(stderr, "  ");    \
    fprintf(stderr,  __VA_ARGS__); \
    fputc('\n', stderr); \
} while (0)

#define ENTER(name) do { \
    int _i; \
    struct call fn = { lastfn, (name) }; \
    lastfn = &fn; \
    for (_i = 0; _i < nindent; ++_i) \
        fprintf(stderr, "  "); \
    fprintf(stderr, "Entered %s()\n", (name)); \
    ++nindent; \
} while (0)

#define RETURN(val) do { \
    int _i; \
    --nindent; \
    for (_i = 0; _i < nindent; ++_i) \
        fprintf(stderr, "  "); \
    fprintf(stderr, "Exiting %s()\n", lastfn->name); \
    lastfn = lastfn->prev; \
    return (val); \
} while (0)

#define EXIT()  do { \
    int _i; \
    --nindent; \
    for (_i = 0; _i < nindent; ++_i) \
        fprintf(stderr, "  "); \
    fprintf(stderr, "Exiting %s()\n", lastfn->name); \
    lastfn = lastfn->prev; \
} while (0)
#else
#define PRINT(...)
#define ENTER(name)
#define RETURN(val) return (val)
#define EXIT()
#endif

enum Token { AND, OR, NOT, IF, IFF, LPAREN, RPAREN, ID };

struct Node {
    int type;
    int id;
    struct Node *left;
    struct Node *right;
};
typedef struct Node Node;

struct Pair {
    unsigned p;
    unsigned s;
};
typedef struct Pair Pair;

enum { MAXEXPR = 10, MAXVARS = 100, MAXTOK = 64 };

struct Parser {
    char *var[MAXVARS];
    unsigned nvar;
    char tok[MAXTOK];
    FILE *fp;
    int next;
};
typedef struct Parser Parser;

static void delete(Node *n);

static Node *
mknode(enum Token type, int id, Node *left, Node *right)
{
    Node *n;
    n = malloc(sizeof *n);
    assert(n != NULL);
    n->type = type;
    n->id = id;
    n->left = left;
    n->right = right;
    return n;
}

static int
readtoken(Parser *p)
{
    int ch;
    int len;
    assert(p != NULL);

    ENTER("readtoken");
    len = 0;
    while ((ch = fgetc(p->fp)) != EOF && isspace(ch))
        ;
    if (ch == EOF)
        RETURN(EOF);
    p->tok[len++] = ch;
    if (isalpha(ch)) {
        for ( ; (ch = fgetc(p->fp)) != EOF && isalpha(ch); ++len) {
            p->tok[len] = ch;
        }
    } else {
        for ( ; (ch = fgetc(p->fp)) != EOF && !isspace(ch) && !isalpha(ch); ++len) {
            p->tok[len] = ch;
        }
    }
    if (ch == EOF)
        RETURN(EOF);
    else
        ungetc(ch, p->fp);
    p->tok[len] = '\0';
    PRINT("Read token: %s", p->tok);
    if (!strcmp("~", p->tok)) {
        RETURN(NOT);
    } else if (!strcmp("or", p->tok)) {
        RETURN(OR);
    } else if (!strcmp("not", p->tok)) {
        RETURN(NOT);
    } else if (!strcmp("<->", p->tok)) {
        RETURN(IFF);
    } else if (!strcmp("->", p->tok)) {
        RETURN(IF);
    } else if (!strcmp("(", p->tok)) {
        RETURN(LPAREN);
    } else if (!strcmp(")", p->tok)) {
        RETURN(RPAREN);
    } else {
        RETURN(ID);
    }
    RETURN(EOF);
}

static void
accept(Parser *p)
{
    ENTER("accept");
    assert(p != NULL);
    p->next = readtoken(p);
    EXIT();
}

static int
find(char *key, char *s[], int n)
{
    int i;
    ENTER("find");
    for (i = 0; i < n; ++i)
        if (!strcmp(key, s[i]))
            RETURN(i);
    RETURN(-1);
}

static Node * readexpr(Parser *p);

#ifndef NDEBUG
static char *
tokenstr(int t)
{
    switch (t) {
    case OR:
        return "or";
    case AND:
        return "and";
    case IF:
        return "->";
    case IFF:
        return "<->";
    case NOT:
        return "not";
    case LPAREN:
        return "(";
    case RPAREN:
        return ")";
    case ID:
        return "<id>";
    case EOF:
        return "<EOF>";
    }
    return "";
}
#endif

static Node * readcond(Parser *p);

static Node *
readterm(Parser *p)
{
    Node *t;
    int i;
    char *s;

    ENTER("readterm");
    PRINT("Next: %s", p->tok);
    t = NULL;
    switch (p->next) {
    case ID:
        i = find(p->tok, p->var, p->nvar);
        PRINT("find('%s', p->var, %d) = %d", p->tok, p->nvar, i);
        if (i == -1) {
            s = malloc(strlen(p->tok) + 1);
            assert(s != NULL);
            strcpy(s, p->tok);
            p->var[p->nvar] = s;
            i = p->nvar++;
        }
        accept(p);
        PRINT("var[%d] = '%s'", i, p->var[i]);
        t = mknode(ID, i, NULL, NULL);
        break;
    case NOT:
        PRINT("Got a NOT");
        accept(p);
        t = mknode(NOT, 0, readterm(p), NULL);
        break;
    case LPAREN:
        accept(p);
        t = readcond(p);
        if (p->next != RPAREN) {
            delete(t);
            t = NULL;
        } else {
            accept(p);
        }
        break;
    case EOF:
        PRINT("EOF");
        RETURN(NULL);
    default:
        assert(0);
        break;
    }
    RETURN(t);
}

static Parser *
mkparser(FILE *fp)
{
    Parser *p;

    assert(fp != NULL);
    p = malloc(sizeof *p);
    assert(p != NULL);
    p->fp = fp;
    p->nvar = 0;
    accept(p);
    return p;
}

static void
print(FILE *fp, Parser *p, Node *e);

static Node *
readexpr(Parser *p)
{
    Node *e, *f;
    enum Token type;
    ENTER("readexpr");
    PRINT("Reading first term");
    e = readterm(p);
    if (e == NULL)
        RETURN(NULL);
    while (p->next == AND || p->next == OR) {
        PRINT("Reading next term");
        type = p->next;
        accept(p);
        f = readterm(p);
        if (f == NULL) {
            delete(e);
            RETURN(NULL);
        }
        e = mknode(type, 0, e, f);
    }
    RETURN(e);
}

static void
delete(Node *n)
{
    if (n->left != NULL)
        delete(n->left);
    if (n->right != NULL)
        delete(n->right);
    free(n);
}

static Node *
readcond(Parser *p)
{
    Node *e, *f;
    enum Token type;

    ENTER("readcond");
    e = readexpr(p);
    if (e == NULL)
        RETURN(NULL);
    while (p->next == IF || p->next == IFF) {
        PRINT("Reading an IF");
        type = p->next;
        accept(p);
        f = readexpr(p);
        if (f == NULL) {
            delete(e);
            RETURN(NULL);
        }
        e = mknode(type, 0, e, f);
    }
    RETURN(e);
}

static void
print(FILE *fp, Parser *p, Node *e)
{
    switch (e->type) {
    case AND:
        fputc('(', fp);
        print(fp, p, e->left);
        fprintf(fp, " and ");
        print(fp, p, e->right);
        fputc(')', fp);
        break;
    case OR:
        fputc('(', fp);
        print(fp, p, e->left);
        fprintf(fp, " or ");
        print(fp, p, e->right);
        fputc(')', fp);
        break;
    case NOT:
        fputc('~', fp);
        print(fp, p, e->left);
        break;
    case IF:
        fputc('(', fp);
        print(fp, p, e->left);
        fprintf(fp, " -> ");
        print(fp, p, e->right);
        fputc(')', fp);
        break;
    case IFF:
        fputc('(', fp);
        print(fp, p, e->left);
        fprintf(fp, " <-> ");
        print(fp, p, e->right);
        fputc(')', fp);
        break;
    case ID:
        fprintf(fp, "%s", p->var[e->id]);
        break;
    default:
        assert(0);
        break;
    }
}

static int
eval(Node *n, unsigned env)
{
    unsigned m;
    
    ENTER("eval");
    PRINT("Evaluating %s", tokenstr(n->type));
    PRINT("env = %u", env);
    switch (n->type) {
    case ID:
        m = (env >> n->id) & 1U;
        PRINT("ID = %u", m);
        RETURN(m);
    case AND:
        RETURN(eval(n->left, env) | eval(n->right, env));
    case OR:
        RETURN(eval(n->left, env) | eval(n->right, env));
    case NOT:
        RETURN(~eval(n->left, env) & 1U);
    case IF:
        RETURN((~eval(n->left, env) | eval(n->right, env)) & 1U);
    case IFF:
        RETURN((~eval(n->left, env) | eval(n->right, env)) & 1U);
    default:
        assert(0);
        break;
    }
    RETURN(0);
}

int
main(void)
{
    Node *expr[MAXEXPR];
    int nexpr;
    /* Pair env[MAXENV]; */
    Parser *p;
    Node *e;
    unsigned i, n;
    int j;
    int valid;

    ENTER("main");
    
    /* Read each expression as a premise or a conclusion */
    p = mkparser(stdin);
    for (nexpr = 0; (e = readcond(p)) != NULL; ++nexpr)
        expr[nexpr] = e;

    valid = 1;
    n = 1 << p->nvar;
    for (i = 0; i < n; ++i) {
        for (j = 0; j < nexpr; ++j) {
            if (!eval(expr[j], i)) {
                if (j == nexpr-1) {
                    valid = 0;
                    goto done;
                } else
                    j = nexpr;
                    
            }
        }
    }
done:
    for (i = 0; i < nexpr; ++i)
        delete(expr[i]);
    RETURN(valid ? EXIT_SUCCESS : EXIT_FAILURE);
}
