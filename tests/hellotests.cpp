#include "gtest/gtest.h"
#include "modelA.h"

TEST(HelloTests, testHello) {
    ASSERT_STREQ("Hello Jim", generateHelloString("Jim").c_str());
}