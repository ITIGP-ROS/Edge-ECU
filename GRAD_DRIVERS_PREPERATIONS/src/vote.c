/*****************************************************************************
 * vote.c — Temporal majority-voting module implementation
 *
 * See vote.h for the public contract. ~60 lines of logic + comments.
 *****************************************************************************/

#include "vote.h"

/* Compile-time invariants — fail the build if norm_params.h drifts. */
#if (VOTE_WINDOW < 1)
#  error "VOTE_WINDOW must be >= 1"
#endif
#if (N_CLASSES < 1)
#  error "N_CLASSES must be >= 1"
#endif

/* ========================================================= */
/* ================= Public API ============================ */
/* ========================================================= */

void Vote_Init(Vote_t *v)
{
    if (v == NULL)
    {
        return;
    }

    for (uint32_t i = 0U; i < (uint32_t)VOTE_WINDOW; ++i)
    {
        v->history[i] = 0U;
    }
    v->head  = 0U;
    v->count = 0U;
}

void Vote_Push(Vote_t *v, uint8_t label)
{
    if (v == NULL)
    {
        return;
    }

    /* Out-of-range labels are silently ignored — head and count both stay. */
    if (label >= (uint8_t)N_CLASSES)
    {
        return;
    }

    v->history[v->head] = label;

    /* Advance head modulo VOTE_WINDOW. With VOTE_WINDOW = 9 the modulo
     * is not a power of two, so we use an explicit wrap test rather
     * than a bitmask. Branch is predictable on Cortex-M4. */
    uint8_t next_head = (uint8_t)(v->head + 1U);
    if (next_head >= (uint8_t)VOTE_WINDOW)
    {
        next_head = 0U;
    }
    v->head = next_head;

    /* Saturate count at VOTE_WINDOW. */
    if (v->count < (uint8_t)VOTE_WINDOW)
    {
        v->count = (uint8_t)(v->count + 1U);
    }
}

uint8_t Vote_Ready(const Vote_t *v)
{
    if (v == NULL)
    {
        return 0U;
    }
    return (v->count >= (uint8_t)VOTE_WINDOW) ? 1U : 0U;
}

uint8_t Vote_Decide(const Vote_t *v)
{
    if ((v == NULL) || (v->count == 0U))
    {
        return 0U;   /* defined fallback for empty buffer */
    }

    /* Tally counts per class. int16_t is overkill for VOTE_WINDOW = 9
     * (uint8_t suffices) but matches the spec wording and leaves room
     * if VOTE_WINDOW ever grows past 255. */
    int16_t counts[N_CLASSES];
    for (uint32_t c = 0U; c < (uint32_t)N_CLASSES; ++c)
    {
        counts[c] = 0;
    }

    for (uint32_t i = 0U; i < (uint32_t)v->count; ++i)
    {
        uint8_t lbl = v->history[i];
        /* Defensive: history is only ever populated by Vote_Push which
         * already filters out-of-range labels. The check is cheap and
         * guards against a corrupted struct or a future code path that
         * writes the field directly. */
        if (lbl < (uint8_t)N_CLASSES)
        {
            counts[lbl] = (int16_t)(counts[lbl] + 1);
        }
    }

    /* Find the class with the highest count. Strict-greater comparison
     * means the first (lowest-indexed) class with the max wins → ties
     * favour the lower index (SMOOTH at N_CLASSES = 2). */
    uint8_t best_lbl   = 0U;
    int16_t best_count = counts[0];
    for (uint32_t c = 1U; c < (uint32_t)N_CLASSES; ++c)
    {
        if (counts[c] > best_count)
        {
            best_count = counts[c];
            best_lbl   = (uint8_t)c;
        }
    }

    return best_lbl;
}

uint8_t Vote_GetCount(const Vote_t *v)
{
    if (v == NULL)
    {
        return 0U;
    }
    return v->count;
}
