#include <doctest/doctest.h>
#include "Core/FWindow.hpp"

TEST_SUITE("Window display policy") {

    TEST_CASE("720p and 1080p stay 16:9 HD") {
        unsigned int w = 1280;
        unsigned int h = 720;
        Leon::FWindowDisplayPolicy::ConstrainClientSize(w, h);
        CHECK(w == 1280);
        CHECK(h == 720);

        w = 1920;
        h = 1080;
        Leon::FWindowDisplayPolicy::ConstrainClientSize(w, h);
        CHECK(w == 1920);
        CHECK(h == 1080);
    }

    TEST_CASE("intermediate 16:9 between 720p and 1080p is kept") {
        unsigned int w = 1600;
        unsigned int h = 900;
        Leon::FWindowDisplayPolicy::ConstrainClientSize(w, h);
        CHECK(w == 1600);
        CHECK(h == 900);
    }

    TEST_CASE("4K and 16:10 clamp to 1080p 16:9") {
        unsigned int w = 2560;
        unsigned int h = 1440;
        Leon::FWindowDisplayPolicy::ConstrainClientSize(w, h);
        CHECK(w == 1920);
        CHECK(h == 1080);

        w = 1920;
        h = 1200;
        Leon::FWindowDisplayPolicy::ConstrainClientSize(w, h);
        CHECK(w == 1920);
        CHECK(h == 1080);
    }

    TEST_CASE("below 720p snaps up to default windowed size") {
        unsigned int w = 800;
        unsigned int h = 600;
        Leon::FWindowDisplayPolicy::ConstrainClientSize(w, h);
        CHECK(w == Leon::FWindowDisplayPolicy::DefaultWidth);
        CHECK(h == Leon::FWindowDisplayPolicy::DefaultHeight);
    }
}
