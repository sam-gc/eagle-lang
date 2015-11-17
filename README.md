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
The latest version of the compiler will create an executable file from one input file. When compiling,
it implicitly links a file called `rc.o` that it expects in the running directory. To create `rc.o`,
first run `./eagle rc.egl --compile-rc -o rc.o`. At this point you can compile any code files. Simply
run `./eagle <name-of-file.egl> [-o <executable name>]`.

If you are using the makefile associated with the project, there is a shortcut. Any code file in the
`examples` folder can be compiled using `make <name-of-file>` omitting the `.egl` suffix. This command
will create `rc.o` if it doesn't already exist and will then procede to create an executable.
Alternatively you can run `make all-examples` to compile every example in the examples folder. The
resulting binaries will be put in a folder called `builtex`. All of the built examples can be removed
using `make clean-examples`.
