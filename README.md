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
You will need a development version of LLVM with version number >= 3.5 (i.e. the LLVM headers
and associated helper programs
like `llvm-config`). The process of getting LLVM will depend on your particular system.

You will also need a C compiler, a C++ compiler, make, and GNU Bison and Flex. LLVM also requires
libzip, curses, and pthreads.

The compiler should compile under Linux and OS X. OS X Mavericks, Arch Linux, and Ubuntu have
all been tested. Please note that, under Ubuntu installations (and likely other Linux systems),
there may be multiple versions of LLVM in the repositories. Make sure to choose the newest one
possible; the default will likely be relatively old and therefore unusable.

#### Build
As of the current versions of the compiler, users should use the standard UNIX build incantation:
 * `cd` to build folder
 * Run `./configure [--enable-debug]`
 * Run `make`
There is currently no `make install` ... the compiler is far too unstable to think about installing
it in the standard system. I have merely added the repository directory to my `PATH`.

At this point you will have a binary called `eagle` which will take code files as input.

### Running
The latest version of the compiler can accept multiple input files. There are a variety of command
line switches, which generally mirror the standard GNU `gcc` switches. The compiler currently supports
exporting executables, object files (through `-c`), and assembly files. All output has an optional
optimization pass, defined through `-O0 ... -O3`. Only files ending in ".egl" will be
compiled. 

The reference counting runtime support code is built in
to the compiler (the code is copied from `rc.egl` at configure time). Thus there is no need to carry
around that file or any object files (as in previous versions). To omit the resource counted module
headers, use the command switch `--no-rc`. Output filename can be chosen with `-o [filename]`.
See `src/core/main.c` to see a full listing of available commands.

If you are using the makefile associated with the project, there is a shortcut for building examples.
Any code file in the
`examples` folder can be compiled using `make <name-of-file>` omitting the `.egl` suffix.
Alternatively you can run `make all-examples` to compile every example in the examples folder. The
resulting binaries will be put in a folder called `builtex`. All of the built examples can be removed
using `make clean-examples`. The more complex "Boggle" example has its own makefile which will work if
the `eagle` executable exists in the main project directory.
