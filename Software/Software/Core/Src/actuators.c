#include "actuators.h"
#include "tim.h"

#define PUMP_PORT    GPIOC
#define PUMP_PIN     GPIO_PIN_2

#define FAN_PORT     GPIOC
#define FAN_PIN      GPIO_PIN_1

#define PEST_LAMP_PORT GPIOD
#define PEST_LAMP_PIN  GPIO_PIN_13

#define BUZZ_PORT    GPIOA
#define BUZZ_PIN     GPIO_PIN_0

#define BUZZ_TIMER_CLK_HZ       1000000U
#define BUZZ_TIMER_PRESCALER    83U
#define BUZZ_LOCUST_FREQ_HZ     4200U
#define BUZZ_CABBAGE_FREQ_HZ    2800U
#define BUZZER_USE_PWM_TONE     0U

#define SERVO_MIN    500U
#define SERVO_MAX    2500U
#define SERVO_HALF   90U
#define SERVO_FULL   180U
#define SERVO_CLOSE  0U

#if BUZZER_USE_PWM_TONE
static TIM_HandleTypeDef htim2_buzzer;
static uint8_t buzzer_pwm_ready = 0U;
#endif
static uint8_t buzzer_mode = 0U;
static uint8_t buzzer_output_on = 0U;
static uint32_t buzzer_last_toggle_tick = 0U;
static uint8_t pump_state = 0U;
static uint8_t fan_state = 0U;
static uint8_t pest_lamp_state = 0U;
static uint8_t sky_window_state = 0U;

static void Buzzer_PinAsGpioOff(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin = BUZZ_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BUZZ_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(BUZZ_PORT, BUZZ_PIN, GPIO_PIN_SET);
    buzzer_output_on = 0U;
}

static void Buzzer_GpioSetOutput(uint8_t on)
{
    HAL_GPIO_WritePin(BUZZ_PORT, BUZZ_PIN, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
    buzzer_output_on = on ? 1U : 0U;
}

static void Buzzer_PwmInit(void)
{
#if BUZZER_USE_PWM_TONE
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    TIM_OC_InitTypeDef sConfigOC = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();

    GPIO_InitStruct.Pin = BUZZ_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(BUZZ_PORT, &GPIO_InitStruct);

    htim2_buzzer.Instance = TIM2;
    htim2_buzzer.Init.Prescaler = BUZZ_TIMER_PRESCALER;
    htim2_buzzer.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2_buzzer.Init.Period = (BUZZ_TIMER_CLK_HZ / BUZZ_LOCUST_FREQ_HZ) - 1U;
    htim2_buzzer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2_buzzer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_PWM_Init(&htim2_buzzer) != HAL_OK) {
        Buzzer_PinAsGpioOff();
        buzzer_pwm_ready = 0U;
        return;
    }

    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0U;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_LOW;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim2_buzzer, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) {
        Buzzer_PinAsGpioOff();
        buzzer_pwm_ready = 0U;
        return;
    }

    if (HAL_TIM_PWM_Start(&htim2_buzzer, TIM_CHANNEL_1) != HAL_OK) {
        Buzzer_PinAsGpioOff();
        buzzer_pwm_ready = 0U;
        return;
    }

    buzzer_pwm_ready = 1U;
#else
    Buzzer_PinAsGpioOff();
#endif
}

static void Buzzer_SetFrequency(uint32_t freq_hz)
{
#if BUZZER_USE_PWM_TONE
    uint32_t period;

    if (!buzzer_pwm_ready || freq_hz == 0U) {
        if (buzzer_pwm_ready) {
            __HAL_TIM_SET_COMPARE(&htim2_buzzer, TIM_CHANNEL_1, 0U);
        } else {
            HAL_GPIO_WritePin(BUZZ_PORT, BUZZ_PIN, GPIO_PIN_SET);
        }
        return;
    }

    period = (BUZZ_TIMER_CLK_HZ / freq_hz) - 1U;
    __HAL_TIM_SET_AUTORELOAD(&htim2_buzzer, period);
    __HAL_TIM_SET_COMPARE(&htim2_buzzer, TIM_CHANNEL_1, (period + 1U) / 2U);
    __HAL_TIM_SET_COUNTER(&htim2_buzzer, 0U);
    htim2_buzzer.Instance->EGR = TIM_EVENTSOURCE_UPDATE;
#else
    if (freq_hz == 0U) {
        Buzzer_GpioSetOutput(0U);
    } else {
        Buzzer_GpioSetOutput(1U);
    }
#endif
}

static void Servo_SetPulse(uint16_t pulse)
{
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, pulse);
}

static uint16_t Servo_AngleToPulse(uint8_t angle_deg)
{
    if (angle_deg > 180U) {
        angle_deg = 180U;
    }
    return (uint16_t)(SERVO_MIN + (uint32_t)angle_deg * (SERVO_MAX - SERVO_MIN) / 180U);
}

void Actuators_Init(void)
{
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
    Buzzer_PwmInit();
    SkyWindow_Close();
    Pump_Off();
    Fan_Off();
    PestLamp_Off();
    Buzzer_SetMode(0U);
}

void Actuators_GetState(ActuatorState_t *state)
{
    if (state == NULL) {
        return;
    }

    state->pump_on = pump_state;
    state->fan_on = fan_state;
    state->pest_lamp_on = pest_lamp_state;
    state->buzzer_mode = buzzer_mode;
    state->sky_window_state = sky_window_state;
}

void Pump_On(void)
{
    HAL_GPIO_WritePin(PUMP_PORT, PUMP_PIN, GPIO_PIN_SET);
    pump_state = 1U;
}

void Pump_Off(void)
{
    HAL_GPIO_WritePin(PUMP_PORT, PUMP_PIN, GPIO_PIN_RESET);
    pump_state = 0U;
}

void Pump_Toggle(void)
{
    if (pump_state) {
        Pump_Off();
    } else {
        Pump_On();
    }
}

void Fan_On(void)
{
    HAL_GPIO_WritePin(FAN_PORT, FAN_PIN, GPIO_PIN_SET);
    fan_state = 1U;
}

void Fan_Off(void)
{
    HAL_GPIO_WritePin(FAN_PORT, FAN_PIN, GPIO_PIN_RESET);
    fan_state = 0U;
}

void Fan_Toggle(void)
{
    if (fan_state) {
        Fan_Off();
    } else {
        Fan_On();
    }
}

void PestLamp_On(void)
{
    HAL_GPIO_WritePin(PEST_LAMP_PORT, PEST_LAMP_PIN, GPIO_PIN_SET);
    pest_lamp_state = 1U;
}

void PestLamp_Off(void)
{
    HAL_GPIO_WritePin(PEST_LAMP_PORT, PEST_LAMP_PIN, GPIO_PIN_RESET);
    pest_lamp_state = 0U;
}

void PestLamp_Toggle(void)
{
    if (pest_lamp_state) {
        PestLamp_Off();
    } else {
        PestLamp_On();
    }
}

void Buzzer_On(void)
{
    Buzzer_SetMode(1U);
}

void Buzzer_Off(void)
{
    Buzzer_SetMode(0U);
}

void Buzzer_Beep(uint16_t ms)
{
    Buzzer_SetMode(1U);
    HAL_Delay(ms);
    Buzzer_SetMode(0U);
}

void Buzzer_SetMode(uint8_t mode)
{
    if (mode > 2U) {
        mode = 0U;
    }

    buzzer_mode = mode;
    buzzer_last_toggle_tick = HAL_GetTick();
    if (buzzer_mode == 1U) {
        Buzzer_SetFrequency(BUZZ_LOCUST_FREQ_HZ);
    } else if (buzzer_mode == 2U) {
        Buzzer_SetFrequency(BUZZ_CABBAGE_FREQ_HZ);
    } else {
        Buzzer_SetFrequency(0U);
    }
}

void Buzzer_Process(void)
{
#if !BUZZER_USE_PWM_TONE
    uint32_t interval_ms;
    uint32_t now;

    if (buzzer_mode == 0U) {
        if (buzzer_output_on) {
            Buzzer_GpioSetOutput(0U);
        }
        return;
    }

    interval_ms = (buzzer_mode == 1U) ? 220U : 80U;
    now = HAL_GetTick();
    if (now - buzzer_last_toggle_tick >= interval_ms) {
        buzzer_last_toggle_tick = now;
        Buzzer_GpioSetOutput(!buzzer_output_on);
    }
#endif
}

void SkyWindow_Open(void)
{
    SkyWindow_FullOpen();
}

void SkyWindow_HalfOpen(void)
{
    Servo_SetPulse(Servo_AngleToPulse(SERVO_HALF));
    sky_window_state = 1U;
}

void SkyWindow_FullOpen(void)
{
    Servo_SetPulse(Servo_AngleToPulse(SERVO_FULL));
    sky_window_state = 2U;
}

void SkyWindow_Close(void)
{
    Servo_SetPulse(Servo_AngleToPulse(SERVO_CLOSE));
    sky_window_state = 0U;
}

void SkyWindow_SetState(uint8_t state)
{
    if (state >= 2U) {
        SkyWindow_FullOpen();
    } else if (state == 1U) {
        SkyWindow_HalfOpen();
    } else {
        SkyWindow_Close();
    }
}

void SkyWindow_SetAngle(uint8_t angle_deg)
{
    Servo_SetPulse(Servo_AngleToPulse(angle_deg));
    if (angle_deg == 0U) {
        sky_window_state = 0U;
    } else if (angle_deg < 135U) {
        sky_window_state = 1U;
    } else {
        sky_window_state = 2U;
    }
}
