#include "light.h"

int val=0;
uint16_t luminance=0;
float voltage=0;
float R=0;;

uint16_t get_light(void)
{	
    val=Get_ADC(&hadc2);
    voltage=3.3*val/4095;
	  R = voltage / (3.3f - voltage) * 10000;
    luminance = 40000 * pow(R, -0.6021);
		if (luminance > 999)
	  {
		  luminance = 999;
	  }
                
 return luminance;
}
