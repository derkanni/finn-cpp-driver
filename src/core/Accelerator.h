#ifndef ACCELERATOR_H
#define ACCELERATOR_H

#include <cinttypes>   // for uint8_t
#include <filesystem>  // for path
#include <string>      // for string
#include <vector>      // for vector

#include "../utils/Types.h"
#include "DeviceHandler.h"  // for BufferDescriptor, DeviceHandler


namespace Finn {


    /**
     * @brief The Accelerator class wraps one or more Devices into a single Accelerator
     *
     */
    class Accelerator {
         public:
        /**
         * @brief Default Constructor
         *
         */
        Accelerator() = default;
        /**
         * @brief Construct a new Accelerator object using a list of DeviceWrappers
         *
         * @param deviceDefinitions Vector of @ref DeviceWrapper
         */
        explicit Accelerator(const std::vector<DeviceWrapper>& deviceDefinitions);
        /**
         * @brief Construct a new Accelerator object using a single DeviceWrapper
         *
         * @param deviceWrapper
         */
        explicit Accelerator(const DeviceWrapper& deviceWrapper);
        /**
         * @brief Default move Constructor
         *
         */
        Accelerator(Accelerator&&) = default;
        /**
         * @brief Deleted copy constructor. Copying the Accelerator can result in multiple Accelerators managing the same Device
         *
         */
        Accelerator(const Accelerator&) = delete;
        /**
         * @brief Default move assignment operator
         *
         * @return Accelerator& other
         */
        Accelerator& operator=(Accelerator&&) = default;
        /**
         * @brief Deleted copy assignment operator. Copying the Accelerator can result in multiple Accelerators managing the same Device
         *
         * @return Accelerator&
         */
        Accelerator& operator=(const Accelerator&) = delete;
        /**
         * @brief Destroy the Accelerator object. Default Implementation.
         *
         */
        ~Accelerator() = default;

        /**
         * @brief Fast and dirty implementation that only supports a single FPGA with a single input
         *
         * @param inputVec
         * @return true
         * @return false
         */
        bool write(const std::vector<uint8_t>& inputVec);

         private:
        /**
         * @brief Vector of Devices managed by the Accelerator object.
         *
         */
        std::vector<DeviceHandler> devices;
    };


}  // namespace Finn

#endif  // ACCELERATOR_H
