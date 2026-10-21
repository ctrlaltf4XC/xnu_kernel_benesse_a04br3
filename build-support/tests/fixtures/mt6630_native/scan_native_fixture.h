/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/* Test-only original excerpts; see scan_provenance.json and COPYING. */

#define PARAM_MAX_LEN_SSID 32

#define CFG_SCAN_SSID_MAX_NUM 10

#define MAXIMUM_OPERATION_CHANNEL_LIST 54

#define MAX_IE_LENGTH 600

typedef struct _PARAM_SSID_T {
	UINT_32 u4SsidLen;	/*!< SSID length in bytes. Zero length is broadcast(any) SSID */
	UINT_8 aucSsid[PARAM_MAX_LEN_SSID];
} PARAM_SSID_T, *P_PARAM_SSID_T;

typedef struct _CHANNEL_INFO_T {
	UINT_8 ucBand;
	UINT_8 ucChannelNum;
} CHANNEL_INFO_T, *P_CHANNEL_INFO_T;

typedef struct _CMD_SCAN_REQ_V2_T {
	UINT_8 ucSeqNum;
	UINT_8 ucBssIndex;
	UINT_8 ucScanType;
	UINT_8 ucSSIDType;
	UINT_8 ucSSIDNum;
	UINT_8 ucNumProbeReq;
	UINT_8 ucReserved;
	UINT_8 ucStructVersion;
	PARAM_SSID_T arSSID[CFG_SCAN_SSID_MAX_NUM];
	UINT_16 u2ProbeDelayTime;
	UINT_16 u2ChannelMinDwellTime;
	UINT_16 u2ChannelDwellTime;
	UINT_16 u2TimeoutValue;
	UINT_8 aucBSSID[MAC_ADDR_LEN];
	UINT_8 ucChannelType;
	UINT_8 ucChannelListNum;
	CHANNEL_INFO_T arChannelList[MAXIMUM_OPERATION_CHANNEL_LIST];
	UINT_16 u2IELen;
	UINT_8 aucIE[MAX_IE_LENGTH];
} CMD_SCAN_REQ_V2, *P_CMD_SCAN_REQ_V2;

typedef struct _EVENT_SCAN_DONE_T {
	UINT_8 ucSeqNum;
	UINT_8 ucSparseChannelValid;
	CHANNEL_INFO_T rSparseChannel;
	/*scan done version #2 */
	UINT_8 ucCompleteChanCount;
	UINT_8 ucCurrentState;
	UINT_8 ucScanDoneVersion;
	/*scan done version #3 */
	UINT_8 ucReserved;
	UINT_32 u4ScanDurBcnCnt;
	UINT_8 fgIsPNOenabled;
	UINT_8 aucReserving[3];
} EVENT_SCAN_DONE, *P_EVENT_SCAN_DONE;

typedef struct _EVENT_SLEEPY_INFO_T {
	UINT_8 ucSleepyState;
	UINT_8 aucReserved[3];
} EVENT_SLEEPY_INFO_T, *P_EVENT_SLEEPY_INFO_T;

/* Prefix excerpt; test-only enum closing follows original SCAN_DONE. */
typedef enum _ENUM_EVENT_ID_T {
	EVENT_ID_NIC_CAPABILITY = 0x01,	/* 0x01 (Query) */
	EVENT_ID_LINK_QUALITY,	/* 0x02 (Query / Unsolicited) */
	EVENT_ID_STATISTICS,	/* 0x03 (Query) */
	EVENT_ID_MIC_ERR_INFO,	/* 0x04 (Unsolicited) */
	EVENT_ID_ACCESS_REG,	/* 0x05 (Query - CMD_ID_ACCESS_REG) */
	EVENT_ID_ACCESS_EEPROM,	/* 0x06 (Query - CMD_ID_ACCESS_EEPROM) */
	EVENT_ID_SLEEPY_INFO,	/* 0x07 (Unsolicited) */
	EVENT_ID_BT_OVER_WIFI,	/* 0x08 (Unsolicited) */
	EVENT_ID_TEST_STATUS,	/* 0x09 (Query - CMD_ID_TEST_CTRL) */
	EVENT_ID_RX_ADDBA,	/* 0x0a (Unsolicited) */
	EVENT_ID_RX_DELBA,	/* 0x0b (Unsolicited) */
	EVENT_ID_ACTIVATE_STA_REC,	/* 0x0c (Response) */
	EVENT_ID_SCAN_DONE,	/* 0x0d (Unsoiicited) */
	} A04_NATIVE_EVENT_PREFIX;

/* Original scan mode enum from include/mgmt/scan.h. */
typedef enum _ENUM_SCAN_TYPE_T {
	SCAN_TYPE_PASSIVE_SCAN = 0,
	SCAN_TYPE_ACTIVE_SCAN,
	SCAN_TYPE_NUM
} ENUM_SCAN_TYPE_T, *P_ENUM_SCAN_TYPE_T;
