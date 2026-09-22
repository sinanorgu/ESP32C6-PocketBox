#include <unity.h>
#include "GalleryImage.hpp"
#include <cstring>

namespace {
void put32(uint8_t* p, uint32_t n) {
    for (int i = 0; i < 4; ++i) p[i] = uint8_t(n >> (i * 8));
}

void bmpHeader(uint8_t* h) {
    memset(h, 0, 54);
    h[0] = 'B'; h[1] = 'M';
    put32(h + 10, 54); put32(h + 14, 40);
    put32(h + 18, 3); put32(h + 22, 2);
    h[26] = 1; h[28] = 24;
}

void test_dimension_limits() {
    TEST_ASSERT_TRUE(gallery::validDimensions(1024, 1024));
    TEST_ASSERT_FALSE(gallery::validDimensions(0, 10));
    TEST_ASSERT_FALSE(gallery::validDimensions(10, 0));
    TEST_ASSERT_FALSE(gallery::validDimensions(1025, 1));
    TEST_ASSERT_FALSE(gallery::validDimensions(1, 1025));
    TEST_ASSERT_FALSE(gallery::validDimensions(UINT32_MAX, UINT32_MAX));
}

void test_fit_preserves_aspect_and_bounds() {
    auto landscape = gallery::fit(800, 400, 320, 147);
    TEST_ASSERT_EQUAL_INT(294, landscape.width);
    TEST_ASSERT_EQUAL_INT(147, landscape.height);
    auto portrait = gallery::fit(400, 800, 320, 147);
    TEST_ASSERT_EQUAL_INT(73, portrait.width);
    TEST_ASSERT_EQUAL_INT(147, portrait.height);
    auto small = gallery::fit(10, 20, 320, 147);
    TEST_ASSERT_EQUAL_INT(10, small.width);
    TEST_ASSERT_EQUAL_INT(20, small.height);
    auto thin = gallery::fit(1, 1024, 320, 147);
    TEST_ASSERT_EQUAL_INT(1, thin.width);
    TEST_ASSERT_EQUAL_INT(147, thin.height);
    TEST_ASSERT_EQUAL_INT(0, gallery::fit(0, 10, 320, 147).width);
}

void test_block_sampling_has_no_gaps_or_out_of_bounds_access() {
    // Odd image widths and JPEG MCU boundaries must match whole-row resizing.
    const int widths[] = {1, 3, 17, 319, 321, 724, 1024};
    const int blocks[] = {1, 8, 16, 48};
    for (int width : widths) {
        int destination = width < 320 ? width : 320;
        for (int block : blocks) {
            int visits[320] = {};
            for (int start = 0; start < width; start += block) {
                int end = start + block < width ? start + block : width;
                int left = gallery::boundary(start, width, destination);
                int right = gallery::boundary(end, width, destination);
                for (int x = left; x < right; ++x) {
                    TEST_ASSERT_TRUE(x >= 0 && x < destination);
                    int sample = x * width / destination;
                    TEST_ASSERT_TRUE(sample >= start && sample < end);
                    ++visits[x];
                }
            }
            for (int x = 0; x < destination; ++x) TEST_ASSERT_EQUAL_INT(1, visits[x]);
        }
    }
}

void test_bmp_padding_and_top_down() {
    uint8_t h[54]; bmpHeader(h);
    gallery::BmpInfo info{};
    TEST_ASSERT_TRUE(gallery::parseBmp(h, sizeof(h), 78, info));
    TEST_ASSERT_EQUAL_UINT32(12, info.stride);
    TEST_ASSERT_EQUAL_UINT8(3, info.bytesPerPixel);
    TEST_ASSERT_FALSE(info.topDown);
    put32(h + 22, uint32_t(-2)); h[28] = 32;
    TEST_ASSERT_TRUE(gallery::parseBmp(h, sizeof(h), 78, info));
    TEST_ASSERT_TRUE(info.topDown);
    TEST_ASSERT_EQUAL_UINT32(2, info.height);
    TEST_ASSERT_EQUAL_UINT8(4, info.bytesPerPixel);
}

void test_bmp_rejects_truncation_and_invalid_offsets() {
    uint8_t h[54]; bmpHeader(h);
    gallery::BmpInfo info{};
    TEST_ASSERT_FALSE(gallery::parseBmp(h, 53, 78, info));
    TEST_ASSERT_FALSE(gallery::parseBmp(h, 54, 77, info));
    put32(h + 10, 20);
    TEST_ASSERT_FALSE(gallery::parseBmp(h, 54, 78, info));
    put32(h + 10, UINT32_MAX);
    TEST_ASSERT_FALSE(gallery::parseBmp(h, 54, 78, info));
    bmpHeader(h); put32(h + 14, UINT32_MAX);
    TEST_ASSERT_FALSE(gallery::parseBmp(h, 54, 78, info));
}

void test_bmp_rejects_unsupported_and_oversized_inputs() {
    uint8_t h[54]; gallery::BmpInfo info{};
    bmpHeader(h); put32(h + 30, 1);
    TEST_ASSERT_FALSE(gallery::parseBmp(h, 54, 78, info));
    bmpHeader(h); h[28] = 16;
    TEST_ASSERT_FALSE(gallery::parseBmp(h, 54, 78, info));
    bmpHeader(h); put32(h + 18, 1025);
    TEST_ASSERT_FALSE(gallery::parseBmp(h, 54, 100000, info));
    bmpHeader(h); put32(h + 22, 0x80000000U);
    TEST_ASSERT_FALSE(gallery::parseBmp(h, 54, 78, info));
    bmpHeader(h);
    TEST_ASSERT_FALSE(gallery::parseBmp(h, 54, gallery::MaxFileBytes + 1, info));
}

void test_png_preflight_rejects_unsafe_headers() {
    uint8_t h[33] = {};
    h[11] = 13;
    memcpy(h + 12, "IHDR", 4);
    h[18] = 4; h[22] = 4; // 1024 x 1024, RGBA, 8-bit channels
    h[24] = 8; h[25] = 6;
    TEST_ASSERT_NULL(gallery::checkPngHeader(h, sizeof(h)));
    TEST_ASSERT_NOT_NULL(gallery::checkPngHeader(h, 32));
    h[19] = 1;
    TEST_ASSERT_NOT_NULL(gallery::checkPngHeader(h, sizeof(h)));
    h[19] = 0; h[16] = 0xff;
    TEST_ASSERT_NOT_NULL(gallery::checkPngHeader(h, sizeof(h)));
    h[16] = 0; h[24] = 16;
    TEST_ASSERT_NOT_NULL(gallery::checkPngHeader(h, sizeof(h)));
    h[24] = 8; h[28] = 1;
    TEST_ASSERT_NOT_NULL(gallery::checkPngHeader(h, sizeof(h)));
    h[28] = 0; h[25] = 7;
    TEST_ASSERT_NOT_NULL(gallery::checkPngHeader(h, sizeof(h)));
    h[25] = 6; h[24] = 1;
    TEST_ASSERT_NOT_NULL(gallery::checkPngHeader(h, sizeof(h)));
    h[25] = 3;
    TEST_ASSERT_NULL(gallery::checkPngHeader(h, sizeof(h)));
}
} // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_dimension_limits);
    RUN_TEST(test_fit_preserves_aspect_and_bounds);
    RUN_TEST(test_block_sampling_has_no_gaps_or_out_of_bounds_access);
    RUN_TEST(test_bmp_padding_and_top_down);
    RUN_TEST(test_bmp_rejects_truncation_and_invalid_offsets);
    RUN_TEST(test_bmp_rejects_unsupported_and_oversized_inputs);
    RUN_TEST(test_png_preflight_rejects_unsafe_headers);
    return UNITY_END();
}
