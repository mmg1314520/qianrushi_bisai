#include "dht11.h"

#define DHT11_GPIO_Port GPIOC
#define DHT11_Pin       GPIO_PIN_0

static uint8_t DHT11_DbgStep = 0;
static uint8_t DHT11_RawData[5] = {0};

static void DHT11_DelayUs(uint32_t us)
{
    uint32_t start = SysTick->VAL;
    uint32_t ticks = us * (SystemCoreClock / 1000000U);
    uint32_t reload = SysTick->LOAD + 1U;
    uint32_t elapsed = 0;
    uint32_t now;

    while (elapsed < ticks) {
        now = SysTick->VAL;
        if (start >= now) {
            elapsed += start - now;
        } else {
            elapsed += start + (reload - now);
        }
        start = now;
    }
}

static uint8_t DHT11_ReadPinFast(void)
{
    return (GPIOC->IDR & DHT11_Pin) ? 1U : 0U;
}

static void DHT11_Mode(uint8_t output)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = DHT11_Pin;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    if (output) {
        gpio.Mode = GPIO_MODE_OUTPUT_PP;
        gpio.Pull = GPIO_NOPULL;
    } else {
        gpio.Mode = GPIO_MODE_INPUT;
        gpio.Pull = GPIO_PULLUP;
    }
    HAL_GPIO_Init(DHT11_GPIO_Port, &gpio);
}

static uint8_t DHT11_WaitLevel(uint8_t level, uint16_t timeout_us)
{
    while (DHT11_ReadPinFast() != level) {
        if (timeout_us == 0U) {
            return 0;
        }
        timeout_us--;
        DHT11_DelayUs(1);
    }
    return 1;
}

static void DHT11_Rst(void)
{
    DHT11_Mode(1);
    HAL_GPIO_WritePin(DHT11_GPIO_Port, DHT11_Pin, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(DHT11_GPIO_Port, DHT11_Pin, GPIO_PIN_SET);
    DHT11_DelayUs(13);
}

static uint8_t DHT11_Check(void)
{
    DHT11_Mode(0);

    if (!DHT11_WaitLevel(0U, 100U)) {
        DHT11_DbgStep = 2;
        return 1;
    }
    if (!DHT11_WaitLevel(1U, 100U)) {
        DHT11_DbgStep = 3;
        return 1;
    }
    return 0;
}

static uint8_t DHT11_ReadBit(uint8_t *bit)
{
    if (!DHT11_WaitLevel(0U, 100U)) {
        DHT11_DbgStep = 4;
        return 4;
    }
    if (!DHT11_WaitLevel(1U, 100U)) {
        DHT11_DbgStep = 5;
        return 5;
    }

    DHT11_DelayUs(40);
    *bit = DHT11_ReadPinFast();
    return 0;
}

static uint8_t DHT11_ReadByte(uint8_t *byte)
{
    uint8_t i;
    uint8_t bit;
    uint8_t value = 0;
    uint8_t ret;

    for (i = 0; i < 8U; i++) {
        value <<= 1;
        ret = DHT11_ReadBit(&bit);
        if (ret != 0U) {
            return ret;
        }
        value |= bit;
    }

    *byte = value;
    return 0;
}

uint8_t DHT11_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    gpio.Pin = DHT11_Pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_GPIO_Port, &gpio);
    HAL_GPIO_WritePin(DHT11_GPIO_Port, DHT11_Pin, GPIO_PIN_SET);

    DHT11_Rst();
    return (DHT11_Check() == 0U) ? 0U : 1U;
}

uint8_t DHT11_ReadData(uint8_t *humi, uint8_t *temp)
{
    uint8_t buf[5] = {0};
    uint8_t i;
    uint8_t ret = 0;
    uint32_t primask;

    if (humi == NULL || temp == NULL) {
        return 1;
    }

    DHT11_DbgStep = 0;
    DHT11_Rst();

    primask = __get_PRIMASK();
    __disable_irq();

    if (DHT11_Check() != 0U) {
        ret = DHT11_DbgStep;
    } else {
        for (i = 0; i < 5U; i++) {
            ret = DHT11_ReadByte(&buf[i]);
            if (ret != 0U) {
                break;
            }
        }
    }

    if (primask == 0U) {
        __enable_irq();
    }

    if (ret != 0U) {
        return ret;
    }

    if ((uint8_t)(buf[0] + buf[1] + buf[2] + buf[3]) != buf[4]) {
        DHT11_DbgStep = 6;
        return 6;
    }

    for (i = 0; i < 5U; i++) {
        DHT11_RawData[i] = buf[i];
    }
    *humi = buf[0];
    *temp = buf[2];
    return 0;
}

uint8_t DHT11_GetDbgStep(void)
{
    return DHT11_DbgStep;
}

const uint8_t* DHT11_GetRawData(void)
{
    return DHT11_RawData;
}
