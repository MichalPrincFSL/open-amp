#include "unity.h"
#include "board.h"
#include "mu_imx.h"
#include "machine_system.h"

#if defined(FSL_RTOS_FREE_RTOS)
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#endif

#define TEST_TASK_STACK_SIZE 300

/*
 * APP decided interrupt priority
 */
#define APP_MU_IRQ_PRIORITY 3

extern void run_tests();

void setUp(void)
{
}

void tearDown(void)
{
}

/*
 * MU Interrrupt ISR
 */
void BOARD_MU_HANDLER(void)
{
    /* Check whether RPMsg related message arrived */
    rpmsg_handler();

    /* Check other MU message receivers >here< (MU_TryReceiveMsg()) */
}

void init_hardware(void)
{
    hardware_init();

    /*
     * Prepare for the MU Interrupt
     *  MU must be initialized before rpmsg init is called
     */
    MU_Init(BOARD_MU_BASE_ADDR);
    NVIC_SetPriority(BOARD_MU_IRQ_NUM, APP_MU_IRQ_PRIORITY);
    NVIC_EnableIRQ(BOARD_MU_IRQ_NUM);
    PRINTF("Hardware initialized\r\n");
}

void run_test_suite(void *unused)
{
    init_hardware();
    UnityBegin(NULL);
    PRINTF("Run tests\r\n");
    run_tests();
    UnityEnd();
    while(1);
}


#if defined(FSL_RTOS_FREE_RTOS)
    TaskHandle_t test_task_handle = NULL;
    int main(void)
    {
        int result = xTaskCreate(run_test_suite, "TEST_TASK", TEST_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &test_task_handle);
        assert(pdPASS == result);
        vTaskStartScheduler();
        return 0;
    }
#else
    int main(void)
    {
        run_test_suite(NULL);
        return 0;
    }
#endif


