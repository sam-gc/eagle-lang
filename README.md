# Eagle
### A compiled language that is halfway between C and Go/Swift/Rust

Eagle is under active and heavy development; old code may break, but the programs in the `example` folder
will always compile.

### Pre-Built Binaries

Pre-built binaries for Linux and Mac are made available through the releases
section of this repository.

### Some Quick Examples

 * **Simple `hello world`**

```swift
func main()
{
    puts 'Hello, world!'
}
```

 * **Generators and loops**

```swift
gen range(int max) : int
{
    for int i = 0; i < max; i += 1
    {
        yield i
    }
}

func main()
{
    for int i in range(10)
    {
        puts i
    }
}
```
 * **Reference counting**

```swift
func main()
{
    var i = new int(5) -- Allocate new integer
    puts i! -- Dereference the pointer
} -- Memory automatically freed
```

 * **Closures**

```swift
func main()
{
    var i = 0
    var closure = func() {
        i += 2
    }

    closure()
    closure()

    puts i -- Prints '4'
}
```

### Project Status

| Working | Incomplete | Buggy | Not Working |
| ------- | ----------- |----|----|
| Code compilation | 32-bit code generation | Reference counting in class contexts | Nested geneerators |
| Basic reference counting | Static variables | Counted variables in loops | Reference counting in generators |
| Multiple file compilation | Imports and exports | | Debug information |
| Control flow (if/elif/else, for, etc) | Type system |
| Optimization (with levels) | C-ABI compatibility (no passing `structs` by value) |
| Functions, closures, generators | Errors and warnings |
| Object orientation (classes, interfaces, views) |
| Linking, both other object files and libraries |
| Type inference |
| Semicolon injection |

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

### Command Line switches
| Argument | Action |
|---------|--------|
| `[filename].egl` | File will be compiled |
| `-o [output name]` | Specify binary output |
| `-c` | Generate object files from inputs |
| `-S` | Generate assembly code from inputs |
| `-O[0-3]` | Specify optimization level (default 2) |
| `--no-rc` | Do not include reference counting headers |
| `--code [extra eagle code]` | Specify extra code to compile from command line |
| `-l[libname]` | Link external library |
| `--llvm` | Dump llvm bitcode |
| `--verbose` | Provide details of compilation process |
| `--threads [thread-count]` | Specify number of threads to use during code generation |
