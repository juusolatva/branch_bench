# branch bench

A simple benchmark for testing out gains from branch prediction or conversely losses from mispredictions.

## Notes

You **MUST** run this with ./run.sh or use the flags *-fno-tree-vectorize* *-fno-if-conversion* when compiling or the compiler will optimize the mispredictions away.
