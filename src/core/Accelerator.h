#ifndef ACCELERATOR_H
#define ACCELERATOR_H

#include <cinttypes>   // for uint8_t
#include <filesystem>  // for path
#include <string>      // for string
#include <vector>      // for vector

#include "DeviceHandler.h"  // for BufferDescriptor, DeviceHandler
#include "../utils/ConfigurationStructs.h"


namespace Finn {
    // Fwd declarations
    // class DeviceHandler;
    // struct BufferDescriptor;


    /**
     * @brief The Accelerator class wraps one or more Devices into a single Accelerator
     *
     */
    class Accelerator {
         private:
        /**
         * @brief A vector of DeviceHandler instances that belong to this accelerator 
         * 
         */
        std::vector<DeviceHandler> devices;

         public:
        Accelerator() = default;
        /**
         * @brief Construct a new Accelerator object using a list of DeviceWrappers
         *
         * @param deviceDefinitions Vector of @ref DeviceWrapper
         */
        explicit Accelerator(const std::vector<DeviceWrapper>& deviceDefinitions, unsigned int hostBufferSize);
        Accelerator(Accelerator&&) = default;
        Accelerator(const Accelerator&) = delete;
        Accelerator& operator=(Accelerator&&) = default;
        Accelerator& operator=(const Accelerator&) = delete;
        ~Accelerator() = default;

        /**
         * @brief Return a reference to the deviceHandler with the given index. Crashes the driver if the index is invalid
         * 
         * @param deviceIndex 
         * @return DeviceHandler& 
         */
        DeviceHandler& getDeviceHandlerByDeviceIndex(unsigned int deviceIndex);

        /**
         * @brief Checks whether a device handler with the given device index exists
         * 
         * @param deviceIndex 
         * @return true 
         * @return false 
         */
        bool containsDeviceHandlerWithDeviceIndex(unsigned int deviceIndex);

        /**
         * @brief Store data in the device handler with the given deviceIndex, and in the buffer with the given inputBufferKernelName.
         * 
         * @param data 
         * @param deviceIndex If such a deviceIndex does not exist, use the first (0) device handler. If it doesnt exist, crash.
         * @param inputBufferKernelName 
         * @return true 
         * @return false 
         */
        bool store(const std::vector<uint8_t>& data, const unsigned int deviceIndex, const std::string& inputBufferKernelName);

        /**
         * @brief Run the given buffer. Returns false if no valid data was found to execute on. 
         * 
         * @param deviceIndex 
         * @param inputBufferKernelName 
         * @return true 
         * @return false 
         */
        bool run(const unsigned int deviceIndex, const std::string& inputBufferKernelName);

        /**
         * @brief Return a vector of output samples.  
         * 
         * @param deviceIndex 
         * @param outputBufferKernelName 
         * @param samples The number of samples to read
         * @param forceArchive Whether or not to force a readout into archive. Necessary to get new data
         * @return std::vector<std::vector<uint8_t>> 
         */
        std::vector<std::vector<uint8_t>> readOut(const unsigned int deviceIndex, const std::string& outputBufferKernelName, unsigned int samples, bool forceArchive);

        /**
         * @brief Read data from device but dont return it for performance reasons 
         * 
         * @param deviceIndex 
         * @param outputBufferKernelName 
         * @param samples 
         * @param forceArchive 
         */
        void read(const unsigned int deviceIndex, const std::string& outputBufferKernelName, unsigned int samples, bool forceArchive);
    
        /**
         * @brief Get the size of the buffer with the specified device index and buffer name 
         * 
         * @param ss 
         * @param deviceIndex 
         * @param bufferName 
         * @return size_t 
         */
        size_t size(SIZE_SPECIFIER ss, unsigned int deviceIndex, const std::string& bufferName);
    };


}  // namespace Finn

#endif  // ACCELERATOR_H
