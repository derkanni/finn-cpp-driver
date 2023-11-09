# A C/C++ Driver for FINN generated accelerators

[![C++ Standard](https://img.shields.io/badge/C++_Standard-C%2B%2B20-blue.svg?style=flat&logo=c%2B%2B)](https://isocpp.org/)
[![GitHub license](https://img.shields.io/badge/license-MIT-blueviolet.svg)](LICENSE)

## Getting Started

* Install XRT Runtime and development packages [Download](https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/alveo/u280.html)
* If you use a non default install directory for XRT (default: ```/opt/xilinx/xrt```) set the XRT env var to your XRT development package installation ```export XILINX_XRT=<path>```.

```bash
git clone git@github.com:eki-project/finn-cpp-driver.git
(git checkout <branch>)
./buildDependencies.sh
mkdir build && cd build
cmake ..
make -j $(nprocs)
```

## FINN Pipeline

Doing the build like shown above will produce a build with placeholder configurations. To actually use the driver it has to be compiled for the network it should use.
During the FINN pipeline (or of course it can also be done manually afterwards), the project has to be compiled.
The driver requires two types of configurations:

* A header file which defines the type of the driver. The driver depends on which FINN-datatypes are used in the network for input and output, and can run a little faster by optimizing this at compile time
* A configuration file in the JSON format. This specifies the path to the .xclbin produced by FINN, the devices by their XRT device indices, their inputs and outputs and how they are shaped, which is needed for the folding and packing operations. Note that this is passed during the _runtime_! This means that the location of the xclbin for example does not need to be fixed.

If you ever need help on which arguments the driver requires, simply use the ```--help``` flag on the driver.

```console
$ ./finn --help
  -h [ --help ]                  Display help
  -m [ --mode ] arg (=test)      Mode of execution (file or test)
  -c [ --configpath ] arg        Path to the config.json file emitted by the 
                                 FINN compiler
  -i [ --input ] arg             Path to the input file. Only required if mode 
                                 is set to "file"
  -b [ --buffersize ] arg (=100) How large (in samples) the host buffer is 
                                 supposed to be
```

The header file location be passed by letting cmake set the macro

```bash
FINN_HEADER_LOCATION
```

If left undefined, the path will be ```config/FinnDriverUsedDatatypes.h``` (as included from ```./src/FINNDriver.cpp```).

For unittest, the used configuration (meaning the runtime-JSON-config) can also be changed (because when running unittests, the JSON is actually needed at compile time). This can be done by setting

```bash
FINN_CUSTOM_UNITTEST_CONFIG
```

If left undefined, the path will be ```../../src/config/exampleConfig.json``` (as included from ```./unittests/core/UnittestConfig.h```).

## Manual Building

### Getting Started on the N2 Cluster

You will first have to load a few dependencies before being able to build the project:

```bash
ml fpga
ml xilinx/xrt/2.14
ml devel/Doxygen/1.9.5-GCCcore-12.2.0
ml compiler/GCC/12.2.0
ml devel/CMake/3.24.3-GCCcore-12.2.0
```

To execute the driver on the boards, write a job script. The job script should look something like this:

```bash
#!/bin/bash
#SBATCH -t 0:07:00
#SBATCH -A hpc-prf-ekiapp
#SBATCH -p fpga
#SBATCH --gres=fpga:u280:3
#SBATCH -o cpp-finn_out_%j.out
#SBATCH --constraint=xilinx_u280_xrt2.14

ml fpga &> /dev/null
ml xilinx/xrt/2.14 &> /dev/null
ml lang/Python/3.10.4-GCCcore-11.3.0-bare &> /dev/null
ml devel/Boost/1.81.0-GCC-12.2.0
ml compiler/GCC/12.2.0

./finn
```

Execute it with ```sbatch write.sh```.
Use ```xbutil``` to get information about the cards and configure them manually if necessary.

(Project name, resource usage, output filename, xrt version etc. are all examples and to bet set by the user themselves).

### Setup

```bash
./buildDependencies.sh
export LD_LIBRARY_PATH="$(pwd)/deps/finn_boost/stage/lib/boost/:$LD_LIBRARY_PATH"
```

### Developer Documentation

If you want to contribute some changes to the C++ driver for FINN, please make sure you have the pre-commit git hook installed before you commit a pull-request.

It can be installed using the following command:

```shell
./scripts/install_precommit.sh
```

Without this precommit hook it is very likely that your code can not be merged, because it is blocked by our linter.

## TODO

* Check if XRT frees the memory map itself

## Structure

```none
Driver (bjarne)
    Accelerator (linus)
        DeviceHandler<uint8>[] OR DeviceHandler<InputType, OutputType>[] (linus)
            XCLBIN-File
            Name
            DeviceIndex
            xrt::kernel[]
            DeviceBuffer[]<Type> (bjarne)
                xrt::bo[]
                bo-map*
                sizes
                Boost CircularBuffer
                Boost CircularBuffer Methoden
```

### Driver

```cpp
entrypoint?()
```

### DeviceHandler

```cpp
DeviceHandler()
initializeDevice()
initializerDeviceBuffers()
getDeviceBuffer()
(getDeviceBufferInputOutputPair())
syncBuffers() (vlt. mehrere)
```

### DeviceBuffer

(only ever write from ringBuffer, never manually!)

```cpp
Flag: IS_INPUT_OR_OUTPUT
(Iterator für BOMap)
fillBOMapRandom()
sync()
loadFromRingBuffer() (operator overloading?)
loadToRingBuffer() (operator overloading?)
[all RingBuffer convenience functions]
getSizes()
isEmpty()
isFull()
clear()
```

## Filetree

* Filenames after class names, with UpperCamelCase
* .h no template, .hpp templated
* Split every non-template files into header and cpp file
* utils.hpp only contains items that usable everywhere

* utils: Utility functions and objects
* utils - types.h: Enums and usings

```none
src
    FinnDriverUsercode.cpp
    config
        Config.h
        protobuf_generated_code...
    core
        DeviceHandler.hpp
        DeviceBuffer.hpp
        Accelerator.hpp
        Driver.hpp
    utils
        FinnDatatypes.hpp
        Types.h
        Mdspan.h
```
