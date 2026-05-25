#include "utest.h"
#include "../communication/utils/crc.h"

/* CRC-16/MCRF4XX check value for "123456789" = 0x6F91 */
static void test_crc_check_value(void)
{
    const uint8_t data[] = "123456789";
    uint16_t crc = crc16_mcrf4xx(data, 9);
    UTEST_ASSERT_EQ(0x6F91, crc);
    UTEST_PASS();
}

static void test_crc_empty(void)
{
    uint16_t crc = crc16_mcrf4xx(NULL, 0);
    UTEST_ASSERT_EQ(0xFFFF, crc);
    UTEST_PASS();
}

static void test_crc_single_byte(void)
{
    uint8_t byte = 0x00;
    uint16_t crc = crc16_mcrf4xx(&byte, 1);
    UTEST_ASSERT_NEQ(0xFFFF, crc);
    UTEST_PASS();
}

static void test_crc_incremental_matches_bulk(void)
{
    const uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint16_t bulk = crc16_mcrf4xx(data, sizeof(data));

    uint16_t inc = 0xFFFFu;
    for (int i = 0; i < (int)sizeof(data); i++) {
        inc = crc16_mcrf4xx_update(inc, data[i]);
    }
    UTEST_ASSERT_EQ(bulk, inc);
    UTEST_PASS();
}

static void test_crc_different_data_different_crc(void)
{
    const uint8_t a[] = {0xAA, 0xBB};
    const uint8_t b[] = {0xAA, 0xBC};
    UTEST_ASSERT_NEQ(crc16_mcrf4xx(a, 2), crc16_mcrf4xx(b, 2));
    UTEST_PASS();
}

void run_crc_tests(void)
{
    UTEST_SUITE("CRC-16/MCRF4XX");
    UTEST_RUN(test_crc_check_value);
    UTEST_RUN(test_crc_empty);
    UTEST_RUN(test_crc_single_byte);
    UTEST_RUN(test_crc_incremental_matches_bulk);
    UTEST_RUN(test_crc_different_data_different_crc);
}
