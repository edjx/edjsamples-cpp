<!--
title: .'EDJX Serverless C++ Samples'
description: 'Different detailed serverless example functions in C++ to get started with Serverless@EDJX'
platform: EDJX
language: C++
-->

# edjsamples-cpp

This repository contains EDJX's sample serverless functions written in C++.
Each sample also contains a README, which has a detailed explanation about
the specifics of the Serverless Function. These sample functions can be
deployed to EDJX network using EDJX CLI. More details on how to deploy via
EDJX CLI can be found [in the EDJX Documentation](https://docs.edjx.io/docs/latest/how_tos/cli_build_wasm_file.html).

It is also possible to build C++ applications into WASM executables manually
using the following steps.

## Clone This Repository

Clone the this repository with all example code:

    git clone https://github.com/edjx/edjsamples-cpp.git

## Directory Structure

Each subdirectory in this repository corresponds to a different application
example. Each application example contains these subdirectories:

- `bin/` &mdash; Compiled WASM executable of the example application
(generated during the compilation)
- `build/` &mdash; Intermediate object files of the example application
(generated during the compilation)
- `src/` &mdash; Source code of the example application

## Prepare a Build Environment

In order to build an application, the [EDJX C++ SDK](https://github.com/edjx/edjx-cpp-sdk)
and the [WASI C++ SDK](https://github.com/WebAssembly/wasi-sdk) need to be
installed.

### Install the EDJX C++ SDK

Download the SDK to `/opt/edjx-cpp-sdk`:

    git clone --branch wasi-12 --depth 1 https://github.com/edjx/edjx-cpp-sdk.git /opt/edjx-cpp-sdk

The `wasi-12` branch contains a build of the EDJX C++ SDK that was built
with the WASI SDK version 12.

It is possible to install the SDK to an arbitrary location. In that case,
update the `INCLUDE_DIR` and `LIB_DIR` variables in the `Makefile`
to point to `edjx-cpp-sdk/include` and `edjx-cpp-sdk/lib` in the custom
location path.

### Install the WASI C++ SDK

WASI SDK ([github.com/WebAssembly/wasi-sdk](https://github.com/WebAssembly/wasi-sdk))
provides the necessary C and C++ standard library functions.

Install the WASI SDK to `/opt/wasi-sdk/` (so that the path to the bundled
compiler is `/opt/wasi-sdk/bin/clang++`). Use the same WASI
SDK version that was used to build the EDJX C++ SDK library (e.g., use
`wasi-sdk-12` when using the `wasi-12` version of the EDJX C++ SDK).

    wget https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-12/wasi-sdk-12.0-linux.tar.gz
    tar -xvf wasi-sdk-12.0-linux.tar.gz -C /opt

It is possible to install the WASI SDK to
a different location and then manually update the `WASI_SDK_PATH` variable in
the Makefile. It is also possible to specify a different compiler than
the one provided by the WASI SDK by editing the `CC` variable in the Makefile.
However, to avoid any compatibility issues, we recommend using the clang
compiler that comes bundled with the WASI SDK.

## Build the Example Application

Once the EDJX C++ SDK and the correct version of the WASI C++ SDK are installed,
the example application can be compiled by running `make` inside the directory
of each application:

    cd <application>
    make all

The resulting WASM file will be created in `<application>/bin/<app>.wasm`.

To remove files created during the compilation, run:

    make clean

## A Note About WASI Imports

EdjExecutor doesn't provide implementation of functions that the
WASI standard library imports. System functions like `fd_open`, `fd_read`,
`fd_write`, `fd_seek`, `fd_close`, `proc_exit`, `environ_get`,
`environ_sizes_get`, and possibly others, are not implemented and won't work.
The file `/share/wasi-sysroot/lib/wasm32-wasi/libc.imports` in the WASI SDK
installation directory has a list of symbols that are imported by the wasi-libc.
