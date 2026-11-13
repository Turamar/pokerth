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
