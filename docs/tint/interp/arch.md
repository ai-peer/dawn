# Tint interpreter architecture

This document describes that core components of the WGSL interpreter.

## Invocation

The `Invocation` class holds the state for a single Invocation in the shader
dispatch, and contains the logic for emulating execution of statements and
expressions.

Invocations are stepped one expression at a time, using `Step()`. Each evaluated
expression produces an `ExprResult` object, which contains either a value or a
memory view. The Invocation maintains a queue of pending expressions for the
current statement in evaluation order. When preparing to execute a new
statement, it populates this queue with a recursive depth-first traversal of the
statement's expressions, and stores a lambda that will be used to execute the
statement itself once all expressions have been evaluated.

Expression results of value type are represented using Tint's `constant::Value`
class. This allows the interpreter to use Tint's `ConstEval` logic to implement
expression evaluation for unary ops, binary ops, and most builtin functions.
Expression results of pointer or reference type are represented using a
`MemoryView` object (see [Memory](#memory) below).

## Workgroup

The `Workgroup` class holds a set of Invocations for a single workgroup, and
allocates memory for any global variables declared with the `workgroup` address
space. It exposes a `Step()` method to step the current invocation.

Invocations are stored in one of three sets to capture their current state:
`ready`, `barrier`, or `finished` (implicit). When the currently executing
invocation reaches a workgroup barrier, it is moved to the `barrier` set and the
next invocation in the `ready` set is selected. If `ready` is empty and
`barrier` is not empty, invocations in `barrier` are copied to `ready` and
execution resumes. If both `ready` and `barrier` are empty, then the workgroup
has completed.

## Memory

The `Memory` class represents a memory allocation in the interpreter. A memory
allocation is an array of bytes with a particular size, but with no type or
address space. Allocations are created in a few different scenarios:
- Invocations allocate memory for module-scope `var<private>` declarations.
- Invocations allocate memory for function-scope `var<function>` declarations.
- Workgroups allocate memory for module-scope `var<workgroup>` declarations.
- External users of the interpreter allocate memory for buffer objects.

The `MemoryView` class represents a view into a memory allocation from with a
shader. It has a type (`type::Type`), address space, size, and an offset into
the memory allocation. It provides a `Load()` method which reads the contents
of the view and returns a `constant::Value` object, and a `Store()` method that
writes a `constant::Value` to the view.

Each `var` declaration in the shader has an associated `MemoryView` (the
"root memory view"), which has a size equal to that of the underlying allocation
and an offset of zero. Array index accessor and member accessor expressions on
reference types create sub-views with different types, and (potentially) smaller
sizes and non-zero offsets.

A `MemoryView` object can be created with a `valid` flag set to `false`. Any
attempt to dynamically access such a view will generate an out-of-bounds
diagnostic message.

## ShaderExecutor

The `ShaderExecutor` class handles the execution of a compute shader dispatch
command. It holds the `ProgramBuilder`, `ConstEval`, and `IntrinsicTable`
objects that are used during execution. It deals with evaluating
pipeline-overridable constants and creating `MemoryView` objects for each
binding resource.

The `ShaderExecutor` keeps track of the [`Workgroup`](#workgroup) that is
currently executing. At the start of the dispatch, it creates a list of pending
workgroup IDs. When a workgroup finishes executing, the next pending ID is taken
from the list and a new `Workgroup` is created for it. The `ShaderExecutor`
provides a method to switch to a new `Workgroup` on demand and pause the current
`Workgroup`, by re-inserting the current group into the pending ID list along
with the `Workgroup` object that represents it.

The `ShaderExecutor` object allows tools to register callback functions to
receive notifications of various events that occur during execution. The
[data race detector](data-race-detector.md) and the interactive debugger use
these callbacks. The following events have callbacks associated with them:
- Barrier
- DispatchBegin
- DispatchComplete
- Error
- MemoryLoad
- MemoryStore
- PostStep
- PreStep
- WorkgroupBegin
- WorkgroupComplete
