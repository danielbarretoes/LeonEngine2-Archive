#pragma once

#include "Core/Base.hpp"
#include <memory>
#include <string>

namespace Leon {

    /**
     * @brief Base class for all objects in the Unreal-aligned LeonEngine2 object model.
     *
     * Provides identity, shared reference semantics, and base naming.
     */
    class UObject : public std::enable_shared_from_this<UObject> {
    public:
        UObject() = default;
        explicit UObject(const std::string& InName) : Name(InName) {}
        virtual ~UObject() = default;

        const std::string& GetName() const { return Name; }
        virtual void SetName(const std::string& InName) { Name = InName; }

    protected:
        std::string Name;
    };

} // namespace Leon
