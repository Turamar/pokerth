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
