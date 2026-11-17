.pragma library

// Helpers of the chip flight animations (ChipFly): the flights show a handful of
// chips, not the whole pile — the pill or the row has the truth (PLAN.md §6, the
// chips section). The split itself is the pile's own (`BetChip.splitOf` /
// `BetRows.columnsOf`), so the flying chips are exactly the denominations of the
// bet they come from and no third ladder copy is needed.

// Up to `n` chip values out of a split: piles are walked biggest first and a pile
// contributes as many chips as still fit. A pile with count 0 is not in a split.
function take(piles, n) {
    var out = []
    if (!piles)
        return out
    for (var i = 0; i < piles.length && out.length < n; ++i) {
        var count = Math.min(piles[i].count, n - out.length)
        for (var k = 0; k < count; ++k)
            out.push(piles[i].value)
    }
    return out
}

// Divide the pot's chips between the winners (the payout, 2026-11-18): each pile's
// VALUE should be close to that winner's money share (the pills have the exact sums).
// The chips go biggest first, each to the share whose value deficit is largest, so the
// big winner gets the big chips. Every winner gets at least one chip ("минимум одну"):
// an empty share takes the smallest chip of the richest pile. With ONE winner the pot's
// own order is kept, which is what makes the switch from the pot to the payout pile
// invisible. The input lists are not mutated.
function divide(chips, amounts) {
    var k = amounts.length
    var out = []
    if (!chips || chips.length === 0 || k <= 0)
        return out
    for (var i = 0; i < k; ++i)
        out.push([])
    if (k === 1) {
        out[0] = chips.slice()
        return out
    }
    var order = chips.slice().sort(function(a, b) { return b - a })
    var chipsValue = 0, money = 0
    for (var c = 0; c < order.length; ++c)
        chipsValue += order[c]
    for (var m = 0; m < k; ++m)
        money += Math.max(0, amounts[m])
    var got = []
    for (var g = 0; g < k; ++g)
        got.push(0)
    for (var n = 0; n < order.length; ++n) {
        var pick = 0, best = -Infinity
        for (var j = 0; j < k; ++j) {
            var target = money > 0 ? chipsValue * Math.max(0, amounts[j]) / money
                                   : chipsValue / k
            var deficit = target - got[j]
            if (deficit > best) {
                best = deficit
                pick = j
            }
        }
        out[pick].push(order[n])
        got[pick] += order[n]
    }
    // Nobody is left without chips: the richest pile gives its smallest chip away; when
    // every pile already holds a single chip the smallest of them is repeated (more
    // winners than chips — the sums in the pills are the truth, the chips are decoration).
    for (var e = 0; e < k; ++e) {
        if (out[e].length > 0)
            continue
        var rich = -1
        for (var r = 0; r < k; ++r)
            if (out[r].length > 1 && (rich < 0 || got[r] > got[rich]))
                rich = r
        if (rich < 0) {
            var small = -1
            for (var q = 0; q < k; ++q) {
                if (out[q].length === 0)
                    continue
                var last = out[q][out[q].length - 1]
                if (small < 0 || last < small)
                    small = last
            }
            if (small >= 0)
                out[e].push(small)
            continue
        }
        var chip = out[rich].pop()          // the list is descending: the smallest
        out[e].push(chip)
        got[rich] -= chip
        got[e] += chip
    }
    return out
}
