#include "utest.h"

/* Suite entry points */
void run_crc_tests(void);
void run_queue_tests(void);
void run_protocol_tests(void);
void run_comm_manager_tests(void);

int main(void)
{
    run_crc_tests();
    run_queue_tests();
    run_protocol_tests();
    run_comm_manager_tests();

    UTEST_REPORT_AND_EXIT();
}
