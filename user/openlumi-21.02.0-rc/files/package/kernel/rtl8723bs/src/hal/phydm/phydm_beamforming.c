/* SPDX-License-Identifier: GPL-2.0 */
/******************************************************************************
 *
 * Copyright(c) 2007 - 2017 Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 *****************************************************************************/

#include "mp_precomp.h"
#include "phydm_precomp.h"

#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	#if WPP_SOFTWARE_TRACE
		#include "phydm_beamforming.tmh"
	#endif
#endif

#if (BEAMFORMING_SUPPORT == 1)

struct _RT_BEAMFORM_STAINFO *
phydm_sta_info_init(
	struct PHY_DM_STRUCT		*p_dm,
	u16			sta_idx
)
{
	struct _RT_BEAMFORMING_INFO		*p_beam_info = &p_dm->beamforming_info;
	struct _RT_BEAMFORM_STAINFO		*p_entry = &(p_beam_info->beamform_sta_info);
	struct sta_info					*p_sta = p_dm->p_odm_sta_info[sta_idx];
	struct cmn_sta_info				*p_cmn_sta = p_dm->p_phydm_sta_info[sta_idx];
	struct _ADAPTER					*adapter = p_dm->adapter;
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	PMGNT_INFO					p_MgntInfo = &adapter->MgntInfo;
	PRT_HIGH_THROUGHPUT		p_ht_info = GET_HT_INFO(p_MgntInfo);
	PRT_VERY_HIGH_THROUGHPUT	p_vht_info = GET_VHT_INFO(p_MgntInfo);
	u1Byte						iotpeer = 0;

	iotpeer = p_MgntInfo->IOTPeer;
	odm_move_memory(p_dm, p_entry->my_mac_addr, adapter->CurrentAddress, 6);

	p_entry->ht_beamform_cap = p_ht_info->HtBeamformCap;
	p_entry->vht_beamform_cap = p_vht_info->VhtBeamformCap;

	/*IBSS, AP mode*/
	if (sta_idx != 0) {
		p_entry->aid = p_cmn_sta->aid;
		p_entry->ra = p_cmn_sta->mac_addr;
		p_entry->mac_id = p_cmn_sta->mac_id;
		p_entry->wireless_mode = p_sta->WirelessMode;
		p_entry->bw = p_cmn_sta->bw_mode;
		p_entry->cur_beamform = p_cmn_sta->bf_info.ht_beamform_cap;
	} else {/*client mode*/
		p_entry->aid = p_MgntInfo->mAId;
		p_entry->ra = p_MgntInfo->Bssid;
		p_entry->mac_id = p_MgntInfo->mMacId;
		p_entry->wireless_mode = p_MgntInfo->dot11CurrentWirelessMode;
		p_entry->bw = p_MgntInfo->dot11CurrentChannelBandWidth;
		p_entry->cur_beamform = p_ht_info->HtCurBeamform;
	}

	if ((p_entry->wireless_mode & WIRELESS_MODE_AC_5G) || (p_entry->wireless_mode & WIRELESS_MODE_AC_24G)) {
		if (sta_idx != 0)
			p_entry->cur_beamform_vht = p_cmn_sta->bf_info.vht_beamform_cap;
		else
			p_entry->cur_beamform_vht = p_vht_info->VhtCurBeamform;
	}

	PHYDM_DBG(p_dm, DBG_TXBF, ("p_sta->wireless_mode = 0x%x, staidx = %d\n", p_sta->WirelessMode, sta_idx));
#elif (DM_ODM_SUPPORT_TYPE == ODM_CE)

	if (!is_sta_active(p_cmn_sta)) {
		PHYDM_DBG(p_dm, DBG_TXBF, ("%s => sta_info(mac_id:%d) failed\n", __func__, sta_idx));
		rtw_warn_on(1);
		return p_entry;
	}

	odm_move_memory(p_dm, p_entry->my_mac_addr, adapter_mac_addr(p_sta->padapter), 6);
	#ifdef CONFIG_80211N_HT
	p_entry->ht_beamform_cap = p_cmn_sta->bf_info.ht_beamform_cap;
	#endif

	p_entry->aid = p_cmn_sta->aid;
	p_entry->ra = p_cmn_sta->mac_addr;
	p_entry->mac_id = p_cmn_sta->mac_id;
	p_entry->wireless_mode = p_sta->wireless_mode;
	p_entry->bw = p_cmn_sta->bw_mode;
	#ifdef CONFIG_80211N_HT
	p_entry->cur_beamform = p_cmn_sta->bf_info.ht_beamform_cap;
	#endif
#if	ODM_IC_11AC_SERIES_SUPPORT
	if ((p_entry->wireless_mode & WIRELESS_MODE_AC_5G) || (p_entry->wireless_mode & WIRELESS_MODE_AC_24G)) {
		p_entry->cur_beamform_vht = p_cmn_sta->bf_info.vht_beamform_cap;
		p_entry->vht_beamform_cap = p_cmn_sta->bf_info.vht_beamform_cap;
	}
#endif
	PHYDM_DBG(p_dm, DBG_TXBF, ("p_sta->wireless_mode = 0x%x, staidx = %d\n", p_sta->wireless_mode, sta_idx));
#endif
	PHYDM_DBG(p_dm, DBG_TXBF, ("p_entry->cur_beamform = 0x%x, p_entry->cur_beamform_vht = 0x%x\n", p_entry->cur_beamform, p_entry->cur_beamform_vht));
	return p_entry;

}
void phydm_sta_info_update(
	struct PHY_DM_STRUCT			*p_dm,
	u16				sta_idx,
	struct _RT_BEAMFORMEE_ENTRY	*p_beamform_entry
)
{
	struct cmn_sta_info *p_sta = p_dm->p_phydm_sta_info[sta_idx];

	if (!is_sta_active(p_sta))
		return;

	p_sta->bf_info.p_aid = p_beamform_entry->p_aid;
	p_sta->bf_info.g_id = p_beamform_entry->g_id;
}

struct _RT_BEAMFORMEE_ENTRY *
phydm_beamforming_get_bfee_entry_by_addr(
	void		*p_dm_void,
	u8		*RA,
	u8		*idx
)
{
	struct PHY_DM_STRUCT					*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u8	i = 0;
	struct _RT_BEAMFORMING_INFO *p_beam_info = &p_dm->beamforming_info;

	for (i = 0; i < BEAMFORMEE_ENTRY_NUM; i++) {
		if (p_beam_info->beamformee_entry[i].is_used && (eq_mac_addr(RA, p_beam_info->beamformee_entry[i].mac_addr))) {
			*idx = i;
			return &(p_beam_info->beamformee_entry[i]);
		}
	}

	return NULL;
}

struct _RT_BEAMFORMER_ENTRY *
phydm_beamforming_get_bfer_entry_by_addr(
	void	*p_dm_void,
	u8	*TA,
	u8	*idx
)
{
	struct PHY_DM_STRUCT	*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u8		i = 0;
	struct _RT_BEAMFORMING_INFO	*p_beam_info = &p_dm->beamforming_info;

	for (i = 0; i < BEAMFORMER_ENTRY_NUM; i++) {
		if (p_beam_info->beamformer_entry[i].is_used && (eq_mac_addr(TA, p_beam_info->beamformer_entry[i].mac_addr))) {
			*idx = i;
			return &(p_beam_info->beamformer_entry[i]);
		}
	}

	return NULL;
}


struct _RT_BEAMFORMEE_ENTRY *
phydm_beamforming_get_entry_by_mac_id(
	void		*p_dm_void,
	u8		mac_id,
	u8		*idx
)
{
	struct PHY_DM_STRUCT					*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u8	i = 0;
	struct _RT_BEAMFORMING_INFO *p_beam_info = &p_dm->beamforming_info;

	for (i = 0; i < BEAMFORMEE_ENTRY_NUM; i++) {
		if (p_beam_info->beamformee_entry[i].is_used && (mac_id == p_beam_info->beamformee_entry[i].mac_id)) {
			*idx = i;
			return &(p_beam_info->beamformee_entry[i]);
		}
	}

	return NULL;
}


enum beamforming_cap
phydm_beamforming_get_entry_beam_cap_by_mac_id(
	void		*p_dm_void,
	u8		mac_id
)
{
	struct PHY_DM_STRUCT					*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u8	i = 0;
	struct _RT_BEAMFORMING_INFO	*p_beam_info = &p_dm->beamforming_info;
	enum beamforming_cap			beamform_entry_cap = BEAMFORMING_CAP_NONE;

	for (i = 0; i < BEAMFORMEE_ENTRY_NUM; i++) {
		if (p_beam_info->beamformee_entry[i].is_used && (mac_id == p_beam_info->beamformee_entry[i].mac_id)) {
			beamform_entry_cap =  p_beam_info->beamformee_entry[i].beamform_entry_cap;
			i = BEAMFORMEE_ENTRY_NUM;
		}
	}

	return beamform_entry_cap;
}


struct _RT_BEAMFORMEE_ENTRY *
phydm_beamforming_get_free_bfee_entry(
	void		*p_dm_void,
	u8		*idx
)
{
	struct PHY_DM_STRUCT					*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u8	i = 0;
	struct _RT_BEAMFORMING_INFO *p_beam_info = &p_dm->beamforming_info;

	for (i = 0; i < BEAMFORMEE_ENTRY_NUM; i++) {
		if (p_beam_info->beamformee_entry[i].is_used == false) {
			*idx = i;
			return &(p_beam_info->beamformee_entry[i]);
		}
	}
	return NULL;
}

struct _RT_BEAMFORMER_ENTRY *
phydm_beamforming_get_free_bfer_entry(
	void		*p_dm_void,
	u8		*idx
)
{
	struct PHY_DM_STRUCT					*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u8	i = 0;
	struct _RT_BEAMFORMING_INFO *p_beam_info = &p_dm->beamforming_info;

	PHYDM_DBG(p_dm, DBG_TXBF, ("%s ===>\n", __func__));

	for (i = 0; i < BEAMFORMER_ENTRY_NUM; i++) {
		if (p_beam_info->beamformer_entry[i].is_used == false) {
			*idx = i;
			return &(p_beam_info->beamformer_entry[i]);
		}
	}
	return NULL;
}

/*
 * Description: Get the first entry index of MU Beamformee.
 *
 * Return value: index of the first MU sta.
 *
 * 2015.05.25. Created by tynli.
 *
 */
u8
phydm_beamforming_get_first_mu_bfee_entry_idx(
	void		*p_dm_void
)
{
	struct PHY_DM_STRUCT				*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u8					idx = 0xFF;
	struct _RT_BEAMFORMING_INFO	*p_beam_info = &p_dm->beamforming_info;
	boolean					is_found = false;

	for (idx = 0; idx < BEAMFORMEE_ENTRY_NUM; idx++) {
		if (p_beam_info->beamformee_entry[idx].is_used && p_beam_info->beamformee_entry[idx].is_mu_sta) {
			PHYDM_DBG(p_dm, DBG_TXBF, ("[%s] idx=%d!\n", __func__, idx));
			is_found = true;
			break;
		}
	}

	if (!is_found)
		idx = 0xFF;

	return idx;
}


/*Add SU BFee and MU BFee*/
struct _RT_BEAMFORMEE_ENTRY *
beamforming_add_bfee_entry(
	void					*p_dm_void,
	struct _RT_BEAMFORM_STAINFO	*p_sta,
	enum beamforming_cap		beamform_cap,
	u8					num_of_sounding_dim,
	u8					comp_steering_num_of_bfer,
	u8					*idx
)
{
	struct PHY_DM_STRUCT				*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _RT_BEAMFORMEE_ENTRY	*p_entry = phydm_beamforming_get_free_bfee_entry(p_dm, idx);

	PHYDM_DBG(p_dm, DBG_TXBF, ("%s Start!\n", __func__));

	if (p_entry != NULL) {
		p_entry->is_used = true;
		p_entry->aid = p_sta->aid;
		p_entry->mac_id = p_sta->mac_id;
		p_entry->sound_bw = p_sta->bw;
		odm_move_memory(p_dm, p_entry->my_mac_addr, p_sta->my_mac_addr, 6);

		if (phydm_acting_determine(p_dm, phydm_acting_as_ap)) {
			/*BSSID[44:47] xor BSSID[40:43]*/
			u16 bssid = ((p_sta->my_mac_addr[5] & 0xf0) >> 4) ^ (p_sta->my_mac_addr[5] & 0xf);
			/*(dec(A) + dec(B)*32) mod 512*/
			p_entry->p_aid = (p_sta->aid + bssid * 32) & 0x1ff;
			p_entry->g_id = 63;
			PHYDM_DBG(p_dm, DBG_TXBF, ("%s: BFee P_AID addressed to STA=%d\n", __func__, p_entry->p_aid));
		} else if (phydm_acting_determine(p_dm, phydm_acting_as_ibss)) {
			/*ad hoc mode*/
			p_entry->p_aid = 0;
			p_entry->g_id = 63;
			PHYDM_DBG(p_dm, DBG_TXBF, ("%s: BFee P_AID as IBSS=%d\n", __func__, p_entry->p_aid));
		} else {
			/*client mode*/
			p_entry->p_aid =  p_sta->ra[5];
			/*BSSID[39:47]*/
			p_entry->p_aid = (p_entry->p_aid << 1) | (p_sta->ra[4] >> 7);
			p_entry->g_id = 0;
			PHYDM_DBG(p_dm, DBG_TXBF, ("%s: BFee P_AID addressed to AP=0x%X\n", __func__, p_entry->p_aid));
		}
		cp_mac_addr(p_entry->mac_addr, p_sta->ra);
		p_entry->is_txbf = false;
		p_entry->is_sound = false;
		p_entry->sound_period = 400;
		p_entry->beamform_entry_cap = beamform_cap;
		p_entry->beamform_entry_state = BEAMFORMING_ENTRY_STATE_UNINITIALIZE;

		/*		p_entry->log_seq = 0xff;				Move to beamforming_add_bfer_entry*/
		/*		p_entry->log_retry_cnt = 0;			Move to beamforming_add_bfer_entry*/
		/*		p_entry->LogSuccessCnt = 0;		Move to beamforming_add_bfer_entry*/

		p_entry->log_status_fail_cnt = 0;

		p_entry->num_of_sounding_dim = num_of_sounding_dim;
		p_entry->comp_steering_num_of_bfer = comp_steering_num_of_bfer;

		if (beamform_cap & BEAMFORMER_CAP_VHT_MU) {
			p_dm->beamforming_info.beamformee_mu_cnt += 1;
			p_entry->is_mu_sta = true;
			p_dm->beamforming_info.first_mu_bfee_index = phydm_beamforming_get_first_mu_bfee_entry_idx(p_dm);
		} else if (beamform_cap & (BEAMFORMER_CAP_VHT_SU | BEAMFORMER_CAP_HT_EXPLICIT)) {
			p_dm->beamforming_info.beamformee_su_cnt += 1;
			p_entry->is_mu_sta = false;
		}

		return p_entry;
	} else
		return NULL;
}

/*Add SU BFee and MU BFer*/
struct _RT_BEAMFORMER_ENTRY *
beamforming_add_bfer_entry(
	void					*p_dm_void,
	struct _RT_BEAMFORM_STAINFO	*p_sta,
	enum beamforming_cap		beamform_cap,
	u8					num_of_sounding_dim,
	u8					*idx
)
{
	struct PHY_DM_STRUCT				*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _RT_BEAMFORMER_ENTRY	*p_entry = phydm_beamforming_get_free_bfer_entry(p_dm, idx);

	PHYDM_DBG(p_dm, DBG_TXBF, ("%s Start!\n", __func__));

	if (p_entry != NULL) {
		p_entry->is_used = true;
		odm_move_memory(p_dm, p_entry->my_mac_addr, p_sta->my_mac_addr, 6);
		if (phydm_acting_determine(p_dm, phydm_acting_as_ap)) {
			/*BSSID[44:47] xor BSSID[40:43]*/
			u16 bssid = ((p_sta->my_mac_addr[5] & 0xf0) >> 4) ^ (p_sta->my_mac_addr[5] & 0xf);

			p_entry->p_aid = (p_sta->aid + bssid * 32) & 0x1ff;
			p_entry->g_id = 63;
			/*(dec(A) + dec(B)*32) mod 512*/
		} else if (phydm_acting_determine(p_dm, phydm_acting_as_ibss)) {
			p_entry->p_aid = 0;
			p_entry->g_id = 63;
		} else {
			p_entry->p_aid =  p_sta->ra[5];
			/*BSSID[39:47]*/
			p_entry->p_aid = (p_entry->p_aid << 1) | (p_sta->ra[4] >> 7);
			p_entry->g_id = 0;
			PHYDM_DBG(p_dm, DBG_TXBF, ("%s: P_AID addressed to AP=0x%X\n", __func__, p_entry->p_aid));
		}

		cp_mac_addr(p_entry->mac_addr, p_sta->ra);
		p_entry->beamform_entry_cap = beamform_cap;

		p_entry->pre_log_seq = 0;	/*Modified by Jeffery @2015-04-13*/
		p_entry->log_seq = 0;		/*Modified by Jeffery @2014-10-29*/
		p_entry->log_retry_cnt = 0;	/*Modified by Jeffery @2014-10-29*/
		p_entry->log_success = 0;	/*log_success is NOT needed to be accumulated, so  LogSuccessCnt->log_success, 2015-04-13, Jeffery*/
		p_entry->clock_reset_times = 0;	/*Modified by Jeffery @2015-04-13*/

		p_entry->num_of_sounding_dim = num_of_sounding_dim;

		if (beamform_cap & BEAMFORMEE_CAP_VHT_MU) {
			p_dm->beamforming_info.beamformer_mu_cnt += 1;
			p_entry->is_mu_ap = true;
			p_entry->aid = p_sta->aid;
		} else if (beamform_cap & (BEAMFORMEE_CAP_VHT_SU | BEAMFORMEE_CAP_HT_EXPLICIT)) {
			p_dm->beamforming_info.beamformer_su_cnt += 1;
			p_entry->is_mu_ap = false;
		}

		return p_entry;
	} else
		return NULL;
}

#if 0
boolean
beamforming_remove_entry(
	struct _ADAPTER			*adapter,
	u8		*RA,
	u8		*idx
)
{
	HAL_DATA_TYPE			*p_hal_data = GET_HAL_DATA(adapter);
	struct PHY_DM_STRUCT				*p_dm = &p_hal_data->DM_OutSrc;

	struct _RT_BEAMFORMER_ENTRY	*p_bfer_entry = phydm_beamforming_get_bfer_entry_by_addr(p_dm, RA, idx);
	struct _RT_BEAMFORMEE_ENTRY	*p_entry = phydm_beamforming_get_bfee_entry_by_addr(p_dm, RA, idx);
	boolean ret = false;

	RT_DISP(FBEAM, FBEAM_FUN, ("[Beamforming]@%s Start!\n", __func__));
	RT_DISP(FBEAM, FBEAM_FUN, ("[Beamforming]@%s, p_bfer_entry=0x%x\n", __func__, p_bfer_entry));
	RT_DISP(FBEAM, FBEAM_FUN, ("[Beamforming]@%s, p_entry=0x%x\n", __func__, p_entry));

	if (p_entry != NULL) {
		p_entry->is_used = false;
		p_entry->beamform_entry_cap = BEAMFORMING_CAP_NONE;
		/*p_entry->beamform_entry_state = BEAMFORMING_ENTRY_STATE_UNINITIALIZE;*/
		p_entry->is_beamforming_in_progress = false;
		ret = true;
	}
	if (p_bfer_entry != NULL) {
		p_bfer_entry->is_used = false;
		p_bfer_entry->beamform_entry_cap = BEAMFORMING_CAP_NONE;
		ret = true;
	}
	return ret;

}
#endif

/* Used for beamforming_start_v1 */
void
phydm_beamforming_ndpa_rate(
	void		*p_dm_void,
	enum channel_width	BW,
	u8			rate
)
{
	u16			ndpa_rate = rate;
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;

	PHYDM_DBG(p_dm, DBG_TXBF, ("%s Start!\n", __func__));

	if (ndpa_rate == 0) {
		if (p_dm->rssi_min > 30) /* link RSSI > 30% */
			ndpa_rate = ODM_RATE24M;
		else
			ndpa_rate = ODM_RATE6M;
	}

	if (ndpa_rate < ODM_RATEMCS0)
		BW = (enum channel_width)CHANNEL_WIDTH_20;

	ndpa_rate = (ndpa_rate << 8) | BW;
	hal_com_txbf_set(p_dm, TXBF_SET_SOUNDING_RATE, (u8 *)&ndpa_rate);

}


/* Used for beamforming_start_sw and  beamforming_start_fw */
void
phydm_beamforming_dym_ndpa_rate(
	void		*p_dm_void
)
{
	u16			ndpa_rate = ODM_RATE6M, BW;
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;

	ndpa_rate = ODM_RATE6M;
	BW = CHANNEL_WIDTH_20;

	ndpa_rate = ndpa_rate << 8 | BW;
	hal_com_txbf_set(p_dm, TXBF_SET_SOUNDING_RATE, (u8 *)&ndpa_rate);
	PHYDM_DBG(p_dm, DBG_TXBF, ("%s End, NDPA rate = 0x%X\n", __func__, ndpa_rate));
}

/*
*	SW Sounding : SW Timer unit 1ms
*				 HW Timer unit (1/32000) s  32k is clock.
*	FW Sounding : FW Timer unit 10ms
*/
void
beamforming_dym_period(
	void		*p_dm_void,
	u8          status
)
{
	u8					idx;
	boolean					is_change_period = false;
	u16					sound_period_sw, sound_period_fw;
	struct PHY_DM_STRUCT				*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;

	struct _RT_BEAMFORMEE_ENTRY	*p_beamform_entry;
	struct _RT_BEAMFORMING_INFO	*p_beam_info = &(p_dm->beamforming_info);
	struct _RT_SOUNDING_INFO		*p_sound_info = &(p_beam_info->sounding_info);

	struct _RT_BEAMFORMEE_ENTRY	*p_entry = &(p_beam_info->beamformee_entry[p_beam_info->beamformee_cur_idx]);

	PHYDM_DBG(p_dm, DBG_TXBF, ("[%s] Start!\n", __func__));

	/* 3 TODO  per-client throughput caculation. */

	if ((*(p_dm->p_current_tx_tp) + *(p_dm->p_current_rx_tp) > 2) && ((p_entry->log_status_fail_cnt <= 20) || status)) {
		sound_period_sw = 40;	/* 40ms */
		sound_period_fw = 40;	/* From  H2C cmd, unit = 10ms */
	} else {
		sound_period_sw = 4000;/* 4s */
		sound_period_fw = 400;
	}
	PHYDM_DBG(p_dm, DBG_TXBF, ("[%s]sound_period_sw=%d, sound_period_fw=%d\n",	__func__, sound_period_sw, sound_period_fw));

	for (idx = 0; idx < BEAMFORMEE_ENTRY_NUM; idx++) {
		p_beamform_entry = p_beam_info->beamformee_entry + idx;

		if (p_beamform_entry->default_csi_cnt > 20) {
			/*Modified by David*/
			sound_period_sw = 4000;
			sound_period_fw = 400;
		}

		PHYDM_DBG(p_dm, DBG_TXBF, ("[%s] period = %d\n", __func__, sound_period_sw));
		if (p_beamform_entry->beamform_entry_cap & (BEAMFORMER_CAP_HT_EXPLICIT | BEAMFORMER_CAP_VHT_SU)) {
			if (p_sound_info->sound_mode == SOUNDING_FW_VHT_TIMER || p_sound_info->sound_mode == SOUNDING_FW_HT_TIMER) {
				if (p_beamform_entry->sound_period != sound_period_fw) {
					p_beamform_entry->sound_period = sound_period_fw;
					is_change_period = true;		/*Only FW sounding need to send H2C packet to change sound period. */
				}
			} else if (p_beamform_entry->sound_period != sound_period_sw)
				p_beamform_entry->sound_period = sound_period_sw;
		}
	}

	if (is_change_period)
		hal_com_txbf_set(p_dm, TXBF_SET_SOUNDING_FW_NDPA, (u8 *)&idx);
}




boolean
beamforming_send_ht_ndpa_packet(
	void			*p_dm_void,
	u8			*RA,
	enum channel_width	BW,
	u8			q_idx
)
{
	boolean		ret = true;
	struct PHY_DM_STRUCT	*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;

	if (q_idx == BEACON_QUEUE)
		ret = send_fw_ht_ndpa_packet(p_dm, RA, BW);
	else
		ret = send_sw_ht_ndpa_packet(p_dm, RA, BW);

	return ret;
}



boolean
beamforming_send_vht_ndpa_packet(
	void			*p_dm_void,
	u8			*RA,
	u16			AID,
	enum channel_width	BW,
	u8			q_idx
)
{
	struct PHY_DM_STRUCT	*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _RT_BEAMFORMING_INFO	*p_beam_info = &(p_dm->beamforming_info);
	boolean					ret = true;

	hal_com_txbf_set(p_dm, TXBF_SET_GET_TX_RATE, NULL);

	if ((p_beam_info->tx_bf_data_rate >= ODM_RATEVHTSS3MCS7) && (p_beam_info->tx_bf_data_rate <= ODM_RATEVHTSS3MCS9) && (p_beam_info->snding3ss == false))
		PHYDM_DBG(p_dm, DBG_TXBF, ("@%s: 3SS VHT 789 don't sounding\n", __func__));

	else  {
		if (q_idx == BEACON_QUEUE) /* Send to reserved page => FW NDPA */
			ret = send_fw_vht_ndpa_packet(p_dm, RA, AID, BW);
		else {
#ifdef SUPPORT_MU_BF
#if (SUPPORT_MU_BF == 1)
			p_beam_info->is_mu_sounding = true;
			ret = send_sw_vht_mu_ndpa_packet(p_dm, BW);
#else
			p_beam_info->is_mu_sounding = false;
			ret = send_sw_vht_ndpa_packet(p_dm, RA, AID, BW);
#endif
#else
			p_beam_info->is_mu_sounding = false;
			ret = send_sw_vht_ndpa_packet(p_dm, RA, AID, BW);
#endif
		}
	}
	return ret;
}


enum beamforming_notify_state
phydm_beamfomring_is_sounding(
	void				*p_dm_void,
	struct _RT_BEAMFORMING_INFO	*p_beam_info,
	u8					*idx
)
{
	enum beamforming_notify_state	is_sounding = BEAMFORMING_NOTIFY_NONE;
	struct _RT_BEAMFORMING_OID_INFO	beam_oid_info = p_beam_info->beamforming_oid_info;
	struct PHY_DM_STRUCT					*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;

	PHYDM_DBG(p_dm, DBG_TXBF, ("%s Start!\n", __func__));

	/*if(( Beamforming_GetBeamCap(p_beam_info) & BEAMFORMER_CAP) == 0)*/
	/*is_sounding = BEAMFORMING_NOTIFY_RESET;*/
	if (beam_oid_info.sound_oid_mode == sounding_stop_all_timer)
		is_sounding = BEAMFORMING_NOTIFY_RESET;
	else {
		u8 i;

		for (i = 0 ; i < BEAMFORMEE_ENTRY_NUM ; i++) {
			PHYDM_DBG(p_dm, DBG_TXBF, ("@%s: BFee Entry %d is_used=%d, is_sound=%d\n", __func__, i, p_beam_info->beamformee_entry[i].is_used, p_beam_info->beamformee_entry[i].is_sound));
			if (p_beam_info->beamformee_entry[i].is_used && (!p_beam_info->beamformee_entry[i].is_sound)) {
				PHYDM_DBG(p_dm, DBG_TXBF, ("%s: Add BFee entry %d\n", __func__, i));
				*idx = i;
				if (p_beam_info->beamformee_entry[i].is_mu_sta)
					is_sounding = BEAMFORMEE_NOTIFY_ADD_MU;
				else
					is_sounding = BEAMFORMEE_NOTIFY_ADD_SU;
			}

			if ((!p_beam_info->beamformee_entry[i].is_used) && p_beam_info->beamformee_entry[i].is_sound) {
				PHYDM_DBG(p_dm, DBG_TXBF, ("%s: Delete BFee entry %d\n", __func__, i));
				*idx = i;
				if (p_beam_info->beamformee_entry[i].is_mu_sta)
					is_sounding = BEAMFORMEE_NOTIFY_DELETE_MU;
				else
					is_sounding = BEAMFORMEE_NOTIFY_DELETE_SU;
			}
		}
	}

	PHYDM_DBG(p_dm, DBG_TXBF, ("%s End, is_sounding = %d\n", __func__, is_sounding));
	return is_sounding;
}


/* This function is unused */
u8
phydm_beamforming_sounding_idx(
	void				*p_dm_void,
	struct _RT_BEAMFORMING_INFO		*p_beam_info
)
{
	u8					idx = 0;
	struct _RT_BEAMFORMING_OID_INFO	beam_oid_info = p_beam_info->beamforming_oid_info;
	struct PHY_DM_STRUCT					*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;

	PHYDM_DBG(p_dm, DBG_TXBF, ("%s Start!\n", __func__));

	if (beam_oid_info.sound_oid_mode == SOUNDING_SW_HT_TIMER || beam_oid_info.sound_oid_mode == SOUNDING_SW_VHT_TIMER ||
	    beam_oid_info.sound_oid_mode == SOUNDING_HW_HT_TIMER || beam_oid_info.sound_oid_mode == SOUNDING_HW_VHT_TIMER)
		idx = beam_oid_info.sound_oid_idx;
	else {
		u8	i;
		for (i = 0; i < BEAMFORMEE_ENTRY_NUM; i++) {
			if (p_beam_info->beamformee_entry[i].is_used && (false == p_beam_info->beamformee_entry[i].is_sound)) {
				idx = i;
				break;
			}
		}
	}

	return idx;
}


enum sounding_mode
phydm_beamforming_sounding_mode(
	void				*p_dm_void,
	struct _RT_BEAMFORMING_INFO	*p_beam_info,
	u8					idx
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u8			support_interface = p_dm->support_interface;

	struct _RT_BEAMFORMEE_ENTRY		beam_entry = p_beam_info->beamformee_entry[idx];
	struct _RT_BEAMFORMING_OID_INFO	beam_oid_info = p_beam_info->beamforming_oid_info;
	enum sounding_mode				mode = beam_oid_info.sound_oid_mode;

	if (beam_oid_info.sound_oid_mode == SOUNDING_SW_VHT_TIMER || beam_oid_info.sound_oid_mode == SOUNDING_HW_VHT_TIMER) {
		if (beam_entry.beamform_entry_cap & BEAMFORMER_CAP_VHT_SU)
			mode = beam_oid_info.sound_oid_mode;
		else
			mode = sounding_stop_all_timer;
	} else if (beam_oid_info.sound_oid_mode == SOUNDING_SW_HT_TIMER || beam_oid_info.sound_oid_mode == SOUNDING_HW_HT_TIMER) {
		if (beam_entry.beamform_entry_cap & BEAMFORMER_CAP_HT_EXPLICIT)
			mode = beam_oid_info.sound_oid_mode;
		else
			mode = sounding_stop_all_timer;
	} else if (beam_entry.beamform_entry_cap & BEAMFORMER_CAP_VHT_SU) {
		if ((support_interface == ODM_ITRF_USB) && !(p_dm->support_ic_type & (ODM_RTL8814A | ODM_RTL8822B)))
			mode = SOUNDING_FW_VHT_TIMER;
		else
			mode = SOUNDING_SW_VHT_TIMER;
	} else if (beam_entry.beamform_entry_cap & BEAMFORMER_CAP_HT_EXPLICIT) {
		if ((support_interface == ODM_ITRF_USB) && !(p_dm->support_ic_type & (ODM_RTL8814A | ODM_RTL8822B)))
			mode = SOUNDING_FW_HT_TIMER;
		else
			mode = SOUNDING_SW_HT_TIMER;
	} else
		mode = sounding_stop_all_timer;

	PHYDM_DBG(p_dm, DBG_TXBF, ("[%s] support_interface=%d, mode=%d\n", __func__, support_interface, mode));

	return mode;
}


u16
phydm_beamforming_sounding_time(
	void				*p_dm_void,
	struct _RT_BEAMFORMING_INFO	*p_beam_info,
	enum sounding_mode			mode,
	u8					idx
)
{
	u16						sounding_time = 0xffff;
	struct _RT_BEAMFORMEE_ENTRY		beam_entry = p_beam_info->beamformee_entry[idx];
	struct _RT_BEAMFORMING_OID_INFO	beam_oid_info = p_beam_info->beamforming_oid_info;
	struct PHY_DM_STRUCT					*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;

	PHYDM_DBG(p_dm, DBG_TXBF, ("%s Start!\n", __func__));

	if (mode == SOUNDING_HW_HT_TIMER || mode == SOUNDING_HW_VHT_TIMER)
		sounding_time = beam_oid_info.sound_oid_period * 32;
	else if (mode == SOUNDING_SW_HT_TIMER || mode == SOUNDING_SW_VHT_TIMER)
		/*Modified by David*/
		sounding_time = beam_entry.sound_period;	/*beam_oid_info.sound_oid_period;*/
	else
		sounding_time = beam_entry.sound_period;

	return sounding_time;
}


enum channel_width
phydm_beamforming_sounding_bw(
	void				*p_dm_void,
	struct _RT_BEAMFORMING_INFO	*p_beam_info,
	enum sounding_mode			mode,
	u8					idx
)
{
	enum channel_width				sounding_bw = CHANNEL_WIDTH_20;
	struct _RT_BEAMFORMEE_ENTRY		beam_entry = p_beam_info->beamformee_entry[idx];
	struct _RT_BEAMFORMING_OID_INFO	beam_oid_info = p_beam_info->beamforming_oid_info;
	struct PHY_DM_STRUCT					*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;

	if (mode == SOUNDING_HW_HT_TIMER || mode == SOUNDING_HW_VHT_TIMER)
		sounding_bw = beam_oid_info.sound_oid_bw;
	else if (mode == SOUNDING_SW_HT_TIMER || mode == SOUNDING_SW_VHT_TIMER)
		/*Modified by David*/
		sounding_bw = beam_entry.sound_bw;		/*beam_oid_info.sound_oid_bw;*/
	else
		sounding_bw = beam_entry.sound_bw;

	PHYDM_DBG(p_dm, DBG_TXBF, ("%s, sounding_bw=0x%X\n", __func__, sounding_bw));

	return sounding_bw;
}


boolean
phydm_beamforming_select_beam_entry(
	void				*p_dm_void,
	struct _RT_BEAMFORMING_INFO	*p_beam_info
)
{
	struct _RT_SOUNDING_INFO		*p_sound_info = &(p_beam_info->sounding_info);
	struct PHY_DM_STRUCT				*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;

	/*p_entry.is_sound is different between first and latter NDPA, and should not be used as BFee entry selection*/
	/*BTW, latter modification should sync to the selection mechanism of AP/ADSL instead of the fixed sound_idx.*/
	p_sound_info->sound_idx = phydm_beamforming_sounding_idx(p_dm, p_beam_info);
	/*p_sound_info->sound_idx = 0;*/

	if (p_sound_info->sound_idx < BEAMFORMEE_ENTRY_NUM)
		p_sound_info->sound_mode = phydm_beamforming_sounding_mode(p_dm, p_beam_info, p_sound_info->sound_idx);
	else
		p_sound_info->sound_mode = sounding_stop_all_timer;

	if (sounding_stop_all_timer == p_sound_info->sound_mode) {
		PHYDM_DBG(p_dm, DBG_TXBF, ("[%s] Return because of sounding_stop_all_timer\n", __func__));
		return false;
	} else {
		p_sound_info->sound_bw = phydm_beamforming_sounding_bw(p_dm, p_beam_info, p_sound_info->sound_mode, p_sound_info->sound_idx);
		p_sound_info->sound_period = phydm_beamforming_sounding_time(p_dm, p_beam_info, p_sound_info->sound_mode, p_sound_info->sound_idx);
		return true;
	}
}

/*SU BFee Entry Only*/
boolean
phydm_beamforming_start_period(
	void				*p_dm_void
)
{
	struct PHY_DM_STRUCT					*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	boolean						ret = true;
	struct _RT_BEAMFORMING_INFO		*p_beam_info = &p_dm->beamforming_info;
	struct _RT_SOUNDING_INFO			*p_sound_info = &(p_beam_info->sounding_info);

	phydm_beamforming_dym_ndpa_rate(p_dm);

	phydm_beamforming_select_beam_entry(p_dm, p_beam_info);		/* Modified */

	if (p_sound_info->sound_mode == SOUNDING_SW_VHT_TIMER || p_sound_info->sound_mode == SOUNDING_SW_HT_TIMER)
		odm_set_timer(p_dm, &p_beam_info->beamforming_timer, p_sound_info->sound_period);
	else if (p_sound_info->sound_mode == SOUNDING_HW_VHT_TIMER || p_sound_info->sound_mode == SOUNDING_HW_HT_TIMER ||
		p_sound_info->sound_mode == SOUNDING_AUTO_VHT_TIMER || p_sound_info->sound_mode == SOUNDING_AUTO_HT_TIMER) {
		HAL_HW_TIMER_TYPE timer_type = HAL_TIMER_TXBF;
		u32	val = (p_sound_info->sound_period | (timer_type << 16));

		/* HW timer stop: All IC has the same setting */
		phydm_set_hw_reg_handler_interface(p_dm, HW_VAR_HW_REG_TIMER_STOP, (u8 *)(&timer_type));
		/* odm_write_1byte(p_dm, 0x15F, 0); */
		/* HW timer init: All IC has the same setting, but 92E & 8812A only write 2 bytes */
		phydm_set_hw_reg_handler_interface(p_dm, HW_VAR_HW_REG_TIMER_INIT, (u8 *)(&val));
		/* odm_write_1byte(p_dm, 0x164, 1); */
		/* odm_write_4byte(p_dm, 0x15C, val); */
		/* HW timer start: All IC has the same setting */
		phydm_set_hw_reg_handler_interface(p_dm, HW_VAR_HW_REG_TIMER_START, (u8 *)(&timer_type));
		/* odm_write_1byte(p_dm, 0x15F, 0x5); */
	} else if (p_sound_info->sound_mode == SOUNDING_FW_VHT_TIMER || p_sound_info->sound_mode == SOUNDING_FW_HT_TIMER)
		ret = beamforming_start_fw(p_dm, p_sound_info->sound_idx);
	else
		ret = false;

	PHYDM_DBG(p_dm, DBG_TXBF, ("[%s] sound_idx=%d, sound_mode=%d, sound_bw=%d, sound_period=%d\n", __func__,
		p_sound_info->sound_idx, p_sound_info->sound_mode, p_sound_info->sound_bw, p_sound_info->sound_period));

	return ret;
}

/* Used after beamforming_leave, and will clear the setting of the "already deleted" entry
 *SU BFee Entry Only*/
void
phydm_beamforming_end_period_sw(
	void				*p_dm_void
)
{
	struct PHY_DM_STRUCT					*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	/*struct _ADAPTER					*adapter = p_dm->adapter;*/
	struct _RT_BEAMFORMING_INFO		*p_beam_info = &p_dm->beamforming_info;
	struct _RT_SOUNDING_INFO			*p_sound_info = &(p_beam_info->sounding_info);

	HAL_HW_TIMER_TYPE timer_type = HAL_TIMER_TXBF;

	PHYDM_DBG(p_dm, DBG_TXBF, ("%s Start!\n", __func__));

	if (p_sound_info->sound_mode == SOUNDING_SW_VHT_TIMER || p_sound_info->sound_mode == SOUNDING_SW_HT_TIMER)
		odm_cancel_timer(p_dm, &p_beam_info->beamforming_timer);
	else if (p_sound_info->sound_mode == SOUNDING_HW_VHT_TIMER || p_sound_info->sound_mode == SOUNDING_HW_HT_TIMER ||
		p_sound_info->sound_mode == SOUNDING_AUTO_VHT_TIMER || p_sound_info->sound_mode == SOUNDING_AUTO_HT_TIMER)
		/*HW timer stop: All IC has the same setting*/
		phydm_set_hw_reg_handler_interface(p_dm, HW_VAR_HW_REG_TIMER_STOP, (u8 *)(&timer_type));
	/*odm_write_1byte(p_dm, 0x15F, 0);*/
}

void
phydm_beamforming_end_period_fw(
	void				*p_dm_void
)
{
	struct PHY_DM_STRUCT			*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u8				idx = 0;

	hal_com_txbf_set(p_dm, TXBF_SET_SOUNDING_FW_NDPA, (u8 *)&idx);
	PHYDM_DBG(p_dm, DBG_TXBF, ("[%s]\n", __func__));
}


/*SU BFee Entry Only*/
void
phydm_beamforming_clear_entry_sw(
	void			*p_dm_void,
	boolean				is_delete,
	u8				delete_idx
)
{
	u8						idx = 0;
	struct _RT_BEAMFORMEE_ENTRY		*p_beamform_entry = NULL;
	struct PHY_DM_STRUCT					*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _RT_BEAMFORMING_INFO		*p_beam_info = &p_dm->beamforming_info;

	if (is_delete) {
		if (delete_idx < BEAMFORMEE_ENTRY_NUM) {
			p_beamform_entry = p_beam_info->beamformee_entry + delete_idx;
			if (!((!p_beamform_entry->is_used) && p_beamform_entry->is_sound)) {
				PHYDM_DBG(p_dm, DBG_TXBF, ("[%s] SW delete_idx is wrong!!!!!\n", __func__));
				return;
			}
		}

		PHYDM_DBG(p_dm, DBG_TXBF, ("[%s] SW delete BFee entry %d\n", __func__, delete_idx));
		if (p_beamform_entry->beamform_entry_state == BEAMFORMING_ENTRY_STATE_PROGRESSING) {
			p_beamform_entry->is_beamforming_in_progress = false;
			p_beamform_entry->beamform_entry_state = BEAMFORMING_ENTRY_STATE_UNINITIALIZE;
		} else if (p_beamform_entry->beamform_entry_state == BEAMFORMING_ENTRY_STATE_PROGRESSED) {
			p_beamform_entry->beamform_entry_state  = BEAMFORMING_ENTRY_STATE_UNINITIALIZE;
			hal_com_txbf_set(p_dm, TXBF_SET_SOUNDING_STATUS, (u8 *)&delete_idx);
		}
		p_beamform_entry->is_sound = false;
	} else {
		for (idx = 0; idx < BEAMFORMEE_ENTRY_NUM; idx++) {
			p_beamform_entry = p_beam_info->beamformee_entry + idx;

			/*Used after is_sounding=RESET, and will clear the setting of "ever sounded" entry, which is not necessarily be deleted.*/
			/*This function is mainly used in case "beam_oid_info.sound_oid_mode == sounding_stop_all_timer".*/
			/*However, setting oid doesn't delete entries (is_used is still true), new entries may fail to be added in.*/

			if (p_beamform_entry->is_sound) {
				PHYDM_DBG(p_dm, DBG_TXBF, ("[%s] SW reset BFee entry %d\n", __func__, idx));
				/*
				*	If End procedure is
				*	1. Between (Send NDPA, C2H packet return), reset state to initialized.
				*	After C2H packet return , status bit will be set to zero.
				*
				*	2. After C2H packet, then reset state to initialized and clear status bit.
				*/

				if (p_beamform_entry->beamform_entry_state == BEAMFORMING_ENTRY_STATE_PROGRESSING)
					phydm_beamforming_end_sw(p_dm, 0);
				else if (p_beamform_entry->beamform_entry_state == BEAMFORMING_ENTRY_STATE_PROGRESSED) {
					p_beamform_entry->beamform_entry_state  = BEAMFORMING_ENTRY_STATE_INITIALIZED;
					hal_com_txbf_set(p_dm, TXBF_SET_SOUNDING_STATUS, (u8 *)&idx);
				}

				p_beamform_entry->is_sound = false;
			}
		}
	}
}

void
phydm_beamforming_clear_entry_fw(
	void			*p_dm_void,
	boolean				is_delete,
	u8				delete_idx
)
{
	u8						idx = 0;
	struct _RT_BEAMFORMEE_ENTRY		*p_beamform_entry = NULL;
	struct PHY_DM_STRUCT					*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _RT_BEAMFORMING_INFO		*p_beam_info = &p_dm->beamforming_info;

	if (is_delete) {
		if (delete_idx < BEAMFORMEE_ENTRY_NUM) {
			p_beamform_entry = p_beam_info->beamformee_entry + delete_idx;

			if (!((!p_beamform_entry->is_used) && p_beamform_entry->is_sound)) {
				PHYDM_DBG(p_dm, DBG_TXBF, ("[%s] FW delete_idx is wrong!!!!!\n", __func__));
				return;
			}
		}
		PHYDM_DBG(p_dm, DBG_TXBF, ("%s: FW delete BFee entry %d\n", __func__, delete_idx));
		p_beamform_entry->beamform_entry_state = BEAMFORMING_ENTRY_STATE_UNINITIALIZE;
		p_beamform_entry->is_sound = false;
	} else {
		for (idx = 0; idx < BEAMFORMEE_ENTRY_NUM; idx++) {
			p_beamform_entry = p_beam_info->beamformee_entry + idx;

			/*Used after is_sounding=RESET, and will clear the setting of "ever sounded" entry, which is not necessarily be deleted.*/
			/*This function is mainly used in case "beam_oid_info.sound_oid_mode == sounding_stop_all_timer".*/
			/*However, setting oid doesn't delete entries (is_used is still true), new entries may fail to be added in.*/

			if (p_beamform_entry->is_sound) {
				PHYDM_DBG(p_dm, DBG_TXBF, ("[%s]FW reset BFee entry %d\n", __func__, idx));
				/*
				*	If End procedure is
				*	1. Between (Send NDPA, C2H packet return), reset state to initialized.
				*	After C2H packet return , status bit will be set to zero.
				*
				*	2. After C2H packet, then reset state to initialized and clear status bit.
				*/

				p_beamform_entry->beamform_entry_state = BEAMFORMING_ENTRY_STATE_INITIALIZED;
				p_beamform_entry->is_sound = false;
			}
		}
	}
}

/*
*	Called :
*	1. Add and delete entry : beamforming_enter/beamforming_leave
*	2. FW trigger :  Beamforming_SetTxBFen
*	3. Set OID_RT_BEAMFORMING_PERIOD : beamforming_control_v2
*/
void
phydm_beamforming_notify(
	void			*p_dm_void
)
{
	u8						idx = BEAMFORMEE_ENTRY_NUM;
	enum beamforming_notify_state	is_sounding = BEAMFORMING_NOTIFY_NONE;
	struct PHY_DM_STRUCT					*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _RT_BEAMFORMING_INFO		*p_beam_info = &p_dm->beamforming_info;
	struct _RT_SOUNDING_INFO			*p_sound_info = &(p_beam_info->sounding_info);

	PHYDM_DBG(p_dm, DBG_TXBF, ("%s Start!\n", __func__));

	is_sounding = phydm_beamfomring_is_sounding(p_dm, p_beam_info, &idx);

	PHYDM_DBG(p_dm, DBG_TXBF, ("%s, Before notify, is_sounding=%d, idx=%d\n", __func__, is_sounding, idx));
	PHYDM_DBG(p_dm, DBG_TXBF, ("%s: p_beam_info->beamformee_su_cnt = %d\n", __func__, p_beam_info->beamformee_su_cnt));


	switch (is_sounding) {
	case BEAMFORMEE_NOTIFY_ADD_SU:
		PHYDM_DBG(p_dm, DBG_TXBF, ("%s: BEAMFORMEE_NOTIFY_ADD_SU\n", __func__));
		phydm_beamforming_start_period(p_dm);
		break;

	case BEAMFORMEE_NOTIFY_DELETE_SU:
		PHYDM_DBG(p_dm, DBG_TXBF, ("%s: BEAMFORMEE_NOTIFY_DELETE_SU\n", __func__));
		if (p_sound_info->sound_mode == SOUNDING_FW_HT_TIMER || p_sound_info->sound_mode == SOUNDING_FW_VHT_TIMER) {
			phydm_beamforming_clear_entry_fw(p_dm, true, idx);
			if (p_beam_info->beamformee_su_cnt == 0) { /* For 2->1 entry, we should not cancel SW timer */
				phydm_beamforming_end_period_fw(p_dm);
				PHYDM_DBG(p_dm, DBG_TXBF, ("%s: No BFee left\n", __func__));
			}
		} else {
			phydm_beamforming_clear_entry_sw(p_dm, true, idx);
			if (p_beam_info->beamformee_su_cnt == 0) { /* For 2->1 entry, we should not cancel SW timer */
				phydm_beamforming_end_period_sw(p_dm);
				PHYDM_DBG(p_dm, DBG_TXBF, ("%s: No BFee left\n", __func__));
			}
		}
		break;

	case BEAMFORMEE_NOTIFY_ADD_MU:
		PHYDM_DBG(p_dm, DBG_TXBF, ("%s: BEAMFORMEE_NOTIFY_ADD_MU\n", __func__));
		if (p_beam_info->beamformee_mu_cnt == 2) {
			/*if (p_sound_info->sound_mode == SOUNDING_SW_VHT_TIMER || p_sound_info->sound_mode == SOUNDING_SW_HT_TIMER)
				odm_set_timer(p_dm, &p_beam_info->beamforming_timer, p_sound_info->sound_period);*/
			odm_set_timer(p_dm, &p_beam_info->beamforming_timer, 1000); /*Do MU sounding every 1sec*/
		} else
			PHYDM_DBG(p_dm, DBG_TXBF, ("%s: Less or larger than 2 MU STAs, not to set timer\n", __func__));
		break;

	case BEAMFORMEE_NOTIFY_DELETE_MU:
		PHYDM_DBG(p_dm, DBG_TXBF, ("%s: BEAMFORMEE_NOTIFY_DELETE_MU\n", __func__));
		if (p_beam_info->beamformee_mu_cnt == 1) {
			/*if (p_sound_info->sound_mode == SOUNDING_SW_VHT_TIMER || p_sound_info->sound_mode == SOUNDING_SW_HT_TIMER)*/{
				odm_cancel_timer(p_dm, &p_beam_info->beamforming_timer);
				PHYDM_DBG(p_dm, DBG_TXBF, ("%s: Less than 2 MU STAs, stop sounding\n", __func__));
			}
		}
		break;

	case BEAMFORMING_NOTIFY_RESET:
		if (p_sound_info->sound_mode == SOUNDING_FW_HT_TIMER || p_sound_info->sound_mode == SOUNDING_FW_VHT_TIMER) {
			phydm_beamforming_clear_entry_fw(p_dm, false, idx);
			phydm_beamforming_end_period_fw(p_dm);
		} else {
			phydm_beamforming_clear_entry_sw(p_dm, false, idx);
			phydm_beamforming_end_period_sw(p_dm);
		}

		break;

	default:
		break;
	}

}



boolean
beamforming_init_entry(
	void		*p_dm_void,
	u16		sta_idx,
	u8			*bfer_bfee_idx
)
{
	struct PHY_DM_STRUCT					*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _RT_BEAMFORMEE_ENTRY		*p_beamform_entry = NULL;
	struct _RT_BEAMFORMER_ENTRY		*p_beamformer_entry = NULL;
	struct _RT_BEAMFORM_STAINFO		*p_sta = NULL;
	enum beamforming_cap			beamform_cap = BEAMFORMING_CAP_NONE;
	u8						bfer_idx = 0xF, bfee_idx = 0xF;
	u8						num_of_sounding_dim = 0, comp_steering_num_of_bfer = 0;

	p_sta = phydm_sta_info_init(p_dm, sta_idx);

	/*The current setting does not support Beaforming*/
	if (BEAMFORMING_CAP_NONE == p_sta->ht_beamform_cap && BEAMFORMING_CAP_NONE == p_sta->vht_beamform_cap) {
		PHYDM_DBG(p_dm, DBG_TXBF, ("The configuration disabled Beamforming! Skip...\n"));
		return false;
	}

	if (p_sta->wireless_mode < WIRELESS_MODE_N_24G)
		return false;
	else {
		if (p_sta->wireless_mode & WIRELESS_MODE_N_5G || p_sta->wireless_mode & WIRELESS_MODE_N_24G) {/*HT*/
			if (TEST_FLAG(p_sta->cur_beamform, BEAMFORMING_HT_BEAMFORMER_ENABLE)) {/*We are Beamformee because the STA is Beamformer*/
				beamform_cap = (enum beamforming_cap)(beamform_cap | BEAMFORMEE_CAP_HT_EXPLICIT);
				num_of_sounding_dim = (p_sta->cur_beamform & BEAMFORMING_HT_BEAMFORMEE_CHNL_EST_CAP) >> 6;
			}
			/*We are Beamformer because the STA is Beamformee*/
			if (TEST_FLAG(p_sta->cur_beamform, BEAMFORMING_HT_BEAMFORMEE_ENABLE) ||
			    TEST_FLAG(p_sta->ht_beamform_cap, BEAMFORMING_HT_BEAMFORMER_TEST)) {
				beamform_cap = (enum beamforming_cap)(beamform_cap | BEAMFORMER_CAP_HT_EXPLICIT);
				comp_steering_num_of_bfer = (p_sta->cur_beamform & BEAMFORMING_HT_BEAMFORMER_STEER_NUM) >> 4;
			}
			PHYDM_DBG(p_dm, DBG_TXBF, ("[%s] HT cur_beamform=0x%X, beamform_cap=0x%X\n", __func__, p_sta->cur_beamform, beamform_cap));
			PHYDM_DBG(p_dm, DBG_TXBF, ("[%s] HT num_of_sounding_dim=%d, comp_steering_num_of_bfer=%d\n", __func__, num_of_sounding_dim, comp_steering_num_of_bfer));
		}
#if (ODM_IC_11AC_SERIES_SUPPORT == 1)
		if (p_sta->wireless_mode & WIRELESS_MODE_AC_5G || p_sta->wireless_mode & WIRELESS_MODE_AC_24G) {	/*VHT*/

			/* We are Beamformee because the STA is SU Beamformer*/
			if (TEST_FLAG(p_sta->cur_beamform_vht, BEAMFORMING_VHT_BEAMFORMER_ENABLE)) {
				beamform_cap = (enum beamforming_cap)(beamform_cap | BEAMFORMEE_CAP_VHT_SU);
				num_of_sounding_dim = (p_sta->cur_beamform_vht & BEAMFORMING_VHT_BEAMFORMEE_SOUND_DIM) >> 12;
			}
			/* We are Beamformer because the STA is SU Beamformee*/
			if (TEST_FLAG(p_sta->cur_beamform_vht, BEAMFORMING_VHT_BEAMFORMEE_ENABLE) ||
			    TEST_FLAG(p_sta->vht_beamform_cap, BEAMFORMING_VHT_BEAMFORMER_TEST)) {
				beamform_cap = (enum beamforming_cap)(beamform_cap | BEAMFORMER_CAP_VHT_SU);
				comp_steering_num_of_bfer = (p_sta->cur_beamform_vht & BEAMFORMING_VHT_BEAMFORMER_STS_CAP) >> 8;
			}
			/* We are Beamformee because the STA is MU Beamformer*/
			if (TEST_FLAG(p_sta->cur_beamform_vht, BEAMFORMING_VHT_MU_MIMO_AP_ENABLE)) {
				beamform_cap = (enum beamforming_cap)(beamform_cap | BEAMFORMEE_CAP_VHT_MU);
				num_of_sounding_dim = (p_sta->cur_beamform_vht & BEAMFORMING_VHT_BEAMFORMEE_SOUND_DIM) >> 12;
			}
			/* We are Beamformer because the STA is MU Beamformee*/
			if (phydm_acting_determine(p_dm, phydm_acting_as_ap)) { /* Only AP mode supports to act an MU beamformer */
				if (TEST_FLAG(p_sta->cur_beamform_vht, BEAMFORMING_VHT_MU_MIMO_STA_ENABLE) ||
				    TEST_FLAG(p_sta->vht_beamform_cap, BEAMFORMING_VHT_BEAMFORMER_TEST)) {
					beamform_cap = (enum beamforming_cap)(beamform_cap | BEAMFORMER_CAP_VHT_MU);
					comp_steering_num_of_bfer = (p_sta->cur_beamform_vht & BEAMFORMING_VHT_BEAMFORMER_STS_CAP) >> 8;
				}
			}
			PHYDM_DBG(p_dm, DBG_TXBF, ("[%s]VHT cur_beamform_vht=0x%X, beamform_cap=0x%X\n", __func__, p_sta->cur_beamform_vht, beamform_cap));
			PHYDM_DBG(p_dm, DBG_TXBF, ("[%s]VHT num_of_sounding_dim=0x%X, comp_steering_num_of_bfer=0x%X\n", __func__, num_of_sounding_dim, comp_steering_num_of_bfer));

		}
#endif
	}


	if (beamform_cap == BEAMFORMING_CAP_NONE)
		return false;

	PHYDM_DBG(p_dm, DBG_TXBF, ("[%s] Self BF Entry Cap = 0x%02X\n", __func__, beamform_cap));

	/*We are BFee, so the entry is BFer*/
	if (beamform_cap & (BEAMFORMEE_CAP_VHT_MU | BEAMFORMEE_CAP_VHT_SU | BEAMFORMEE_CAP_HT_EXPLICIT)) {
		p_beamformer_entry = phydm_beamforming_get_bfer_entry_by_addr(p_dm, p_sta->ra, &bfer_idx);

		if (p_beamformer_entry == NULL) {
			p_beamformer_entry = beamforming_add_bfer_entry(p_dm, p_sta, beamform_cap, num_of_sounding_dim, &bfer_idx);
			if (p_beamformer_entry == NULL)
				PHYDM_DBG(p_dm, DBG_TXBF, ("[%s]Not enough BFer entry!!!!!\n", __func__));
		}
	}

	/*We are BFer, so the entry is BFee*/
	if (beamform_cap & (BEAMFORMER_CAP_VHT_MU | BEAMFORMER_CAP_VHT_SU | BEAMFORMER_CAP_HT_EXPLICIT)) {
		p_beamform_entry = phydm_beamforming_get_bfee_entry_by_addr(p_dm, p_sta->ra, &bfee_idx);

		/*�p�GBFeeIdx = 0xF �h�N��ثeentry���S���ۦP��MACID�b��*/
		PHYDM_DBG(p_dm, DBG_TXBF, ("[%s] Get BFee entry 0x%X by address\n", __func__, bfee_idx));
		if (p_beamform_entry == NULL) {
			p_beamform_entry = beamforming_add_bfee_entry(p_dm, p_sta, beamform_cap, num_of_sounding_dim, comp_steering_num_of_bfer, &bfee_idx);
			PHYDM_DBG(p_dm, DBG_TXBF, ("[%s]: p_sta->AID=%d, p_sta->mac_id=%d\n", __func__, p_sta->aid, p_sta->mac_id));

			PHYDM_DBG(p_dm, DBG_TXBF, ("[%s]: Add BFee entry %d\n", __func__, bfee_idx));

			if (p_beamform_entry == NULL)
				return false;
			else
				p_beamform_entry->beamform_entry_state = BEAMFORMING_ENTRY_STATE_INITIALIZEING;
		} else {
			/*Entry has been created. If entry is initialing or progressing then errors occur.*/
			if (p_beamform_entry->beamform_entry_state != BEAMFORMING_ENTRY_STATE_INITIALIZED &&
			    p_beamform_entry->beamform_entry_state != BEAMFORMING_ENTRY_STATE_PROGRESSED)
				return false;
			else
				p_beamform_entry->beamform_entry_state = BEAMFORMING_ENTRY_STATE_INITIALIZEING;
		}
		p_beamform_entry->beamform_entry_state = BEAMFORMING_ENTRY_STATE_INITIALIZED;
		phydm_sta_info_update(p_dm, sta_idx, p_beamform_entry);
	}

	*bfer_bfee_idx = (bfer_idx << 4) | bfee_idx;
	PHYDM_DBG(p_dm, DBG_TXBF, ("[%s] End: bfer_idx=0x%X, bfee_idx=0x%X, bfer_bfee_idx=0x%X\n", __func__, bfer_idx, bfee_idx, *bfer_bfee_idx));

	return true;
}


void
beamforming_deinit_entry(
	void		*p_dm_void,
	u8			*RA
)
{
	struct PHY_DM_STRUCT			*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u8				idx = 0;

	struct _RT_BEAMFORMER_ENTRY	*p_bfer_entry = phydm_beamforming_get_bfer_entry_by_addr(p_dm, RA, &idx);
	struct _RT_BEAMFORMEE_ENTRY	*p_bfee_entry = phydm_beamforming_get_bfee_entry_by_addr(p_dm, RA, &idx);
	boolean ret = false;

	PHYDM_DBG(p_dm, DBG_TXBF, ("%s Start!\n",  __func__));

	if (p_bfee_entry != NULL) {
		PHYDM_DBG(p_dm, DBG_TXBF, ("%s, p_bfee_entry\n", __func__));
		p_bfee_entry->is_used = false;
		p_bfee_entry->beamform_entry_cap = BEAMFORMING_CAP_NONE;
		p_bfee_entry->is_beamforming_in_progress = false;
		if (p_bfee_entry->is_mu_sta) {
			p_dm->beamforming_info.beamformee_mu_cnt -= 1;
			p_dm->beamforming_info.first_mu_bfee_index = phydm_beamforming_get_first_mu_bfee_entry_idx(p_dm);
		} else
			p_dm->beamforming_info.beamformee_su_cnt -= 1;
		ret = true;
	}

	if (p_bfer_entry != NULL) {
		PHYDM_DBG(p_dm, DBG_TXBF, ("%s, p_bfer_entry\n", __func__));
		p_bfer_entry->is_used = false;
		p_bfer_entry->beamform_entry_cap = BEAMFORMING_CAP_NONE;
		if (p_bfer_entry->is_mu_ap)
			p_dm->beamforming_info.beamformer_mu_cnt -= 1;
		else
			p_dm->beamforming_info.beamformer_su_cnt -= 1;
		ret = true;
	}

	if (ret == true)
		hal_com_txbf_set(p_dm, TXBF_SET_SOUNDING_LEAVE, (u8 *)&idx);

	PHYDM_DBG(p_dm, DBG_TXBF, ("%s End, idx = 0x%X\n", __func__, idx));
}


boolean
beamforming_start_v1(
	void		*p_dm_void,
	u8			*RA,
	boolean			mode,
	enum channel_width	BW,
	u8			rate
)
{
	struct PHY_DM_STRUCT				*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u8					idx = 0;
	struct _RT_BEAMFORMEE_ENTRY	*p_entry;
	boolean					ret = true;
	struct _RT_BEAMFORMING_INFO	*p_beam_info = &(p_dm->beamforming_info);

	p_entry = phydm_beamforming_get_bfee_entry_by_addr(p_dm, RA, &idx);

	if (p_entry->is_used == false) {
		p_entry->is_beamforming_in_progress = false;
		return false;
	} else {
		if (p_entry->is_beamforming_in_progress)
			return false;

		p_entry->is_beamforming_in_progress = true;

		if (mode == 1) {
			if (!(p_entry->beamform_entry_cap & BEAMFORMER_CAP_HT_EXPLICIT)) {
				p_entry->is_beamforming_in_progress = false;
				return false;
			}
		} else if (mode == 0) {
			if (!(p_entry->beamform_entry_cap & BEAMFORMER_CAP_VHT_SU)) {
				p_entry->is_beamforming_in_progress = false;
				return false;
			}
		}

		if (p_entry->beamform_entry_state != BEAMFORMING_ENTRY_STATE_INITIALIZED && p_entry->beamform_entry_state != BEAMFORMING_ENTRY_STATE_PROGRESSED) {
			p_entry->is_beamforming_in_progress = false;
			return false;
		} else {
			p_entry->beamform_entry_state = BEAMFORMING_ENTRY_STATE_PROGRESSING;
			p_entry->is_sound = true;
		}
	}

	p_entry->sound_bw = BW;
	p_beam_info->beamformee_cur_idx = idx;
	phydm_beamforming_ndpa_rate(p_dm, BW, rate);
	hal_com_txbf_set(p_dm, TXBF_SET_SOUNDING_STATUS, (u8 *)&idx);

	if (mode == 1)
		ret = beamforming_send_ht_ndpa_packet(p_dm, RA, BW, NORMAL_QUEUE);
	else
		ret = beamforming_send_vht_ndpa_packet(p_dm, RA, p_entry->aid, BW, NORMAL_QUEUE);

	if (ret == false) {
		beamforming_leave(p_dm, RA);
		p_entry->is_beamforming_in_progress = false;
		return false;
	}

	PHYDM_DBG(p_dm, DBG_TXBF, ("%s  idx %d\n", __func__, idx));
	return true;
}


boolean
beamforming_start_sw(
	void		*p_dm_void,
	u8			idx,
	u8			mode,
	enum channel_width	BW
)
{
	u8					*ra = NULL;
	struct PHY_DM_STRUCT				*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _RT_BEAMFORMEE_ENTRY	*p_entry;
	boolean					ret = true;
	struct _RT_BEAMFORMING_INFO	*p_beam_info = &(p_dm->beamforming_info);

	if (p_beam_info->is_mu_sounding) {
		p_beam_info->is_mu_sounding_in_progress = true;
		p_entry = &(p_beam_info->beamformee_entry[idx]);
		ra = p_entry->mac_addr;

	} else {
		p_entry = &(p_beam_info->beamformee_entry[idx]);

		if (p_entry->is_used == false) {
			PHYDM_DBG(p_dm, DBG_TXBF, ("Skip Beamforming, no entry for idx =%d\n", idx));
			p_entry->is_beamforming_in_progress = false;
			return false;
		} else {
			if (p_entry->is_beamforming_in_progress) {
				PHYDM_DBG(p_dm, DBG_TXBF, ("is_beamforming_in_progress, skip...\n"));
				return false;
			}

			p_entry->is_beamforming_in_progress = true;
			ra = p_entry->mac_addr;

			if (mode == SOUNDING_SW_HT_TIMER || mode == SOUNDING_HW_HT_TIMER || mode == SOUNDING_AUTO_HT_TIMER) {
				if (!(p_entry->beamform_entry_cap & BEAMFORMER_CAP_HT_EXPLICIT)) {
					p_entry->is_beamforming_in_progress = false;
					PHYDM_DBG(p_dm, DBG_TXBF, ("%s Return by not support BEAMFORMER_CAP_HT_EXPLICIT <==\n", __func__));
					return false;
				}
			} else if (mode == SOUNDING_SW_VHT_TIMER || mode == SOUNDING_HW_VHT_TIMER || mode == SOUNDING_AUTO_VHT_TIMER) {
				if (!(p_entry->beamform_entry_cap & BEAMFORMER_CAP_VHT_SU)) {
					p_entry->is_beamforming_in_progress = false;
					PHYDM_DBG(p_dm, DBG_TXBF, ("%s Return by not support BEAMFORMER_CAP_VHT_SU <==\n", __func__));
					return false;
				}
			}
			if (p_entry->beamform_entry_state != BEAMFORMING_ENTRY_STATE_INITIALIZED && p_entry->beamform_entry_state != BEAMFORMING_ENTRY_STATE_PROGRESSED) {
				p_entry->is_beamforming_in_progress = false;
				PHYDM_DBG(p_dm, DBG_TXBF, ("%s Return by incorrect beamform_entry_state(%d) <==\n", __func__, p_entry->beamform_entry_state));
				return false;
			} else {
				p_entry->beamform_entry_state = BEAMFORMING_ENTRY_STATE_PROGRESSING;
				p_entry->is_sound = true;
			}
		}

		p_beam_info->beamformee_cur_idx = idx;
	}

	/*2014.12.22 Luke: Need to be checked*/
	/*GET_TXBF_INFO(adapter)->fTxbfSet(adapter, TXBF_SET_SOUNDING_STATUS, (u8*)&idx);*/

	if (mode == SOUNDING_SW_HT_TIMER || mode == SOUNDING_HW_HT_TIMER || mode == SOUNDING_AUTO_HT_TIMER)
		ret = beamforming_send_ht_ndpa_packet(p_dm, ra, BW, NORMAL_QUEUE);
	else
		ret = beamforming_send_vht_ndpa_packet(p_dm, ra, p_entry->aid, BW, NORMAL_QUEUE);

	if (ret == false) {
		beamforming_leave(p_dm, ra);
		p_entry->is_beamforming_in_progress = false;
		return false;
	}


	/*--------------------------
	 * Send BF Report Poll for MU BF
	--------------------------*/
#ifdef SUPPORT_MU_BF
#if (SUPPORT_MU_BF == 1)
	{
		u8				idx, poll_sta_cnt = 0;
		boolean				is_get_first_bfee = false;

		if (p_beam_info->beamformee_mu_cnt > 1) { /* More than 1 MU STA*/

			for (idx = 0; idx < BEAMFORMEE_ENTRY_NUM; idx++) {
				p_entry = &(p_beam_info->beamformee_entry[idx]);
				if (p_entry->is_mu_sta) {
					if (is_get_first_bfee) {
						poll_sta_cnt++;
						if (poll_sta_cnt == (p_beam_info->beamformee_mu_cnt - 1))/* The last STA*/
							send_sw_vht_bf_report_poll(p_dm, p_entry->mac_addr, true);
						else
							send_sw_vht_bf_report_poll(p_dm, p_entry->mac_addr, false);
					} else
						is_get_first_bfee = true;
				}
			}
		}
	}
#endif
#endif
	return true;
}


boolean
beamforming_start_fw(
	void		*p_dm_void,
	u8			idx
)
{
	struct PHY_DM_STRUCT				*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _RT_BEAMFORMEE_ENTRY	*p_entry;
	struct _RT_BEAMFORMING_INFO	*p_beam_info = &(p_dm->beamforming_info);

	p_entry = &(p_beam_info->beamformee_entry[idx]);
	if (p_entry->is_used == false) {
		PHYDM_DBG(p_dm, DBG_TXBF, ("Skip Beamforming, no entry for idx =%d\n", idx));
		return false;
	}

	p_entry->beamform_entry_state = BEAMFORMING_ENTRY_STATE_PROGRESSING;
	p_entry->is_sound = true;
	hal_com_txbf_set(p_dm, TXBF_SET_SOUNDING_FW_NDPA, (u8 *)&idx);

	PHYDM_DBG(p_dm, DBG_TXBF, ("[%s] End, idx=0x%X\n", __func__, idx));
	return true;
}

void
beamforming_check_sounding_success(
	void			*p_dm_void,
	boolean			status
)
{
	struct PHY_DM_STRUCT				*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _RT_BEAMFORMING_INFO	*p_beam_info = &(p_dm->beamforming_info);
	struct _RT_BEAMFORMEE_ENTRY	*p_entry = &(p_beam_info->beamformee_entry[p_beam_info->beamformee_cur_idx]);

	PHYDM_DBG(p_dm, DBG_TXBF, ("[David]@%s Start!\n", __func__));

	if (status == 1) {
		if (p_entry->log_status_fail_cnt == 21)
			beamforming_dym_period(p_dm, status);
		p_entry->log_status_fail_cnt = 0;
	} else if (p_entry->log_status_fail_cnt <= 20) {
		p_entry->log_status_fail_cnt++;
		PHYDM_DBG(p_dm, DBG_TXBF, ("%s log_status_fail_cnt %d\n", __func__, p_entry->log_status_fail_cnt));
	}
	if (p_entry->log_status_fail_cnt > 20) {
		p_entry->log_status_fail_cnt = 21;
		PHYDM_DBG(p_dm, DBG_TXBF, ("%s log_status_fail_cnt > 20, Stop SOUNDING\n", __func__));
		beamforming_dym_period(p_dm, status);
	}
}

void
phydm_beamforming_end_sw(
	void		*p_dm_void,
	boolean			status
)
{
	struct PHY_DM_STRUCT				*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _RT_BEAMFORMING_INFO	*p_beam_info = &p_dm->beamforming_info;
	struct _RT_BEAMFORMEE_ENTRY	*p_entry = &(p_beam_info->beamformee_entry[p_beam_info->beamformee_cur_idx]);

	if (p_beam_info->is_mu_sounding) {
		PHYDM_DBG(p_dm, DBG_TXBF, ("%s: MU sounding done\n", __func__));
		p_beam_info->is_mu_sounding_in_progress = false;
		hal_com_txbf_set(p_dm, TXBF_SET_SOUNDING_STATUS, (u8 *)&(p_beam_info->beamformee_cur_idx));
	} else {
		if (p_entry->beamform_entry_state != BEAMFORMING_ENTRY_STATE_PROGRESSING) {
			PHYDM_DBG(p_dm, DBG_TXBF, ("[%s] BeamformStatus %d\n", __func__, p_entry->beamform_entry_state));
			return;
		}

		if ((p_beam_info->tx_bf_data_rate >= ODM_RATEVHTSS3MCS7) && (p_beam_info->tx_bf_data_rate <= ODM_RATEVHTSS3MCS9) && (p_beam_info->snding3ss == false)) {
			PHYDM_DBG(p_dm, DBG_TXBF, ("[%s] VHT3SS 7,8,9, do not apply V matrix.\n", __func__));
			p_entry->beamform_entry_state = BEAMFORMING_ENTRY_STATE_INITIALIZED;
			hal_com_txbf_set(p_dm, TXBF_SET_SOUNDING_STATUS, (u8 *)&(p_beam_info->beamformee_cur_idx));
		} else if (status == 1) {
			p_entry->log_status_fail_cnt = 0;
			p_entry->beamform_entry_state = BEAMFORMING_ENTRY_STATE_PROGRESSED;
			hal_com_txbf_set(p_dm, TXBF_SET_SOUNDING_STATUS, (u8 *)&(p_beam_info->beamformee_cur_idx));
		} else {
			p_entry->log_status_fail_cnt++;
			p_entry->beamform_entry_state = BEAMFORMING_ENTRY_STATE_INITIALIZED;
			hal_com_txbf_set(p_dm, TXBF_SET_TX_PATH_RESET, (u8 *)&(p_beam_info->beamformee_cur_idx));
			PHYDM_DBG(p_dm, DBG_TXBF, ("[%s] log_status_fail_cnt %d\n", __func__, p_entry->log_status_fail_cnt));
		}

		if (p_entry->log_status_fail_cnt > 50) {
			PHYDM_DBG(p_dm, DBG_TXBF, ("%s log_status_fail_cnt > 50, Stop SOUNDING\n", __func__));
			p_entry->is_sound = false;
			beamforming_deinit_entry(p_dm, p_entry->mac_addr);

			/*Modified by David - Every action of deleting entry should follow by Notify*/
			phydm_beamforming_notify(p_dm);
		}

		p_entry->is_beamforming_in_progress = false;
	}
	PHYDM_DBG(p_dm, DBG_TXBF, ("%s: status=%d\n", __func__, status));
}


void
beamforming_timer_callback(
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	void			*p_dm_void
#elif (DM_ODM_SUPPORT_TYPE == ODM_CE)
	void            *p_context
#endif
)
{
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	struct PHY_DM_STRUCT					*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
#elif (DM_ODM_SUPPORT_TYPE == ODM_CE)
	struct _ADAPTER					*adapter = (struct _ADAPTER *)p_context;
	PHAL_DATA_TYPE				p_hal_data = GET_HAL_DATA(adapter);
	struct PHY_DM_STRUCT					*p_dm = &p_hal_data->odmpriv;
#endif
	boolean						ret = false;
	struct _RT_BEAMFORMING_INFO		*p_beam_info = &(p_dm->beamforming_info);
	struct _RT_BEAMFORMEE_ENTRY		*p_entry = &(p_beam_info->beamformee_entry[p_beam_info->beamformee_cur_idx]);
	struct _RT_SOUNDING_INFO			*p_sound_info = &(p_beam_info->sounding_info);
	boolean					is_beamforming_in_progress;

	PHYDM_DBG(p_dm, DBG_TXBF, ("%s Start!\n", __func__));

	if (p_beam_info->is_mu_sounding)
		is_beamforming_in_progress = p_beam_info->is_mu_sounding_in_progress;
	else
		is_beamforming_in_progress = p_entry->is_beamforming_in_progress;

	if (is_beamforming_in_progress) {
		PHYDM_DBG(p_dm, DBG_TXBF, ("is_beamforming_in_progress, reset it\n"));
		phydm_beamforming_end_sw(p_dm, 0);
	}

	ret = phydm_beamforming_select_beam_entry(p_dm, p_beam_info);
#if (SUPPORT_MU_BF == 1)
	if (ret && p_beam_info->beamformee_mu_cnt > 1)
		ret = 1;
	else
		ret = 0;
#endif
	if (ret)
		ret = beamforming_start_sw(p_dm, p_sound_info->sound_idx, p_sound_info->sound_mode, p_sound_info->sound_bw);
	else
		PHYDM_DBG(p_dm, DBG_TXBF, ("%s, Error value return from BeamformingStart_V2\n", __func__));

	if ((p_beam_info->beamformee_su_cnt != 0) || (p_beam_info->beamformee_mu_cnt > 1)) {
		if (p_sound_info->sound_mode == SOUNDING_SW_VHT_TIMER || p_sound_info->sound_mode == SOUNDING_SW_HT_TIMER)
			odm_set_timer(p_dm, &p_beam_info->beamforming_timer, p_sound_info->sound_period);
		else {
			u32	val = (p_sound_info->sound_period << 16) | HAL_TIMER_TXBF;
			phydm_set_hw_reg_handler_interface(p_dm, HW_VAR_HW_REG_TIMER_RESTART, (u8 *)(&val));
		}
	}
}


void
beamforming_sw_timer_callback(
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	struct timer_list		*p_timer
#elif (DM_ODM_SUPPORT_TYPE == ODM_CE)
	void *function_context
#endif
)
{
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	struct _ADAPTER		*adapter = (struct _ADAPTER *)p_timer->Adapter;
	HAL_DATA_TYPE	*p_hal_data = GET_HAL_DATA(adapter);
	struct PHY_DM_STRUCT		*p_dm = &p_hal_data->DM_OutSrc;

	PHYDM_DBG(p_dm, DBG_TXBF, ("[%s] Start!\n", __func__));
	beamforming_timer_callback(p_dm);
#elif (DM_ODM_SUPPORT_TYPE == ODM_CE)
	struct PHY_DM_STRUCT	*p_dm = (struct PHY_DM_STRUCT *)function_context;
	struct _ADAPTER	*adapter = p_dm->adapter;

	if (adapter->net_closed == true)
		return;
	rtw_run_in_thread_cmd(adapter, beamforming_timer_callback, adapter);
#endif

}


void
phydm_beamforming_init(
	void			*p_dm_void
)
{
	struct PHY_DM_STRUCT					*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _RT_BEAMFORMING_INFO		*p_beam_info = &p_dm->beamforming_info;
	struct _RT_BEAMFORMING_OID_INFO	*p_beam_oid_info = &(p_beam_info->beamforming_oid_info);
	#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	struct _ADAPTER		*adapter = p_dm->adapter;
	HAL_DATA_TYPE	*p_hal_data = GET_HAL_DATA(adapter);

	#ifdef BEAMFORMING_VERSION_1
	if (p_hal_data->beamforming_version != BEAMFORMING_VERSION_1) {
		return;
	}
	#endif
	#endif

	p_beam_oid_info->sound_oid_mode = SOUNDING_STOP_OID_TIMER;
	PHYDM_DBG(p_dm, DBG_TXBF, ("%s mode (%d)\n", __func__, p_beam_oid_info->sound_oid_mode));

	p_beam_info->beamformee_su_cnt = 0;
	p_beam_info->beamformer_su_cnt = 0;
	p_beam_info->beamformee_mu_cnt = 0;
	p_beam_info->beamformer_mu_cnt = 0;
	p_beam_info->beamformee_mu_reg_maping = 0;
	p_beam_info->mu_ap_index = 0;
	p_beam_info->is_mu_sounding = false;
	p_beam_info->first_mu_bfee_index = 0xFF;
	p_beam_info->apply_v_matrix = true;
	p_beam_info->snding3ss = false;

#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	p_beam_info->source_adapter = p_dm->adapter;
#endif
	hal_com_txbf_beamform_init(p_dm);
}


boolean
phydm_acting_determine(
	void			*p_dm_void,
	enum phydm_acting_type	type
)
{
	struct PHY_DM_STRUCT	*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	boolean		ret = false;
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	struct _ADAPTER	*adapter = p_dm->beamforming_info.source_adapter;
#else
	struct _ADAPTER	*adapter = p_dm->adapter;
#endif

#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
	if (type == phydm_acting_as_ap)
		ret = ACTING_AS_AP(adapter);
	else if (type == phydm_acting_as_ibss)
		ret = ACTING_AS_IBSS(adapter);
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
	struct mlme_priv			*pmlmepriv = &(adapter->mlmepriv);

	if (type == phydm_acting_as_ap)
		ret = check_fwstate(pmlmepriv, WIFI_AP_STATE);
	else if (type == phydm_acting_as_ibss)
		ret = check_fwstate(pmlmepriv, WIFI_ADHOC_STATE) || check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE);
#endif

	return ret;

}

void
beamforming_enter(
	void			*p_dm_void,
	u16		sta_idx
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u8			bfer_bfee_idx = 0xff;

	if (beamforming_init_entry(p_dm, sta_idx, &bfer_bfee_idx))
		hal_com_txbf_set(p_dm, TXBF_SET_SOUNDING_ENTER, (u8 *)&bfer_bfee_idx);

	PHYDM_DBG(p_dm, DBG_TXBF, ("[%s] End!\n", __func__));
}


void
beamforming_leave(
	void			*p_dm_void,
	u8			*RA
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;

	if (RA != NULL) {
		beamforming_deinit_entry(p_dm, RA);
		phydm_beamforming_notify(p_dm);
	}

	PHYDM_DBG(p_dm, DBG_TXBF, ("[%s] End!!\n", __func__));
}

#if 0
/* Nobody calls this function */
void
phydm_beamforming_set_txbf_en(
	void		*p_dm_void,
	u8			mac_id,
	boolean			is_txbf
)
{
	struct PHY_DM_STRUCT				*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u8					idx = 0;
	struct _RT_BEAMFORMEE_ENTRY	*p_entry;

	PHYDM_DBG(p_dm, DBG_TXBF, ("%s Start!\n", __func__));

	p_entry = phydm_beamforming_get_entry_by_mac_id(p_dm, mac_id, &idx);

	if (p_entry == NULL)
		return;
	else
		p_entry->is_txbf = is_txbf;

	PHYDM_DBG(p_dm, DBG_TXBF, ("%s mac_id %d TxBF %d\n", __func__, p_entry->mac_id, p_entry->is_txbf));

	phydm_beamforming_notify(p_dm);
}
#endif

enum beamforming_cap
phydm_beamforming_get_beam_cap(
	void						*p_dm_void,
	struct _RT_BEAMFORMING_INFO	*p_beam_info
)
{
	u8					i;
	boolean				is_self_beamformer = false;
	boolean				is_self_beamformee = false;
	struct _RT_BEAMFORMEE_ENTRY	beamformee_entry;
	struct _RT_BEAMFORMER_ENTRY	beamformer_entry;
	enum beamforming_cap		beamform_cap = BEAMFORMING_CAP_NONE;
	struct PHY_DM_STRUCT				*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;

	PHYDM_DBG(p_dm, DBG_TXBF, ("[%s] Start!\n", __func__));

	for (i = 0; i < BEAMFORMEE_ENTRY_NUM; i++) {
		beamformee_entry = p_beam_info->beamformee_entry[i];

		if (beamformee_entry.is_used) {
			is_self_beamformer = true;
			PHYDM_DBG(p_dm, DBG_TXBF, ("[%s] BFee entry %d is_used=true\n", __func__, i));
			break;
		}
	}

	for (i = 0; i < BEAMFORMER_ENTRY_NUM; i++) {
		beamformer_entry = p_beam_info->beamformer_entry[i];

		if (beamformer_entry.is_used) {
			is_self_beamformee = true;
			PHYDM_DBG(p_dm, DBG_TXBF, ("[%s]: BFer entry %d is_used=true\n", __func__, i));
			break;
		}
	}

	if (is_self_beamformer)
		beamform_cap = (enum beamforming_cap)(beamform_cap | BEAMFORMER_CAP);
	if (is_self_beamformee)
		beamform_cap = (enum beamforming_cap)(beamform_cap | BEAMFORMEE_CAP);

	return beamform_cap;
}


boolean
beamforming_control_v1(
	void			*p_dm_void,
	u8			*RA,
	u8			AID,
	u8			mode,
	enum channel_width	BW,
	u8			rate
)
{
	struct PHY_DM_STRUCT	*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	boolean		ret = true;

	PHYDM_DBG(p_dm, DBG_TXBF, ("%s Start!\n", __func__));

	PHYDM_DBG(p_dm, DBG_TXBF, ("AID (%d), mode (%d), BW (%d)\n", AID, mode, BW));

	switch (mode) {
	case 0:
		ret = beamforming_start_v1(p_dm, RA, 0, BW, rate);
		break;
	case 1:
		ret = beamforming_start_v1(p_dm, RA, 1, BW, rate);
		break;
	case 2:
		phydm_beamforming_ndpa_rate(p_dm, BW, rate);
		ret = beamforming_send_vht_ndpa_packet(p_dm, RA, AID, BW, NORMAL_QUEUE);
		break;
	case 3:
		phydm_beamforming_ndpa_rate(p_dm, BW, rate);
		ret = beamforming_send_ht_ndpa_packet(p_dm, RA, BW, NORMAL_QUEUE);
		break;
	}
	return ret;
}

/*Only OID uses this function*/
boolean
phydm_beamforming_control_v2(
	void		*p_dm_void,
	u8			idx,
	u8			mode,
	enum channel_width	BW,
	u16			period
)
{
	struct PHY_DM_STRUCT					*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _RT_BEAMFORMING_INFO		*p_beam_info =  &p_dm->beamforming_info;
	struct _RT_BEAMFORMING_OID_INFO	*p_beam_oid_info = &(p_beam_info->beamforming_oid_info);

	PHYDM_DBG(p_dm, DBG_TXBF, ("%s Start!\n", __func__));
	PHYDM_DBG(p_dm, DBG_TXBF, ("idx (%d), mode (%d), BW (%d), period (%d)\n", idx, mode, BW, period));

	p_beam_oid_info->sound_oid_idx = idx;
	p_beam_oid_info->sound_oid_mode = (enum sounding_mode) mode;
	p_beam_oid_info->sound_oid_bw = BW;
	p_beam_oid_info->sound_oid_period = period;

	phydm_beamforming_notify(p_dm);

	return true;
}


void
phydm_beamforming_watchdog(
	void		*p_dm_void
)
{
	struct PHY_DM_STRUCT					*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _RT_BEAMFORMING_INFO		*p_beam_info = &p_dm->beamforming_info;

	PHYDM_DBG(p_dm, DBG_TXBF, ("%s Start!\n", __func__));

	if (p_beam_info->beamformee_su_cnt == 0)
		return;

	beamforming_dym_period(p_dm, 0);
}
enum beamforming_cap
phydm_get_beamform_cap(
	void			*p_dm_void
)
{
	struct PHY_DM_STRUCT                    *p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct cmn_sta_info                     *p_sta = NULL;
	struct bf_cmn_info                      *p_bf_info = NULL;
	struct _RT_BEAMFORMING_INFO             *p_beam_info = &p_dm->beamforming_info;
	struct _ADAPTER                         *adapter = p_dm->adapter;
	enum beamforming_cap                     beamform_cap = BEAMFORMING_CAP_NONE;
	u8                                       macid;
	u8                                       ht_curbeamformcap = 0;
	u16                                      vht_curbeamformcap = 0;


#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	PMGNT_INFO                              p_MgntInfo = &adapter->MgntInfo;
	PRT_VERY_HIGH_THROUGHPUT                p_vht_info = GET_VHT_INFO(p_MgntInfo);
	PRT_HIGH_THROUGHPUT                     p_ht_info  = GET_HT_INFO(p_MgntInfo);

	ht_curbeamformcap = p_ht_info->HtCurBeamform;
	vht_curbeamformcap = p_vht_info->VhtCurBeamform;

	PHYDM_DBG(p_dm, DBG_ANT_DIV, ("[%s] WIN ht_curcap = %d ; vht_curcap = %d\n", __func__, ht_curbeamformcap, vht_curbeamformcap));

	if (TEST_FLAG(ht_curbeamformcap, BEAMFORMING_HT_BEAMFORMER_ENABLE)) /*We are Beamformee because the STA is Beamformer*/
		beamform_cap = (enum beamforming_cap)(beamform_cap | (BEAMFORMEE_CAP_HT_EXPLICIT | BEAMFORMEE_CAP));

	/*We are Beamformer because the STA is Beamformee*/
	if (TEST_FLAG(ht_curbeamformcap, BEAMFORMING_HT_BEAMFORMEE_ENABLE))
		beamform_cap = (enum beamforming_cap)(beamform_cap | (BEAMFORMER_CAP_HT_EXPLICIT | BEAMFORMER_CAP));

	#if (ODM_IC_11AC_SERIES_SUPPORT == 1)

	/* We are Beamformee because the STA is SU Beamformer*/
	if (TEST_FLAG(vht_curbeamformcap, BEAMFORMING_VHT_BEAMFORMER_ENABLE))
		beamform_cap = (enum beamforming_cap)(beamform_cap | (BEAMFORMEE_CAP_VHT_SU | BEAMFORMEE_CAP));

	/* We are Beamformer because the STA is SU Beamformee*/
	if (TEST_FLAG(vht_curbeamformcap, BEAMFORMING_VHT_BEAMFORMEE_ENABLE))
		beamform_cap = (enum beamforming_cap)(beamform_cap | (BEAMFORMER_CAP_VHT_SU | BEAMFORMER_CAP));

	/* We are Beamformee because the STA is MU Beamformer*/
	if (TEST_FLAG(vht_curbeamformcap, BEAMFORMING_VHT_MU_MIMO_AP_ENABLE))
		beamform_cap = (enum beamforming_cap)(beamform_cap | (BEAMFORMEE_CAP_VHT_MU | BEAMFORMEE_CAP));
	#endif
#elif (DM_ODM_SUPPORT_TYPE == ODM_CE)


		for (macid = 0; macid < ODM_ASSOCIATE_ENTRY_NUM; macid++) {

		p_sta = p_dm->p_phydm_sta_info[macid];

		if (!is_sta_active(p_sta))
			continue;

		p_bf_info = &(p_sta->bf_info);
		vht_curbeamformcap = p_bf_info->vht_beamform_cap;
		ht_curbeamformcap  = p_bf_info->ht_beamform_cap;

		if (TEST_FLAG(ht_curbeamformcap, BEAMFORMING_HT_BEAMFORMER_ENABLE)) /*We are Beamformee because the STA is Beamformer*/
			beamform_cap = (enum beamforming_cap)(beamform_cap | (BEAMFORMEE_CAP_HT_EXPLICIT | BEAMFORMEE_CAP));

		/*We are Beamformer because the STA is Beamformee*/
		if (TEST_FLAG(ht_curbeamformcap, BEAMFORMING_HT_BEAMFORMEE_ENABLE))
			beamform_cap = (enum beamforming_cap)(beamform_cap | (BEAMFORMER_CAP_HT_EXPLICIT | BEAMFORMER_CAP));

	#if (ODM_IC_11AC_SERIES_SUPPORT == 1)
		/* We are Beamformee because the STA is SU Beamformer*/
		if (TEST_FLAG(vht_curbeamformcap, BEAMFORMING_VHT_BEAMFORMER_ENABLE))
			beamform_cap = (enum beamforming_cap)(beamform_cap | (BEAMFORMEE_CAP_VHT_SU | BEAMFORMEE_CAP));

		/* We are Beamformer because the STA is SU Beamformee*/
		if (TEST_FLAG(vht_curbeamformcap, BEAMFORMING_VHT_BEAMFORMEE_ENABLE))
			beamform_cap = (enum beamforming_cap)(beamform_cap | (BEAMFORMER_CAP_VHT_SU | BEAMFORMER_CAP));

		/* We are Beamformee because the STA is MU Beamformer*/
		if (TEST_FLAG(vht_curbeamformcap, BEAMFORMING_VHT_MU_MIMO_AP_ENABLE))
			beamform_cap = (enum beamforming_cap)(beamform_cap | (BEAMFORMEE_CAP_VHT_MU | BEAMFORMEE_CAP));
	#endif
}
	PHYDM_DBG(p_dm, DBG_ANT_DIV, ("[%s] CE ht_curcap = %d ; vht_curcap = %d\n", __func__, ht_curbeamformcap, vht_curbeamformcap));

#endif

return beamform_cap;

}

#endif
