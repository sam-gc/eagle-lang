# Eagle
### A compiled language that is halfway between C and Go/Swift/Rust

Eagle is under active and heavy development; old code may break, but the programs in the `example` folder
will always compile.

#### Principles
 * Low level—no kid gloves
 * Opt-in reference counting
 * (Basic) opt-in object-oriented functionality
 * High level features
 * Modern syntax

### Compiling
#### Prerequisites
You will need a development version of LLVM (i.e. the LLVM headers and associated helper programs
like `llvm-config`). The process of getting LLVM will depend on your particular system.

You will also need a C compiler, a C++ compiler, make, and GNU Bison and Flex.

#### Build
 * `cd` to build folder
 * Run `make guts` to generate C files and header files from the eagle.l and eagle.y grammar files
 * Run `make` to compile the source

At this point you will have a binary called `eagle` which will take as its sole input the name of a
code file for which to generate LLVM IR.

### Running
As of the current version, the compiler does not create machine code itself; it merely generates IR.
To get a working program, you will need the linked functions in rc.egl/rc.c. To make a binary from
the examples folder, simply type `make <name-of-file>`, omitting the `.egl` suffix. For example,
you might run `make class-linked-list` which would generate a binary named `class-linked-list.e` in
the current directory.

Alternatively all the examples can be built using `make all-examples` and can be removed using
`make clean-examples`.
