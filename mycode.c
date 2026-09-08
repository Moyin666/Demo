#include "mycode.h"

// ========================================================================
// 1. 个人专属信息配置区
// ========================================================================
#define WIFI_SSID           "123"                   //热点名
#define WIFI_PASSWORD       "1234"						      //热点密码

#define MQTT_SERVER         "mqtts.heclouds.com"    //云服务器网址
#define MQTT_PORT           1883										//端口号

#define PRODUCT_ID          "XixLQTghIb"						//产品ID
#define DEVICE_NAME         "Arg_device"						//设备名
#define MQTT_TOKEN          "version=2018-10-31&res=products%2FXixLQTghIb%2Fdevices%2FArg_device&et=2102420640&method=md5&sign=XMh0kjlWRKS8I%2FH6EL565w%3D%3D"

// ------------------------------------------------------------------------
// 利用 C 语言字符串自动拼接特性，动态生成对应的 Topic
// ------------------------------------------------------------------------
#define TOPIC_POST_REPLY    "$sys/" PRODUCT_ID "/" DEVICE_NAME "/thing/property/post/reply"
#define TOPIC_SET           "$sys/" PRODUCT_ID "/" DEVICE_NAME "/thing/property/set"
#define TOPIC_POST          "$sys/" PRODUCT_ID "/" DEVICE_NAME "/thing/property/post"
#define TOPIC_SET_REPLY     "$sys/" PRODUCT_ID "/" DEVICE_NAME "/thing/property/set_reply"
// ========================================================================


// ==========================================
// 2. 中断接收所需的全局变量
// ==========================================
uint16_t co2_val=0;
uint8_t co2_usrt_cnt = 0;
uint8_t co2_uart_data[6] = {0};
uint8_t rx_data=0;
uint8_t flag=0;
uint8_t manu=0;
uint32_t t_count=0;
uint8_t humi_flag=0;
uint8_t pr_humi=50;
uint8_t humi_time=0;
uint16_t humi_time_s;
DHT11_Data_TypeDef DHT11_Data; 

volatile uint8_t uart_rx_byte;
char uart_rx_buf[512];
volatile uint16_t uart_rx_len = 0;

// ==========================================
// 3. 串口中断回调函数
// ==========================================
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if(huart->Instance==USART1)
	{
		if(rx_data==0x02)
		{
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
			flag=1;
		}
		else if(rx_data==0x03)
		{
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
			flag=0;
		}
		else if(rx_data==0x04)
		{
			manu=1;
		}
		else if(rx_data==0x05)
		{
			manu=0;
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
		}
		else if(rx_data==0x06)
		{
			humi_flag=1;
		}
		HAL_UART_Receive_IT(&huart1,(uint8_t *)&rx_data,1);
	}
	
	if(huart->Instance==USART2)
	{
		co2_usrt_cnt++;
    if(co2_usrt_cnt == 6)
			{
        co2_usrt_cnt = 0;
        if(co2_uart_data[5] == (uint8_t)(co2_uart_data[0] + co2_uart_data[1] + co2_uart_data[2] + co2_uart_data[3] + co2_uart_data[4]))
        co2_val = co2_uart_data[1]*256 + co2_uart_data[2];
				memset(co2_uart_data,0,6);
      }
		HAL_UART_Receive_IT(&huart2,&co2_uart_data[co2_usrt_cnt],1);
	}
	
   if (huart->Instance == USART3)
		{
		 if (uart_rx_len < (sizeof(uart_rx_buf) - 1))
			 {
					uart_rx_buf[uart_rx_len++] = (char)uart_rx_byte;
          uart_rx_buf[uart_rx_len] = '\0';
       }
     HAL_UART_Receive_IT(&huart3, (uint8_t *)&uart_rx_byte, 1);
    }
}

void mycode_init()
{
    LCD_Init();
    LCD_Clear(WHITE);
		POINT_COLOR=BLACK;
}

void uart_print(UART_HandleTypeDef *huart, char* format, ...)
{
  static char buf[512] = {0};
  va_list ap;
  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);
  HAL_UART_Transmit(huart, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);
}

void Update_Top_Status(uint8_t step)
{
	LCD_ShowString(10,120,380,32,32,(uint8_t*)"System running step:");
	LCD_ShowNum(390,120,step,1,32);
}

void Update_Error_Status(char* err_msg)
{
	LCD_ShowString(10,160,240,32,32,(uint8_t*)err_msg);
}

// 仅用于初始化阶段的阻塞发送
int8_t Send_Cmd_Wait_Resp_IT(UART_HandleTypeDef *huart, char *cmd, char *expected_resp, uint32_t timeout_ms, uint8_t max_retries)
{
    uint8_t retry_count = 0;
    uint32_t start_time = 0;

    while (retry_count < max_retries)
    {
        memset(uart_rx_buf, 0, sizeof(uart_rx_buf));
        uart_rx_len = 0;

        uart_print(huart, "%s", cmd);
        start_time = HAL_GetTick();

        while ((HAL_GetTick() - start_time) < timeout_ms)
        {
            if (uart_rx_len > 0 && strstr(uart_rx_buf, expected_resp) != NULL)
            {
                uart_print(&huart1, "%s", uart_rx_buf);
                memset(uart_rx_buf, 0, sizeof(uart_rx_buf));
                uart_rx_len = 0;
                return 0;
            }
            HAL_Delay(10);
        }

        if (uart_rx_len > 0) {
            uart_print(&huart1, "%s", uart_rx_buf);
        }

        retry_count++;
        HAL_Delay(500);
    }
    return -1;
}

// ==========================================
// 万能 OneNet 多参数 JSON 构建器
// ==========================================
void Build_OneNet_Cmd(char *out_buf, const char *topic, const char *msg_id, uint8_t param_count, ...)
{
    static char payload[384];
    memset(payload, 0, sizeof(payload));
    int offset = 0;
    va_list ap;

    offset += sprintf(payload + offset, "{\\\"id\\\":\\\"%s\\\"\\,\\\"version\\\":\\\"1.0\\\"\\,\\\"params\\\":{", msg_id);

    va_start(ap, param_count);
    for(uint8_t i = 0; i < param_count; i++)
    {
        char *key = va_arg(ap, char*);
        int type = va_arg(ap, int);

        if (i > 0) offset += sprintf(payload + offset, "\\,");

        if (type == 'i') {
            int val = va_arg(ap, int);
            offset += sprintf(payload + offset, "\\\"%s\\\":{\\\"value\\\":%d}", key, val);
        }
        else if (type == 'f') {
            //解决 10.2 变成 10.19 的精度截断问题
            double val = va_arg(ap, double);
            int int_part = (int)val;

            // 1. 提取小数部分，强制转正
            double diff = val - int_part;
            if (diff < 0) diff = -diff;

            // 2. 放大 100 倍，并加上 0.5 实现四舍五入
            int dec_part = (int)(diff * 100.0 + 0.5);

            // 3. 处理进位：如果小数部分四舍五入后达到了 100 (例如 10.999)
            if (dec_part >= 100) {
                dec_part -= 100;
                if (val >= 0) int_part += 1;
                else int_part -= 1;
            }

            // 4. 处理 -0.xx 这种极其特殊的负小数情况
            if (val < 0 && int_part == 0) {
                offset += sprintf(payload + offset, "\\\"%s\\\":{\\\"value\\\":-%d.%02d}", key, int_part, dec_part);
            } else {
                offset += sprintf(payload + offset, "\\\"%s\\\":{\\\"value\\\":%d.%02d}", key, int_part, dec_part);
            }
        }
        else if (type == 's') {
            char *val = va_arg(ap, char*);
            offset += sprintf(payload + offset, "\\\"%s\\\":{\\\"value\\\":\\\"%s\\\"}", key, val);
        }
    }
    va_end(ap);

    sprintf(payload + offset, "}}");
    sprintf(out_buf, "AT+MQTTPUB=0,\"%s\",\"%s\",0,0\r\n", topic, payload);
}

uint16_t lm=0;
uint8_t soil_humi=0;
int duty=50;
uint8_t duty_flag;
uint32_t time_count=0;

void mycode_run()
{
    uint8_t biaozhi = 0;
    uint16_t i = 0;
    static char cmd_buf[512];
	  HAL_UART_Receive_IT(&huart1,(uint8_t *)&rx_data,1);
	  HAL_UART_Receive_IT(&huart2,co2_uart_data,1);


//		LCD_ShowString(100,0,480,32,32,(uint8_t*)"Agriculture System");
	  lcd_show_chinese(110,0,0,40,BLACK);
		lcd_show_chinese(150,0,1,40,BLACK);
		lcd_show_chinese(190,0,2,40,BLACK);
		lcd_show_chinese(230,0,3,40,BLACK);
		lcd_show_chinese(270,0,4,40,BLACK);
		lcd_show_chinese(310,0,5,40,BLACK);
	  LCD_ShowString(200,60,100,32,32,(uint8_t*)"Wait...");
	

    HAL_UART_Receive_IT(&huart3, (uint8_t *)&uart_rx_byte, 1);

    uart_print(&huart3, "AT+RST\r\n");
    HAL_Delay(2000);

    Update_Top_Status(1);
    if (Send_Cmd_Wait_Resp_IT(&huart3, "AT+CWMODE=1\r\n", "OK", 2000, 3) != 0) Update_Error_Status("Error Step: CWMODE");

    Update_Top_Status(2);
    sprintf(cmd_buf, "AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_SSID, WIFI_PASSWORD);
    if (Send_Cmd_Wait_Resp_IT(&huart3, cmd_buf, "OK", 10000, 3) != 0) Update_Error_Status("Error Step: WIFI   ");
    HAL_Delay(2000);

    Update_Top_Status(3);
    sprintf(cmd_buf, "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"\r\n", DEVICE_NAME, PRODUCT_ID, MQTT_TOKEN);
    if (Send_Cmd_Wait_Resp_IT(&huart3, cmd_buf, "OK", 3000, 3) != 0) Update_Error_Status("Error Step: CFG     ");

    Update_Top_Status(4);
    sprintf(cmd_buf, "AT+MQTTCONN=0,\"%s\",%d,1\r\n", MQTT_SERVER, MQTT_PORT);
    if (Send_Cmd_Wait_Resp_IT(&huart3, cmd_buf, "OK", 8000, 3) != 0) Update_Error_Status("Error Step: CONN    ");

    Update_Top_Status(5);
    sprintf(cmd_buf, "AT+MQTTSUB=0,\"%s\",0\r\n", TOPIC_POST_REPLY);
    if (Send_Cmd_Wait_Resp_IT(&huart3, cmd_buf, "OK", 5000, 3) != 0) Update_Error_Status("Error Step: SUB1    ");

    Update_Top_Status(6);
    sprintf(cmd_buf, "AT+MQTTSUB=0,\"%s\",0\r\n", TOPIC_SET);
    if (Send_Cmd_Wait_Resp_IT(&huart3, cmd_buf, "OK", 5000, 3) != 0) Update_Error_Status("Error Step: SUB2    ");

		LCD_ShowString(200,60,100,32,32,(uint8_t*)" RUN    ");
    Update_Top_Status(7);


    while (1)
    {
			  LCD_ShowNum(160,760,uwTick / 1000,6,32);
			  ctr_duty();
			  humidity_pr();
		    humi_crl();
			  TIM3->CCR4=duty;
        if(uwTick / 1000 % 2 == 0) 
					{
            if(biaozhi == 0) 
							{
                biaozhi = 1;
                i++;
				
                // 不能超过256
                Build_OneNet_Cmd(cmd_buf, TOPIC_POST, "123",4,
								 "CO2", 'i', co2_val,
                 "EH", 'i', DHT11_Data.humi_int,
                 "ET",'i',DHT11_Data.temp_int,
								 "Lux",'i',lm);
													
							
                uart_print(&huart3, "%s", cmd_buf);
								
								HAL_Delay(100);
								
								Build_OneNet_Cmd(cmd_buf, TOPIC_POST, "123",4,
								 "SH",'i',soil_humi,	
								 "duty",'i',duty,	
								 "flag",'i',duty_flag,	
							   "IO",'i',flag);	
 													
                uart_print(&huart3, "%s", cmd_buf);
								
                Update_Error_Status("Uploading...");
							
							  // 数据显示
//							LCD_ShowString(150,240,300,32,32,(uint8_t*)"System Data");
								lcd_show_chinese(150,240,4,40,BLACK);
								lcd_show_chinese(190,240,5,40,BLACK);
								lcd_show_chinese(230,240,6,40,BLACK);
								lcd_show_chinese(270,240,7,40,BLACK);

//				      LCD_ShowString(140,720,240,32,32,(uint8_t*)"System uptime");
								lcd_show_chinese(110,720,4,40,BLACK);
								lcd_show_chinese(150,720,5,40,BLACK);
								lcd_show_chinese(190,720,8,40,BLACK);
								lcd_show_chinese(230,720,9,40,BLACK);
								lcd_show_chinese(270,720,10,40,BLACK);
								lcd_show_chinese(310,720,11,40,BLACK);

//							LCD_ShowString(10,290,300,24,24,(uint8_t*)"Environment temperature:");
								lcd_show_chinese(10,300,0,16,BLACK);
								lcd_show_chinese(30,300,1,16,BLACK);
								lcd_show_chinese(50,300,2,16,BLACK);
								lcd_show_chinese(70,300,3,16,BLACK);
								LCD_ShowString(90,300,20,16,16,(uint8_t*)":");							
								
//							LCD_ShowString(10,330,300,24,24,(uint8_t*)"Environment humidity:");
								lcd_show_chinese(10,330,0,16,BLACK);
								lcd_show_chinese(30,330,1,16,BLACK);
								lcd_show_chinese(50,330,4,16,BLACK);
								lcd_show_chinese(70,330,3,16,BLACK);
								LCD_ShowString(90,330,20,16,16,(uint8_t*)":");
								
//							LCD_ShowString(10,370,300,24,24,(uint8_t*)"Soil humidity:");
								lcd_show_chinese(10,370,5,16,BLACK);
								lcd_show_chinese(30,370,6,16,BLACK);
								lcd_show_chinese(50,370,4,16,BLACK);
								lcd_show_chinese(70,370,3,16,BLACK);
								LCD_ShowString(90,370,20,16,16,(uint8_t*)":");

								
//							LCD_ShowString(10,410,300,24,24,(uint8_t*)"Illuminance:");
								lcd_show_chinese(10,410,7,16,BLACK);
								lcd_show_chinese(30,410,8,16,BLACK);
								lcd_show_chinese(50,410,9,16,BLACK);
								lcd_show_chinese(70,410,3,16,BLACK);
								LCD_ShowString(90,410,20,16,16,(uint8_t*)":");

								LCD_ShowString(10,450,100,24,24,(uint8_t*)"CO2:");
								
//							LCD_ShowString(120,540,300,32,32,(uint8_t*)"Equipment Status");
								lcd_show_chinese(150,540,12,40,BLACK);
								lcd_show_chinese(190,540,13,40,BLACK);
								lcd_show_chinese(230,540,14,40,BLACK);
								lcd_show_chinese(270,540,15,40,BLACK);
								
//							LCD_ShowString(10,640,300,32,32,(uint8_t*)"Light Duty:");								
								lcd_show_chinese(10,640,18,40,BLACK);
								lcd_show_chinese(50,640,19,40,BLACK);
								lcd_show_chinese(90,640,20,40,BLACK);
								lcd_show_chinese(130,640,21,40,BLACK);
								LCD_ShowString(170,640,32,32,32,(uint8_t*)":");
								//水泵
								lcd_show_chinese(10,600,16,40,BLACK);
								lcd_show_chinese(50,600,17,40,BLACK);
								
								if(flag==1)
								{
									LCD_ShowString(90,605,300,32,32,(uint8_t*)": ON   ");
								}
								else if(flag==0)
								{
									LCD_ShowString(90,605,300,32,32,(uint8_t*)": OFF  ");
								}
								
								LCD_ShowNum(120,300,DHT11_Data.temp_int,2,16);
								LCD_ShowNum(120,330,DHT11_Data.humi_int,2,16);
								LCD_ShowNum(120,370,soil_humi,2,16);
								LCD_ShowNum(120,410,lm,3,16);
								LCD_ShowNum(70,450,co2_val,4,24);
								LCD_ShowNum(190,645,duty,3,32);
								
								LCD_ShowString(150,300,40,16,16,(uint8_t*)"C");
								LCD_ShowString(150,330,80,16,16,(uint8_t*)"%RH");
								LCD_ShowString(150,370,40,16,16,(uint8_t*)"%");
								LCD_ShowString(150,410,40,16,16,(uint8_t*)"lx");
								LCD_ShowString(135,450,40,24,24,(uint8_t*)"ppm");
								LCD_ShowString(250,645,40,32,32,(uint8_t*)"%");
              }
        } else {
            biaozhi = 0;
        }

        if (uart_rx_len > 0)
        {
            // 稍微延时 50ms，确保这一帧串口数据彻底接收完毕，防止腰斩解析
            HAL_Delay(50);

            // 处理云端下发的命令
            if (strstr(uart_rx_buf, "+MQTTSUBRECV") != NULL && strstr(uart_rx_buf, "/set\"") != NULL)
            {
                if (strstr(uart_rx_buf, "\"IO\":1") != NULL)
                {
                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
										flag=1;
                }
                else if (strstr(uart_rx_buf, "\"IO\":0") != NULL)
                {
                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
										flag=0;
                }
								else if (strstr(uart_rx_buf, "\"flag\":1") != NULL)
                {
                    duty+=10;
									  duty_flag=1;
                }
								else if (strstr(uart_rx_buf, "\"flag\":0") != NULL)
                {
									  duty-=10;
									  duty_flag=0;
                }

                char msg_id[16] = {0};
                char *id_start = strstr(uart_rx_buf, "\"id\":\"");

                if (id_start != NULL)
                {
                    id_start += 6;
                    char *id_end = strchr(id_start, '"');

                    if (id_end != NULL && (id_end - id_start) < sizeof(msg_id))
                    {
                        strncpy(msg_id, id_start, id_end - id_start);
                        msg_id[id_end - id_start] = '\0';
                        //uart_print(&huart1, "AT+MQTTPUB=0,\"%s\",\"{\\\"id\\\":\\\"%s\\\"\\,\\\"code\\\":200\\,\\\"msg\\\":\\\"success\\\"}\",0,0\r\n", TOPIC_SET_REPLY, msg_id);
                    }
                }
            }
						
            if (strstr(uart_rx_buf, "+MQTTSUBRECV") != NULL && strstr(uart_rx_buf, "/post/reply") != NULL)
            {
                // 主动发送数据的 reply 回应，更新一下屏幕状态
                Update_Error_Status("Send OK!      ");
            }

            // 安全清空接收缓存
            memset(uart_rx_buf, 0, sizeof(uart_rx_buf));
            uart_rx_len = 0;
        }
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if(htim->Instance==TIM6)
	{
		time_count++;
		if(time_count>=200)
		{
			DHT11_ReadData(&DHT11_Data);
			soil_humi = Soil_humi_GetData();
			lm = get_light();
			time_count=0;
		}
		
		if(humi_flag==1)
		{
			t_count++;
			if(t_count>=humi_time_s*1000)
			{
				HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
				humi_flag=0;
				flag=0;
				t_count=0;
			}
		}
	}
}

void ctr_duty(void)
{
	if(duty>=100)
	{
		duty=100;
	}
	else if(duty<=0)
	{
		duty=0;
	}
}

void humidity_pr(void)
{
	uint8_t Q = 5;
	uint8_t V_S = 1;
	double V_W = 3600*V_S * (pr_humi-soil_humi)/100;
	humi_time_s=V_W/Q;
	humi_time = humi_time_s/60;
}

void humi_crl(void)
{
	if(humi_flag==1)
	{
		LCD_ShowString(10,500,300,24,24,(uint8_t*)"Time to go:");
		LCD_ShowNum(150,500,humi_time_s-(t_count/1000),3,24);
		LCD_ShowString(195,500,300,24,24,(uint8_t*)"S");
	  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
		flag=1;
	}
	else
	{
		LCD_ShowString(0,500,480,24,24,(uint8_t*)"                                ");
	}
	
	if(manu==1)
	{
		if(soil_humi<30)
		{
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
			flag=1;
		}
		else if(soil_humi>45)
		{
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
			flag=0;
		}
		
		if(lm<100&&lm>0)
		{
			duty=100;
		}
		else if(lm<500&&lm>100)
		{
			duty=60;
		}
		else if(lm<900&&lm>500)
		{
			duty=20;
		}
		else if(lm>900)
		{
			duty=0;
		}		
	}
}

