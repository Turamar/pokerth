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

// The POT: columns (stacks) of chips, bottom -> top per column (decision
// 2026-11-17, the player's rule):
//   • a column holds up to `maxPerStack` chips, the biggest chip at the bottom;
//   • ONE DENOMINATION PER COLUMN while columns are left: 200 = 2x100 is ONE
//     column (not 100 + 100 in two), 900 = 500 + 4x100 is two columns;
//   • a denomination that needs more chips than one column holds continues in the
//     next column (by the ladder this never happens — a rung repeats at most 4
//     times — but the rule is kept general);
//   • when the denominations outnumber the columns, the smallest ones are mixed
//     into the LAST column while it has room, and whatever still does not fit is
//     not drawn: the pill has the truth.
function columns(amount, cash, maxColumns, maxPerStack) {
    var chips = chipList(amount, cash)
    var cols = [], i = 0
    while (i < chips.length) {
        if (cols.length < maxColumns) {
            var v = chips[i], col = []
            while (i < chips.length && col.length < maxPerStack && chips[i] === v)
                col.push(chips[i++])
            cols.push(col)
        } else {
            var last = cols[cols.length - 1]
            if (!last || last.length >= maxPerStack)
                break
            last.push(chips[i++])
        }
    }
    return cols
}
