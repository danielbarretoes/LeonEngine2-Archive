#pragma once

#include "core/Application.hpp"
#include "core/Log.hpp"

extern Leon::FApplication* Leon::CreateApplication();

int main(int argc, char** argv) {
    Leon::FLog::Init();
    auto app = Leon::CreateApplication();
    app->Run();
    delete app;
    return 0;
}
