/*****************************************************************************
 * vote.h — Temporal majority-voting module
 *
 * Smooths per-window classifier output by taking the majority vote over the
 * last VOTE_WINDOW predictions. Re-entrant per instance — caller owns the
 * Vote_t state, no module-static storage.
 *
 * Usage (single instance, typical):
 *
 *     static Vote_t g_vote;
 *
 *     Vote_Init(&g_vote);
 *
 *     for (;;) {
 *         uint8_t raw_label = run_inference(...);
 *         Vote_Push(&g_vote, raw_label);
 *         if (Vote_Ready(&g_vote)) {
 *             uint8_t smoothed = Vote_Decide(&g_vote);
 *             emit(smoothed);
 *         } else {
 *             emit(raw_label);   // fallback during warm-up
 *         }
 *     }
 *
 * Tie-breaking: favours the lower-indexed class. With N_CLASSES = 2, this
 * means a 4-vs-4-vs-1-invalid history would resolve to SMOOTH. Rationale:
 * false positives for "rough" are operationally worse than false negatives
 * in a road monitor.
 *
 * MISRA-C:2012 compliant (no dynamic memory, no recursion, all loop bounds
 * statically known, all variables initialised at declaration).
 *****************************************************************************/

#pragma once

#include "STD_TYPES.h"
#include "norm_params.h"

/* ========================================================= */
/* ================= Public State Type ===================== */
/* ========================================================= */

/**
 * @brief Vote buffer state. Caller owns the storage; no module-static state.
 *
 * Internal layout (informational — do not access directly):
 *   history[head]                         = oldest (once count == VOTE_WINDOW)
 *   history[(head - 1) mod VOTE_WINDOW]   = newest
 *   count saturates at VOTE_WINDOW.
 */
typedef struct {
    uint8_t history[VOTE_WINDOW];
    uint8_t head;     /* next write position, 0..VOTE_WINDOW-1   */
    uint8_t count;    /* number of valid entries, 0..VOTE_WINDOW */
} Vote_t;

/* ========================================================= */
/* ================= Public API ============================ */
/* ========================================================= */

/**
 * @brief Initialise a vote buffer to empty state.
 *
 * Zeros all fields. Must be called once before any other Vote_* function.
 *
 * @param v  Vote buffer (must not be NULL — undefined behaviour otherwise).
 */
void Vote_Init(Vote_t *v);

/**
 * @brief Insert a new label into the circular buffer.
 *
 * Out-of-range labels (>= N_CLASSES) are silently ignored — neither the
 * head nor the count advances. This keeps the history semantically clean
 * and prevents a buggy upstream from polluting the vote.
 *
 * @param v      Vote buffer (must not be NULL).
 * @param label  Class label, expected in [0, N_CLASSES).
 */
void Vote_Push(Vote_t *v, uint8_t label);

/**
 * @brief Reports whether the buffer has been filled at least once.
 *
 * Returns 1 once VOTE_WINDOW valid pushes have accumulated, else 0.
 * Useful to gate the "trust the smoothed output" decision during warm-up.
 *
 * @param v  Vote buffer (must not be NULL).
 * @return   1U if ready, 0U otherwise.
 */
uint8_t Vote_Ready(const Vote_t *v);

/**
 * @brief Compute the majority label across the current history.
 *
 * Tie-breaking: lower-indexed class wins (favours SMOOTH at N_CLASSES = 2).
 * On an empty buffer, returns 0U (defined fallback — spec says undefined).
 *
 * @param v  Vote buffer (must not be NULL).
 * @return   Majority label in [0, N_CLASSES).
 */
uint8_t Vote_Decide(const Vote_t *v);

/**
 * @brief Diagnostic accessor — current number of valid entries.
 *
 * Range: 0..VOTE_WINDOW. Useful for logging "warm-up progress" without
 * peeking inside the Vote_t struct.
 *
 * @param v  Vote buffer (must not be NULL).
 * @return   Current count, 0..VOTE_WINDOW.
 */
uint8_t Vote_GetCount(const Vote_t *v);
