#include "internal_temp.h"
#include "stm32f4xx_hal.h"

// We definiëren hadc1 hier OF extern, afhankelijk van je projectstructuur.
// Vaak doet CubeMX dit in mx_adc.c, dus let op dubbele definities!
extern ADC_HandleTypeDef hadc1;  

void InternalTemp_ADC_Init(void)
{
  // Als je CubeMX gebruikt, heb je meestal MX_ADC1_Init() i.p.v. handmatige init.
  // Anders doe je het handmatig zoals in je code:
  
  __HAL_RCC_ADC1_CLK_ENABLE();

  ADC_ChannelConfTypeDef sConfig = {0};
  hadc1.Instance = ADC1;
  hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode          = DISABLE;
  hadc1.Init.ContinuousConvMode    = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion       = 1;
  hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
  hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;

  HAL_ADC_Init(&hadc1);

  sConfig.Channel      = ADC_CHANNEL_TEMPSENSOR;
  sConfig.Rank         = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
  HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

float InternalTemp_ReadCelsius(void)
{
  HAL_ADC_Start(&hadc1);
  if (HAL_ADC_PollForConversion(&hadc1, 100) != HAL_OK) {
    return -999.0f;
  }
  uint32_t raw = HAL_ADC_GetValue(&hadc1);
  HAL_ADC_Stop(&hadc1);

  float vsense = (float)raw * 3.3f / 4095.0f;
  float temperature = ((vsense - 0.76f) / 0.0025f) + 25.0f;
  return temperature;
}
