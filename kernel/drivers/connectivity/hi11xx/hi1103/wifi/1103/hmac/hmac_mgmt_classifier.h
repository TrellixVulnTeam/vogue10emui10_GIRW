

#ifndef __HMAC_MGMT_CLASSIFIER_H__
#define __HMAC_MGMT_CLASSIFIER_H__

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif


/*****************************************************************************
  1 ����ͷ�ļ�����
*****************************************************************************/
#include "frw_ext_if.h"
#include "dmac_ext_if.h"
#include "hmac_vap.h"

#undef  THIS_FILE_ID
#define THIS_FILE_ID OAM_FILE_ID_HMAC_MGMT_CLASSIFIER_H
/*****************************************************************************
  2 �궨��
*****************************************************************************/


/*****************************************************************************
  3 ö�ٶ���
*****************************************************************************/


/*****************************************************************************
  4 ȫ�ֱ�������
*****************************************************************************/


/*****************************************************************************
  5 ��Ϣͷ����
*****************************************************************************/


/*****************************************************************************
  6 ��Ϣ����
*****************************************************************************/


/*****************************************************************************
  7 STRUCT����
*****************************************************************************/


/*****************************************************************************
  8 UNION����
*****************************************************************************/


/*****************************************************************************
  9 OTHERS����
*****************************************************************************/


/*****************************************************************************
  10 ��������
*****************************************************************************/
#ifdef  _PRE_WLAN_FEATURE_11AX
extern oal_uint32  hmac_mgmt_tx_twt_action_etc(
                hmac_vap_stru              *pst_hmac_vap,
                hmac_user_stru             *pst_hmac_user,
                mac_twt_action_mgmt_args_stru  *pst_twt_action_args);
#endif


extern oal_uint32  hmac_rx_process_mgmt_event_etc(frw_event_mem_stru *pst_event_mem);
extern oal_uint32  hmac_mgmt_rx_delba_event_etc(frw_event_mem_stru *pst_event_mem);
extern oal_uint32  hmac_mgmt_tx_action_etc(
                hmac_vap_stru              *pst_hmac_vap,
                hmac_user_stru             *pst_hmac_user,
                mac_action_mgmt_args_stru  *pst_action_args);
extern oal_uint32  hmac_mgmt_tx_priv_req_etc(
                hmac_vap_stru              *pst_hmac_vap,
                hmac_user_stru             *pst_hmac_user,
                mac_priv_req_args_stru     *pst_priv_req);
extern oal_uint32  hmac_mgmt_tbtt_event_etc(frw_event_mem_stru *pst_event_mem);
extern oal_uint32 hmac_mgmt_send_disasoc_deauth_event_etc(frw_event_mem_stru *pst_event_mem);
extern oal_uint32  hmac_proc_disasoc_misc_event_etc(frw_event_mem_stru *pst_event_mem);

#ifdef _PRE_WLAN_FEATURE_ROAM
extern oal_uint32  hmac_proc_roam_trigger_event_etc(frw_event_mem_stru *pst_event_mem);
#endif  //_PRE_WLAN_FEATURE_ROAM

#ifdef __cplusplus
    #if __cplusplus
        }
    #endif
#endif

#endif /* end of hmac_mgmt_classifier.h */