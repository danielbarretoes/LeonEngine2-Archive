#include "RHI/IRenderDriver.hpp"
#include "Core/FLog.hpp"
#include <unordered_map>

namespace Leon {

    static std::unordered_map<ERenderAPI, TScope<IRenderDriver>>& GetDriversMap() {
        static std::unordered_map<ERenderAPI, TScope<IRenderDriver>> Drivers;
        return Drivers;
    }

    void FRenderDriverRegistry::RegisterDriver(ERenderAPI InAPI, TScope<IRenderDriver> InDriver) {
        GetDriversMap()[InAPI] = std::move(InDriver);
    }

    IRenderDriver* FRenderDriverRegistry::GetDriver(ERenderAPI InAPI) {
        auto& drivers = GetDriversMap();
        auto it = drivers.find(InAPI);
        if (it != drivers.end())
            return it->second.get();
        return nullptr;
    }

    IRenderDriver* FRenderDriverRegistry::GetActiveDriver() {
        IRenderDriver* driver = GetDriver(IRenderAPI::GetAPI());
        if (!driver) {
            LE_CORE_ERROR("No RenderDriver registered for active IRenderAPI ({0})!",
                          static_cast<int>(IRenderAPI::GetAPI()));
        }
        return driver;
    }

} // namespace Leon
