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
/* Unmodified declaration excerpts; pinned source and ranges in provenance.json.
 * Test harness supplies UINT_8/16/32 and MAC_ADDR_LEN. See COPYING. */
#define CMD_PACKET_TYPE_ID  (0xA0)

typedef struct _WIFI_CMD_T {
	UINT_16 u2TxByteCount;	/* Max value is over 2048 */
	UINT_16 u2PQ_ID;	/* Must be 0x8000 (Port1, Queue 0) */
	UINT_8 ucCID;
	UINT_8 ucPktTypeID;	/* Must be 0x20 (CMD Packet) */
	UINT_8 ucSetQuery;
	UINT_8 ucSeqNum;

	UINT_8 aucBuffer[0];
} WIFI_CMD_T, *P_WIFI_CMD_T;

typedef struct _EVENT_NIC_CAPABILITY_T {
	UINT_16 u2ProductID;
	UINT_16 u2FwVersion;
	UINT_16 u2DriverVersion;
	UINT_8 ucHw5GBandDisabled;
	UINT_8 ucEepromUsed;
	UINT_8 aucMacAddr[6];
	UINT_8 ucEndianOfMacAddrNumber;
	UINT_8 ucReserved;

	UINT_8 ucRfVersion;
	UINT_8 ucPhyVersion;
	UINT_8 ucRfCalFail;
	UINT_8 ucBbCalFail;
	UINT_8 aucDateCode[16];
	UINT_32 u4FeatureFlag0;
	UINT_32 u4FeatureFlag1;
	UINT_32 u4CompileFlag0;
	UINT_32 u4CompileFlag1;
	UINT_8 aucFwVerExtend[4];
	UINT_8 aucReserved0[60];
} EVENT_NIC_CAPABILITY_T, *P_EVENT_NIC_CAPABILITY_T;

typedef struct _CMD_SET_BSS_RLM_PARAM_T {
	UINT_8 ucBssIndex;
	UINT_8 ucRfBand;
	UINT_8 ucPrimaryChannel;
	UINT_8 ucRfSco;
	UINT_8 ucErpProtectMode;
	UINT_8 ucHtProtectMode;
	UINT_8 ucGfOperationMode;
	UINT_8 ucTxRifsMode;
	UINT_16 u2HtOpInfo3;
	UINT_16 u2HtOpInfo2;
	UINT_8 ucHtOpInfo1;
	UINT_8 ucUseShortPreamble;
	UINT_8 ucUseShortSlotTime;
	UINT_8 ucVhtChannelWidth;
	UINT_8 ucVhtChannelFrequencyS1;
	UINT_8 ucVhtChannelFrequencyS2;
	UINT_16 u2VhtBasicMcsSet;
} CMD_SET_BSS_RLM_PARAM_T, *P_CMD_SET_BSS_RLM_PARAM_T;

typedef struct _CMD_SET_BSS_INFO {
	UINT_8 ucBssIndex;
	UINT_8 ucConnectionState;
	UINT_8 ucCurrentOPMode;
	UINT_8 ucSSIDLen;
	UINT_8 aucSSID[32];
	UINT_8 aucBSSID[6];
	UINT_8 ucIsQBSS;
	UINT_8 ucReserved1;
	UINT_16 u2OperationalRateSet;
	UINT_16 u2BSSBasicRateSet;
	UINT_8 ucStaRecIdxOfAP;
	UINT_16 u2HwDefaultFixedRateCode;
	UINT_8 ucNonHTBasicPhyType;	/* For Slot Time and CWmin */
	UINT_8 ucAuthMode;
	UINT_8 ucEncStatus;
	UINT_8 ucPhyTypeSet;
	UINT_8 ucWapiMode;
	UINT_8 ucIsApMode;
	UINT_8 ucBMCWlanIndex;
	UINT_8 ucHiddenSsidMode;
	UINT_8 ucDisconnectDetectTh;
	UINT_32 u4PrivateData;
	CMD_SET_BSS_RLM_PARAM_T rBssRlmParam;
} CMD_SET_BSS_INFO, *P_CMD_SET_BSS_INFO;

typedef struct _CMD_UPDATE_STA_RECORD_T {
	UINT_8 ucStaIndex;
	UINT_8 ucStaType;
	/* This field should assign at create and keep consistency for update usage */
	UINT_8 aucMacAddr[MAC_ADDR_LEN];

	UINT_16 u2AssocId;
	UINT_16 u2ListenInterval;
	UINT_8 ucBssIndex;	/* This field should assign at create and keep consistency for update usage */
	UINT_8 ucDesiredPhyTypeSet;
	UINT_16 u2DesiredNonHTRateSet;

	UINT_16 u2BSSBasicRateSet;
	UINT_8 ucIsQoS;
	UINT_8 ucIsUapsdSupported;
	UINT_8 ucStaState;
	UINT_8 ucMcsSet;
	UINT_8 ucSupMcs32;
	UINT_8 aucReserved1[1];

	UINT_8 aucRxMcsBitmask[10];
	UINT_16 u2RxHighestSupportedRate;
	UINT_32 u4TxRateInfo;

	UINT_16 u2HtCapInfo;
	UINT_16 u2HtExtendedCap;
	UINT_32 u4TxBeamformingCap;

	UINT_8 ucAmpduParam;
	UINT_8 ucAselCap;
	UINT_8 ucRCPI;
	UINT_8 ucNeedResp;
	UINT_8 ucUapsdAc;	/* b0~3: Trigger enabled, b4~7: Delivery enabled */
	UINT_8 ucUapsdSp;	/* 0: all, 1: max 2, 2: max 4, 3: max 6 */
	UINT_8 ucWlanIndex;	/* This field should assign at create and keep consistency for update usage */
	UINT_8 ucBMCWlanIndex;	/* This field should assign at create and keep consistency for update usage */

	UINT_32 u4VhtCapInfo;
	UINT_16 u2VhtRxMcsMap;
	UINT_16 u2VhtRxHighestSupportedDataRate;
	UINT_16 u2VhtTxMcsMap;
	UINT_16 u2VhtTxHighestSupportedDataRate;
	UINT_8 ucRtsPolicy;	/* 0: auto 1: Static BW 2: Dynamic BW 3: Legacy 7: WoRts */
	UINT_8 aucReserved2[1];

	UINT_8 ucTrafficDataType;	/* 0: auto 1: data 2: video 3: voice */
	UINT_8 ucTxGfMode;
	UINT_8 ucTxSgiMode;
	UINT_8 ucTxStbcMode;
	UINT_16 u2HwDefaultFixedRateCode;
	UINT_8 ucTxAmpdu;
	UINT_8 ucRxAmpdu;
	UINT_32 u4FixedPhyRate;	/* */
	UINT_16 u2MaxLinkSpeed;	/* unit is 0.5 Mbps */
	UINT_16 u2MinLinkSpeed;

	UINT_32 u4Flags;

	UINT_8 ucTxBaSize;
	UINT_8 ucRxBaSize;
	UINT_16 u2KeepAliveDuration; /* unit is 1s */
	UINT_8 ucKeepAliveOption; /* only bit0 is used now */
	UINT_8 aucReserved4[27];
} CMD_UPDATE_STA_RECORD_T, *P_CMD_UPDATE_STA_RECORD_T;

typedef struct _CMD_802_11_KEY {
	UINT_8 ucAddRemove;
	UINT_8 ucTxKey;
	UINT_8 ucKeyType;
	UINT_8 ucIsAuthenticator;
	UINT_8 aucPeerAddr[6];
	UINT_8 ucBssIdx;
	UINT_8 ucAlgorithmId;
	UINT_8 ucKeyId;
	UINT_8 ucKeyLen;
	UINT_8 ucWlanIndex;
	UINT_8 ucReverved;
	UINT_8 aucKeyMaterial[32];
	UINT_8 aucKeyRsc[16];
} CMD_802_11_KEY, *P_CMD_802_11_KEY;

typedef struct _CMD_BSS_ACTIVATE_CTRL {
	UINT_8 ucBssIndex;
	UINT_8 ucActive;
	UINT_8 ucNetworkType;
	UINT_8 ucOwnMacAddrIndex;
	UINT_8 aucBssMacAddr[6];
	UINT_8 ucBMCWlanIndex;
	UINT_8 ucReserved;
} CMD_BSS_ACTIVATE_CTRL, *P_CMD_BSS_ACTIVATE_CTRL;
