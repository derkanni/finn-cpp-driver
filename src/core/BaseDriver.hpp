#ifndef BASEDRIVER_HPP
#define BASEDRIVER_HPP

#include <cinttypes>  // for uint8_t
#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>

#include "../utils/ConfigurationStructs.h"
#include "../utils/FinnDatatypes.hpp"
#include "../utils/FinnUtils.h"
#include "../utils/Logger.h"
#include "../utils/Types.h"
#include "Accelerator.h"
#include "ert.h"
using json = nlohmann::json;

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

        unsigned int defaultInputDeviceIndex = 0;
        unsigned int defaultOutputDeviceIndex = 0;
        std::string defaultInputKernelName;
        std::string defaultOutputKernelName;

         public:
        /**
         * @brief Create a BaseDriver from a config file. This needs to be templated by the FINN-datatypes. The corresponding header file is generated by the FINN compiler
         *
         * @param configPath
         * @param hostBufferSize
         */
        BaseDriver(const std::filesystem::path& configPath, unsigned int hostBufferSize) : configuration(createConfigFromPath(configPath)), logger(Logger::getLogger()) {
            accelerator = Accelerator(configuration.deviceWrappers, hostBufferSize);
#ifndef NDEBUG
            logDriver();
#endif
        };
        BaseDriver(BaseDriver&&) noexcept = default;
        BaseDriver(const BaseDriver&) noexcept = delete;
        BaseDriver& operator=(BaseDriver&&) noexcept = default;
        BaseDriver& operator=(const BaseDriver&) = delete;
        virtual ~BaseDriver() = default;

        /**
         * @brief A logger prefix to determine the source of a log write
         *
         * @return std::string
         */
         private:
        std::string loggerPrefix() { return "[BaseDriver] "; }

         public:
        // TODO(bwintermann): Add methods for Iterator reading/storing

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
        DeviceHandler& getDeviceHandler(unsigned int index) { return accelerator.getDeviceHandler(index); }

        /**
         * @brief Get a specific buffer object specified by its name and the device it is on
         *
         * @param deviceIndex
         * @param bufferName
         * @return DeviceInputBuffer<uint8_t>
         */
        DeviceInputBuffer<uint8_t>& getInputBuffer(unsigned int deviceIndex, const std::string& bufferName) { return getDeviceHandler(deviceIndex).getInputBuffer(bufferName); }

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
         * @return std::vector<std::vector<uint8_t>>
         */
        [[nodiscard]] std::vector<std::vector<uint8_t>> inferRaw(const std::vector<uint8_t>& data, unsigned int inputDeviceIndex, const std::string& inputBufferKernelName, unsigned int outputDeviceIndex,
                                                                 const std::string& outputBufferKernelName, unsigned int samples, bool forceArchival) {
            FINN_LOG_DEBUG(logger, loglevel::info) << loggerPrefix() << "Starting inference (raw data)";
            auto storeFunc = accelerator.storeFactory(inputDeviceIndex, inputBufferKernelName);

            bool stored = storeFunc(data);
            bool ran = accelerator.run(inputDeviceIndex, inputBufferKernelName);
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

        // TODO(linusjun): Implement iterator version and document everything
        [[nodiscard]] std::vector<std::vector<uint8_t>> infer(const std::vector<uint8_t>& data, unsigned int inputDeviceIndex, const std::string& inputBufferKernelName, unsigned int outputDeviceIndex,
                                                              const std::string& outputBufferKernelName, unsigned int samples, bool forceArchival) {
            FINN_LOG_DEBUG(logger, loglevel::info) << "Starting inference";
            bool stored = accelerator.store(data, inputDeviceIndex, inputBufferKernelName);
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
         * @brief Normal inference with packing
         *  ! TODO
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
        //[[nodiscard]] std::vector<std::vector<uint8_t>> infer(const std::vector<uint8_t>& data, unsigned int inputDeviceIndex, const std::string& inputBufferKernelName, unsigned int outputDeviceIndex,
        //                                                      const std::string& outputBufferKernelName, unsigned int samples, bool forceArchival) {
        //}

        /**
         * @brief Return the size (type specified by SIZE_SPECIFIER) at the given device at the given buffer
         *
         * @param ss
         * @param deviceIndex
         * @param bufferName
         * @return size_t
         */
        size_t size(SIZE_SPECIFIER ss, unsigned int deviceIndex, const std::string& bufferName) { return accelerator.size(ss, deviceIndex, bufferName); }


#ifndef NDEBUG
        /**
         * @brief Return whether the data that is currently held on the FPGA is equivalent to the passed data
         *
         * @param deviceIndex
         * @param bufferName
         * @param data
         * @return true
         * @return false
         */
        bool isSyncedDataEquivalent(unsigned int deviceIndex, const std::string& bufferName, const std::vector<uint8_t>& data) {
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