// Minimal adversarial probe: quickjs 0.15.0 with the formjs sandbox contract
// (16 MiB cap, 1 MiB stack, interrupt deadline) — measures wall time and
// classification of native-loop candidates. Review-lane scratch, not a test.
#include <quickjs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static clock_t start;
static int deadline_ms;
static int fired = 0;

static int handler(JSRuntime *rt, void *opaque)
{
    (void)rt; (void)opaque;
    if ((clock() - start) * 1000 / CLOCKS_PER_SEC > deadline_ms) { fired = 1; return 1; }
    return 0;
}

static void run(const char *label, const char *code, int dl)
{
    JSRuntime *rt = JS_NewRuntime();
    JS_SetMemoryLimit(rt, 16 * 1024 * 1024);
    JS_SetMaxStackSize(rt, 1024 * 1024);
    JS_SetInterruptHandler(rt, handler, NULL);
    JSContext *ctx = JS_NewContext(rt);
    deadline_ms = dl; fired = 0; start = clock();
    JSValue v = JS_Eval(ctx, code, strlen(code), "<probe>", JS_EVAL_TYPE_GLOBAL);
    double ms = (clock() - start) * 1000.0 / CLOCKS_PER_SEC;
    const char *outcome = "ok";
    if (JS_IsException(v)) {
        JSValue exc = JS_GetException(ctx);
        outcome = JS_IsNull(exc) ? (fired ? "TIMEOUT(sentinel)" : "sentinel")
                                 : "exception";
        if (!JS_IsNull(exc)) {
            JSValue n = JS_GetPropertyStr(ctx, exc, "name");
            const char *ns = JS_ToCString(ctx, n);
            static char buf[128];
            snprintf(buf, sizeof buf, "%s%s", outcome, ns ? ns : "?");
            outcome = buf;
            JS_FreeCString(ctx, ns);
            JS_FreeValue(ctx, n);
        }
        JS_FreeValue(ctx, exc);
    }
    printf("%-38s dl=%4dms  wall=%8.1fms  %s\n", label, dl, ms, outcome);
    JS_FreeValue(ctx, v);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

int main(void)
{
    printf("quickjs runtime: %s\n", JS_GetVersion());
    run("baseline loop 1e8", "let s=0; for (let i=0;i<1e8;i++) s+=i;", 150);
    run("ReDoS fail-match 24a+b", "Array(24).fill('a').join('') + 'b'.search? 0 : 0; var s='aaaaaaaaaaaaaaaaaaaaaaaaaab'; /(?:a+)+$/.test(s);", 150);
    run("ReDoS replace 24a+b", "'aaaaaaaaaaaaaaaaaaaaaaaaab'.replace(/(a+)+$/, 'x');", 150);
    run("ReDoS test 28a+b", "'aaaaaaaaaaaaaaaaaaaaaaaaaaaab'.search(/(a+)+$/);", 150);
    run("nested quantifier 40 chars", "'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaX'.match(/^(a|a)*$/);", 150);
    run("Array(2^31).join('a')", "Array(2147483648).join('a');", 150);
    run("Array(2^31).includes(x)", "Array(2147483648).includes('x');", 150);
    run("Array(2^31).indexOf(x)", "Array(2147483648).indexOf('x');", 150);
    run("Array(16.7M).includes (pin)", "Array(16777216).includes('x');", 150);
    return 0;
}
