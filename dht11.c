#include "dht11.h"
#include "tim.h"

static void DHT11_PP_OUT(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.Pin = DHT11_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;	
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}


static void DHT11_UP_IN(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.Pin = DHT11_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;	
	HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

uint8_t DHT11_ReadByte(void)
{
	uint8_t i, temp = 0;

	for (i = 0; i < 8; i++)
	{
		while (DHT11_ReadPin == 0);		
		
		d_us(40);			
		
		if (DHT11_ReadPin == 1)
		{
			while (DHT11_ReadPin == 1);	
			
			temp |= (uint8_t)(0X01 << (7 - i));			
		}
		else
		{
			temp &= (uint8_t)~(0X01 << (7 - i));
		}
	}
	return temp;
}


uint8_t DHT11_ReadData(DHT11_Data_TypeDef*DHT11_Data)
{
	DHT11_PP_OUT();			
	DHT11_PULL_0;	
  d_ms(18);				
	
	DHT11_PULL_1;					
	d_us(30);	

	DHT11_UP_IN();				
	
	if (DHT11_ReadPin == 0)				
	{
		while (DHT11_ReadPin == 0);		
		
		while (DHT11_ReadPin == 1);		

		DHT11_Data->humi_int  = DHT11_ReadByte();
		DHT11_Data->humi_dec = DHT11_ReadByte();
		DHT11_Data->temp_int  = DHT11_ReadByte();
		DHT11_Data->temp_dec = DHT11_ReadByte();
		DHT11_Data->check_sum = DHT11_ReadByte();
		
		DHT11_PP_OUT();		
		DHT11_PULL_1;	
		
		
		if (DHT11_Data->check_sum == DHT11_Data->humi_int + DHT11_Data->humi_dec + DHT11_Data->temp_int + DHT11_Data->temp_dec)	
		{
			return 1;
		}		
		else
		{
			return 0;
		}
	}
	else		
	{
		return 0;
	}
}

void d_us(uint32_t us)
{
	
    __HAL_TIM_SET_COUNTER(&htim4, 0);
    
    HAL_TIM_Base_Start(&htim4);
    
    do
		{
			
		}while(__HAL_TIM_GET_COUNTER(&htim4) != us);
		HAL_TIM_Base_Stop(&htim4);
}


void d_ms(uint32_t ms)
{
    d_us(ms * 1000);
}


void DHT11_Rst(void)	   
{                 
	DHT11_PP_OUT(); 	//设置为输出
	DHT11_PULL_0; 	//拉低DQ
	d_ms(20);    	//拉低至少18ms
	DHT11_PULL_1; 	//DQ=1 
	d_us(30);     	//主机拉高20~40us
}
