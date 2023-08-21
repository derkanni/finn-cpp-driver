#include <concepts>
#include <experimental/random>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>

// Helper
#include "utils/driver.h"
#include "utils/finn_types/datatype.hpp"
#include "utils/mdspan.h"

// Created by FINN during compilation
#include "template_driver.hpp"

// XRT
#include "experimental/xrt_ip.h"
#include "xrt.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

using std::string;

// Because Datatype is now implemented using CRTP, an additional template parameter U is needed.
template<typename T, typename U, IsDatatype<U> D = Datatype<U>>  // This typeparameter should usually be a pointer, which was returned by xrt::bo.map<>()
void fillBufferMapRandomized(T& map, size_t& size, D& datatype) {
    // TODO(bwintermann): Need ability to differentiate between float and int!
    // FIXME: Datatype is an abstract class

    // Integer values
    for (unsigned int i = 0; i < size; ++i) {
        map[i] = std::experimental::randint(datatype.min(), datatype.max());
    }
}


// Create buffers, one buffer per given shape, and bytewidth
std::vector<xrt::bo> createIOBuffers(const xrt::device& device, const std::initializer_list<unsigned int>& widths, const std::initializer_list<std::initializer_list<unsigned int>>& shape) {
    std::vector<xrt::bo> buffers = {};
    for (unsigned int i = 0; i < widths.size(); ++i) {
        auto elements = static_cast<unsigned int>(std::accumulate(std::begin(shape.begin()[i]), std::end(shape.begin()[i]), 1, std::multiplies<>()));
        buffers.emplace_back(xrt::bo(device, static_cast<size_t>(widths.begin()[i] * elements), xrt::bo::flags::cacheable, 1));  // TODO(bwintermann): Correct memory group setting missing, assuming 1 here
    }
    return buffers;
}


// Create mappings of the given datatype for the given buffers
template<typename T>
std::vector<MemoryMap<T>> createMemoryMaps(std::vector<xrt::bo>& buffers, std::initializer_list<std::initializer_list<unsigned int>>& shapes, SHAPE_TYPE shapeType) {
    std::vector<MemoryMap<T>> maps = {};
    unsigned int index = 0;
    for (xrt::bo& buffer : buffers) {
        MemoryMap<T> memmap = {buffer.map<T*>(), buffer.size(), shapes.begin()[index], shapeType};
        maps.emplace_back(memmap);
        index++;
    }
    return maps;
}

template<typename T>
decltype(auto) createTensorFromMap(MemoryMap<T>& mmap, std::initializer_list<unsigned int>& dimensions) {
    auto expectedElements = static_cast<unsigned int>(std::accumulate(std::begin(dimensions), std::end(dimensions), 1, std::multiplies<>()));
    if (expectedElements != mmap.size) {
        throw std::length_error("During creation of Tensor (mdspan) from a xrt::bo map: Map has size " + std::to_string(mmap.size) + " but the dimensions array fits " + std::to_string(expectedElements) + " elements!");
    }
    return makeMDSpan(mmap.map, dimensions);
}


int main() {
    // TODO(bwintermann): Make into command line arguments
    int deviceIndex = 0;
    string binaryFile = "finn_accel.xclbin";
    DRIVER_MODE driverExecutionMode = DRIVER_MODE::THROUGHPUT_TEST;

    xrt::device device = xrt::device(deviceIndex);
    xrt::uuid uuid = device.load_xclbin(binaryFile);

    // TODO(bwintermann): Replace palceholder kernel name
    xrt::ip kernelIp = xrt::ip(device, uuid, "PLACEHOLDER_KERNEL_NAME");

    if (TRANSFER_MODE == "memory_buffered") {
        std::vector<xrt::bo> inputBuffers = createIOBuffers(device, INPUT_BYTEWIDTH, ISHAPE_PACKED);
        std::vector<xrt::bo> outputBuffers = createIOBuffers(device, OUTPUT_BYTEWIDTH, OSHAPE_PACKED);


        // TODO(bwintermann): Depends on datatypes!
        std::vector<MemoryMap<int>> inputBufferMaps = createMemoryMaps<int>(inputBuffers, ISHAPE_PACKED, SHAPE_TYPE::PACKED);
        std::vector<MemoryMap<int>> outputBufferMaps = createMemoryMaps<int>(outputBuffers, OSHAPE_PACKED, SHAPE_TYPE::PACKED);


        // Using tensor as a name for an mdspan
        // TODO(bwintermann): Depends on datatypes!


    } else if (TRANSFER_MODE == "stream") {
        // TODO(bwintermann): Add stream implementation
    } else {
        throw std::runtime_error("Unknown transfer mode (" + std::string(TRANSFER_MODE) + "). Please specify a known one in the DataflowBuildConfig!");
    }

    // Test execution
    if (driverExecutionMode == DRIVER_MODE::THROUGHPUT_TEST) {
        // generate random data
    } else if (driverExecutionMode == DRIVER_MODE::EXECUTE) {
        // read from npy / npz file
    }

    return 0;
}
