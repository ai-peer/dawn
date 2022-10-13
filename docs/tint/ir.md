# Intermediate Representation

Tint has introduced an intermediate representation in order to make
transforming the input program simpler and more efficient. When mutating
based on the current AST we've run into issues with complexity, the AST
has specific rigid structure and performance, needing to re-create the
program and AST nodes with each transform.

The IR aims to help both those issues. The IR is not immuatable. The
transforms are free to mutate the existing IR as needed in order to
achieve the needed results. The IR is also less rigid in the
allowable representations. This means a single expression could be
pointed to as part of multiple statements.

The AST is transformable into IR, and IR is transformable into AST.
This allows for a few benefits.

1. SPIR-V Reader could emit IR and have that transform back into AST
   later.
2. Transforms can be converted piecemeal, going to and from AST as
   needed.
3. Easier to debug by emitting WGSL then the IR directly.

## SSA
Note the IR is _not_ SSA. In most of our current backends (HLSL, MSL,
GLSL, WGSL) the emitted form is not SSA. Only SPIR-V emits an SSA form.
This would mean the IR would convert to SSA and then be force to convert
back to a non-SSA form for most of our backends.

## AST Simplifications in the IR.

### DAG structure
Where the AST is a tree, the IR is a Directed Acyclic Graph (DAG).

### Mutability
The IR is mutable. The transforms will directly modify the IR nodes as
needed during transformation.

### Names
Variables are not named. This removes any issues to do with shadow
variables. We track the variable node, not the name of the variable.

### Looping constructs
There is only `loop/continuing` form. The `for` and `while` loops are
both converted down to `loop/continuing`

### Short-form arithmetic
There is no short form arithmetic. So, `i += 1` and `i++` would both
convert into `i = i + 1`.

### Short-circuiting expressions
Short-circuiting expressions are replaced by the equivalent `if`
structure. So,

```groovy
let c = a() && b()
```
becomes
```groovy
let tmp c = a(); if (c) { c = b(); }
```

### Type information
The IR is much more explicit about types then the AST. Instead of a
simple `a * b` the IR would have a FMul operation which is used.

### Loads/Stores/Deref
The required loads, stores, drefs and address-of calls will all be
explicitly listed in the IR.

### if/else-if/else
`if/else-if/else` chains are converted into `if/else` chains. Each
`else-if` portion becomes another `if/else` inside the associated
`else` statement.

## Preservation of programmer intent
As code moves through the AST -> IR and back again there will be a loss
of programmer intent (`for` converting to `loop`, etc). This will
make the resulting WGSL program harder to read and "further" from the
source WGSL. As a first pass, the IR does not worry about this
preservation. In the future there may be work done to improve
readability by:

- Preserving original variable names where possible.
- Folding single use `let` declarations where possible.
- Determine what type of loop should be emitted (`loop`, `for`, `while`)
  based on the loop body, initializer, conditional and continuing blocks.
  The originally declared loop style may also want to be taken into
  consideration.
- Determine how to structure `else-if` chains so they are not deeply
  nested.

## Steps

### Add basic IR nodes
Create a `src/tint/ir` folder and start populating with IR nodes. The IR
will exist in the `tint::ir` namespace.

### Create an `IRBuilder`
Similar to the `ProgramBuilder` for `Program` the `IRBuilder` provides
helpers for generating IR.

### Add `tint::transform::AST` and `tint::transform::IR` interfaces

`tint::transform::AST` and `tint::transform::IR` would both derive from the
existing `tint::transform::Transform` interface.

All existing transforms would derive from `tint::transform::AST` instead of
`tint::transform::Transform` as they do now.

The new `tint::transform::IR` interface will be used for transforms that mutate
programs in the intermediate representation.

### Transform migration
On a transform-by-transform basis decide which should work on IR instead
of AST and convert over to the new form. Consideration should be given
in order to have chains of transforms in the same level (so, several AST
transforms and then several IR transforms). This is to avoid the cost of
converting from AST-IR-AST multiple times.

### Convert writers
The writers will be updated to emit from the IR instead of the AST.
(Although, possibly the WGSL writer will continue to emit from the AST).

- Hopefully at this point, the transforms which generate backend
  specific information will be converted to IR. Then the resolver can be
  cleaned up to not require the Internal Attributes which are currently
  being used to allow backend specific AST.

### Convert SPIR-V Reader
It maybe worthwhile to have the SPIR-V reader emit to IR instead of to
AST as it may be closer to the SPIR-V representation.

