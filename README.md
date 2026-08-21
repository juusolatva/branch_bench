# branch bench

A simple benchmark for testing out gains from branch prediction or conversely losses from mispredictions.

## Notes

You **MUST** run this with ./run.sh or use the flags *-fno-tree-vectorize* *-fno-if-conversion* when compiling or the compiler will optimize the mispredictions away.

## Saving results

Console output is meant to be read as it scrolls by. To get a clean,
table-formatted Markdown report instead use `-o`/`--output`:

```sh
./branch_bench -o report.md
```

This writes the report file in addition to the normal
console output.
