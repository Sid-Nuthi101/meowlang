// Builtin functions: meowlang names that compile to a direct call to a
// fixed assembly symbol (typically libc) instead of a user-defined
// `_meowfn_<name>` label. Add new builtins here rather than special-casing
// them individually in runner.c.

typedef struct {
    char *name;
    char *asmSymbol;
    int argCount;
} Builtin;

static Builtin builtins[] = {
    { "yowl", "_puts", 1 },
};
static int builtinCount = sizeof(builtins) / sizeof(builtins[0]);

Builtin *findBuiltin(char *name) {
    for (int i = 0; i < builtinCount; i++) {
        if (strcmp(builtins[i].name, name) == 0) {
            return &builtins[i];
        }
    }
    return NULL;
}
