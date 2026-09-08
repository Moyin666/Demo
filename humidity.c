#include "humidity.h"

uint16_t Soil_humi_GetData(void)
{
	uint32_t  HumiData = 0;
	HumiData = Get_ADC(&hadc1);

	return 100 - (double)HumiData/40.95;
}
