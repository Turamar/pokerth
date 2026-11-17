.pragma library

// The chip split shared by everything that draws chips on the felt: the bet pile
// (BetChip), the compact rows (BetRows) and the pot pile (PotPile).
//
// Greedy over the ladder, from the biggest denomination down: every rung keeps
// its own stack while a stack is free, chips of one rung are spread evenly over
// the free stacks instead of being cut off at `maxPerStack`, and the remainder
// is not drawn — the sum pill is the truth (decision 2026-11-12).
//
// The ladders: a tournament counts chips, a cash table counts money in CENTS and
// has the 1¢ rung on top, so every cash amount is a whole number of pennies.
function ladder(cash) {
    return cash ? [50000, 10000, 2500, 500, 100, 25, 10, 5, 1]
                : [50000, 10000, 2500, 500, 100, 25, 10, 5]
}

function split(amount, cash, maxPiles, maxPerStack) {
    var steps = ladder(cash)
    var minChip = steps[steps.length - 1]
    amount = Math.max(0, Math.round(amount))
    if (amount <= 0)
        return []
    // Below the smallest chip: one neutral chip as decoration (an all-in of 1-4
    // tournament chips); the pill tells the sum.
    if (amount < minChip)
        return [{ "value": 0, "count": 1 }]
    var rest = amount, out = []
    for (var i = 0; i < steps.length && rest > 0; ++i) {
        var n = Math.floor(rest / steps[i])
        if (n <= 0)
            continue
        var free = maxPiles - out.length
        if (free > 0) {
            var shown = Math.min(n, free * maxPerStack)
            var k = Math.max(1, Math.min(free, Math.ceil(shown / maxPerStack)))
            var base = Math.floor(shown / k)
            var extra = shown % k
            for (var j = 0; j < k; ++j)
                out.push({ "value": steps[i], "count": base + (j < extra ? 1 : 0) })
        }
        rest -= n * steps[i]
    }
    return out
}

// Every chip of an amount as a flat list, biggest first — the chip multiset of
// the pot, independent of how it is stacked.
function chipList(amount, cash) {
    var piles = split(amount, cash, 4096, 4)
    var out = []
    for (var i = 0; i < piles.length; ++i)
        for (var k = 0; k < piles[i].count; ++k)
            out.push(piles[i].value)
    return out
}

// The POT: columns (stacks) of chips (decision 2026-11-17, player's rules):
//   • a column holds ONE denomination, up to `maxPerStack` chips (a denomination
//     that needs more chips continues in the next column — by the ladder that
//     never happens, a rung repeats at most 4 times, but the rule stays general);
//   • the columns are laid out in the CREATION ORDER the player described:
//     1 front left, 2 front right ("two chips as now, a row"), 3 top centre (in
//     the hollow — a pyramid), 4 top right (two below, two above), 5 the front
//     centre that CLOSES the front row and takes the leftovers;
//   • the POSITION of a column is random: the biggest denomination must not
//     always land in slot 1. The columns themselves are not touched — a stack
//     stays one denomination; only the slots are shuffled (the closing slot 5
//     keeps the leftovers);
//   • when the denominations outnumber the slots, the chips that did not fit are
//     merged into the closing column; whatever still does not fit is not drawn —
//     the pill has the truth.
function columns(amount, cash, maxColumns, maxPerStack) {
    var piles = split(amount, cash, 4096, maxPerStack)
    var cols = []
    for (var i = 0; i < piles.length; ++i) {
        var rest = piles[i].count
        while (rest > 0) {
            var take = Math.min(rest, maxPerStack)
            var col = []
            for (var k = 0; k < take; ++k)
                col.push(piles[i].value)
            cols.push(col)
            rest -= take
        }
    }
    // More columns than slots: the tail is poured into the closing column.
    if (cols.length > maxColumns) {
        var dump = cols[maxColumns - 1]
        for (var c = maxColumns; c < cols.length; ++c)
            for (var k2 = 0; k2 < cols[c].length && dump.length < maxPerStack; ++k2)
                dump.push(cols[c][k2])
        cols = cols.slice(0, maxColumns)
    }
    // Random POSITIONS only (the closing column, if the pot has one, stays last).
    var movable = Math.min(cols.length, maxColumns - 1)
    for (var s = movable - 1; s > 0; --s) {
        var j = Math.floor(Math.random() * (s + 1))
        var t = cols[s]; cols[s] = cols[j]; cols[j] = t
    }
    return cols
}
