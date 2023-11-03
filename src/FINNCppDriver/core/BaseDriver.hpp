/**
 * @file BaseDriver.hpp
 * @author Bjarne Wintermann (bjarne.wintermann@uni-paderborn.de), Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Implements the base driver for FINN
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef BASEDRIVER_HPP
#define BASEDRIVER_HPP

#include <FINNCppDriver/utils/ConfigurationStructs.h>
#include <FINNCppDriver/utils/FinnUtils.h>
#include <FINNCppDriver/utils/Logger.h>
#include <FINNCppDriver/utils/Types.h>

#include <FINNCppDriver/utils/DataPacking.hpp>
#include <FINNCppDriver/utils/FinnDatatypes.hpp>
#include <cinttypes>  // for uint8_t
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>

#include "Accelerator.h"
#include "ert.h"


namespace Finn {
    /**
     * @brief A class to represent a basic FINN-Driver
     *
     * @tparam F The FINN input datatype
     * @tparam S The FINN output datatype
     * @tparam T The C-datatype used to pass data to the FPGA
     */
    template<typename F, typename S, typename T = uint8_t>
    class BaseDriver {
         private:
        Accelerator accelerator;
        Config configuration;
        logger_type& logger = Logger::getLogger();

        uint defaultInputDeviceIndex = 0;
        std::string defaultInputKernelName;
        uint defaultOutputDeviceIndex = 0;
        std::string defaultOutputKernelName;
        uint batchElements = 1;
        bool forceAchieval = false;

         public:
        /**
         * @brief Create a BaseDriver from a config file. This needs to be templated by the FINN-datatypes. The corresponding header file is generated by the FINN compiler
         *
         * @param configPath
         * @param hostBufferSize
         */
        BaseDriver(const std::filesystem::path& configPath, uint hostBufferSize) : configuration(createConfigFromPath(configPath)), logger(Logger::getLogger()) {
            accelerator = Accelerator(configuration.deviceWrappers, hostBufferSize);
#ifdef UNITTEST
            logDriver();
#endif
        };

        BaseDriver(const std::filesystem::path& configPath, uint hostBufferSize, uint inputDeviceIndex, const std::string& inputKernelName, uint outputDeviceIndex, const std::string& outputKernelName, uint batchSize, bool pForceAchieval)
            : configuration(createConfigFromPath(configPath)),
              logger(Logger::getLogger()),
              defaultInputDeviceIndex(inputDeviceIndex),
              defaultInputKernelName(inputKernelName),
              defaultOutputDeviceIndex(outputDeviceIndex),
              defaultOutputKernelName(outputKernelName),
              batchElements(batchSize),
              forceAchieval(pForceAchieval) {
            accelerator = Accelerator(configuration.deviceWrappers, hostBufferSize);
        }

        /**
         * @brief Create a new base driver based on an existing configuration
         *
         * @param pConfig
         */
        BaseDriver(const Config& pConfig, uint hostBufferSize) : configuration(pConfig), logger(Logger::getLogger()) { accelerator = Accelerator(configuration.deviceWrappers, hostBufferSize); }

        BaseDriver(const Config& pConfig, uint hostBufferSize, uint inputDeviceIndex, const std::string& inputKernelName, uint outputDeviceIndex, const std::string& outputKernelName, uint batchSize, bool pForceAchieval)
            : configuration(pConfig),
              logger(Logger::getLogger()),
              defaultInputDeviceIndex(inputDeviceIndex),
              defaultInputKernelName(inputKernelName),
              defaultOutputDeviceIndex(outputDeviceIndex),
              defaultOutputKernelName(outputKernelName),
              batchElements(batchSize),
              forceAchieval(pForceAchieval) {
            accelerator = Accelerator(configuration.deviceWrappers, hostBufferSize);
        }


        BaseDriver(BaseDriver&&) noexcept = default;
        BaseDriver(const BaseDriver&) noexcept = delete;
        BaseDriver& operator=(BaseDriver&&) noexcept = default;
        BaseDriver& operator=(const BaseDriver&) = delete;
        virtual ~BaseDriver() = default;

        /**
         * @brief Set the Default Input Device Index
         *
         * @param index
         */
        void setDefaultInputDeviceIndex(uint index) { defaultInputDeviceIndex = index; }

        /**
         * @brief Set the Default Output Device Index
         *
         * @param index
         */
        void setDefaultOutputDeviceIndex(uint index) { defaultOutputDeviceIndex = index; }

        /**
         * @brief Set the Default Input Kernel Name
         *
         * @param kernelName
         */
        void setDefaultInputKernelName(const std::string& kernelName) { defaultInputKernelName = kernelName; }

        /**
         * @brief Set the Default Output Kernel Name
         *
         * @param kernelName
         */
        void setDefaultOutputKernelName(const std::string& kernelName) { defaultInputKernelName = kernelName; }

        /**
         * @brief Set the Batch Size
         *
         * @param elements
         */
        void setBatchSize(uint elements) { batchElements = elements; }

        /**
         * @brief Set the Force Achieval
         *
         * @param force
         */
        void setForceAchieval(bool force) { forceAchieval = force; }

        /**
         * @brief A logger prefix to determine the source of a log write
         *
         * @return std::string
         */
         private:
        static std::string loggerPrefix() { return "[BaseDriver] "; }

         public:
        /**
         * @brief Get the Config object. Simple getter to check things outside the driver
         *
         * @return Config
         */
        Config getConfig() const { return configuration; }

        /**
         * @brief Get the Device object, specified by its index
         *
         * @param index
         * @return DeviceHandler&
         */
        DeviceHandler& getDeviceHandler(uint index) { return accelerator.getDeviceHandler(index); }

        /**
         * @brief Get a specific buffer object specified by its name and the device it is on
         *
         * @param deviceIndex
         * @param bufferName
         * @return DeviceInputBuffer<uint8_t>
         */
        DeviceInputBuffer<uint8_t>& getInputBuffer(uint deviceIndex, const std::string& bufferName) { return getDeviceHandler(deviceIndex).getInputBuffer(bufferName); }

        /**
         * @brief Return the size (type specified by SIZE_SPECIFIER) at the given device at the given buffer
         *
         * @param ss
         * @param deviceIndex
         * @param bufferName
         * @return size_t
         */
        size_t size(SIZE_SPECIFIER ss, uint deviceIndex, const std::string& bufferName) { return accelerator.size(ss, deviceIndex, bufferName); }

        template<typename IteratorType, typename V = Finn::UnpackingAutoRetType::AutoRetType<S>>
        Finn::vector<V> inferSynchronous(IteratorType first, IteratorType last, uint inputDeviceIndex, const std::string& inputBufferKernelName, uint outputDeviceIndex, const std::string& outputBufferKernelName, uint samples,
                                         bool forceArchival) {
            auto packed = Finn::pack<F>(first, last);
            auto result = inferRaw(packed.begin(), packed.end(), inputDeviceIndex, inputBufferKernelName, outputDeviceIndex, outputBufferKernelName, samples, forceArchival);
            return unpack<S, V>(result);
        }

        template<typename IteratorType, typename V = Finn::UnpackingAutoRetType::AutoRetType<S>>
        Finn::vector<V> inferSynchronous(IteratorType first, IteratorType last) {
            return inferSynchronous(first, last, defaultInputDeviceIndex, defaultInputKernelName, defaultOutputDeviceIndex, defaultOutputKernelName, batchElements, forceAchieval);
        }

        template<typename U, typename V = Finn::UnpackingAutoRetType::AutoRetType<S>>
        Finn::vector<V> inferSynchronous(const Finn::vector<U>& data, uint inputDeviceIndex, const std::string& inputBufferKernelName, uint outputDeviceIndex, const std::string& outputBufferKernelName, uint samples, bool forceArchival) {
            return inferSynchronous(data.begin(), data.end(), inputDeviceIndex, inputBufferKernelName, outputDeviceIndex, outputBufferKernelName, samples, forceAchieval);
        }

        template<typename U, typename V = Finn::UnpackingAutoRetType::AutoRetType<S>>
        Finn::vector<V> inferSynchronous(const Finn::vector<U>& data) {
            return inferSynchronous(data, defaultInputDeviceIndex, defaultInputKernelName, defaultOutputDeviceIndex, defaultOutputKernelName, batchElements, forceAchieval);
        }

        // template<typename T, typename U, typename IteratorType>
        // Finn::vector<T> inferSynchronous(IteratorType first, IteratorType last) {

        // }


         protected:
        /**
         *
         * @brief Do an inference with the given data. This assumes already flattened data in uint8_t's. Specify inputs and outputs.
         *
         * @param first Iterator to first element in input sequence
         * @param last  Iterator to last element in input sequence
         * @param inputDeviceIndex
         * @param inputBufferKernelName
         * @param outputDeviceIndex
         * @param outputBufferKernelName
         * @param samples
         * @param forceArchival If true, the data gets written to LTS either way, ensuring that there is data to be read!
         * @return Finn::vector<uint8_t>
         */
        template<typename IteratorType>
        [[nodiscard]] Finn::vector<uint8_t> inferRaw(IteratorType first, IteratorType last, uint inputDeviceIndex, const std::string& inputBufferKernelName, uint outputDeviceIndex, const std::string& outputBufferKernelName, uint samples,
                                                     bool forceArchival) {
            FINN_LOG_DEBUG(logger, loglevel::info) << loggerPrefix() << "Starting inference (raw data)";
            auto storeFunc = accelerator.storeFactory(inputDeviceIndex, inputBufferKernelName);

            bool stored = storeFunc(first, last);
            bool ran = accelerator.run(inputDeviceIndex, inputBufferKernelName);

#ifdef UNITTEST
            Finn::vector<uint8_t> data(first, last);
            FINN_LOG(logger, loglevel::info) << "Readback from device buffer confirming data was written to board successfully: " << isSyncedDataEquivalent(inputDeviceIndex, inputBufferKernelName, data);
#endif

            if (stored && ran) {
                FINN_LOG_DEBUG(logger, loglevel::info) << "Reading out buffers";
                ert_cmd_state resultState = accelerator.read(outputDeviceIndex, outputBufferKernelName, samples);

                // If the kernel run is completed (success or by timeout (more reads than were in the pipeline)), return the data
                if (resultState == ERT_CMD_STATE_COMPLETED || resultState == ERT_CMD_STATE_TIMEOUT || resultState == ERT_CMD_STATE_NEW) {
                    return accelerator.retrieveResults(outputDeviceIndex, outputBufferKernelName, forceArchival);
                } else {
                    FinnUtils::logAndError<std::runtime_error>("Unspecifiable error during inference (ert_cmd_state is " + std::to_string(resultState) + ")!");
                }
            } else {
                FinnUtils::logAndError<std::runtime_error>("Data either couldnt be stored or there was no data to execute!");
            }
        }

        /**
         *
         * @brief Do an inference with the given data. This assumes already flattened data in uint8_t's. Specify inputs and outputs.
         *
         * @param data
         * @param inputDeviceIndex
         * @param inputBufferKernelName
         * @param outputDeviceIndex
         * @param outputBufferKernelName
         * @param samples
         * @param forceArchival If true, the data gets written to LTS either way, ensuring that there is data to be read!
         * @return Finn::vector<uint8_t>
         */
        [[nodiscard]] Finn::vector<uint8_t> inferRaw(const Finn::vector<uint8_t>& data, uint inputDeviceIndex, const std::string& inputBufferKernelName, uint outputDeviceIndex, const std::string& outputBufferKernelName, uint samples,
                                                     bool forceArchival) {
            return inferRaw(data.begin(), data.end(), inputDeviceIndex, inputBufferKernelName, outputDeviceIndex, outputBufferKernelName, samples, forceArchival);
        }

        /**
         * @brief Do an inference with the given data. This assumes already flattened data in uint8_t's. Specify inputs and outputs.
         *
         * @tparam IteratorType
         * @param first
         * @param last
         * @param inputDeviceIndex
         * @param inputBufferKernelName
         * @param outputDeviceIndex
         * @param outputBufferKernelName
         * @param samples
         * @param forceArchival
         * @return Finn::vector<uint8_t>
         */
        template<typename IteratorType>
        [[nodiscard]] Finn::vector<uint8_t> infer(IteratorType first, IteratorType last, uint inputDeviceIndex, const std::string& inputBufferKernelName, uint outputDeviceIndex, const std::string& outputBufferKernelName, uint samples,
                                                  bool forceArchival) {
            FINN_LOG_DEBUG(logger, loglevel::info) << "Starting inference";
            bool stored = accelerator.store(first, last, inputDeviceIndex, inputBufferKernelName);
            FINN_LOG_DEBUG(logger, loglevel::info) << "Running kernels";
            bool ran = accelerator.run(inputDeviceIndex, inputBufferKernelName);
            if (stored && ran) {
                FINN_LOG_DEBUG(logger, loglevel::info) << loggerPrefix() << "Reading out buffers";
                ert_cmd_state resultState = accelerator.read(outputDeviceIndex, outputBufferKernelName, samples);

                // If the kernel run is completed (success or by timeout (more reads than were in the pipeline)), return the data
                if (resultState == ERT_CMD_STATE_COMPLETED || resultState == ERT_CMD_STATE_TIMEOUT || resultState == ERT_CMD_STATE_NEW) {
                    return accelerator.retrieveResults(outputDeviceIndex, outputBufferKernelName, forceArchival);
                } else {
                    FinnUtils::logAndError<std::runtime_error>("Unspecifiable error during inference (ert_cmd_state is " + std::to_string(resultState) + ")!");
                }
            } else {
                FinnUtils::logAndError<std::runtime_error>("Data either couldnt be stored or there was no data to execute!");
            }
        }

        /**
         * @brief Do an inference with the given data. This assumes already flattened data in uint8_t's. Specify inputs and outputs.
         *
         * @param data
         * @param inputDeviceIndex
         * @param inputBufferKernelName
         * @param outputDeviceIndex
         * @param outputBufferKernelName
         * @param samples
         * @param forceArchival
         * @return std::vector<std::vector<uint8_t>>
         */
        [[nodiscard]] std::vector<std::vector<uint8_t>> infer(const std::vector<uint8_t>& data, uint inputDeviceIndex, const std::string& inputBufferKernelName, uint outputDeviceIndex, const std::string& outputBufferKernelName,
                                                              uint samples, bool forceArchival) {
            return infer(data.begin(), data.end(), inputDeviceIndex, inputBufferKernelName, outputDeviceIndex, outputBufferKernelName, samples, forceArchival);
        }

#ifdef UNITTEST
        /**
         * @brief Return whether the data that is currently held on the FPGA is equivalent to the passed data
         *
         * @param deviceIndex
         * @param bufferName
         * @param data
         * @return true
         * @return false
         */
        bool isSyncedDataEquivalent(uint deviceIndex, const std::string& bufferName, const Finn::vector<uint8_t>& data) {
            DeviceInputBuffer<uint8_t>& devInBuf = getInputBuffer(deviceIndex, bufferName);
            devInBuf.testSyncBackFromDevice();
            return devInBuf.testGetMap() == data;
        }

        /**
         * @brief Log out the entire structure of the driver (devices and their buffers)
         *
         */
        void logDriver() {
            FINN_LOG(logger, loglevel::info) << loggerPrefix() << "Driver Overview:\n";
            for (DeviceHandler& devHandler : accelerator) {
                FINN_LOG(logger, loglevel::info) << "\tDevice Index: " << devHandler.getDeviceIndex();
                for (auto& keyValuePair : devHandler.getInputBufferMap()) {
                    FINN_LOG(logger, loglevel::info) << "\t\tInput buffers: ";
                    FINN_LOG(logger, loglevel::info) << "\t\t\tName: " << keyValuePair.second.getName() << " (in hashmap as " << keyValuePair.first << ")";
                    FINN_LOG(logger, loglevel::info) << "\t\t\tShape packed: " << FinnUtils::shapeToString(keyValuePair.second.getPackedShape());
                    FINN_LOG(logger, loglevel::info) << "\t\t\tElements of type T (usually uint8_t) per sample: " << keyValuePair.second.size(SIZE_SPECIFIER::ELEMENTS_PER_PART);
                    FINN_LOG(logger, loglevel::info) << "\t\t\tElements of type T (usually uint8_t) in buffer overall: " << keyValuePair.second.size(SIZE_SPECIFIER::ELEMENTS);
                }
                for (auto& keyValuePair : devHandler.getOutputBufferMap()) {
                    FINN_LOG(logger, loglevel::info) << "\t\tOutput buffers: ";
                    FINN_LOG(logger, loglevel::info) << "\t\t\tName: " << keyValuePair.second.getName() << " (in hashmap as " << keyValuePair.first << ")";
                    FINN_LOG(logger, loglevel::info) << "\t\t\tShape packed: " << FinnUtils::shapeToString(keyValuePair.second.getPackedShape());
                    FINN_LOG(logger, loglevel::info) << "\t\t\tElements of type T (usually uint8_t) per sample: " << keyValuePair.second.size(SIZE_SPECIFIER::ELEMENTS_PER_PART);
                    FINN_LOG(logger, loglevel::info) << "\t\t\tElements of type T (usually uint8_t) in buffer overall: " << keyValuePair.second.size(SIZE_SPECIFIER::ELEMENTS);
                }
            }
        }
#endif
    };
}  // namespace Finn

#endif  // BASEDRIVER_H