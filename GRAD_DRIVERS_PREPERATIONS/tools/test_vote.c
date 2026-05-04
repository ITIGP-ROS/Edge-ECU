/* tools/test_vote.c
 *
 * Host-side unit test for the vote module.
 *
 * Build:
 *   gcc -DHOST_BUILD -I../include -I../src \
 *       test_vote.c ../src/vote.c -o test_vote
 *
 * Run:
 *   ./test_vote
 */

#include <stdio.h>
#include "vote.h"

/* Tiny assert that prints the failing expression and exits non-zero,
 * so we don't depend on glibc's <assert.h> macro magic. */
#define CHECK(expr)                                                     \
    do {                                                                 \
        if (!(expr)) {                                                   \
            fprintf(stderr,                                              \
                    "FAIL  %s:%d  %s\n", __FILE__, __LINE__, #expr);    \
            return 1;                                                    \
        }                                                                \
    } while (0)

int main(void)
{
    Vote_t v;

    /* ---- Empty buffer ---- */
    Vote_Init(&v);
    CHECK(Vote_Ready(&v)    == 0U);
    CHECK(Vote_Decide(&v)   == 0U);   /* fallback */
    CHECK(Vote_GetCount(&v) == 0U);

    /* ---- 5 SMOOTH then 4 ROUGH → SMOOTH wins, count=9, ready=1 ---- */
    for (int i = 0; i < 5; ++i) { Vote_Push(&v, 0); }
    for (int i = 0; i < 4; ++i) { Vote_Push(&v, 1); }
    CHECK(Vote_Ready(&v)    == 1U);
    CHECK(Vote_Decide(&v)   == 0U);
    CHECK(Vote_GetCount(&v) == 9U);

    /* ---- Push another ROUGH → 4 SMOOTH, 5 ROUGH → ROUGH wins ---- */
    Vote_Push(&v, 1);
    CHECK(Vote_Decide(&v) == 1U);

    /* ---- Out-of-range labels are ignored (head/count unchanged) ---- */
    {
        uint8_t saved_head  = v.head;
        uint8_t saved_count = v.count;
        Vote_Push(&v, 99U);
        Vote_Push(&v, (uint8_t)N_CLASSES);
        CHECK(v.head  == saved_head);
        CHECK(v.count == saved_count);
    }

    /* ---- Tie-break favours SMOOTH (4 vs 4 with count=8, not yet ready) ---- */
    Vote_Init(&v);
    for (int i = 0; i < 4; ++i) { Vote_Push(&v, 0); }
    for (int i = 0; i < 4; ++i) { Vote_Push(&v, 1); }
    CHECK(Vote_Ready(&v)  == 0U);   /* not yet 9 */
    CHECK(Vote_Decide(&v) == 0U);   /* tie → SMOOTH */

    /* ---- After warm-up, alternating push doesn't break invariants ---- */
    Vote_Init(&v);
    for (int i = 0; i < 100; ++i) { Vote_Push(&v, (uint8_t)(i & 1)); }
    CHECK(Vote_Ready(&v)    == 1U);
    CHECK(Vote_GetCount(&v) == 9U);

    /* ---- NULL safety ---- */
    Vote_Init(NULL);                 /* must not crash */
    Vote_Push(NULL, 0);              /* must not crash */
    CHECK(Vote_Ready(NULL)    == 0U);
    CHECK(Vote_Decide(NULL)   == 0U);
    CHECK(Vote_GetCount(NULL) == 0U);

    printf("Vote_t unit tests PASS\n");
    return 0;
}
