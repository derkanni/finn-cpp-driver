#ifndef BASEDRIVER_HPP
#define BASEDRIVER_HPP

#include <cinttypes>  // for uint8_t
#include <filesystem>
#include <memory>

#include "../utils/FinnUtils.h"
#include "Accelerator.h"

#include "../utils/Types.h"
#include "../utils/Logger.h"
#include "../utils/ConfigurationStructs.h"
#include "../utils/FinnDatatypes.hpp"
#include "ert.h"

#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace Finn {
    template<typename F, typename S, typename T = uint8_t>
    class BaseDriver {
         private:
        Accelerator accelerator;
        Config configuration;
        logger_type& logger = Logger::getLogger();

        unsigned int defaultInputDeviceIndex;
        unsigned int defaultOutputDeviceIndex;
        std::string defaultInputKernelName;
        std::string defaultOutputKernelName;

         public:
        /**
         * @brief Create a BaseDriver from a config file. This needs to be templated by the FINN-datatypes. The corresponding header file is generated by the FINN compiler
         * 
         * @param configPath 
         * @param hostBufferSize 
         */
        BaseDriver(const std::filesystem::path& configPath, unsigned int hostBufferSize) {
            configuration = createConfigFromPath(configPath);
            accelerator = Accelerator(configuration.deviceWrappers, hostBufferSize);
            logger = Logger::getLogger();
        };
        BaseDriver(BaseDriver&&) noexcept = default;
        BaseDriver(const BaseDriver&) noexcept = delete;
        BaseDriver& operator=(BaseDriver&&) noexcept = default;
        BaseDriver& operator=(const BaseDriver&) = delete;
        virtual ~BaseDriver() = default;
    
        // TODO:
        // TODO(bwintermann): Add methods for Iterator reading/storing

        /**
         * @brief Get the Config object. Simple getter to check things outside the driver
         * 
         * @return Config 
         */
        Config getConfig() {
            return config;
        }

        /**
         * @brief Get the Device object, specified by its index
         * 
         * @param index 
         * @return DeviceHandler& 
         */
        DeviceHandler& getDeviceHandler(unsigned int index) {
            return accelerator.getDeviceHandlerByDeviceIndex(index);
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
         * @return std::vector<std::vector<uint8_t>> 
         */
        std::vector<std::vector<uint8_t>> inferRaw(const std::vector<uint8_t>& data, unsigned int inputDeviceIndex, const std::string& inputBufferKernelName, unsigned int outputDeviceIndex, const std::string& outputBufferKernelName, unsigned int samples) {
            FINN_LOG(logger, loglevel::info) << "Starting inference (raw data)";
            bool stored = accelerator.store(data, inputDeviceIndex, inputBufferKernelName);
            FINN_LOG(logger, loglevel::info) << "Running kernels";
            bool ran = accelerator.run(inputDeviceIndex, inputBufferKernelName);
            if (stored && ran) {
                FINN_LOG(logger, loglevel::info) << "Reading out buffers";
                ert_cmd_state resultState = accelerator.read(outputDeviceIndex, outputBufferKernelName, samples);
                if (resultState == ERT_CMD_STATE_COMPLETED || resultState == ERT_CMD_STATE_TIMEOUT) {
                    return accelerator.retrieveResults(outputDeviceIndex, outputBufferKernelName);
                } else {
                    FinnUtils::logAndError<std::runtime_error>("Unspecifiable error during inference (ert_cmd_state is " + std::to_string(resultState) + ")!");
                }
            } else {
                FinnUtils::logAndError<std::runtime_error>("Data either couldnt be stored or there was no data to execute!");
            }
        }

        /**
         * @brief Return the size (type specified by SIZE_SPECIFIER) at the given device at the given buffer 
         * 
         * @param ss 
         * @param deviceIndex 
         * @param bufferName 
         * @return size_t 
         */
        size_t size(SIZE_SPECIFIER ss, unsigned int deviceIndex, const std::string& bufferName) {
           return accelerator.size(ss, deviceIndex, bufferName);
        }

    };
}  // namespace Finn

#endif  // BASEDRIVER_H