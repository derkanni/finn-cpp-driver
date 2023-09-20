#ifndef BASEDRIVER_HPP
#define BASEDRIVER_HPP

#include <cinttypes>  // for uint8_t
#include <filesystem>
#include <memory>

#include "../utils/FinnUtils.h"
#include "Accelerator.h"

#include "../utils/Types.h"
#include "../utils/FinnDatatypes.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace Finn {
    template<typename F, typename S, typename T = uint8_t>
    class BaseDriver {
         private:
        Accelerator accelerator;
        Config configuration;
         public:
        BaseDriver(const std::filesystem::path& configPath, unsigned int hostBufferSize) {
            configuration = createConfigFromPath(configPath);
            accelerator = Accelerator(configuration.deviceWrappers, hostBufferSize);

        };
        BaseDriver(BaseDriver&&) noexcept = default;
        BaseDriver(const BaseDriver&) noexcept = delete;
        BaseDriver& operator=(BaseDriver&&) noexcept = default;
        BaseDriver& operator=(const BaseDriver&) = delete;
        virtual ~BaseDriver() = default;
    };

}  // namespace Finn

#endif  // BASEDRIVER_H