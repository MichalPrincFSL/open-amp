#include "fsl_mu.h"
#include "rpmsg.h"
#include "unity.h"
#include "board.h"

#if defined(FSL_FEATURE_MU_SIDE_A)
#include "app_core0.h"
#elif defined(FSL_FEATURE_MU_SIDE_B)
#include "app_core1.h"
#endif

#if defined(FSL_FEATURE_MU_SIDE_A)
#define MU_INSTANCE MU0_A
#elif defined(FSL_FEATURE_MU_SIDE_B)
#define MU_INSTANCE MU0_B
#endif

#if defined(FSL_RTOS_FREE_RTOS)
#include "Freertos.h"
#include "task.h"
#endif

#define TEST_TASK_STACK_SIZE 400

extern void run_tests();

void setUp(void)
{
}

void tearDown(void)
{
}

void run_test_suite(void *unused)
{
    BOARD_InitHardware();
    MU_Init(MU_INSTANCE);
#if defined(FSL_FEATURE_MU_SIDE_A)
    MU_BootCoreB(MU_INSTANCE, kMU_CoreBootFromImem);
#endif
    UnityBegin(NULL);
    run_tests();
    UnityEnd();
    while (1)
        ;
}

#if defined(FSL_RTOS_FREE_RTOS)
TaskHandle_t test_task_handle = NULL;
int main(void)
{
    int result =
        xTaskCreate(run_test_suite, "TEST_TASK", TEST_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &test_task_handle);
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
