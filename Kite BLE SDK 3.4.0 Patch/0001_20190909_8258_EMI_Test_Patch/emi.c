/********************************************************************************************************
 * @file     emi.c
 *
 * @brief    This is the source file for TLSR8258
 *
 * @author	 Driver Group
 * @date     May 8, 2018
 *
 * @par      Copyright (c) 2018, Telink Semiconductor (Shanghai) Co., Ltd.
 *           All rights reserved.
 *
 *           The information contained herein is confidential property of Telink
 *           Semiconductor (Shanghai) Co., Ltd. and is available under the terms
 *           of Commercial License Agreement between Telink Semiconductor (Shanghai)
 *           Co., Ltd. and the licensee or the terms described here-in. This heading
 *           MUST NOT be removed from this file.
 *
 *           Licensees are granted free, non-transferable use of the information in this
 *           file under Mutual Non-Disclosure Agreement. NO WARRENTY of ANY KIND is provided.
 * @par      History:
 * 			 1.initial release(DEC. 26 2018)
 *
 * @version  A001
 *
 *******************************************************************************************************/
#include "emi.h"
#include "clock.h"
#include "timer.h"
#include "bsp.h"
#include "rf_drv.h"


#define STATE0		0x1234
#define STATE1		0x5678
#define STATE2		0xabcd
#define STATE3		0xef01

static unsigned char  emi_rx_packet[64] __attribute__ ((aligned (4)));
static unsigned char  emi_zigbee_tx_packet[48]  __attribute__ ((aligned (4))) = {19,0,0,0,20,0,0};
static unsigned char  emi_ble_tx_packet [48]  __attribute__ ((aligned (4))) = {39, 0, 0, 0,0, 37};
static unsigned int   emi_rx_cnt=0,emi_rssibuf=0;
static signed  char   rssi=0;
static unsigned int   state0,state1;


const TBLCMDSET Tbl_rf_init[] = {
    {0x12d2, 0x9b,  TCMD_UNDER_BOTH | TCMD_WRITE}, //DCOC_DBG0
    {0x12d3, 0x19,  TCMD_UNDER_BOTH | TCMD_WRITE}, //DCOC_DBG1
    {0x127b, 0x0e,  TCMD_UNDER_BOTH | TCMD_WRITE}, //BYPASS_FILT_1
    {0x1276, 0x50,  TCMD_UNDER_BOTH | TCMD_WRITE}, //FREQ_CORR_CFG2_0
    {0x1277, 0x73,  TCMD_UNDER_BOTH | TCMD_WRITE}, //FREQ_CORR_CFG2_1
};

const TBLCMDSET  Tbl_rf_zigbee_250k[] =
{
    {0x1220, 0x04, TCMD_UNDER_BOTH | TCMD_WRITE},
    {0x1221, 0x2b, TCMD_UNDER_BOTH | TCMD_WRITE},
    {0x1222, 0x43, TCMD_UNDER_BOTH | TCMD_WRITE},
    {0x1223, 0x86, TCMD_UNDER_BOTH | TCMD_WRITE},
    {0x122a, 0x90, TCMD_UNDER_BOTH | TCMD_WRITE},
    {0x1254, 0x0e,  TCMD_UNDER_BOTH | TCMD_WRITE}, //AGC_THRSHLD1_2M_0
    {0x1255, 0x09,  TCMD_UNDER_BOTH | TCMD_WRITE}, //AGC_THRSHLD1_2M_1
    {0x1256, 0x0c,  TCMD_UNDER_BOTH | TCMD_WRITE}, //AGC_THRSHLD2_2M_0
    {0x1257, 0x08,  TCMD_UNDER_BOTH | TCMD_WRITE}, //AGC_THRSHLD2_2M_1
    {0x1258, 0x09,  TCMD_UNDER_BOTH | TCMD_WRITE}, //AGC_THRSHLD3_2M_0
    {0x1259, 0x0f,  TCMD_UNDER_BOTH | TCMD_WRITE}, //AGC_THRSHLD3_2M_1

    {0x400, 0x13, TCMD_UNDER_BOTH | TCMD_WRITE},//{0x400, 0x0a,	TCMD_UNDER_BOTH | TCMD_WRITE},
    {0x401, 0x00,	TCMD_UNDER_BOTH | TCMD_WRITE},	// PN disable
    {0x420, 0x18, TCMD_UNDER_BOTH | TCMD_WRITE},

    {0x402, 0x46, TCMD_UNDER_BOTH | TCMD_WRITE}, //pre-amble


    {0x404, 0xc0, TCMD_UNDER_BOTH | TCMD_WRITE},
    {0x405, 0x04, TCMD_UNDER_BOTH | TCMD_WRITE},
    {0x421, 0x23, TCMD_UNDER_BOTH | TCMD_WRITE},
    {0x422, 0x04, TCMD_UNDER_BOTH | TCMD_WRITE},
    {0x408, 0xa7, TCMD_UNDER_BOTH | TCMD_WRITE},
    {0x409, 0x00, TCMD_UNDER_BOTH | TCMD_WRITE},
    {0x40a, 0x00, TCMD_UNDER_BOTH | TCMD_WRITE},
    {0x40b, 0x00, TCMD_UNDER_BOTH | TCMD_WRITE},
    //AGC table 2M
    {0x460, 0x36, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_0
    {0x461, 0x46, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_1
    {0x462, 0x51, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_2
    {0x463, 0x61, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_3
    {0x464, 0x6d, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_4
    {0x465, 0x78, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_5
};

const TBLCMDSET  Tbl_rf_1m[] = {

		//these setting below is for AURA RF module BLE 1M mode
		{0x1220, 0x16,	TCMD_UNDER_BOTH | TCMD_WRITE},
		{0x1221, 0x0a,	TCMD_UNDER_BOTH | TCMD_WRITE},
		{0x1222, 0x20,	TCMD_UNDER_BOTH | TCMD_WRITE},
		{0x1223, 0x23,	TCMD_UNDER_BOTH | TCMD_WRITE},
		{0x124a, 0x0e,  TCMD_UNDER_BOTH | TCMD_WRITE}, //AGC_THRSHLD1_1M_0
		{0x124b, 0x09,  TCMD_UNDER_BOTH | TCMD_WRITE}, //AGC_THRSHLD1_1M_1
		{0x124e, 0x09,  TCMD_UNDER_BOTH | TCMD_WRITE}, //AGC_THRSHLD3_1M_0
		{0x124f, 0x0f,  TCMD_UNDER_BOTH | TCMD_WRITE}, //AGC_THRSHLD3_1M_1
		//8258 script set 0x1f, but 8255 ble  SDK set 0x0f
		//BIT<4>:tx output data mode, 0: xor 32'hcccccccc; 1: xor 32'h66666666
		{0x400, 0x1f,	TCMD_UNDER_BOTH | TCMD_WRITE}, //poweron_dft: 0x1f    	//<4>default,8255 use 0 with error

		//core_402, [7:5] trailer_len,  [4:0] new 2m mode preamble length
		//8255 is 0x26, 8258 script is 0x43
		{0x402, 0x46,	TCMD_UNDER_BOTH | TCMD_WRITE},//<4:0>preamble length   	//<7:4>default

		{0x401, 0x00,	TCMD_UNDER_BOTH | TCMD_WRITE},	// PN disable
		{0x404, 0xd5,	TCMD_UNDER_BOTH | TCMD_WRITE},//BLE header and 1Mbps  BIT<5>: ble_wt(PN) enable

		{0x405, 0x04,	TCMD_UNDER_BOTH | TCMD_WRITE},

		{0x420, 0x1e,	TCMD_UNDER_BOTH | TCMD_WRITE}, //sync thre bit match
		{0x421, 0xa1,	TCMD_UNDER_BOTH | TCMD_WRITE}, //len_0_en

		{0x430, 0x3e,	TCMD_UNDER_BOTH | TCMD_WRITE},
	    //AGC table
	    {0x460, 0x34, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_0
	    {0x461, 0x44, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_1
	    {0x462, 0x4f, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_2
	    {0x463, 0x5f, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_3
	    {0x464, 0x6b, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_4
	    {0x465, 0x76, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_5
};

const TBLCMDSET  Tbl_rf_2m[] =
{
	{0x1220, 0x04,	TCMD_UNDER_BOTH | TCMD_WRITE},
	{0x1221, 0x2a,	TCMD_UNDER_BOTH | TCMD_WRITE},
	{0x1222, 0x43,	TCMD_UNDER_BOTH | TCMD_WRITE},
	{0x1223, 0x06,	TCMD_UNDER_BOTH | TCMD_WRITE},
    {0x1254, 0x0e,  TCMD_UNDER_BOTH | TCMD_WRITE}, //AGC_THRSHLD1_2M_0
    {0x1255, 0x09,  TCMD_UNDER_BOTH | TCMD_WRITE}, //AGC_THRSHLD1_2M_1
    {0x1256, 0x0c,  TCMD_UNDER_BOTH | TCMD_WRITE}, //AGC_THRSHLD2_2M_0
    {0x1257, 0x08,  TCMD_UNDER_BOTH | TCMD_WRITE}, //AGC_THRSHLD2_2M_1
    {0x1258, 0x09,  TCMD_UNDER_BOTH | TCMD_WRITE}, //AGC_THRSHLD3_2M_0
    {0x1259, 0x0f,  TCMD_UNDER_BOTH | TCMD_WRITE}, //AGC_THRSHLD3_2M_1

	{0x400, 0x1f,	TCMD_UNDER_BOTH | TCMD_WRITE},
	{0x401, 0x00,	TCMD_UNDER_BOTH | TCMD_WRITE},	// PN disable
	{0x402, 0x46,	TCMD_UNDER_BOTH | TCMD_WRITE},//preamble length<4:0>
	{0x404, 0xc5,	TCMD_UNDER_BOTH | TCMD_WRITE},//BLE header and 2Mbps  BIT<5>: ble_wt(PN) disable

	{0x405, 0x04,	TCMD_UNDER_BOTH | TCMD_WRITE},
	{0x420, 0x1e,	TCMD_UNDER_BOTH | TCMD_WRITE},//access code match threshold
	{0x421, 0xa1,	TCMD_UNDER_BOTH | TCMD_WRITE},

	{0x430, 0x3e,	TCMD_UNDER_BOTH | TCMD_WRITE}, //<1> hd_timestamp

    //AGC table
    {0x460, 0x36, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_0
    {0x461, 0x46, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_1
    {0x462, 0x51, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_2
    {0x463, 0x61, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_3
    {0x464, 0x6d, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_4
    {0x465, 0x78, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_5
};

const TBLCMDSET  Tbl_rf_s2_500k[] =
{
	{0x1220, 0x17,	TCMD_UNDER_BOTH | TCMD_WRITE},
	{0x1221, 0x0a,	TCMD_UNDER_BOTH | TCMD_WRITE},
	{0x1222, 0x20,	TCMD_UNDER_BOTH | TCMD_WRITE},
	{0x1223, 0x23,	TCMD_UNDER_BOTH | TCMD_WRITE},

	{0x1273, 0x21,	TCMD_UNDER_BOTH | TCMD_WRITE},
	{0x1237, 0x8c,	TCMD_UNDER_BOTH | TCMD_WRITE},
	{0x1236, 0xee,	TCMD_UNDER_BOTH | TCMD_WRITE},
	{0x1238, 0xc8,	TCMD_UNDER_BOTH | TCMD_WRITE},
	{0x1239, 0x7d,  TCMD_UNDER_BOTH | TCMD_WRITE},//  poweron_dft: 0x71  // //add to resolve frequency offset(add by zhaowei 2019-7-25)

	{0x124a, 0x0e,  TCMD_UNDER_BOTH | TCMD_WRITE}, //AGC_THRSHLD1_1M_0
	{0x124b, 0x09,  TCMD_UNDER_BOTH | TCMD_WRITE}, //AGC_THRSHLD1_1M_1
	{0x124e, 0x09,  TCMD_UNDER_BOTH | TCMD_WRITE}, //AGC_THRSHLD3_1M_0
	{0x124f, 0x0f,  TCMD_UNDER_BOTH | TCMD_WRITE}, //AGC_THRSHLD3_1M_1
	{0x400, 0x1f,	TCMD_UNDER_BOTH | TCMD_WRITE},
	{0x401, 0x00,	TCMD_UNDER_BOTH | TCMD_WRITE},	// PN disable
	{0x402, 0x4a,	TCMD_UNDER_BOTH | TCMD_WRITE},//preamble length<4:0>
	{0x404, 0xd5,	TCMD_UNDER_BOTH | TCMD_WRITE},//BLE header and 1Mbps  BIT<5>: ble_wt(PN) disable

	{0x420, 0xf0,	TCMD_UNDER_BOTH | TCMD_WRITE},//access code match threshold
	{0x421, 0xa1,	TCMD_UNDER_BOTH | TCMD_WRITE},
	{0x405, 0xa4,	TCMD_UNDER_BOTH | TCMD_WRITE}, //<7>read only

	{0x430, 0x3e,	TCMD_UNDER_BOTH | TCMD_WRITE}, //<1> hd_timestamp

    //AGC table 1M
    {0x460, 0x34, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_0
    {0x461, 0x44, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_1
    {0x462, 0x4f, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_2
    {0x463, 0x5f, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_3
    {0x464, 0x6b, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_4
    {0x465, 0x76, TCMD_UNDER_BOTH | TCMD_WRITE} //grx_5
};

const TBLCMDSET  Tbl_rf_s8_125k[] =
{
	{0x1220, 0x17,	TCMD_UNDER_BOTH | TCMD_WRITE},
	{0x1221, 0x0a,	TCMD_UNDER_BOTH | TCMD_WRITE},
	{0x1222, 0x20,	TCMD_UNDER_BOTH | TCMD_WRITE},
	{0x1223, 0x23,	TCMD_UNDER_BOTH | TCMD_WRITE},

	{0x1273, 0x21,	TCMD_UNDER_BOTH | TCMD_WRITE},
	{0x1237, 0x8c,	TCMD_UNDER_BOTH | TCMD_WRITE},
	{0x1236, 0xf6,	TCMD_UNDER_BOTH | TCMD_WRITE},
	{0x1238, 0xc8,	TCMD_UNDER_BOTH | TCMD_WRITE},

	{0x1239, 0x7d,  TCMD_UNDER_BOTH | TCMD_WRITE},  //added by junwen kangpingpian

	{0x124a, 0x0e,  TCMD_UNDER_BOTH | TCMD_WRITE}, //AGC_THRSHLD1_1M_0
	{0x124b, 0x09,  TCMD_UNDER_BOTH | TCMD_WRITE}, //AGC_THRSHLD1_1M_1
	{0x124e, 0x09,  TCMD_UNDER_BOTH | TCMD_WRITE}, //AGC_THRSHLD3_1M_0
	{0x124f, 0x0f,  TCMD_UNDER_BOTH | TCMD_WRITE}, //AGC_THRSHLD3_1M_1

	{0x400, 0x1f,	TCMD_UNDER_BOTH | TCMD_WRITE},
	{0x401, 0x00,	TCMD_UNDER_BOTH | TCMD_WRITE},	// PN disable
	{0x402, 0x4a,	TCMD_UNDER_BOTH | TCMD_WRITE},//preamble length<4:0>

	{0x404, 0xd5,	TCMD_UNDER_BOTH | TCMD_WRITE},//BLE header and 1Mbps  BIT<5>: ble_wt(PN) disable

	{0x420, 0xf0,	TCMD_UNDER_BOTH | TCMD_WRITE},//access code match threshold
	{0x421, 0xa1,	TCMD_UNDER_BOTH | TCMD_WRITE},
	{0x405, 0xb4,	TCMD_UNDER_BOTH | TCMD_WRITE}, //<7>read only

	{0x430, 0x3e,	TCMD_UNDER_BOTH | TCMD_WRITE}, //<1> hd_timestamp

    //AGC table 1M
    {0x460, 0x34, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_0
    {0x461, 0x44, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_1
    {0x462, 0x4f, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_2
    {0x463, 0x5f, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_3
    {0x464, 0x6b, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_4
    {0x465, 0x76, TCMD_UNDER_BOTH | TCMD_WRITE},//grx_5
};


/**
*	@brief     This function serves to RF mode initialization
*			   SDK  may remove some features of the rf_drv_init,and thus not supporting multiple modes
*			   rf_multi_mode_drv_init function does not allow modification and is used to support multiple modes
*	@param[in] rf_mode  -  mode of RF
*	@return	   none.
*/

void rf_multi_mode_drv_init(RF_ModeTypeDef rf_mode)
{

	LoadTblCmdSet(Tbl_rf_init, sizeof(Tbl_rf_init)/sizeof (TBLCMDSET));

    if (rf_mode == RF_MODE_BLE_1M) {
    	LoadTblCmdSet (Tbl_rf_1m, sizeof (Tbl_rf_1m)/sizeof (TBLCMDSET));
	}
    else if(rf_mode == RF_MODE_BLE_2M) {
    	LoadTblCmdSet (Tbl_rf_2m, sizeof (Tbl_rf_2m)/sizeof (TBLCMDSET));
	}
    else if (rf_mode == RF_MODE_LR_S2_500K) {
    	LoadTblCmdSet (Tbl_rf_s2_500k, sizeof (Tbl_rf_s2_500k)/sizeof (TBLCMDSET));
	}
    else if (rf_mode == RF_MODE_LR_S8_125K) {
    	LoadTblCmdSet (Tbl_rf_s8_125k, sizeof (Tbl_rf_s8_125k)/sizeof (TBLCMDSET));
    }
    else if (rf_mode == RF_MODE_ZIGBEE_250K) {
    	LoadTblCmdSet(Tbl_rf_zigbee_250k, sizeof(Tbl_rf_zigbee_250k)/sizeof(TBLCMDSET));
    }

	reg_dma_chn_en |= FLD_DMA_CHN_RF_RX  | FLD_DMA_CHN_RF_TX;
}

/**
 * @brief   This function serves to set singletone power.
 * @param   level - the power level.
 * @return  none.
 */
void rf_set_power_level_index_singletone (RF_PowerTypeDef level)
{
	unsigned char value;

	if(level&BIT(7))
	{
		write_reg8(0x1225, (read_reg8(0x1225) & 0xbf) | ((0x01)<<6));//VANT
	}
	else
	{
		write_reg8(0x1225, (read_reg8(0x1225) & 0xbf));
	}
	write_reg8 (0xf02, 0x55);  //tx_en
	sleep_us(150);

	value = (unsigned char)level&0x3f;

	sub_wr(0x1378, 1, 6, 6);  //TX_PA_PWR_OW
	sub_wr(0x137c, value , 6, 1);  //TX_PA_PWR  0x3f18
}

/**
 * @brief   This function serves to set singletone power and channel.
 * @param   power_level - the power level.
 * @param   rf_chn - the channel.
 * @return  none.
 */
void rf_emi_single_tone(RF_PowerTypeDef power_level,signed char rf_chn)
{
	rf_multi_mode_drv_init(RF_MODE_ZIGBEE_250K);
	rf_set_channel_singletone(rf_chn);//set freq
	write_reg8 (0xf02, 0x45);  //tx_en
//	rf_set_power_level_index_singletone(power_level);
	rf_set_power_level_index_singletone(rf_power_Level_list[power_level]);
}


/**
 * @brief   This function serves to close RF
 * @param   none.
 * @return  none.
 */
void rf_emi_stop(void)
{
	write_reg8(0x1378, 0);
	write_reg8(0x137c, 0);//TX_PA_PWR
	rf_set_power_level_index (0);
	rf_set_tx_rx_off();
}

/**
 * @brief   This function serves to set rx mode and channel.
 * @param   mode - mode of RF
 * @param   rf_chn - the rx channel.
 * @return  none.
 */
void rf_emi_rx(RF_ModeTypeDef mode,signed char rf_chn)
{
	write_reg32 (0x408, 0x29417671);	//accesscode: 1001-0100 1000-0010 0110-1110 1000-1110   29 41 76 71
	write_reg8 (0x405, read_reg8(0x405)|0x80); //trig accesscode

	rf_rx_buffer_set(emi_rx_packet,64,0);
	rf_multi_mode_drv_init(mode);
	rf_set_channel(rf_chn,0);//set freq
	rf_set_tx_rx_off();
	rf_set_rxmode();
	sleep_us(150);
	rssi = 0;
	emi_rssibuf = 0;
	emi_rx_cnt = 0;
}

/**
 * @brief   This function serves is receiving service program
 * @param   none.
 * @return  none.
 */
void rf_emi_rx_loop(void)
{
	if(read_reg8(0xf20)&BIT(0))
	{
		if((read_reg8(0x44f)&0x0f)==0)
		{
			emi_rssibuf +=  (read_reg8(0x449));
			if(emi_rx_cnt)
				if(emi_rssibuf!=0)
				emi_rssibuf>>=1;
			rssi = emi_rssibuf-110;
			emi_rx_cnt++;
		}
		write_reg8(0xf20, 1);
		write_reg8(0xf00, 0x80);
	}
}

/**
 * @brief   This function serves to get the number of packets received
 * @param   none.
 * @return  the number of packets received.
 */
unsigned int rf_emi_get_rxpkt_cnt(void)
{
	return emi_rx_cnt;
}

/**
 * @brief   This function serves to get the RSSI of packets received
 * @param   none.
 * @return  the RSSI of packets received.
 */
char rf_emi_get_rssi_avg(void)
{
	return rssi;
}

/**
 * @brief   This function serves to get the address of the received packets
 * @param   none.
 * @return  the address of the received packets
 */
unsigned char *rf_emi_get_rxpkt(void)
{
	return emi_rx_packet;
}

/**
 * @brief   This function serves to generate random number.
 * @param   the old random number.
 * @return  the new random number.
 */
static unsigned int pnGen(unsigned int state)
{
	unsigned int feed = 0;
	feed = (state&0x4000) >> 1;
	state ^= feed;
	state <<= 1;
	state = (state&0xfffe) + ((state&0x8000)>>15);
	return state;
}

/**
 * @brief   This function serves to Set the CD mode correlation register.
 * @param   none.
 * @return  none.
 */
static void rf_continue_mode_setup(void)
{

	write_reg8(0x400,0x0a);
	write_reg8(0x408,0x00);

	write_reg8(0x401,0x80);//kick tx controller to wait data
	state0 = STATE0;
	state1 = STATE1;
}

/**
 * @brief   This function serves to continue to send CD mode.
 * @param   none.
 * @return  none.
 */
void rf_continue_mode_run(void)
{
	if(read_reg8(0x408) == 1){
		write_reg32(0x41c, 0x0f0f0f0f);
	}else if(read_reg8(0x408)==2){
		write_reg32(0x41c, 0x55555555);
	}else if(read_reg8(0x408)==3){
		write_reg32(0x41c, read_reg32(0x409));
	}else if(read_reg8(0x408)==4){
		write_reg32(0x41c, 0);
	}else if(read_reg8(0x408)==5){
		write_reg32(0x41c, 0xffffffff);
	}else{
		write_reg32(0x41c, (state0<<16)+state1);
		state0 = pnGen(state0);
		state1 = pnGen(state1);
	}

	while(read_reg8(0x41c) & 0x1){
	}
}




/**
 * @brief   This function serves to init the CD mode.
 * @param   rf_mode - mode of RF.
 * @param   power_level - power level of RF.
 * @param   rf_chn - channel of RF.
 * @param 	pkt_type - The type of data sent
 * 					   0:random data
 * 					   1:0xf0
 * 					   2:0x55
 * @return  none.
 */
void rf_emi_tx_continue_setup(RF_ModeTypeDef rf_mode,RF_PowerTypeDef power_level,signed char rf_chn,unsigned char pkt_type)
{
	rf_multi_mode_drv_init(rf_mode);//RF_MODE_BLE_1M
	rf_set_channel(rf_chn,0);
	write_reg8(0xf02, 0x45);  //tx_en
	rf_set_power_level_index_singletone (rf_power_Level_list[power_level]);
	rf_continue_mode_setup();
	write_reg8(0x408, pkt_type);//0:pbrs9 	1:0xf0	 2:0x55
}



/**
 * @brief   This function serves to generate random packets that need to be sent in burst mode
 * @param   *p - the address of random packets.
 * @param   n - the number of random packets.
 * @return  none.
 */
static void rf_phy_test_prbs9 (unsigned char *p, int n)
{
	//PRBS9: (x >> 1) | (((x<<4) ^ (x<<8)) & 0x100)
	unsigned short x = 0x1ff;
	int i;
	int j;
	for ( i=0; i<n; i++)
	{
		unsigned char d = 0;
		for (j=0; j<8; j++)
		{
			if (x & 1)
			{
				d |= BIT(j);
			}
			x = (x >> 1) | (((x<<4) ^ (x<<8)) & 0x100);
		}
		*p++ = d;
	}
}

/**
 * @brief   This function serves to init the burst mode.
 * @param   rf_mode - mode of RF.
 * @param   power_level - power level of RF.
 * @param   rf_chn - channel of RF.
 * @param 	pkt_type - The type of data sent
 * 					   0:random data
 * 					   1:0xf0
 * 					   2:0x55
 * @return  none.
 */
void rf_emi_tx_burst_setup(RF_ModeTypeDef rf_mode,RF_PowerTypeDef power_level,signed char rf_chn,unsigned char pkt_type)
{
//#if 0
	unsigned char i;
	unsigned char tx_data=0;

	write_reg32(0x408,0x29417671 );//access code  0xf8118ac9
	write_reg8 (0x405, read_reg8(0x405)|0x80);
	write_reg8(0x13c,0x10); // print buffer size set
	rf_set_channel(rf_chn,0);
	rf_multi_mode_drv_init(rf_mode);

	rf_set_power_level_index ((unsigned char)rf_power_Level_list[power_level&0x3f]);
	if(pkt_type==1)  tx_data = 0x0f;
	else if(pkt_type==2)  tx_data = 0x55;


	switch(rf_mode)
	{
		case RF_MODE_LR_S2_500K:
		case RF_MODE_LR_S8_125K:
		case RF_MODE_BLE_1M:
		case RF_MODE_BLE_2M:
			emi_ble_tx_packet[4] = pkt_type;//type
			for( i=0;i<37;i++)
			{
				emi_ble_tx_packet[6+i]=tx_data;
			}
			break;

		case RF_MODE_ZIGBEE_250K:
			emi_zigbee_tx_packet[5] = pkt_type;//type
			for( i=0;i<37;i++)
			{
				emi_zigbee_tx_packet[5+i]=tx_data;
			}
			break;

		default:
			break;
	}
}

/**
 * @brief   This function serves to init the burst mode with PA ramp up/down.
 * @param   rf_mode - mode of RF.
 * @param   power_level - power level of RF.
 * @param   rf_chn - channel of RF.
 * @param 	pkt_type - The type of data sent
 * 					   0:random data
 * 					   1:0xf0
 * 					   2:0x55
 * @return  none.
 */
void rf_emi_tx_brust_setup_ramp(RF_ModeTypeDef rf_mode,RF_PowerTypeDef power_level,signed char rf_chn,unsigned char pkt_type)
{
//#if 0
	unsigned char i;
	unsigned char tx_data=0;

	write_reg32(0x408,0x29417671 );//access code  0xf8118ac9
	write_reg8 (0x405, read_reg8(0x405)|0x80);
	write_reg8(0x13c,0x10); // print buffer size set
	rf_set_channel(rf_chn,0);
	rf_multi_mode_drv_init(rf_mode);
	rf_set_tx_rx_off();

	rf_set_power_level_index_singletone(rf_power_Level_list[power_level]);
	if(pkt_type==1)  tx_data = 0x0f;
	else if(pkt_type==2)  tx_data = 0x55;


	switch(rf_mode)
	{
		case RF_MODE_LR_S2_500K:
		case RF_MODE_LR_S8_125K:
		case RF_MODE_BLE_1M:
		case RF_MODE_BLE_2M:
			emi_ble_tx_packet[4] = pkt_type;//type
			for( i=0;i<37;i++)
			{
				emi_ble_tx_packet[6+i]=tx_data;
			}
			break;

		case RF_MODE_ZIGBEE_250K:
			emi_zigbee_tx_packet[5] = pkt_type;//type
			for( i=0;i<37;i++)
			{
				emi_zigbee_tx_packet[5+i]=tx_data;
			}
			break;

		default:
			break;
	}
}




/**
 * @brief   This function serves to send packets in the burst mode with PA ramp up/down.
 * @param   rf_mode - mode of RF.
 * @param 	pkt_type - The type of data sent
 * 					   0:random data
 * 					   1:0xf0
 * 					   2:0x55
 * @return  none.
 */
void rf_emi_tx_burst_loop_ramp(RF_ModeTypeDef rf_mode,unsigned char pkt_type)
{
	write_reg8(0xf00, 0x80); // stop SM
	int power = (unsigned char)rf_power_Level_list[read_reg8(0x40008)];
	if((rf_mode==RF_MODE_BLE_1M)||(rf_mode==RF_MODE_BLE_2M)||(rf_mode==RF_MODE_LR_S8_125K)||(rf_mode==RF_MODE_LR_S2_500K))//ble
	{
		for(int i=0;i<=power;i++)
		{
			sub_wr(0x137c, i , 6, 1);
		}
		rf_tx_pkt(emi_ble_tx_packet);
		while(!rf_tx_finish());
		rf_tx_finish_clear_flag();

		for(int i=power;i>=0;i--)
		{
			sub_wr(0x137c, i , 6, 1);
		}
		sleep_ms(2);
		if(pkt_type==0)
			rf_phy_test_prbs9(&emi_ble_tx_packet[6],37);
	}
	else if(rf_mode==RF_MODE_ZIGBEE_250K)//zigbee
	{
		for(int i=0;i<=power;i++)
			sub_wr(0x137c, i , 6, 1);

		rf_tx_pkt(emi_zigbee_tx_packet);
		while(!rf_tx_finish());
		rf_tx_finish_clear_flag();
		for(int i=power;i>=0;i--)
			sub_wr(0x137c, i , 6, 1);
		sleep_ms(4);
		if(pkt_type==0)
			rf_phy_test_prbs9(&emi_zigbee_tx_packet[5],37);
	}
}

/**
 * @brief   This function serves to send packets in the burst mode
 * @param   rf_mode - mode of RF.
 * @param 	pkt_type - The type of data sent
 * 					   0:random data
 * 					   1:0xf0
 * 					   2:0x55
 * @return  none.
 */
void rf_emi_tx_burst_loop(RF_ModeTypeDef rf_mode,unsigned char pkt_type)
{
	write_reg8(0xf00, 0x80); // stop SM

	if((rf_mode==RF_MODE_BLE_1M)||(rf_mode==RF_MODE_BLE_2M))//ble
	{

		rf_start_stx ((void *)emi_ble_tx_packet, read_reg32(0x740) + 10);
		while(!rf_tx_finish());
		rf_tx_finish_clear_flag();

		sleep_ms(2);//
//		delay_us(625);//
		if(pkt_type==0)
			rf_phy_test_prbs9(&emi_ble_tx_packet[6],37);
	}
	else if(rf_mode==RF_MODE_LR_S8_125K)//ble
	{
		rf_start_stx ((void *)emi_ble_tx_packet, read_reg32(0x740) + 10);
		while(!rf_tx_finish());
		rf_tx_finish_clear_flag();
		sleep_ms(2);//
		if(pkt_type==0)
			rf_phy_test_prbs9(&emi_ble_tx_packet[6],37);
	}
	else if(rf_mode==RF_MODE_LR_S2_500K)//ble
	{
		rf_start_stx ((void *)emi_ble_tx_packet, read_reg32(0x740) + 10);
		while(!rf_tx_finish());
		rf_tx_finish_clear_flag();
		sleep_ms(2);//625*1.5
		if(pkt_type==0)
			rf_phy_test_prbs9(&emi_ble_tx_packet[6],37);
	}
	else if(rf_mode==RF_MODE_ZIGBEE_250K)//zigbee
	{

		rf_start_stx ((void *)emi_zigbee_tx_packet, read_reg32(0x740) + 10);
		while(!rf_tx_finish());
		rf_tx_finish_clear_flag();
		sleep_us(625*2);//
		if(pkt_type==0)
			rf_phy_test_prbs9(&emi_zigbee_tx_packet[5],37);
	}
}

/**
 * @brief   This function serves to set the channel in singletone mode.
 * @param   chn - channel of RF.
 * @return  none.
 */
void rf_set_channel_singletone (signed char chn)//general
{
	unsigned short rf_chn =0;
	unsigned char ctrim;
	unsigned short chnl_freq;

	rf_chn = chn+2400;

	if (rf_chn >= 2550)
	    ctrim = 0;
	else if (rf_chn >= 2520)
	    ctrim = 1;
	else if (rf_chn >= 2495)
	    ctrim = 2;
	else if (rf_chn >= 2465)
	    ctrim = 3;
	else if (rf_chn >= 2435)
		ctrim = 4;
	else if (rf_chn >= 2405)
	    ctrim = 5;
	else if (rf_chn >= 2380)
	    ctrim = 6;
	else
	    ctrim = 7;

	chnl_freq = rf_chn * 2 +1;

	write_reg8(0x1244, ((chnl_freq & 0x7f)<<1) | 1  );   //CHNL_FREQ_DIRECT   CHNL_FREQ_L
	write_reg8(0x1245,  ((read_reg8(0x1245) & 0xc0)) | ((chnl_freq>>7)&0x3f) );  //CHNL_FREQ_H
	write_reg8(0x1229,  (read_reg8(0x1229) & 0xC3) | (ctrim<<2) );  //FE_CTRIM

}


