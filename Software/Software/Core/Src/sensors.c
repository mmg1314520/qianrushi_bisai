#include "sensors.h"
#include "dht11.h"
#include "co2.h"
#include "adc.h"

static uint16_t Sensors_ReadAdcChannel(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    uint16_t value = 0;

    sConfig.Channel = channel;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        return 0;
    }

    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        value = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);

    return value;
}

void Sensors_Init(void)
{
    DHT11_Init();
    CO2_Init();
}

void Sensors_ReadAll(SensorData_t *data)
{
    uint8_t humi = 0;
    uint8_t temp = 0;
    uint8_t dht_ret;

    if (data == NULL) {
        return;
    }

    dht_ret = DHT11_ReadData(&humi, &temp);
    data->dht11_status = dht_ret;
    data->dht11_ok = (dht_ret == 0U) ? 1U : 0U;
    if (dht_ret == 0U) {
        data->temperature = (int16_t)temp * 10;
        data->humidity = (int16_t)humi * 10;
    } else {
        data->temperature = 0;
        data->humidity = 0;
    }

    data->co2_ok = CO2_Read(&data->co2_ppm);
    if (!data->co2_ok) {
        data->co2_ppm = 0;
    }

    data->light_adc = Sensors_ReadAdcChannel(ADC_CHANNEL_4);
    data->soil_adc = Sensors_ReadAdcChannel(ADC_CHANNEL_5);
}
