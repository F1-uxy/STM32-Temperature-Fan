#include "stm32f7xx_hal.h"
#include "GLCD_Config.h"
#include "Board_GLCD.h"
//#include "Board_Touch.h"
#include "DHT.h"

#define SYSTICK_DELAY_CALIB (SYSTICK_LOAD >> 1)
#define SYSTICK_LOAD (SystemCoreClock/1000000U)

#define FAN_ROTATION_SPEED_SLOW 1
#define FAN_ROTATION_SPEED_MEDIUM 5
#define FAN_ROTATION_SPEED_FAST 10
 

extern GLCD_FONT GLCD_Font_16x24;
char GLCD_BUFFER[100]={0};

volatile int8_t encoderState = 0;


TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;


#define GPIO_PIN_NUM GPIO_PIN_10
#define GPIO_PORT GPIOB

#define SERVO_PIN_NUM GPIO_PIN_15
#define SERVO_PORT GPIOA

#define FAN_PIN_NUM GPIO_PIN_4
#define FAN_PORT GPIOB

#define TOUCH_SENSOR_PIN_NUM GPIO_PIN_1
#define TOUCH_SENSOR_PORT GPIOI

#define ROTARY_LEFT_PIN  GPIO_PIN_3
#define ROTARY_LEFT_PORT GPIOI

#define ROTARY_RIGHT_PIN GPIO_PIN_6
#define ROTARY_RIGHT_PORT GPIOH

/*

SENSOR PINS:
Touch Sensor 	= 	D13
DHT11 		 	= 	A0
Servo			= 	D9
FAN				=	D3



*/


#ifdef __RTX
extern uint32_t os_time;
uint32_t HAL_GetTick(void){
	return os_time;
}
#endif

#define DELAY_US(us) \
    do { \
         uint32_t start = SysTick->VAL; \
         uint32_t ticks = (us * SYSTICK_LOAD)-SYSTICK_DELAY_CALIB;  \
         while((start - SysTick->VAL) < ticks); \
    } while (0)

	
void Error_Handler(char* s)
{
	GLCD_DrawString(0,150,s);
}


void PA15_Init()
{
	GPIO_InitTypeDef gpio;

	__HAL_RCC_GPIOA_CLK_ENABLE();

	gpio.Pin = SERVO_PIN_NUM;
	gpio.Mode = GPIO_MODE_AF_PP;
	gpio.Pull = GPIO_NOPULL;
	gpio.Speed = GPIO_SPEED_HIGH;
	gpio.Alternate = GPIO_AF1_TIM2;
	HAL_GPIO_Init(SERVO_PORT, &gpio);
}

void PB4_Init()
{
	GPIO_InitTypeDef gpio;

	__HAL_RCC_GPIOB_CLK_ENABLE();

	gpio.Pin = FAN_PIN_NUM;
	gpio.Mode = GPIO_MODE_AF_PP;
	gpio.Pull = GPIO_NOPULL;
	gpio.Speed = GPIO_SPEED_HIGH;
	gpio.Alternate = GPIO_AF2_TIM3;
	HAL_GPIO_Init(FAN_PORT, &gpio);
}

void PI1_Init()
{
	GPIO_InitTypeDef gpio;
	
	__HAL_RCC_GPIOI_CLK_ENABLE();
	
	gpio.Pin = TOUCH_SENSOR_PIN_NUM;
	gpio.Mode = GPIO_MODE_INPUT;
	gpio.Pull = GPIO_NOPULL;
	gpio.Speed = GPIO_SPEED_HIGH;
	HAL_GPIO_Init(TOUCH_SENSOR_PORT, &gpio);

}



void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct;
	RCC_ClkInitTypeDef RCC_ClkInitStruct;
	/* Enable Power Control clock */
	__HAL_RCC_PWR_CLK_ENABLE();
	/* The voltage scaling allows optimizing the power
	consumption when the device is clocked below the
	maximum system frequency. */
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
	/* Enable HSE Oscillator and activate PLL
	with HSE as source */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 25;
	RCC_OscInitStruct.PLL.PLLN = 336;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	//RCC_OscInitStruct.PLL.PLLQ = 7;
	HAL_RCC_OscConfig(&RCC_OscInitStruct);
	/* Select PLL as system clock source and configure
	the HCLK, PCLK1 and PCLK2 clocks dividers */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK | 
	RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
	HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
}

/**
Timer (TIM2) and Channel (CH1) initialization. 
**/
static void TIM2_Init(void){
	

	TIM_ClockConfigTypeDef sClockSourceConfig;
	TIM_OC_InitTypeDef sConfigOC;
	
	__HAL_RCC_TIM2_CLK_ENABLE();


	//Timer configuration
	htim2.Instance = TIM2;
	htim2.Init.Prescaler = 13;
	htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim2.Init.Period = 119999;
	htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	HAL_TIM_Base_Init(&htim2);

	//Set the timer in PWM mode
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig);
	HAL_TIM_PWM_Init(&htim2);

	//Channel configuration
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1);
	
	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);


	PA15_Init();
}


/**
Timer (TIM3) and Channel (CH1) initialization. 
**/
static void TIM3_Init(void){

	TIM_ClockConfigTypeDef sClockSourceConfig2;
	TIM_OC_InitTypeDef sConfigOC2;
	
	__HAL_RCC_TIM3_CLK_ENABLE();

	//Timer configuration
	htim3.Instance = TIM3;
	htim3.Init.Prescaler = 1;
	htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim3.Init.Period = 1650;
	htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    // Initialize the timer
    if (HAL_TIM_Base_Init(&htim3) != HAL_OK) {
        Error_Handler("TIM3 base initialization failed");
    }

    // Set the timer clock source
    sClockSourceConfig2.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig2) != HAL_OK) {
        Error_Handler("TIM3 clock source configuration failed");
    }

    // Initialize PWM
    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) {
        Error_Handler("TIM3 PWM initialization failed");
    }

    // Channel configuration
    sConfigOC2.OCMode = TIM_OCMODE_PWM1;
    sConfigOC2.Pulse = 0;
    sConfigOC2.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC2.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC2, TIM_CHANNEL_1) != HAL_OK) {
        Error_Handler("TIM3 PWM channel configuration failed");
    }

	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);

   
	PB4_Init();
}



void setServoPosition(uint8_t pos) {
    /*
		0 deg = 1400
		180 deg = 9600
		Prescaler = 1 = 16MHz CLK
		Period = 119999 = 20ms
		CCR = 1400->9600
		Pin D9
	*/
	uint16_t inMin = 0;
    uint16_t inMax = 180;
    uint16_t outMin = 1700;
    uint16_t outMax = 9000;
    uint32_t pulse_width = (pos - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;;

    htim2.Instance->CCR1 = pulse_width;
	htim3.Instance->CCR1 = pulse_width;



}



void setFanSpeed(uint16_t speed)
{
	uint16_t i;
	uint8_t input_min = 0;
    uint8_t input_max = 100;
    uint8_t output_min = 100;
    uint16_t output_max = 1900;
    
    i = speed * ((output_max - output_min) / (input_max - input_min));
	
	htim3.Instance->CCR1 = i;
	
}


int8_t getEncoderRotation()
{
	uint8_t i;
	uint8_t aVal = HAL_GPIO_ReadPin(ROTARY_LEFT_PORT, ROTARY_LEFT_PIN);
	uint8_t aLast;
	for(i = 0; i < 255; i++)
	{
		aVal = HAL_GPIO_ReadPin(ROTARY_LEFT_PORT, ROTARY_LEFT_PIN);
		if(aVal != aLast)
		{
			if(HAL_GPIO_ReadPin(ROTARY_RIGHT_PORT, ROTARY_RIGHT_PIN) != aVal)
			{
				return 1;
			}else
			{
				return -1;
			}
		}
		
		aLast = aVal;
	}
	
	return 0;
}


bool getTouchSensorState()
{
	if(HAL_GPIO_ReadPin(TOUCH_SENSOR_PORT, TOUCH_SENSOR_PIN_NUM) == GPIO_PIN_SET) return true;
	else return false;
}	




int main(void)
{
	
	uint16_t servo_pos = 0;
	int8_t servo_direction = 1;
	uint16_t j = 0;
	uint8_t servo_speed_state = 0;


	bool buttonHeld = false;
	bool buttonPressed = false;
	
	DHT_DataTypedef DHT11_Data;
	float Temperature, Humidity;
	
	//Reset of all peripherals, initializes the Flash interface and the Systick
	HAL_Init();
	
	//Configure the system clock
	SystemClock_Config();

	//Initialise TIM2, CH1, PA15 (Servo)
	TIM2_Init();
	
	//Initialise TIM3, CH1, PB4 (Fan)
	TIM3_Init();
	
	//Initialise PI1 (Touch Sensor)
	PI1_Init();
	
	GLCD_Initialize();
	GLCD_ClearScreen();
	GLCD_SetFont(&GLCD_Font_16x24);

	
	while(1)
	{	
		HAL_Delay(1100);
		DHT_GetData(&DHT11_Data);
		Temperature = DHT11_Data.Temperature;
		Humidity = DHT11_Data.Humidity;
		
		sprintf(GLCD_BUFFER, "Temperature: %.2fc   ", Temperature);
		GLCD_DrawString(0, 10, GLCD_BUFFER);
		
		sprintf(GLCD_BUFFER, "Humidity: %.2f%%   ", Humidity);
		GLCD_DrawString(0, 40, GLCD_BUFFER);
		
		//setServoPosition(servo_pos);
		if(Temperature > 24.0)
		{
			j = 100;
			setFanSpeed(j);
		}else if (Temperature > 22.0 && Temperature < 24.0)
		{
			j = 50;
			setFanSpeed(j);
		}
		else 
		{
			j = 0;
			setFanSpeed(j);
		}
		
		sprintf(GLCD_BUFFER, "Fan position: %hu  ", servo_pos);
		GLCD_DrawString(0,120, GLCD_BUFFER);	

		sprintf(GLCD_BUFFER, "Fan Speed: %hu  ", j);
		GLCD_DrawString(0,150, GLCD_BUFFER);
		
		HAL_Delay(200);
		
		buttonPressed = getTouchSensorState();
		
		if(buttonPressed && !buttonHeld)
		{
			buttonHeld = true;
		}
		else if(!buttonPressed && buttonHeld)
		{
			buttonHeld = false;
			
			servo_speed_state = (servo_speed_state + 1) % 3; 
			switch (servo_speed_state) {
				case 0:
					servo_direction = (servo_direction < 0) ? -1 : 1;
					break;
				case 1:
					servo_direction = (servo_direction < 0) ? -2 : 2;
					break;
				case 2:
					servo_direction = (servo_direction < 0) ? -5 : 5;
					break;
			}
		}
		
		
		servo_pos += servo_direction;
		
		if(servo_pos >= 180 || servo_pos <= 0)
		{
			servo_direction *= -1;
		}

	}


}



