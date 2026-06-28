#include "main.h"
#include "usart.h"
#include <rtthread.h>
#include <rthw.h>
#include <string.h>

extern void SystemClock_Config(void);

static uint8_t rt_heap[48 * 1024];

void rt_hw_board_init(void)
{
    HAL_Init();
    SystemClock_Config();

    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / RT_TICK_PER_SECOND);

    rt_system_heap_init(rt_heap, rt_heap + sizeof(rt_heap));
}

void rt_hw_console_output(const char *str)
{
    if (str == RT_NULL) {
        return;
    }

    if (huart1.Instance != RT_NULL) {
        HAL_UART_Transmit(&huart1, (uint8_t *)str, (uint16_t)strlen(str), 1000);
    }
}

void HAL_Delay(uint32_t Delay)
{
    uint32_t tickstart = HAL_GetTick();

    if (rt_scheduler_is_available()) {
        rt_thread_mdelay((rt_int32_t)Delay);
        return;
    }

    while ((HAL_GetTick() - tickstart) < Delay) {
    }
}
