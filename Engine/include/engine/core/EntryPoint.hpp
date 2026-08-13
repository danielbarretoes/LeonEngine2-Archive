#pragma once

#include "engine/core/Application.hpp"
#include "engine/core/Log.hpp"

extern Leon::FApplication* Leon::CreateApplication();

int main(int argc, char** argv) {
    Leon::FLog::Init();
    auto app = Leon::CreateApplication();
    app->Run();
    delete app;
    return 0;
}
