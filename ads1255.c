#include <stdint.h>
#include "ads1255.h"

#define DEBUG_ADS1255

#ifdef DEBUG_ADS1255
#include <stdio.h>
#endif

extern TIM_HandleTypeDef htim1;
void delay_us(uint16_t us) {
	__HAL_TIM_SET_COUNTER(&htim1, 0);  // set the counter value a 0
	while (__HAL_TIM_GET_COUNTER(&htim1) < us)
		;  // wait for the counter to reach the us input in the parameter
}

/**
 * @brief  reset the ADS1255/6
 * @param  *ads pointer to ads handle
 * @return
 * @see    Datasheet p. 37 RESET: Reset Registers to Default Values
 * @note   no need for SDATAC CMD after software reset
 */
uint8_t ADS125X_Reset(ADS125X_t *ads) {
	uint8_t spiTx = ADS125X_CMD_RESET;
	ADS125X_DRDY_Wait(ads);
	ADS125X_CS(ads, 1);
	HAL_SPI_Transmit(ads->hspix, &spiTx, 1, 1);
	//delay_us(4);
	//spiTx = ADS125X_CMD_SDATAC;
	//HAL_SPI_Transmit(ads->hspix, &spiTx, 1, 1);
	ADS125X_CS(ads, 0);
	return 0;
}

/**
 * @brief  resets, initializes and calibrates the ADS125X
 * @param  *ads pointer to ads handle
 * @param  drate the datarate of the converter
 * @param  gain  the gain of the PGA
 * @param  buffer_en [0 = off, 1 = on]
 * @return
 * @see    Datasheet Fig. 33 SDATAC Command Sequence
 */
uint8_t ADS125X_Init(ADS125X_t *ads, SPI_HandleTypeDef *hspi, uint8_t drate,
		uint8_t gain, uint8_t buffer_en) {
	ads->hspix = hspi;
	ads->pga = 1 << gain;

	//To do: remove the Reset pin, just comment the following lines and tie the reset pin to DVDD(3.3V)
	HAL_GPIO_WritePin(ads->rstPort, ads->rstPin, GPIO_PIN_RESET);
	HAL_Delay(10);
	HAL_GPIO_WritePin(ads->rstPort, ads->rstPin, GPIO_PIN_SET);
	HAL_Delay(10);

	ADS125X_Reset(ads);
	HAL_Delay(10);

	uint8_t tmp[5]; // buffer
#ifdef DEBUG_ADS1255
	ADS125X_Register_Read(ads, ADS125X_REG_STATUS, tmp, 1);
	printf("STATUS: %#.2x\n", tmp[0]);
#endif

	// enable clockout | ADS125X_PGA1
	ADS125X_Register_Write(ads, ADS125X_REG_ADCON, ADS125X_CLKOUT_OFF | gain); // enable clockout = clkin/1
#ifdef DEBUG_ADS1255
	ADS125X_Register_Read(ads, ADS125X_REG_ADCON, tmp, 1);
	printf("ADCON: %#.2x\n", tmp[0]);
#endif

	ADS125X_Register_Write(ads, ADS125X_REG_DRATE, drate);
#ifdef DEBUG_ADS1255
	ADS125X_Register_Read(ads, ADS125X_REG_DRATE, tmp, 1);
	printf("DRATE: %#.2x\n", tmp[0]);
#endif

	ADS125X_Register_Write(ads, ADS125X_REG_IO, 0x00); // all GPIOs are outputs (do not leave floating) - D0 is CLKOUT, clock is disabled -
#ifdef DEBUG_ADS1255
	ADS125X_Register_Read(ads, ADS125X_REG_IO, tmp, 1);
	printf("IO   : %#.2x\n", tmp[0]);
#endif

	HAL_Delay(10);
	ADS125X_CMD_Send(ads, ADS125X_CMD_SELFCAL);
	HAL_Delay(5);
	ADS125X_CS(ads, 0);
	ADS125X_DRDY_Wait(ads);  // wait ADS1256 to settle after self calibration

	return 0;
}

/**
 * @brief  waits for DRDY pin to go low
 * @param  *ads pointer to ads handle
 */
uint8_t DRDY_state;
uint8_t ADS125X_DRDY_Wait(ADS125X_t *ads) {
	while (HAL_GPIO_ReadPin(ads->drdyPort, ads->drdyPin)) {
		continue;
	}
	return 0;
}

/**
 * @brief  toggles chip select pin of ADS
 * @param  *ads pointer to ads handle
 * @param  on [0 = unselect, 1 = select]
 */
uint8_t ADS125X_CS(ADS125X_t *ads, uint8_t on) {
	if (on)
		on = 0;
	else
		on = 1;
	HAL_GPIO_WritePin(ads->csPort, ads->csPin, on);
	return 0;
}

/**
 * @brief  converts an <int32_t> array of 2's complement code to <float> array of voltage
 * @param  *ads pointer to ads handle
 * @param  *pCode pointer to input array
 * @param  *pVolt pointer to output array
 * @param  size the length of the array
 * @see    Datasheet Table 16. Ideal Ouput Code vs. Input Signal
 */
void ADS125X_ADC_Code2Volt(ADS125X_t *ads, int32_t *pCode, float *pVolt,
		uint16_t size) {
	for (uint8_t i = 0; i < size; i++) {
		// Vin = Code * 2 * (Vrefp - Vrefn) / ( PGA * 0x7FFFFF )
		if (pCode[i] & 0x800000)
			pCode[i] |= 0xff000000;  // fix 2's complement
		// do all calculations in float. don't change the order of factors --> (adsCode/0x7fffff) will always return 0
		pVolt[i] = ((float) pCode[i] * (2.0f * ads->vref))
				/ (ads->pga * 8388607.0f);  // 0x7fffff = 8388607.0f
	}
}

/**
 * @brief  read an analog voltage from the currently selected channel
 * @param  *ads pointer to ads handle
 * @return <float> voltage value on analog input
 * @see    Datasheet Fig. 30 RDATA Command Sequence
 */

/* LIBRARY MACHEN SO: */
/* ALI NIX SCHULD: */
float ADS125X_ADC_ReadVolt(ADS125X_t *ads) {
	uint8_t spiRx[3] = { 0, 0, 0 };
	spiRx[0] = ADS125X_CMD_RDATA;

	ADS125X_DRDY_Wait(ads);
	ADS125X_CS(ads, 1);
	HAL_SPI_Transmit(ads->hspix, spiRx, 1, 10);
	delay_us(10); 	//t6 time 7us
	HAL_SPI_Receive(ads->hspix, spiRx, 3, 10);
	ADS125X_CS(ads, 0);

#ifdef DEBUG_ADS1255
	printf("RDATA: %#.2x%.2x%.2x\n", spiRx[0], spiRx[1], spiRx[2]);
#endif

	// must be signed integer for 2's complement to work
	int32_t adsCode = (spiRx[0] << 16) | (spiRx[1] << 8) | (spiRx[2]);
	if (adsCode & 0x800000)
		adsCode |= 0xff000000;  // fix 2's complement
	// do all calculations in float. don't change the order of factors --> (adsCode/0x7fffff) will always return 0
	return ((float) adsCode * (2.0f * ads->vref)) / (ads->pga * 8388607.0f); // 0x7fffff = 8388607.0f
}

/**
 * @brief  reads from internal registers
 * @param  *ads pointer to ads handle
 * @param  reg  first register to be read
 * @param  *pData pointer for return data
 * @param  n number of registers to read
 * @return
 * @see    Datasheet Fig. 34 RREG Command Example
 */
uint8_t ADS125X_Register_Read(ADS125X_t *ads, uint8_t reg, uint8_t *pData,
		uint8_t n) {
	uint8_t spiTx[2];
	spiTx[0] = ADS125X_CMD_RREG | reg; // 1st command byte
	spiTx[1] = n - 1;                  // 2nd command byte = bytes to be read -1

	//ADS125X_DRDY_Wait(ads);
	ADS125X_CS(ads, 1);
	//delay_us(10);
	HAL_SPI_Transmit(ads->hspix, spiTx, 2, 1);
	delay_us(10); 	//t6 time 7us
	HAL_SPI_Receive(ads->hspix, pData, n, 1);
	//delay_us(10);
	ADS125X_CS(ads, 0);
	return 0;
}

/**
 * @brief  writes to internal registers
 * @param  *ads pointer to ads handle
 * @param  reg  first register to write to
 * @param  payload to write to register
 * @return
 * @see    Datasheet Fig. 35 WREG Command Example
 */
uint8_t ADS125X_Register_Write(ADS125X_t *ads, uint8_t reg, uint8_t data) {

	uint8_t spiTx[3];
	ADS125X_Register_Read(ads, reg, spiTx, 1);
	if (data != spiTx[0]) {
		delay_us(10);
		//ADS125X_DRDY_Wait(ads);
		ADS125X_CS(ads, 1);
		//delay_us(10);
		spiTx[0] = ADS125X_CMD_WREG | reg; // 1st command byte
		spiTx[1] = 0;      				   // 2nd command byte = payload length = 1 bytes -1 = 0
		spiTx[2] = data;
		HAL_SPI_Transmit(ads->hspix, spiTx, 3, 10);
		//delay_us(10);
		ADS125X_CS(ads, 0);

#ifdef DEBUG_ADS1255
		//Check if write was successful
		ADS125X_Register_Read(ads, reg, spiTx, 1);
		if (data != spiTx[0]) { //Check if write was successful
			printf("Write to Register %#.2x failed\n", reg);
		} else {
			printf("Write to Register %#.2x success\n", reg);
		}
#endif
	}
	return 0;
}

/**
 * @brief  send a single command to the ADS
 * @param  *ads pointer to ads handle
 * @param  cmd the command
 * @return
 * @see    Datasheet Fig. 33 SDATAC Command Sequence
 */
uint8_t ADS125X_CMD_Send(ADS125X_t *ads, uint8_t cmd) {
	uint8_t spiTx = cmd;
	//ADS125X_DRDY_Wait(ads);
	ADS125X_CS(ads, 1);
	//delay_us(10);
	HAL_SPI_Transmit(ads->hspix, &spiTx, 1, 1);
	//delay_us(10);
	ADS125X_CS(ads, 0);
	return 0;
}

/**
 * @brief  set internal multiplexer to single channel with AINCOM as negative input
 * @param  *ads pointer to ads handle
 * @param  p_chan positive analog input
 * @see    Datasheet p. 31 MUX : Input Multiplexer Control Register (Address 01h)
 */
void ADS125X_Channel_Set(ADS125X_t *ads, int8_t channel) {
	ADS125X_ChannelDiff_Set(ads, channel, ADS125X_MUXN_AINCOM);
}

/**
 * @brief  set internal multiplexer to differential input channel
 * @param  *ads pointer to ads handle
 * @param  p_chan positive analog input
 * @param  n_chan negative analog input
 * @see    Datasheet p. 31 MUX : Input Multiplexer Control Register (Address 01h)
 */
uint8_t ADS125X_ChannelDiff_Set(ADS125X_t *ads, int8_t p_chan, int8_t n_chan) {
	// uint8_t channels = ((p_chan << 4)&0xF0) | (n_chan & 0x0F);

	ADS125X_Register_Write(ads, ADS125X_REG_MUX, p_chan | n_chan);
	delay_us(5);
	ADS125X_CMD_Send(ads, ADS125X_CMD_SYNC);
	delay_us(5);
	ADS125X_CMD_Send(ads, ADS125X_CMD_WAKEUP);
	delay_us(1);

	//delay_us(100);
#ifdef DEBUG_ADS1255
	uint8_t tmp = 0;
	ADS125X_Register_Read(ads, ADS125X_REG_MUX, &tmp, 1);
	printf("MUX  : %#.2x\n", tmp);
#endif
	return 0;
}


/**
 * @brief  cycle through the desired channels
 * @param  *ads pointer to ads handle
 * @param  *pVolt pointer to output array
 * @return
 * @see    Datasheet Fig. 19 Cycling the ADS1256 Input Multiplexer
 */
void ADS125X_cycle_Through_Channels(ADS125X_t *ads, float *pVolt) //Cycling through all (8) single ended channels
{
	uint8_t spiTx[6] = {
	ADS125X_CMD_WREG | ADS125X_REG_MUX, // 1st command byte
			0, 							// 2nd command byte = payload length = 1 bytes -1 = 0
			0,							// 3rd command byte = data to write to register
			ADS125X_CMD_SYNC,			// see Figure 19. Cycling the ADS1256 Input Multiplexer
			ADS125X_CMD_WAKEUP,
			ADS125X_CMD_RDATA };
	uint8_t spiRx[3] = { 0, 0, 0 };

	switch (ads->cycle) {
	case 0:
		spiTx[2] = ADS125X_MUXP_AIN1 | ADS125X_MUXN_AINCOM;	//second value stored in the volt array
		ADS125X_CS(ads, 1);
		HAL_SPI_Transmit(ads->hspix, spiTx, 4, 10);
		//delay_us(4);										//t11 delay 24*tau = 3.125 us //delay should be larger, so we delay by 4 us
		HAL_SPI_Transmit(ads->hspix, &spiTx[4], 2, 10);		//splitting the transmit message works fine, combining them don't, you can add a delay of t11
		delay_us(7);										//Wait t6 time (~6.51 us) REF: P34, FIG:30
		HAL_SPI_Receive(ads->hspix, spiRx, 3, 10);
		ADS125X_CS(ads, 0);
		break;
	case 1:
		spiTx[2] = ADS125X_MUXP_AIN0 | ADS125X_MUXN_AIN1; //third value stored in the volt array
		ADS125X_CS(ads, 1);
		HAL_SPI_Transmit(ads->hspix, spiTx, 4, 10);
		//delay_us(4);
		HAL_SPI_Transmit(ads->hspix, &spiTx[4], 2, 10);
		delay_us(7);
		HAL_SPI_Receive(ads->hspix, spiRx, 3, 10);
		ADS125X_CS(ads, 0);
		break;
	case 2:
		spiTx[2] = ADS125X_MUXP_AIN0 | ADS125X_MUXN_AINCOM;	//first value stored in the volt array
		ADS125X_CS(ads, 1);
		HAL_SPI_Transmit(ads->hspix, spiTx, 4, 10);
		//delay_us(4);
		HAL_SPI_Transmit(ads->hspix, &spiTx[4], 2, 10);
		delay_us(7);
		HAL_SPI_Receive(ads->hspix, spiRx, 3, 10);
		ADS125X_CS(ads, 0);
		break;

	default:
		break;
	}

	int32_t adsCode = (spiRx[0] << 16) | (spiRx[1] << 8) | +(spiRx[2]);
	ADS125X_ADC_Code2Volt(ads, &adsCode, &pVolt[ads->cycle], 1);

	ads->cycle = (ads->cycle + 1) % 3;	//3 channels only you have to adjust this manually
}
