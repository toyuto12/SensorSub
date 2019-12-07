/***********************************************************************************************************************
* DISCLAIMER
* This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products.
* No other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
* applicable laws, including copyright laws. 
* THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING THIS SOFTWARE, WHETHER EXPRESS, IMPLIED
* OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NON-INFRINGEMENT.  ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED.TO THE MAXIMUM EXTENT PERMITTED NOT PROHIBITED BY
* LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES SHALL BE LIABLE FOR ANY DIRECT,
* INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO THIS SOFTWARE, EVEN IF RENESAS OR
* ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
* Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability 
* of this software. By using this software, you agree to the additional terms and conditions found by accessing the 
* following link:
* http://www.renesas.com/disclaimer
*
* Copyright (C) 2011, 2018 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/

/***********************************************************************************************************************
* File Name    : r_main.c
* Version      : CodeGenerator for RL78/G14 V2.05.03.02 [06 Nov 2018]
* Device(s)    : R5F104BC
* Tool-Chain   : CCRL
* Description  : This file implements main function.
* Creation Date: 2019/12/07
***********************************************************************************************************************/

/***********************************************************************************************************************
Includes
***********************************************************************************************************************/
#include "r_cg_macrodriver.h"
#include "r_cg_cgc.h"
#include "r_cg_port.h"
#include "r_cg_serial.h"
#include "r_cg_adc.h"
#include "r_cg_wdt.h"
#include "r_cg_it.h"
/* Start user code for include. Do not edit comment generated here */
/* End user code. Do not edit comment generated here */
#include "r_cg_userdefine.h"

/***********************************************************************************************************************
Pragma directive
***********************************************************************************************************************/
/* Start user code for pragma. Do not edit comment generated here */
/* End user code. Do not edit comment generated here */

/***********************************************************************************************************************
Global variables and functions
***********************************************************************************************************************/
/* Start user code for global. Do not edit comment generated here */
uint16_t adReal[6];
uint16_t adTmp[6][16];
uint8_t adCyc;
const uint8_t adCh[] = { ADCHANNEL0, ADCHANNEL1, ADCHANNEL2, ADCHANNEL3, ADCHANNEL16, ADCHANNEL17 };

uint8_t gInterval;
stReceiveData gRev;
uint8_t gIsReceived;

#define SOFT_VER		0
#define SOFT_REV		0

void TaskAdResult(void);
uint16_t ReadAdResult( uint8_t ch );
void SendResponse( uint8_t *data );
uint8_t ReadTaskInput( void );
void debug(void);

/* End user code. Do not edit comment generated here */
void R_MAIN_UserInit(void);

/***********************************************************************************************************************
* Function Name: main
* Description  : This function implements main function.
* Arguments    : None
* Return Value : None
***********************************************************************************************************************/
void main(void)
{
    R_MAIN_UserInit();
    /* Start user code. Do not edit comment generated here */
	
	debug();
    while (1U){

		// AD変換値を最速で取得していく
		if( !ADCS ){
			adReal[adCyc] = (uint16_t)(ADCR >> 6U);
			adCyc = (adCyc==5) ?0 :adCyc+1 ;
			ADS = adCh[adCyc];
			ADCS = 1;
		}
		
		if( gInterval ){
			uint8_t rotNo;
			gInterval --;
			TaskAdResult();		// ADデータを1ms間隔でバッファに取り込み
			rotNo = ReadTaskInput();
			
			if( gIsReceived ){
				gIsReceived = 0;
				if( gRev.adr == rotNo ){
					gRev.adr <<= 4;
					gRev.cmd |= 0x80;
					if( gRev.cmd & 0x40 ){
						SendResponse( (uint8_t *)&gRev );
					}else{
						
						switch( gRev.cmd & 0x3F ){
						case 0x0B:		// SoftwareData
							gRev.param0 = 1;		// ModelNo
							gRev.param1 = SOFT_VER;
							gRev.param2 = SOFT_REV;
							SendResponse( (uint8_t *)&gRev );
							break;
						case 0x01:		// ReadData
						{
							uint16_t *pReg;
							pReg = (uint16_t *)&(gRev.param0);
							pReg[0] = ReadAdResult( 3 );
							pReg[1] = ReadAdResult( 2 );
							pReg[2] = ReadAdResult( 0 );
							pReg[3] = ReadAdResult( 4 );
							pReg[4] = ReadAdResult( 5 );
							pReg[5] = ReadAdResult( 1 );
							SendResponse( (uint8_t *)&gRev );
							break;
						}
						default:
							break;
						}
					}
				}
			}
		}
    }
    /* End user code. Do not edit comment generated here */
}

/***********************************************************************************************************************
* Function Name: R_MAIN_UserInit
* Description  : This function adds user code before implementing main function.
* Arguments    : None
* Return Value : None
***********************************************************************************************************************/
void R_MAIN_UserInit(void)
{
    /* Start user code. Do not edit comment generated here */
	R_ADC_Start();
	R_UART0_Start();
	R_IT_Start();
	EI();
    /* End user code. Do not edit comment generated here */
}

/* Start user code for adding. Do not edit comment generated here */

void TaskAdResult(void){
	static uint8_t Bufcnt;
	uint8_t cnt = 6;
	
	while( cnt-- ){
		adTmp[cnt][Bufcnt] = adReal[cnt];
	}
	Bufcnt = (Bufcnt == 15) ?0 :Bufcnt+1;
}

uint16_t ReadAdResult( uint8_t ch ){
	uint16_t *pBuf = &adTmp[ch][0],sum;
	uint8_t cnt = 16;
	while(cnt--){
		sum += pBuf[cnt];
	}
	
	return sum >> 4;
}

void SendResponse( uint8_t *data ){
	static uint8_t buf[22];
	uint8_t i=0,sum=0,cnt=18;

	P3 |= 0x01;		// RS485 DE/RE
	buf[i++] = 0xFF;
	buf[i++] = 0xFF;
	for(; i<20; i++){
		buf[i] = data[i-2];
		sum += data[i-2];
	}
	buf[i++] = sum;	
	buf[i++] = 0xFE;
	
	R_UART0_Send( buf, sizeof(buf) );
}

uint8_t ReadTaskInput( void ){
	static uint8_t Cnt,exRot=0;
	uint8_t rot = (P1>>4) & 0x0F;
	
	if( exRot == rot ){
		Cnt = 0;
	}else{
		if( Cnt < 100 ) Cnt ++;
		else exRot = rot;
	}
	return exRot;
}

void debug(void){
	const uint8_t d[] = {0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
	uint8_t sum = 0;
	uint8_t i,wait;
	
	RXD0 = 0xff;
	SRIF0 = 1;
	for(wait=0;wait<10;wait++);
	RXD0 = 0xff;
	SRIF0 = 1;
	for(wait=0;wait<10;wait++);
	for(i=0;i<sizeof(d);i++){
		sum += d[i];
		RXD0 = d[i];
		SRIF0 = 1;
		for(wait=0;wait<10;wait++);
	}	
	RXD0 = sum;
	SRIF0 = 1;
	for(wait=0;wait<10;wait++);
	RXD0 = 0xFE;
	SRIF0 = 1;
}

/* End user code. Do not edit comment generated here */
