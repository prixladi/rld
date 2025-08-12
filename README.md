# Rld

## Parts

### librld

Librld is the core of the rld project; it is distributed as a shared object and a collection of headers. It is more of a framework than a library because it takes over the entire application process, and you need to provide implementation for the used functions.

### rld script

Rld script is a bash script that can be installed separately from librld and is used for bootstrapping and running rld in your project.

## Usage

You can run `rld help` to print the rld script help.

You can see multiple examples in the [example folder.](/examples/)

Bear in mind that you need to have rld installed for help examples to work.
