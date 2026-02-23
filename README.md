# Rld

Rld is a c library and a bash script providing generic utils for creating repeatable actions that happen on file system change. Such as compiling code and running a binary on file change.

## librld

**Librld** is the core of the rld project; it is distributed as a shared object and a collection of headers. It is more of a framework than a library because it takes over the entire application process, and you need to provide implementation for the used functions.

## rld script

Rld script is a bash script that can be installed separately from **librld** and is used for bootstrapping and running rld in your project.

## Installation

**Librld** can only be used on linux.

**Requirements**

- [make](https://www.gnu.org/software/make/) - to orchestrate the installation process
- [gcc](https://gcc.gnu.org/) - to compile the **librld** and is also needed when using rld script to compile user application

To install the **librld** run `make install_lib`, this will compile librld (`/librld.so`) and copy it into `/usr/lib64`. And also copies header files into `/usr/local/include/rld/*`.

To install the **rld script** run `make install_script`, this will copy `rld.sh` to /usr/local/bin as `rld`.

To install both **librld** and **rld script** run `make install`.

## Usage

You can run `rld help` to print the rld script help.

After running rld init in your work folder it creates a `.rld` folder  where file `.rld/main.c` is a template for your custom logic with functions that you need to implement. For each of these functions, there is a brief documentation, you can also check these functions in [rld script](/rld.sh) or you can see multiple examples in the [example folder.](/examples/)

Bear in mind that you need to have rld installed for help examples to work.
