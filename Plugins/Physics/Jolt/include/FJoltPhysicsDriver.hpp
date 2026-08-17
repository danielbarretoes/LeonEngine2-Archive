#pragma once

#include "Physics/IPhysicsScene.hpp"

namespace Leon {

    class FJoltPhysicsDriver {
    public:
        static void Register();
        static const char* GetJoltVersion();
    };

} // namespace Leon
