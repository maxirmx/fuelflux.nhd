#include <array>
#include <cstdint>

#include "gtest/gtest.h"
#include "st7565.h"

TEST(St7565Test, NhdDefaultSequenceMatchesDatasheet) {
    constexpr std::array<uint8_t, 9> expected = {
        0xA0, 0xAE, 0xC0, 0xA2, 0x2F, 0x26, 0x81, 0x11, 0xAF,
    };

    EXPECT_EQ(St7565::nhd_c12864a1z_init_sequence(), expected);
}

TEST(St7565Test, NhdSequenceSupportsFixtureOrientation) {
    const auto sequence = St7565::nhd_c12864a1z_init_sequence(
        0x2A, true, true);

    EXPECT_EQ(sequence[0], 0xA1);
    EXPECT_EQ(sequence[2], 0xC8);
    EXPECT_EQ(sequence[7], 0x2A);
}

TEST(St7565Test, NhdSequenceMasksContrastToSixBits) {
    const auto sequence = St7565::nhd_c12864a1z_init_sequence(0xFF);

    EXPECT_EQ(sequence[7], 0x3F);
}
