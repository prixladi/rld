# Rld

## librld

**Librld** is the core of the rld project; it is distributed as a shared object and a collection of headers. It is more of a framework than a library because it takes over the entire application process, and you need to provide implementation for the used functions.

## rld script

Rld script is a bash script that can be installed separately from **librld** and is used for bootstrapping and running rld in your project.

## Installation

**Librld** can only be installed on linux.

**Requirements**

- [make](https://www.gnu.org/software/make/) - to orchestrate the installation process
- [gcc](https://gcc.gnu.org/) - to compile the **librld** and is also needed when using rld script to compile user application

To install the **librld** run `make install_lib`, this will compile librld (`/librld.so`) and copy it into `/usr/lib64`. And also copies header files into `/usr/local/include/rld/*`.

To install the **rld script** run `make install_script`, this will copy `rld.sh` to /usr/local/bin as `rld`.

To install both **librld** and **rld script** run `make install`.

## Usage

You can run `rld help` to print the rld script help.

You can see multiple examples in the [example folder.](/examples/)

Bear in mind that you need to have rld installed for help examples to work.
