#include "DeviceHandler.h"

#include <xrt/xrt_device.h>  // for device
#include <xrt/xrt_kernel.h>  // for kernel

#include <boost/cstdint.hpp>  // for uint8_t
#include <chrono>
#include <iosfwd>
#include <memory>
#include <stdexcept>
#include <system_error>

#include "../utils/Logger.h"  // for operator<<, FINN_LOG, FINN_DEBUG_LOG
#include "../utils/Types.h"   // for shape_t
#include "../utils/ConfigurationStructs.h"


namespace fs = std::filesystem;

namespace Finn {
    DeviceHandler::DeviceHandler(const DeviceWrapper& devWrap, unsigned int hostBufferSize = 100) {   
        // Checks and member assignments
        checkDeviceWrapper(devWrap);
        xrtDeviceIndex = devWrap.xrtDeviceIndex;
        xclbinPath = devWrap.xclbin;

        // XRT Device and DeviceBuffer initialization
        initializeDevice();
        loadXclbinSetUUID();
        initializeBufferObjects(devWrap, hostBufferSize);
    }

    void DeviceHandler::checkDeviceWrapper(const DeviceWrapper& devWrap) {
        if (devWrap.xclbin.empty()) {
            throw fs::filesystem_error("Empty filepath to xclbin. Abort.", std::error_code());
        }
        if (!fs::exists(devWrap.xclbin) || !fs::is_regular_file(devWrap.xclbin)) {
            throw fs::filesystem_error("File " + std::string(devWrap.xclbin.c_str()) + " not found. Abort.", std::error_code());
        }
        if (devWrap.idmas.empty()) {
            throw std::invalid_argument("Empty input kernel list. Abort.");
        }
        for (auto&& bufDesc : devWrap.idmas) {
            if (bufDesc->kernelName.empty()) {
                throw std::invalid_argument("Empty kernel name. Abort.");
            }
            if (bufDesc->packedShape.empty()) {
                throw std::invalid_argument("Empty buffer shape. Abort.");
            }
        }
        if (devWrap.odmas.empty()) {
            throw std::invalid_argument("Empty output kernel list. Abort.");
        }
        for (auto&& bufDesc : devWrap.odmas) {
            if (bufDesc->kernelName.empty()) {
                throw std::invalid_argument("Empty kernel name. Abort.");
            }
            if (bufDesc->packedShape.empty()) {
                throw std::invalid_argument("Empty buffer shape. Abort.");
            }
        }
    }

    void DeviceHandler::initializeDevice() {
        FINN_LOG(log, loglevel::info) << "(" << xrtDeviceIndex << ") " << "Initializing xrt::device, loading xclbin and assigning IP\n";
        device = xrt::device(xrtDeviceIndex);
    }

    void DeviceHandler::loadXclbinSetUUID() {
        FINN_LOG(log, loglevel::info) << "(" << xrtDeviceIndex << ") " << "Loading XCLBIN and setting uuid\n";
        uuid = device.load_xclbin(xclbinPath);
    }

    void DeviceHandler::initializeBufferObjects(const DeviceWrapper& devWrap, unsigned int hostBufferSize) {
        FINN_LOG(log, loglevel::info) << "(" << xrtDeviceIndex << ") " << "Initializing buffer objects\n";
        for (auto&& ebdptr : devWrap.idmas) {
            auto tmpKern = xrt::kernel(device, uuid, ebdptr->kernelName, xrt::kernel::cu_access_mode::shared);
            inputBufferMap.emplace(
                std::make_pair(
                    ebdptr->kernelName,
                    Finn::DeviceInputBuffer<uint8_t>(ebdptr->kernelName, device, tmpKern, ebdptr->packedShape, hostBufferSize)
                )
            );
        }
        for (auto&& ebdptr : devWrap.odmas) {
            auto tmpKern = xrt::kernel(device, uuid, ebdptr->kernelName, xrt::kernel::cu_access_mode::shared);
            outputBufferMap.emplace(
                std::make_pair(
                    ebdptr->kernelName,
                    Finn::DeviceOutputBuffer<uint8_t>(ebdptr->kernelName, device, tmpKern, ebdptr->packedShape, hostBufferSize)
                )
            );
        }
#ifndef NDEBUG
        for (index = 0; index < inputBufferMap.bucket_count(); ++index) {
            if (inputBufferMap.bucket_size(index) > 1) {
                FINN_LOG_DEBUG(log, loglevel::error) << "(" << name << ") "
                                                     << "Hash collision in inputBufferMap. This access to the inputBufferMap is no longer constant time!";
            }
        }
        for (index = 0; index < outputBufferMap.bucket_count(); ++index) {
            if (outputBufferMap.bucket_size(index) > 1) {
                FINN_LOG_DEBUG(log, loglevel::error) << "(" << name << ") "
                                                     << "Hash collision in outputBufferMap. This access to the outputBufferMap is no longer constant time!";
            }
        }
#endif
    }


    bool DeviceHandler::store(const std::vector<uint8_t>& data, const std::string& inputBufferKernelName) {
        if (!inputBufferMap.contains(inputBufferKernelName)) {
            FinnUtils::logAndError<std::runtime_error>("Tried accessing kernel/buffer with name " + inputBufferKernelName + " but this kernel / buffer does not exist!");
        }
        return inputBufferMap.at(inputBufferKernelName).store(data);
    }

    unsigned int DeviceHandler::getDeviceIndex() {
        return xrtDeviceIndex;
    }

    bool DeviceHandler::run(const std::string& inputBufferKernelName) {
        if (!inputBufferMap.contains(inputBufferKernelName)) {
            FinnUtils::logAndError<std::runtime_error>("Tried accessing kernel/buffer with name " + inputBufferKernelName + " but this kernel / buffer does not exist!");
        }
        return inputBufferMap.at(inputBufferKernelName).run();
    }

    std::vector<std::vector<uint8_t>> DeviceHandler::read(const std::string& outputBufferKernelName, unsigned int samples, bool forceArchive) {
        if (!outputBufferMap.contains(outputBufferKernelName)) {
            FinnUtils::logAndError<std::runtime_error>("Tried accessing kernel/buffer with name " + outputBufferKernelName + " but this kernel / buffer does not exist!");
        }
        // TODO: Avoid triple map access
        outputBufferMap.at(outputBufferKernelName).read(samples);
        if (forceArchive) {
            outputBufferMap.at(outputBufferKernelName).retrieveArchive();
        }
        return outputBufferMap.at(outputBufferKernelName).retrieveArchive();
    }


    size_t DeviceHandler::size(SIZE_SPECIFIER ss, const std::string& bufferName) {
        if (inputBufferMap.contains(bufferName)) {
            return static_cast<size_t>(inputBufferMap.at(bufferName).size(ss));
        }
        if (outputBufferMap.contains(bufferName)) {
            return static_cast<size_t>(outputBufferMap.at(bufferName).size(ss));
        }
    }
}  // namespace Finn