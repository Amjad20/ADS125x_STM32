# ADS125x_STM32
STM32 driver for ADS125x 24bit ADCs.

> **Datasheet: [ti.com](https://www.ti.com/lit/ds/sbas288k/sbas288k.pdf?)**

---

This work is mostly based on https://github.com/eta-systems/ADS1255. It didn't work for me, so I modified it a bit.
Some of the notes and code has been taken from https://curiousscientist.tech/blog/ads1256-arduino-stm32-sourcecode.
you 

This library works both polling and interrupt methods.

[![forthebadge](https://forthebadge.com/images/badges/made-with-c.svg)](https://forthebadge.com)

### Usage Polling

```c
/* USER CODE BEGIN Includes */
#include <ads125x.h>
```

```c
/* USER CODE BEGIN PV */
ADS125X_t adc1;
/* USER CODE END PV */
```

```c
int main(void)
{
  /* USER CODE BEGIN 2 */
  //for microseconds delay
  HAL_TIM_Base_Start(&htim1);
  
  adc1.csPort = SPI1_CS_GPIO_Port;
  adc1.csPin = SPI1_CS_Pin;
  adc1.drdyPort = SPI1_DRDY_GPIO_Port;
  adc1.drdyPin = SPI1_DRDY_Pin;
  adc1.rstPort = SPI1_RST_GPIO_Port;
  adc1.rstPin = SPI1_RST_Pin;
  adc1.vref = 2.5f;
  adc1.oscFreq = ADS125X_OSC_FREQ;
  
  printf("\n");
  printf("ADC config...\n");
  //When we start the ADS1256, the preconfiguration already sets the MUX to [AIN0+AINCOM].
  // 10 SPS / PGA=1 / buffer off
  ADS125X_Init(&adc1, &hspi1, ADS125X_DRATE_30000SPS, ADS125X_PGA1, 0);
  
  printf("...done\n");
  /* USER CODE END 2 */


  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    //you can use this way 
    float volt[2] = { 0.0f, 0.0f };
    ADS125X_Channel_Set(&adc1, ADS125X_MUXP_AIN0);
    volt[0] = ADS125X_ADC_ReadVolt(&adc1);
    printf("%.15f\n", volt[0]);

    ADS125X_Channel_Set(&adc1, ADS125X_MUXP_AIN1);
    ADS125X_ChannelDiff_Set(&adc1, ADS125X_MUXP_AIN0, ADS125X_MUXN_AIN1);
    volt[1] = ADS125X_ADC_ReadVolt(&adc1);
    printf("%.15f\n", volt[1])		


    //or this way
//  ADS125X_DRDY_Wait(&adc1);
//  ADS125X_cycle_Through_Channels(&adc1, volt)

    HAL_Delay(100);
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

```

### Usage interrupt

```c
/* USER CODE BEGIN Includes */
#include <ads1255.h>
```

```c
/* USER CODE BEGIN PV */
ADS125X_t adc1;
/* USER CODE END PV */
```

```c
/* USER CODE BEGIN 0 */
float volt[3] = { 0, 0, 0 };
/* USER CODE END 0 */
```

```c
int main(void)
{
  /* USER CODE BEGIN 2 */
  //for microseconds delay
  HAL_TIM_Base_Start(&htim1);
  
  adc1.csPort = SPI1_CS_GPIO_Port;
  adc1.csPin = SPI1_CS_Pin;
  adc1.drdyPort = SPI1_DRDY_GPIO_Port;
  adc1.drdyPin = SPI1_DRDY_Pin;
  adc1.rstPort = SPI1_RST_GPIO_Port;
  adc1.rstPin = SPI1_RST_Pin;
  adc1.vref = 2.5f;
  adc1.oscFreq = ADS125X_OSC_FREQ;
  
  printf("\n");
  printf("ADC config...\n");
  //When we start the ADS1256, the preconfiguration already sets the MUX to [AIN0+AINCOM].
  // 10 SPS / PGA=1 / buffer off
  ADS125X_Init(&adc1, &hspi1, ADS125X_DRATE_30000SPS, ADS125X_PGA1, 0);
  
  printf("...done\n");
  /* USER CODE END 2 */
}
```

```c
/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  ADS125X_cycle_Through_Channels(&adc1, volt);
}
```

---

