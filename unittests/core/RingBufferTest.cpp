#include <algorithm>
#include <functional>
#include <random>
#include <thread>
#include <vector>

#include "../../src/utils/FinnUtils.h"
#include "../../src/utils/Logger.h"
#include "../../src/utils/RingBuffer.hpp"
#include "gtest/gtest.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"


TEST(DummyTest, DT) { EXPECT_TRUE(true); }


// Globals
using RB = RingBuffer<uint8_t>;
const size_t parts = 10;
const size_t elementsPerPart = 30;
auto filler = FinnUtils::BufferFiller(0, 255);


TEST(RBTest, RBInitTest) {
    auto rb = RB(parts, elementsPerPart);

    // Pointers
    EXPECT_EQ(rb.testGetHeadPointer(), 0);
    EXPECT_EQ(rb.testGetReadPointer(), 0);
    EXPECT_FALSE(rb.testGetValidity(0));

    // Sizes
    EXPECT_EQ(rb.size(SIZE_SPECIFIER::PARTS), parts);
    EXPECT_EQ(rb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART), elementsPerPart);
    EXPECT_EQ(rb.size(SIZE_SPECIFIER::BYTES), parts * elementsPerPart * 1);
    EXPECT_EQ(rb.size(SIZE_SPECIFIER::ELEMENTS), parts * elementsPerPart);

    // Initial values
    EXPECT_EQ(rb.countValidParts(), 0);
    for (auto elem : rb.testGetAsVector(0)) {
        EXPECT_EQ(elem, 0);
    }
    EXPECT_FALSE(rb.isFull());
}

TEST(RBTest, RBStoreReadTest) {
    auto rb = RB(parts, elementsPerPart);

    // Store data
    std::vector<uint8_t> data;
    data.resize(elementsPerPart);

    // FIll until all spots are valid
    for (size_t i = 0; i < parts; i++) {
        filler.fillRandom(data);
        EXPECT_TRUE(rb.store(data.begin(), data.end()));
    }

    // Confirm that the head pointer wrapped around
    EXPECT_EQ(rb.testGetHeadPointer(), 0);
    EXPECT_EQ(rb.testGetReadPointer(), 0);

    // Confirm that no new data can be stored until some data is read
    filler.fillRandom(data);
    EXPECT_FALSE(rb.store(data.begin(), data.end()));

    // Read two entries
    uint8_t buf[elementsPerPart];
    EXPECT_TRUE(rb.read(buf, elementsPerPart));
    EXPECT_TRUE(rb.read(buf, elementsPerPart));

    // Check pointer positions
    EXPECT_EQ(rb.testGetHeadPointer(), 0);
    EXPECT_EQ(rb.testGetReadPointer(), 2);
}


int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}