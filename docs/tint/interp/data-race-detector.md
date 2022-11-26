# Data race detector

This document describes a dynamic data race detector that uses the interpreter.

## Implementation

The data race detector (DRD) registers callbacks to be notified about non-atomic
memory accesses, barriers, and workgroup begin/completion events.

During the execution of a single Invocation, the DRD tracks every address within
a workgroup or storage buffer allocation that is accessed, recording the source
location of the access and whether it is a load or a store. Access tracking has
a granularity of bytes.

When workgroup synchronization occurs (either at a barrier or at workgroup
completion), the access lists of every invocation in that workgroup are merged
into a new list of per-workgroup accesses for the memory scope that is being
synchronized. During the merge, a data race is recorded for conflicting
addresses where at least one access is a store. After merging, the accesses in
the merged list are either discarded (for workgroup memory), or retained (for
storage buffers). The per-invocation access lists are then cleared.

When a workgroup completes, the per-workgroup list of storage buffer accesses is
merged into a global storage buffer access list in order to check for
inter-group data races. At this point, the DRD emits any races that have been
recorded so far. Races are de-duplicated such that only one race for a given
pair of source locations is reported.

Stores to a single component of a vector are special-cased such that they are
considered to write to the whole vector.
