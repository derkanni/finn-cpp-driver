/**
 * @file FinnUtilsTest.cpp
 * @author Bjarne Wintermann (bjarne.wintermann@uni-paderborn.de) and others
 * @brief Unittest for the collection of Finn utility functions
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#include <FINNCppDriver/utils/FinnUtils.h>

#include "gtest/gtest.h"


TEST(UtilsTest, FinnUtilsTest) {
    //* Shape to elements
    shape_t myShape = shape_t{1, 3, 120};
    EXPECT_EQ(FinnUtils::shapeToElements(myShape), 360);
    EXPECT_EQ(FinnUtils::shapeToElements(shape_t{}), 0);

    //* Ceil
    EXPECT_EQ(FinnUtils::ceil(0.1f), 1);
    EXPECT_EQ(FinnUtils::ceil(0.7f), 1);
    EXPECT_EQ(FinnUtils::ceil(static_cast<float>(0)), 0);
    EXPECT_EQ(FinnUtils::ceil(1.1f), 2);

    //* Innermost dimension
    EXPECT_EQ(FinnUtils::innermostDimension(myShape), 120);

    //* Get actual buffer size
    EXPECT_EQ(FinnUtils::getActualBufferSize(120), 4096);
    EXPECT_EQ(FinnUtils::getActualBufferSize(0), 4096);
    EXPECT_EQ(FinnUtils::getActualBufferSize(4095), 4096);
    EXPECT_EQ(FinnUtils::getActualBufferSize(4096), 4096);
    EXPECT_EQ(FinnUtils::getActualBufferSize(5000), 8192);
    EXPECT_EQ(FinnUtils::getActualBufferSize(8200), 16384);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}