/**
 * @file deviceHandler.hpp
 * This file contains the DeviceHandler class. When doing FINN inference, one device handler per FPGA is required (Multi-FPGA coming soon). The core of the class is made up of several associated lists.
 * 
 */

#ifndef DEVICE_HANDLER_H
#define DEVICE_HANDLER_H

// Default
#include <string>
#include <vector>
#include <variant>

// Helpers
#include "memoryMap.h"
#include "finnUtils.h"
#include "finn_types/datatype.hpp"

// XRT
#include "experimental/xrt_ip.h"
#include "xrt.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

// Logger
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>


namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;


// TODO(bwintermann): Remove, only here because this type might change a few times over next few iterations
using Shape = std::initializer_list<unsigned int>;
using Shapes = std::initializer_list<Shape>;
using Bytewidths = std::initializer_list<unsigned int>;

/**
 * @brief Describes a list (initializer_list) of types Shape OR MemoryMap<T>. In the first case a buffer with the appropiate byte size is simply created, with the latter, an existing memory mapped is taken as input for chaining FPGAs together
 * 
 * @tparam T The datatype of the DeviceHandler 
 */
template<typename T>
using BOMemoryDefinitionArguments = std::initializer_list<std::variant<Shape, MemoryMap<T>>;


/**
 * @brief This class handles creating a device, it's kernel objects and holds relevant related variables and functions to interact with it, mainly acting as a FINN specific wrapper for xrt::bo and xrt::device objects
 *
 * @tparam T The predominant type of how data is input and output
 */
template<typename T>
class DeviceHandler {
    const std::string name;
    const bool isHelperDevice;
    const int deviceIndex;
    const std::string binaryFile;
    DRIVER_MODE driverMode;
    TRANSFER_MODE transferMode = TRANSFER_MODE::INVALID;

    xrt::device device;
    xrt::uuid uuid;
    std::vector<xrt::kernel> inputKernels;
    std::vector<xrt::kernel> outputKernels;

    std::vector<std::string> inputNames;
    std::vector<std::string> outputNames;

    std::vector<xrt::bo> inputBufferObjects;
    std::vector<xrt::bo> outputBufferObjects;
    std::vector<MemoryMap<T>> inputMemoryMaps;
    std::vector<MemoryMap<T>> outputMemoryMaps;

    src::severity_logger<logging::trivial::severity_level> logger;

     public:
    /**
     * @brief Construct a new Device Handler object. Requires the Bytewidths, dimensions and shape-types of all input and outputs. This class always handles multiple buffers, so everything is passed as a vector
     *
     * @param pName The name of the DeviceHandler for unique identification and logging purposes
     * @param pIsHelperDevice Specifies whether this handler is for a main device, which communicates with the host and is used in both Single- and Multi-FPGAs, or a helper device, which is part of a p2p network for Multi-FPGA applications
     * @param pDeviceIndex The device index, usually 0?
     * @param pBinaryFile The path to the .xclbin FINN file
     * @param pDriverMode The mode for which the driver is instantiated (memory_buffered or memory-less streaming)
     * @param pInputBytewidths How many bytes each buffer element requires
     * @param pOutputBytewidths How many bytes each buffer element requires
     * @param pInputMemoryDefinition For every buffer either specify a shape e.g. (1,2,12), which is expanded to 1*2*12 * bytewidth bytes memory, OR specify a pointer to an existing memory map to receive that one as input for multi-FPGA
     * @param pOutputMemoryDefinition Same as inputMemoryDefintion
     * @param pInputShapeType The type of shape supplied, is of type SHAPE_TYPE
     * @param pOutputShapeType The type of shape supplied, is of type SHAPE_TYPE
     * @param pInputNames Names for input buffers as well as names for the kernels
     * @param pOutputNames Names for output buffers as well as names for the kernels
     * @param ringBufferSizeFactor How many times larger the RingBuffer objects belonging to the xrt::bo objects should be (for efficient loading of data)
     * @param pLogger Boost Severity Logger passed from main
     */
    DeviceHandler(const std::string& pName, const bool pIsHelperDevice, const int pDeviceIndex, const std::string& pBinaryFile, const DRIVER_MODE pDriverMode, const Bytewidths& pInputBytewidths, const Bytewidths& pOutputBytewidths, const BOMemoryDefinitionArguments<T>& pInputMemoryDefinition, const BOMemoryDefinitionArguments<T>& pOutputMemoryDefinition,
                  const SHAPE_TYPE pInputShapeType, const SHAPE_TYPE pOutputShapeType, const std::initializer_list<std::string>& pInputNames, const std::initializer_list<std::string>& pOutputNames, const unsigned int ringBufferSizeFactor, src::severity_logger<logging::trivial::severity_level>& pLogger) : 
        name(pName),
        isHelperDevice(pIsHelperDevice),
        deviceIndex(pDeviceIndex),
        binaryFile(pBinaryFile),
        driverMode(pDriverMode),
        logger(pLogger) {
        if (!pIsHelperDevice) {
            BOOST_LOG_SEV(logger, logging::trivial::info) << "Initializing " << name << " as a host-communicating Single/Multi-FPGA device\n";
        } else {
            BOOST_LOG_SEV(logger, logging::trivial::info) << "Initializing " << name << " as a helper device for Multi-FPGA usage\n";
        }
        for (auto name : pInputNames) {
            inputNames.push_back(name);
        }
        for (auto name : pOutputNames) {
            outputNames.push_back(name);
        }
        initializeDevice(pInputNames, pOutputNames);
        initializeBufferObjects(pInputBytewidths, pInputMemoryDefinition, pOutputBytewidths, pOutputMemoryDefinition);
        initializeMemoryMaps(pInputMemoryDefinition, pInputShapeType, pOutputMemoryDefinition, pOutputShapeType, ringBufferSizeFactor);
    }

    /**
     * @brief Initializes and fill the class members variables for the XRT device, its UUID, and initializes all kernels
     *
     * @param inputKernelNames List of names of kernels
     * @param outputKernelNames List of names of kernels
     */
    void initializeDevice(const std::initializer_list<std::string> inputKernelNames, const std::initializer_list<std::string> outputKernelNames) {
        BOOST_LOG_SEV(logger, logging::trivial::info) << "(" << name << ") " << "Initializing xrt::device, loading xclbin and assigning IP\n";
        device = xrt::device(deviceIndex);
        uuid = device.load_xclbin(binaryFile);
        for (auto kname : inputKernelNames) {
            inputKernels.push_back(xrt::kernel(device, uuid, kname));
        }
        for (auto kname : outputKernelNames) {
            outputKernels.push_back(xrt::kernel(device, uuid, kname));
        }
    }

    /**
     * @brief Create IO XRT BufferObjects from the given bytewidths and dimensions. If this is a device that is directly communicating with the host (main device), the buffer is marked as cachable. If it is a helper device in a Multi-FPGA group, the buffers are marked as p2p, to enable sending between multiple boards. 
     *
     * @param inBytewidths
     * @param inputMemoryDefinitions
     * @param outBytewidths
     * @param outputMemoryDefinitions
     */
    void initializeBufferObjects(const Bytewidths& inBytewidths, const BOMemoryDefinitionArguments<T>& inputMemoryDefinitions, const Bytewidths& outBytewidths, const BOMemoryDefinitionArguments<T>& outputMemoryDefinitions) {
        BOOST_LOG_SEV(logger, logging::trivial::info) << "(" << name << ") " << "Initializing xrt::bo objects for input and output";
        xrt::bo::flags boFlag = (!isHelperDevice) ? xrt::bo::flags::cacheable : xrt::bo::flags::p2p;
        inputBufferObjects = createIOBuffers(device, inBytewidths, inputMemoryDefinitions);
        outputBufferObjects = createIOBuffers(device, outBytewidths, outputMemoryDefinitions);
    }

    /**
     * @brief Create memory maps the template type given by the device handler class.
     *
     * @param inputMemoryDefinitions
     * @param inputShapeType 
     * @param outputMemoryDefinitions
     * @param outputShapeType
     */
    void initializeMemoryMaps(const BOMemoryDefinitionArguments<T>& inputMemoryDefinitions, const SHAPE_TYPE inputShapeType, const BOMemoryDefinitionArguments<T>& outputMemoryDefinitions, const SHAPE_TYPE outputShapeType, const unsigned int ringBufferSizeFactor) {
        BOOST_LOG_SEV(logger, logging::trivial::info) << "(" << name << ") " << "Creating memory maps";
        inputMemoryMaps = createMemoryMaps(inputBufferObjects, inputMemoryDefinitions, inputShapeType, ringBufferSizeFactor);
        outputMemoryMaps = createMemoryMaps(outputBufferObjects, outputMemoryDefinitions, outputShapeType, ringBufferSizeFactor);
    }

    // Create buffers, one buffer per given shape, and bytewidth
    /**
     * @brief Create a vector of XRT buffer objects to interact with the fpga. Errors if the number of passed bitwidths does not match the number of shapes given.
     *
     * @param device The XRT device to map the buffer to
     * @param boFlag The buffer flag passed. Most of the times use cacheable
     * @param widths The bitwidths of the buffers datatypes
     * @param memoryDefinitions List of variants, which can either contain a Shape type or a MemoryMap<T> type
     * @return std::vector<xrt::bo>
     */
    std::vector<xrt::bo> createIOBuffers(const xrt::device& device, const xrt::bo::flags boFlag, const Bytewidths& widths, const BOMemoryDefinitionArguments<T>& memoryDefinitions) {
        if (widths.size() != memoryDefinitions.size()) {
            std::string err = "The number of bytewidths passed does not match the number of shape lists passed (" + std::to_string(widths.size()) + ", " + std::to_string(shape.size()) + ")!";
            BOOST_LOG_SEV(logger, logging::trivial::error) << "(" << name << ") " << err;
            throw std::length_error(err);
        }

        // TODO(bwintermann): Enable possibility to specify a buffer/map instead of the size of the buffer to chain Board1.Output -> Board2.Input, etc.
        // FIXME, TODO(bwintermann): Differentiate between BITwidth and BYTEwidth (which gets passed by FINN?)
        std::vector<xrt::bo> buffers = {};
        for (unsigned int i = 0; i < widths.size(); i++) {
            if (std::holds_alternative<Shape>(memoryDefinitions[i])) {
                auto shape = memoryDefinitions[i];
                auto elements = static_cast<unsigned int>(std::accumulate(std::begin(shape.begin()[i]), std::end(shape.begin()[i]), 1, std::multiplies<>()));
                buffers.emplace_back(xrt::bo(device, static_cast<size_t>(widths.begin()[i] * elements), boFlag, 0));  // TODO(bwintermann): Correct memory group setting missing, assuming 1 here
            } else if (std::holds_alternative<MemoryMap<T>>(memoryDefinitions[i])) {
                buffers.emplace_back(xrt::bo(device, memoryDefinitions[i].map, memoryDefinitions[i].size, boFlag, 0));
            }
        }
        return buffers;
    }


    /**
     * @brief Create a vector of Memory Map objects from a vector of XRT buffer objects, shapes and shapetypes
     *
     * @param buffers The vector of buffers (usually created by createIOBuffers)
     * @param shapes
     * @param shapeType
     * @param ringBufferSizeFactor The amount of times the memory map should fit into the ring buffer
     * @return std::vector<MemoryMap<T>>
     */
    std::vector<MemoryMap<T>> createMemoryMaps(std::vector<xrt::bo>& buffers, Shapes shapes, SHAPE_TYPE shapeType, const unsigned int ringBufferSizeFactor) {
        std::vector<MemoryMap<T>> maps = {};
        unsigned int index = 0;
        for (auto&& buffer : buffers) {
            unsigned int elementsInBuffer = static_cast<unsigned int>(buffer.size() / sizeof(T));
            MemoryMap<T> memmap { 
                inputNames[index],      // IDMA / ODMA Name
                buffer.map<T*>(),       // Datatmap
                buffer.size(),          // Size in bytes
                shapes.begin()[index],  // Shape / Dimensions of the map
                shapeType,              // Type of shape
                RingBuffer<T>(          // Buffer to quickly load new data into/from the map
                    elementsInBuffer,
                    elementsInBuffer * ringBufferSizeFactor 
                )
            };
            maps.emplace_back(memmap);
            ++index;
        }
        return maps;
    }

    /**
     * @brief Get a buffer object by index
     * 
     * @param mode 
     * @param index 
     * @return xrt::bo 
     */
    xrt::bo getBufferObject(IO_SWITCH mode, unsigned int index) {
        if (mode == IO_SWITCH::INPUT) {
            if (index >= inputBufferObjects.size()) {
                std::string err = "Trying to get inputBufferObject at index " + std::to_string(index) + " failed, there are only " + std::to_string(inputBufferObjects.size()) + " objects available!";
                BOOST_LOG_SEV(logger, logging::trivial::error) << "(" << name << ") " << err;
                throw std::length_error(err);
            }
            return inputBufferObjects[index];
        } else if (mode == IO_SWITCH::OUTPUT) {
            if (index >= outputBufferObjects.size()) {
                std::string err = "Trying to get outputBufferObject at index " + std::to_string(index) + " failed, there are only " + std::to_string(outputBufferObjects.size()) + " objects available!";
                BOOST_LOG_SEV(logger, logging::trivial::error) << "(" << name << ") " << err;
                throw std::length_error(err);
            }
            return outputBufferObjects[index];
        } else {
            std::string err = "Unknown retrieval mode for function getBufferObject() " + std::to_string(mode) + "!";
            BOOST_LOG_SEV(logger, logging::trivial::error) << "(" << name << ") " << err;
            throw std::runtime_error(err);
        }
    }

    /**
     * @brief Return a reference to the correct buffer list if mode is input or output. In any other case throw a runtime error 
     * 
     * @param mode 
     * @return std::vector<xrt::bo>* 
     */
    std::vector<xrt::bo>* _resolveIOModeToBuffer(IO_SWITCH mode) {
        if (mode == IO_SWITCH::INPUT) {
            return &inputBufferObjects;
        } else if (mode == IO_SWITCH::OUTPUT) {
            return &outputBufferObjects;
        } else {
            std::string err = "Couldn't resolve IO_SWITCH to Buffer-Vector (IO_SWITCH = " << std::to_string(mode) << ")\n";
            BOOST_LOG_SEV(logger, logging::trivial::error) << "(" << name << ") " << err;
            throw std::runtime_error(err);
        }
    }

    /**
     * @brief General purpose method to sync multiple buffers from and to the device for specific indices. This method may be several times slower than syncInputBuffersToDevice and syncOutputBuffersFromDevice.
     * To only sync one specific buffer use for example: deviceHandler.getBufferObject(IO_SWITCH::INPUT, 3).sync(XCL_BO_SYNC_BO_TO_DEVICE);
     * 
     * @param mode Whether the input or output buffers should be synced 
     * @param indices The indices to sync
     * @param syncDirection The direction to sync. This is on purpose not tied to the IO_SWITCH mode, although in almost all cases you will want to sync input TO device and output FROM device
     */
    void syncBuffers(IO_SWITCH mode, const std::vector<unsigned int>& indices, xclBOSyncDirection syncDirection) {
        std::vector<xrt::bo>* buffers = _resolveIOModeToBuffer(mode);
        for (auto index& : indices) {
            if (index >= buffers->size()) {
                std::string err = "Could not sync buffers. Trying to sync buffer " + std::to_string(index) + " but the buffer list (input or output) of device handler " + name + " has just " + std::to_string(buffers->size()) + " elements!\n";
                BOOST_LOG_SEV(logger, logging::trivial::error) << "(" << name << ") " << err;
                throw std::length_error(err);
            }

            (*buffers)[index].sync(syncDirection);
        }
    }

    /**
     * @brief Sync all inputBufferObjects to the device. This method is faster and should be used over the more general purpose but slower syncBuffers method!
     * 
     */
    void syncInputBuffersToDevice() {
        for (auto bo : inputBufferObjects) {
            bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        }
    }

     /**
     * @brief Sync all outputBufferObjects from the device. This method is faster and should be used over the more general purpose but slower syncBuffers method!
     * 
     */
    void syncOutputBuffersFromDevice() {
        for (auto bo : outputBufferObjects) {
            bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        }
    }

    /**
     * @brief Creates a tensor (stdex::mdspan) from a given memory map (for easier math manipulation)
     *
     * @param mmap
     * @return decltype(auto)
     */
    decltype(auto) createTensorFromMap(MemoryMap<T>& mmap) {
        auto expectedElements = static_cast<unsigned int>(std::accumulate(std::begin(mmap.dims), std::end(mmap.dims), 1, std::multiplies<>()));
        if (expectedElements != mmap.size) {
            std::string err = "During creation of Tensor (mdspan) from a xrt::bo map: Map has size " + std::to_string(mmap.size) + " but the dimensions array fits " + std::to_string(expectedElements) + " elements!";
            BOOST_LOG_SEV(logger, logging::trivial::error) << "(" << name << ") " << err;
            throw std::length_error(err);
        }
        return makeMDSpan(mmap.map, mmap.dims);
    }


    /**
     * @brief Throughput test 
     * @deprecated Based on a deprecated method of the ring buffer
     * 
     * @param min Min random value 
     * @param max Max random value
     */
    void throughputTest(int min, int max, unsigned int times, unsigned int memoryMapIndex) {
        for (unsigned int i = 0; i < times; i++) {
            inputMemoryMaps[0].ringBuffer.fillRandomInt(min, max);
            inputMemoryMaps[0].loadFromRingBuffer(true);
            syncInputBuffersToDevice();

            // TODO(bwintermann): DOES THE ODMA KERNEL WAIT UNTIL DATA ARRVIES?
            // TODO(bwintermann): Which arguments do the input/output kernels require?
            auto dataInRun = inputKernels[0](inputBufferObjects[0]);
            auto dataOutRun = outputKernels[0](outputBufferObjects[0]);

            // TODO(bwintermann): Naive, unbatched, temporary test solution
            dataInRun.start();
            dataOutRun.start();
            dataInRun.wait();
            dataOutRun.wait();

            syncOutputBuffersFromDevice();
        }
    }

    // TODO(bwintermann): Implementation
    void executeBatch();
};

#endif