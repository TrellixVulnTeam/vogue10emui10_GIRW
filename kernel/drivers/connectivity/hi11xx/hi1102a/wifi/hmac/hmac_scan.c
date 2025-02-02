


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif


/*****************************************************************************
  1 头文件包含
*****************************************************************************/
#include "oam_ext_if.h"
#include "mac_ie.h"
#include "mac_regdomain.h"
#include "mac_device.h"
#include "mac_resource.h"
#include "dmac_ext_if.h"
#include "hmac_fsm.h"
#include "hmac_sme_sta.h"
#include "hmac_resource.h"
#include "hmac_device.h"
#include "hmac_scan.h"
#include "hmac_mgmt_sta.h"
#include "frw_ext_if.h"
#ifdef _PRE_PLAT_FEATURE_CUSTOMIZE
#include "hisi_customize_wifi.h"
#endif
#ifdef _PRE_SUPPORT_ACS
#include "dmac_acs.h"
#include "hmac_acs.h"
#endif
#include "hmac_chan_mgmt.h"
#include "hmac_p2p.h"
#ifdef _PRE_WLAN_FEATURE_DFS
#include "hmac_dfs.h"
#endif

#ifdef _PRE_PLAT_FEATURE_CUSTOMIZE
#include "hisi_customize_wifi.h"
#endif /* #ifdef _PRE_PLAT_FEATURE_CUSTOMIZE */

#ifdef _PRE_WLAN_FEATURE_ROAM
#include "hmac_roam_main.h"
#endif //_PRE_WLAN_FEATURE_ROAM
#include "oal_main.h"

#ifdef _PRE_WLAN_FEATURE_SNIFFER
#include <hwnet/ipv4/sysctl_sniffer.h>
#endif
#undef  THIS_FILE_ID
#define THIS_FILE_ID OAM_FILE_ID_HMAC_SCAN_C

#define ROAM_RSSI_DIFF_6_DB               (6)

/*****************************************************************************
  2 全局变量定义
*****************************************************************************/
#if defined(_PRE_WLAN_CHIP_TEST_ALG) && (_PRE_OS_VERSION_LINUX == _PRE_OS_VERSION) && defined(_PRE_DEBUG_MODE)
struct kobject     *g_scan_ct_sys_kobject = OAL_PTR_NULL;
#endif
hmac_scan_state_enum_uint8 g_en_bgscan_enable_flag = HMAC_BGSCAN_ENABLE;
oal_uint32	g_pd_bss_expire_time = 0;
/*****************************************************************************
  3 函数实现
*****************************************************************************/
#define WLAN_INVALD_VHT_MCS     0xff
#define WLAN_GET_VHT_MAX_SUPPORT_MCS(_us_vht_mcs_map)   \
     ((3 == (_us_vht_mcs_map)) ? WLAN_INVALD_VHT_MCS:     \
      (2 == (_us_vht_mcs_map)) ? WLAN_VHT_MCS9:           \
      (1 == (_us_vht_mcs_map)) ? WLAN_VHT_MCS8: WLAN_VHT_MCS7)

#if defined(_PRE_WLAN_FEATURE_WPA) || defined(_PRE_WLAN_FEATURE_WPA2)
OAL_STATIC oal_bool_enum_uint8 hmac_parse_cipher_suit(mac_bss_dscr_stru *pst_bss_dscr, oal_uint8 uc_cipher_type,
                                                      oal_uint8 *puc_ie, oal_int32 l_len);
#endif

#if defined(_PRE_WLAN_CHIP_TEST_ALG) && (_PRE_OS_VERSION_LINUX == _PRE_OS_VERSION) && defined(_PRE_DEBUG_MODE)
OAL_STATIC oal_ssize_t  hmac_scan_ct_proc_read(struct kobject *dev, struct kobj_attribute *attr, char *buf);
OAL_STATIC struct kobj_attribute dev_attr_alg_test_result =
    __ATTR(alg_test_result, OAL_S_IRUGO|OAL_S_IWUSR, hmac_scan_ct_proc_read, OAL_PTR_NULL);


OAL_STATIC oal_ssize_t hmac_scan_ct_proc_read(struct kobject *dev, struct kobj_attribute *attr, char *buf)

{
    hmac_device_stru                *pst_hmac_device;
    hmac_scan_record_stru           *pst_record;
    oal_int32                        l_len;

    /* 获取hmac device和扫描运行记录 */
    pst_hmac_device = hmac_res_get_mac_dev(0);
    pst_record       = &(pst_hmac_device->st_scan_mgmt.st_scan_record_mgmt);

    l_len = OAL_SIZEOF(mac_scan_chan_stats_stru);

    oal_memcopy(buf, &(pst_record->ast_chan_results[0]), l_len);

    return l_len;
}


oal_int32  hmac_scan_ct_init(oal_void)
{
    /* hi1102-cb add sys for 51/02 */
#if (_PRE_OS_VERSION_LINUX == _PRE_OS_VERSION)
    oal_int32           l_ret = OAL_SUCC;

    if(OAL_PTR_NULL == g_scan_ct_sys_kobject)
    {
        g_scan_ct_sys_kobject = kobject_create_and_add("scan_ct", OAL_PTR_NULL);
        l_ret = sysfs_create_file(g_scan_ct_sys_kobject, &dev_attr_alg_test_result.attr);
    }

    return l_ret;
#else
    return OAL_SUCC;
#endif
}


oal_void  hmac_scan_ct_exit(oal_void)
{
    if(OAL_PTR_NULL != g_scan_ct_sys_kobject)
    {
        sysfs_remove_file(g_scan_ct_sys_kobject,&dev_attr_alg_test_result.attr);
        kobject_del(g_scan_ct_sys_kobject);
        g_scan_ct_sys_kobject = OAL_PTR_NULL;
    }
}
#endif

OAL_STATIC oal_void hmac_scan_print_scan_params(mac_scan_req_stru *pst_scan_params, mac_vap_stru *pst_mac_vap)
{
    OAM_WARNING_LOG4(pst_scan_params->uc_vap_id, OAM_SF_SCAN, "hmac_scan_print_scan_params::Now Scan channel_num[%d] in [%d]ms with scan_func[0x%x], and ssid_num[%d]!",
                pst_scan_params->uc_channel_nums,
                pst_scan_params->us_scan_time,
                pst_scan_params->uc_scan_func,
                pst_scan_params->uc_ssid_num);

    OAM_WARNING_LOG3(pst_scan_params->uc_vap_id, OAM_SF_SCAN, "hmac_scan_print_scan_params::Scan param, p2p_scan[%d], max_scan_count_per_channel[%d], need back home_channel[%d]!",
            pst_scan_params->bit_is_p2p0_scan,
            pst_scan_params->uc_max_scan_count_per_channel,
            pst_scan_params->en_need_switch_back_home_channel);
    return;
}

OAL_STATIC oal_void hmac_wifi_hide_ssid(oal_uint8 *puc_frame_body, oal_uint16 us_mac_frame_len)
{
    oal_uint8   *puc_ssid_ie;
    oal_uint8    uc_ssid_len = 0;
    oal_uint8    uc_index = 0;

    if (puc_frame_body == OAL_PTR_NULL)
    {
        return;
    }

    puc_ssid_ie = mac_get_ssid(puc_frame_body, us_mac_frame_len, &uc_ssid_len);

    /* 保留4位，如果不足4位，则不隐藏 */
    if (OAL_PTR_NULL ==  puc_ssid_ie || uc_ssid_len < 4 || uc_ssid_len > WLAN_SSID_MAX_LEN)
    {
        return;
    }

    for (uc_index = 2; uc_index < uc_ssid_len - 2; uc_index++) //保留前两位和后两位
    {
        *(puc_ssid_ie + uc_index) = 0x78; //0x78转换为ASCII码就是字符'x'
    }
}


oal_void hmac_scan_print_scanned_bss_info(oal_uint8 uc_device_id)
{
    hmac_device_stru        *pst_hmac_device;
    hmac_bss_mgmt_stru      *pst_bss_mgmt;
    hmac_scanned_bss_info   *pst_scanned_bss;
    mac_bss_dscr_stru       *pst_bss_dscr;
    oal_dlist_head_stru     *pst_entry;
    mac_ieee80211_frame_stru   *pst_frame_hdr;
    oal_uint8                   auc_sdt_parse_hdr[MAC_80211_FRAME_LEN];
    oal_uint8               *puc_tmp_mac_body_addr;
    oal_uint8               uc_frame_sub_type;


    /* 获取hmac device */
    pst_hmac_device = hmac_res_get_mac_dev(uc_device_id);
    if (OAL_PTR_NULL == pst_hmac_device)
    {
        OAM_WARNING_LOG0(0, OAM_SF_SCAN, "{hmac_scan_print_scanned_bss_info::pst_hmac_device null.}");
        return;
    }

    /* 获取指向扫描结果的管理结构体地址 */
    pst_bss_mgmt = &(pst_hmac_device->st_scan_mgmt.st_scan_record_mgmt.st_bss_mgmt);

    /* 获取锁 */
    oal_spin_lock(&(pst_bss_mgmt->st_lock));

    /* 遍历扫描到的bss信息 */
    OAL_DLIST_SEARCH_FOR_EACH(pst_entry, &(pst_bss_mgmt->st_bss_list_head))
    {
        pst_scanned_bss = OAL_DLIST_GET_ENTRY(pst_entry, hmac_scanned_bss_info, st_dlist_head);
        pst_bss_dscr    = &(pst_scanned_bss->st_bss_dscr_info);

        /* 仅显示新申请到的BSS帧 */
        if (OAL_TRUE == pst_scanned_bss->st_bss_dscr_info.en_new_scan_bss)
        {
            pst_scanned_bss->st_bss_dscr_info.en_new_scan_bss = OAL_FALSE;
            /*上报beacon和probe帧*/
            pst_frame_hdr  = (mac_ieee80211_frame_stru *)pst_bss_dscr->auc_mgmt_buff;

            /* 将beacon中duration字段(2字节)复用为rssi以及channel,方便SDT显示 */
            oal_memcopy((oal_uint8 *)auc_sdt_parse_hdr, (oal_uint8 *)pst_frame_hdr, MAC_80211_FRAME_LEN);
            auc_sdt_parse_hdr[2]  = (oal_uint8)pst_bss_dscr->c_rssi;
            auc_sdt_parse_hdr[3]  = pst_bss_dscr->st_channel.uc_chan_number;

            puc_tmp_mac_body_addr = (oal_uint8 *)oal_memalloc(pst_bss_dscr->ul_mgmt_len - MAC_80211_FRAME_LEN);

            if (OAL_UNLIKELY(OAL_PTR_NULL == puc_tmp_mac_body_addr))
            {
                OAM_WARNING_LOG0(0, OAM_SF_SCAN, "{hmac_scan_print_scanned_bss_info::alloc memory failed for storing tmp mac_frame_body.}");
                continue;
            }

            oal_memcopy(puc_tmp_mac_body_addr, (oal_uint8 *)(pst_bss_dscr->auc_mgmt_buff + MAC_80211_FRAME_LEN), pst_bss_dscr->ul_mgmt_len - MAC_80211_FRAME_LEN);

            uc_frame_sub_type = mac_get_frame_type_and_subtype((oal_uint8 *)pst_frame_hdr);
            if ((WLAN_FC0_SUBTYPE_BEACON == uc_frame_sub_type) ||
                (WLAN_FC0_SUBTYPE_PROBE_RSP == uc_frame_sub_type))
            {
                hmac_wifi_hide_ssid(puc_tmp_mac_body_addr, pst_bss_dscr->ul_mgmt_len - MAC_80211_FRAME_LEN);
            }

            /* 上报beacon帧或者probe rsp帧 */
            /*lint -e416*/
            oam_report_80211_frame(BROADCAST_MACADDR,
                                   (oal_uint8 *)auc_sdt_parse_hdr,
                                   MAC_80211_FRAME_LEN,
                                   puc_tmp_mac_body_addr,
                                   (oal_uint16) pst_bss_dscr->ul_mgmt_len,
                                   OAM_OTA_FRAME_DIRECTION_TYPE_RX);

            oal_free(puc_tmp_mac_body_addr);
            /*lint +e416*/
#ifdef _PRE_WLAN_FEATURE_SNIFFER
            proc_sniffer_write_file(NULL, 0, pst_bss_dscr->auc_mgmt_buff, (oal_uint16) pst_bss_dscr->ul_mgmt_len, 0);
#endif
        }
    }

    /* 解除锁 */
    oal_spin_unlock(&(pst_bss_mgmt->st_lock));

    return;
}




OAL_STATIC hmac_scanned_bss_info *hmac_scan_alloc_scanned_bss(oal_uint32 ul_mgmt_len)
{
    hmac_scanned_bss_info    *pst_scanned_bss;

    /* 申请内存，存储扫描到的bss信息 */
    pst_scanned_bss = oal_memalloc(OAL_SIZEOF(hmac_scanned_bss_info) + ul_mgmt_len - OAL_SIZEOF(pst_scanned_bss->st_bss_dscr_info.auc_mgmt_buff));
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_scanned_bss))
    {
        OAM_WARNING_LOG0(0, OAM_SF_SCAN, "{hmac_scan_alloc_scanned_bss::alloc memory failed for storing scanned result.}");
        return OAL_PTR_NULL;
    }

    /* 为申请的内存清零 */
    OAL_MEMZERO(pst_scanned_bss, OAL_SIZEOF(hmac_scanned_bss_info) + ul_mgmt_len - OAL_SIZEOF(pst_scanned_bss->st_bss_dscr_info.auc_mgmt_buff));

    /* 初始化链表头节点指针 */
    oal_dlist_init_head(&(pst_scanned_bss->st_dlist_head));



    return pst_scanned_bss;
}


OAL_STATIC oal_uint32 hmac_scan_add_bss_to_list(hmac_scanned_bss_info *pst_scanned_bss, hmac_device_stru *pst_hmac_device)
{
    hmac_bss_mgmt_stru  *pst_bss_mgmt;      /* 管理扫描结果的结构体 */

    pst_bss_mgmt = &(pst_hmac_device->st_scan_mgmt.st_scan_record_mgmt.st_bss_mgmt);
    pst_scanned_bss->st_bss_dscr_info.en_new_scan_bss = OAL_TRUE;

    /* 对链表写操作前加锁 */
    oal_spin_lock(&(pst_bss_mgmt->st_lock));

    /* 添加扫描结果到链表中，并更新扫描到的bss计数 */
    oal_dlist_add_tail(&(pst_scanned_bss->st_dlist_head), &(pst_bss_mgmt->st_bss_list_head));

    pst_bss_mgmt->ul_bss_num++;
    /* 解锁 */
    oal_spin_unlock(&(pst_bss_mgmt->st_lock));

    return OAL_SUCC;
}


OAL_STATIC oal_uint32 hmac_scan_del_bss_from_list_nolock(hmac_scanned_bss_info *pst_scanned_bss, hmac_device_stru *pst_hmac_device)
{
    hmac_bss_mgmt_stru  *pst_bss_mgmt;      /* 管理扫描结果的结构体 */

    pst_bss_mgmt = &(pst_hmac_device->st_scan_mgmt.st_scan_record_mgmt.st_bss_mgmt);

    /* 从链表中删除节点，并更新扫描到的bss计数 */
    oal_dlist_delete_entry(&(pst_scanned_bss->st_dlist_head));

    pst_bss_mgmt->ul_bss_num--;

    return OAL_SUCC;
}


oal_void hmac_scan_clean_scan_record(hmac_scan_record_stru  *pst_scan_record)
{
    oal_dlist_head_stru             *pst_entry;
    hmac_scanned_bss_info           *pst_scanned_bss;
    hmac_bss_mgmt_stru              *pst_bss_mgmt;

    /* 参数合法性检查 */
    if (OAL_PTR_NULL == pst_scan_record)
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{hmac_scan_clean_scan_record::pst_scan_record is null.}");
        return;
    }

    /* 1.一定要先清除扫描到的bss信息，再进行清零处理 */
    pst_bss_mgmt = &(pst_scan_record->st_bss_mgmt);

    /* 对链表写操作前加锁 */
    oal_spin_lock(&(pst_bss_mgmt->st_lock));

    /* 遍历链表，删除扫描到的bss信息 */
    while(OAL_FALSE == oal_dlist_is_empty(&(pst_bss_mgmt->st_bss_list_head)))
    {
        pst_entry       = oal_dlist_delete_head(&(pst_bss_mgmt->st_bss_list_head));
        pst_scanned_bss = OAL_DLIST_GET_ENTRY(pst_entry, hmac_scanned_bss_info, st_dlist_head);

        pst_bss_mgmt->ul_bss_num--;

        /* 释放扫描队列里的内存 */
        oal_free(pst_scanned_bss);
    }

    /* 对链表写操作前加锁 */
    oal_spin_unlock(&(pst_bss_mgmt->st_lock));

    /* 2.其它信息清零 */
    OAL_MEMZERO(pst_scan_record, OAL_SIZEOF(hmac_scan_record_stru));
    pst_scan_record->en_scan_rsp_status = MAC_SCAN_STATUS_BUTT;     /* 初始化扫描完成时状态码为无效值 */
    pst_scan_record->en_vap_last_state  = MAC_VAP_STATE_BUTT;       /* 必须置BUTT,否则aput停扫描会vap状态恢复错 */

    /* 3.重新初始化bss管理结果链表和锁 */
    pst_bss_mgmt = &(pst_scan_record->st_bss_mgmt);
    oal_dlist_init_head(&(pst_bss_mgmt->st_bss_list_head));
    oal_spin_lock_init(&(pst_bss_mgmt->st_lock));

    OAM_INFO_LOG0(0, OAM_SF_SCAN, "{hmac_scan_clean_scan_record::cleaned scan record success.}");

    return;
}



OAL_STATIC oal_int32 hmac_is_connected_ap_bssid(oal_uint8 uc_device_id, oal_uint8 auc_bssid[WLAN_MAC_ADDR_LEN])
{
    oal_uint8                    uc_vap_idx;
    mac_vap_stru                *pst_mac_vap;
    mac_device_stru             *pst_mac_device;

    pst_mac_device = mac_res_get_dev(uc_device_id);
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_mac_device))
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{hmac_is_connected_ap_bssid::mac_res_get_dev return null.}");
        return OAL_FALSE;
    }

    for (uc_vap_idx = 0; uc_vap_idx < pst_mac_device->uc_vap_num; uc_vap_idx++)
    {
        pst_mac_vap = mac_res_get_mac_vap(pst_mac_device->auc_vap_id[uc_vap_idx]);
        if (OAL_UNLIKELY(OAL_PTR_NULL == pst_mac_vap))
        {
            OAM_WARNING_LOG1(0, OAM_SF_P2P, "{hmac_is_connected_ap_bssid::mac_res_get_mac_vap fail! vap id is %d}",
                            pst_mac_device->auc_vap_id[uc_vap_idx]);
            continue;
        }

        if(IS_LEGACY_VAP(pst_mac_vap)
           && (MAC_VAP_STATE_UP == pst_mac_vap->en_vap_state))
        {
            if(0 == oal_memcmp(auc_bssid, pst_mac_vap->auc_bssid, WLAN_MAC_ADDR_LEN))
            {
                /* 不老化当前关联的AP */
                OAM_INFO_LOG3(pst_mac_vap->uc_vap_id, OAM_SF_SCAN, "{hmac_is_connected_ap_bssid::connected AP bssid:%02X:XX:XX:XX:%02X:%02X}",
                                 auc_bssid[0], auc_bssid[4], auc_bssid[5]);

                return OAL_TRUE;
            }
        }

    }

    return OAL_FALSE;
}


OAL_STATIC oal_void hmac_scan_clean_expire_scanned_bss(hmac_vap_stru *pst_hmac_vap, hmac_scan_record_stru  *pst_scan_record)
{
    oal_dlist_head_stru             *pst_entry;
    oal_dlist_head_stru             *pst_entry_tmp = OAL_PTR_NULL;
    hmac_bss_mgmt_stru              *pst_bss_mgmt;
    hmac_scanned_bss_info           *pst_scanned_bss;
    mac_bss_dscr_stru               *pst_bss_dscr;
    oal_uint32                       ul_curr_time_stamp;

    /* 参数合法性检查 */
    if (OAL_PTR_NULL == pst_scan_record)
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{hmac_scan_clean_expire_scanned_bss::scan record is null.}");
        return;
    }

    /* 管理扫描的bss结果的结构体 */
    pst_bss_mgmt = &(pst_scan_record->st_bss_mgmt);

    ul_curr_time_stamp = (oal_uint32)OAL_TIME_GET_STAMP_MS();

    /* 对链表写操作前加锁 */
    oal_spin_lock(&(pst_bss_mgmt->st_lock));

    /* 遍历链表，删除上一次扫描结果中到期的bss信息 */
    OAL_DLIST_SEARCH_FOR_EACH_SAFE(pst_entry, pst_entry_tmp, &(pst_bss_mgmt->st_bss_list_head))
    {
        pst_scanned_bss = OAL_DLIST_GET_ENTRY(pst_entry, hmac_scanned_bss_info, st_dlist_head);
        pst_bss_dscr    = &(pst_scanned_bss->st_bss_dscr_info);

        if (oal_time_after32(ul_curr_time_stamp, (pst_bss_dscr->ul_timestamp + HMAC_SCAN_MAX_SCANNED_BSS_EXPIRE)) == OAL_FALSE)
        {
            OAM_INFO_LOG0(0, OAM_SF_SCAN,
                             "{hmac_scan_clean_expire_scanned_bss::do not remove the BSS, because it has not expired.}");
            continue;
        }
        /* 产线老化使能*/
        if ((0 != g_pd_bss_expire_time) && (ul_curr_time_stamp - pst_bss_dscr->ul_timestamp < g_pd_bss_expire_time * 1000))
        {
            continue;
        }

        /* 不老化当前正在关联的AP */
        if (hmac_is_connected_ap_bssid(pst_scan_record->uc_device_id, pst_bss_dscr->auc_bssid))
        {
            pst_bss_dscr->c_rssi = pst_hmac_vap->station_info.signal;
            continue;
        }

        /* 从链表中删除节点，并更新扫描到的bss计数 */
        oal_dlist_delete_entry(&(pst_scanned_bss->st_dlist_head));
        pst_bss_mgmt->ul_bss_num--;

        /* 释放对应内存 */
        oal_free(pst_scanned_bss);
    }

    /* 对链表写操作前加锁 */
    oal_spin_unlock(&(pst_bss_mgmt->st_lock));
    return;
}




mac_bss_dscr_stru *hmac_scan_find_scanned_bss_dscr_by_index(oal_uint8  uc_device_id,
                                                                         oal_uint32 ul_bss_index)
{
    oal_dlist_head_stru             *pst_entry;
    hmac_scanned_bss_info           *pst_scanned_bss;
    hmac_device_stru                *pst_hmac_device;
    hmac_bss_mgmt_stru              *pst_bss_mgmt;
    oal_uint8                        ul_loop;

    /* 获取hmac device 结构 */
    pst_hmac_device = hmac_res_get_mac_dev(uc_device_id);
    if (OAL_PTR_NULL == pst_hmac_device)
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{hmac_scan_find_scanned_bss_by_index::pst_hmac_device is null.}");
        return OAL_PTR_NULL;
    }

    pst_bss_mgmt = &(pst_hmac_device->st_scan_mgmt.st_scan_record_mgmt.st_bss_mgmt);

    /* 对链表删操作前加锁 */
    oal_spin_lock(&(pst_bss_mgmt->st_lock));

    /* 如果索引大于总共扫描的bss个数，返回异常 */
    if (ul_bss_index >= pst_bss_mgmt->ul_bss_num)
    {
        OAM_WARNING_LOG0(0, OAM_SF_SCAN, "{hmac_scan_find_scanned_bss_by_index::no such bss in bss list!}");

        /* 解锁 */
        oal_spin_unlock(&(pst_bss_mgmt->st_lock));
        return OAL_PTR_NULL;
    }

    ul_loop = 0;
    /* 遍历链表，返回对应index的bss dscr信息 */
    OAL_DLIST_SEARCH_FOR_EACH(pst_entry, &(pst_bss_mgmt->st_bss_list_head))
    {
        pst_scanned_bss = OAL_DLIST_GET_ENTRY(pst_entry, hmac_scanned_bss_info, st_dlist_head);

        /* 相同的bss index返回 */
        if (ul_bss_index == ul_loop)
        {
            /* 解锁 */
            oal_spin_unlock(&(pst_bss_mgmt->st_lock));
            return &(pst_scanned_bss->st_bss_dscr_info);
        }

        ul_loop++;
    }
    /* 解锁 */
    oal_spin_unlock(&(pst_bss_mgmt->st_lock));

    return OAL_PTR_NULL;
}


hmac_scanned_bss_info *hmac_scan_find_scanned_bss_by_bssid(hmac_bss_mgmt_stru *pst_bss_mgmt, oal_uint8 *puc_bssid)
{
    oal_dlist_head_stru             *pst_entry;
    hmac_scanned_bss_info           *pst_scanned_bss;

    /* 遍历链表，查找链表中是否已经存在相同bssid的bss信息 */
    OAL_DLIST_SEARCH_FOR_EACH(pst_entry, &(pst_bss_mgmt->st_bss_list_head))
    {
        pst_scanned_bss = OAL_DLIST_GET_ENTRY(pst_entry, hmac_scanned_bss_info, st_dlist_head);

        /* 相同的bssid地址 */
        if (0 == oal_compare_mac_addr(pst_scanned_bss->st_bss_dscr_info.auc_bssid, puc_bssid))
        {
            return pst_scanned_bss;
        }
    }

    return OAL_PTR_NULL;
}

oal_void *hmac_scan_get_scanned_bss_by_bssid(mac_vap_stru *pst_mac_vap, oal_uint8 *puc_mac_addr)
{
    hmac_bss_mgmt_stru             *pst_bss_mgmt;          /* 管理扫描的bss结果的结构体 */
    hmac_scanned_bss_info          *pst_scanned_bss_info;
    hmac_device_stru               *pst_hmac_device;

    /* 获取hmac device 结构 */
    pst_hmac_device = hmac_res_get_mac_dev(pst_mac_vap->uc_device_id);
    if (OAL_PTR_NULL == pst_hmac_device)
    {
        OAM_WARNING_LOG1(0, OAM_SF_SCAN, "{hmac_scan_get_scanned_bss_by_bssid::pst_hmac_device is null, dev id[%d].}", pst_mac_vap->uc_device_id);
        return OAL_PTR_NULL;
    }

    /* 获取管理扫描的bss结果的结构体 */
    pst_bss_mgmt = &(pst_hmac_device->st_scan_mgmt.st_scan_record_mgmt.st_bss_mgmt);

    oal_spin_lock(&(pst_bss_mgmt->st_lock));

    pst_scanned_bss_info = hmac_scan_find_scanned_bss_by_bssid(pst_bss_mgmt, puc_mac_addr);
    if (OAL_PTR_NULL == pst_scanned_bss_info)
    {
       OAM_WARNING_LOG4(pst_mac_vap->uc_vap_id, OAM_SF_SCAN,
                        "{hmac_scan_get_scanned_bss_by_bssid::find the bss failed[%02X:XX:XX:%02X:%02X:%02X]}",
                        puc_mac_addr[0], puc_mac_addr[3], puc_mac_addr[4], puc_mac_addr[5]);

       /* 解锁 */
       oal_spin_unlock(&(pst_bss_mgmt->st_lock));
       return OAL_PTR_NULL;
    }

    /* 解锁 */
    oal_spin_unlock(&(pst_bss_mgmt->st_lock));

    return &(pst_scanned_bss_info->st_bss_dscr_info);
}


#if defined (_PRE_WLAN_FEATURE_WPA2)

OAL_STATIC oal_bool_enum_uint8 hmac_scan_update_bss_list_rsn(mac_bss_dscr_stru *pst_bss_dscr, oal_uint8 *puc_ie)
{
    oal_uint8                    uc_index               = 0;
    oal_uint16                   us_ver                 = 0;
    oal_int32                    l_ie_len_left         = 0;

    /*************************************************************************/
    /*                  RSN Element Format                                   */
    /* --------------------------------------------------------------------- */
    /* |Element ID | Length | Version | Group Cipher Suite | Pairwise Cipher */
    /* --------------------------------------------------------------------- */
    /* |     1     |    1   |    2    |         4          |       2         */
    /* --------------------------------------------------------------------- */
    /* --------------------------------------------------------------------- */
    /* Suite Count| Pairwise Cipher Suite List | AKM Suite Count | AKM Suite List */
    /* --------------------------------------------------------------------- */
    /*            |         4*m                |     2           |   4*n     */
    /* --------------------------------------------------------------------- */
    /* --------------------------------------------------------------------- */
    /* |RSN Capabilities|PMKID Count|PMKID List|Group Management Cipher Suite*/
    /* --------------------------------------------------------------------- */
    /* |        2       |    2      |   16 *s  |               4           | */
    /* --------------------------------------------------------------------- */
    /*                                                                       */
    /*************************************************************************/


    /* RSN IE长度,不包括Element ID 和 Length字段 */
    l_ie_len_left = puc_ie[1];

    /* 忽略 RSN IE 和 IE 长度 */
    uc_index += 2;

    /* 获取RSN 版本号,2个字节 */
    if(l_ie_len_left > 2)
    {
        us_ver = OAL_MAKE_WORD16(puc_ie[uc_index], puc_ie[uc_index + 1]);
        if (MAC_RSN_IE_VERSION != us_ver)
        {
            OAM_WARNING_LOG1(0, OAM_SF_SCAN, "{hmac_scan_update_bss_list_rsn::invalid us_ver[%d].}", us_ver);

            return OAL_FALSE;
        }

        /* 忽略 RSN 版本号长度,2个字节 */
        uc_index += 2;
        l_ie_len_left -= 2;
    }
    else
    {
        OAM_WARNING_LOG1(0, OAM_SF_SCAN, "{hmac_scan_update_bss_list_rsn::ie too short for version:%d.}", l_ie_len_left);
        return OAL_FALSE;
    }

    /* 设置 RSNA */
    pst_bss_dscr->st_bss_sec_info.uc_bss_80211i_mode |= DMAC_RSNA_802_11I;

    return hmac_parse_cipher_suit(pst_bss_dscr, DMAC_RSNA_802_11I, puc_ie + uc_index, l_ie_len_left);
}
#endif

#if defined (_PRE_WLAN_FEATURE_WPA)

OAL_STATIC oal_bool_enum_uint8 hmac_scan_update_bss_list_wpa(mac_bss_dscr_stru *pst_bss_dscr, oal_uint8 *puc_ie)
{
    oal_uint8                    uc_index               = 0;
    oal_uint16                   us_ver                 = 0;
    oal_int32                    l_ie_len_left          = 0;

    /*************************************************************************/
    /*                  WPA Element Format                                   */
    /* --------------------------------------------------------------------- */
    /* |Element ID | Length |    WPA OUI    |  Version |  Group Cipher Suite */
    /* --------------------------------------------------------------------- */
    /* |     1     |   1    |        4      |     2    |         4           */
    /* --------------------------------------------------------------------- */
    /* --------------------------------------------------------------------- */
    /* Pairwise Cipher |  Pairwise Cipher   |                 |              */
    /* Suite Count     |    Suite List      | AKM Suite Count |AKM Suite List*/
    /* --------------------------------------------------------------------- */
    /*        2        |          4*m       |         2       |     4*n      */
    /* --------------------------------------------------------------------- */
    /*                                                                       */
    /*************************************************************************/

    /* WPA IE长度,不包括Element ID 和 Length字段 */
    l_ie_len_left = puc_ie[1];
    /* 最短WPA OUI(4 字节) */
    if(l_ie_len_left < 4)
    {
        OAM_WARNING_LOG1(0, OAM_SF_SCAN, "{hmac_scan_update_bss_list_wpa::invalid WPA IE LEN:%d.}", l_ie_len_left);
        return OAL_FALSE;
    }
    /* 忽略 WPA OUI(4 字节)  */
    l_ie_len_left -= 4;

    /* 忽略 WPA IE(1 字节) ，IE 长度(1 字节) ，WPA OUI(4 字节)  */
    uc_index = 2 + 4;

    /* 获取版本号,2个字节 */
    if(l_ie_len_left > 2)
    {
        us_ver = OAL_MAKE_WORD16(puc_ie[uc_index], puc_ie[uc_index + 1]);
        /* 对比WPA 版本信息 */
        if (MAC_WPA_IE_VERSION != us_ver)
        {
            OAM_WARNING_LOG1(0, OAM_SF_SCAN, "{hmac_scan_update_bss_list_wpa::invalid WPA version[%d].}", us_ver);

            return OAL_FALSE;
        }

        /* 忽略 版本号 长度 */
        uc_index += 2;
        l_ie_len_left -= 2;
    }
    else
    {
        OAM_WARNING_LOG1(0, OAM_SF_SCAN, "{hmac_scan_update_bss_list_wpa::ie too short for version:%d.}", l_ie_len_left);
        return OAL_FALSE;
    }

    /* 设置 WPA */
    pst_bss_dscr->st_bss_sec_info.uc_bss_80211i_mode |= DMAC_WPA_802_11I;

    return hmac_parse_cipher_suit(pst_bss_dscr, DMAC_WPA_802_11I, puc_ie + uc_index, l_ie_len_left);
}
#endif

#ifdef _PRE_WLAN_NARROW_BAND

OAL_STATIC oal_void  hmac_scan_update_bss_list_nb(mac_bss_dscr_stru   *pst_bss_dscr, oal_uint8 *puc_frame_body, oal_uint16 us_frame_len)
{
    oal_uint8 *puc_ie;

    /* 入参合法判断 */
    if (OAL_UNLIKELY((OAL_PTR_NULL == pst_bss_dscr) || (OAL_PTR_NULL == puc_frame_body)))
    {
        OAM_ERROR_LOG0(0, OAM_SF_ANY, "{hmac_scan_update_bss_list_nb: input pointer is null!}");
        return;
    }

    puc_ie = mac_find_vendor_ie(MAC_HUAWEI_VENDER_IE, MAC_HISI_NB_IE, puc_frame_body, us_frame_len);

    /* 判断是否携带该IE */
    if (OAL_PTR_NULL == puc_ie)
    {
        pst_bss_dscr->en_nb_capable = OAL_FALSE;
    }
    else
    {
        pst_bss_dscr->en_nb_capable = OAL_TRUE;
    }
}
#endif

#if defined(_PRE_WLAN_FEATURE_WPA) || defined(_PRE_WLAN_FEATURE_WPA2)


OAL_STATIC oal_bool_enum_uint8 hmac_parse_cipher_suit(mac_bss_dscr_stru *pst_bss_dscr, oal_uint8 uc_cipher_type,
                                                      oal_uint8 *puc_ie, oal_int32 l_len)
{
    oal_uint8                    uc_index               = 0;
    oal_uint16                   us_temp                = 0;
    oal_uint16                   us_pcip_num            = 0;
    oal_uint16                   us_auth_num            = 0;
    oal_uint16                   us_suite_count         = 0;
    oal_int32                    l_ie_len_left          = l_len;

    OAL_CONST oal_uint8         *puc_oui;
    oal_uint8                   *puc_pcip_policy        = OAL_PTR_NULL;
    oal_uint8                   *puc_grp_policy         = OAL_PTR_NULL;
    oal_uint8                   *puc_auth_policy        = OAL_PTR_NULL;
    mac_bss_80211i_info_stru    *pst_bss_80211i_info    = &(pst_bss_dscr->st_bss_sec_info);

    /*************************************************************************/
    /*                  WPA / RSN Element Format                             */
    /* --------------------------------------------------------------------- */
    /* |Group Cipher Suite | Pairwise Cipher Suite Count */
    /* --------------------------------------------------------------------- */
    /* |        4          |       2                     */
    /* --------------------------------------------------------------------- */
    /* --------------------------------------------------------------------- */
    /* | Pairwise Cipher Suite List | AKM Suite Count | AKM Suite List */
    /* --------------------------------------------------------------------- */
    /* |         4*m                |     2           |   4*n          */
    /* --------------------------------------------------------------------- */
    /* --------------------------------------------------------------------- */
    /* |RSN Capabilities|*/
    /* --------------------------------------------------------------------- */
    /* |        2       |*/
    /* --------------------------------------------------------------------- */
    /*                                                                       */
    /*************************************************************************/

    /**解析WPA/RSN公共的cipher suit**/
    /**puc_ie指向Group Cipher Suite,l_len指puc_ie长度 **/
    if(DMAC_WPA_802_11I == uc_cipher_type)
    {
        puc_oui = g_auc_wpa_oui;
        puc_grp_policy  = &(pst_bss_80211i_info->uc_wpa_grp_policy);
        puc_pcip_policy = pst_bss_80211i_info->auc_wpa_pairwise_policy;
        puc_auth_policy = pst_bss_80211i_info->auc_wpa_auth_policy;
    }
    else if(DMAC_RSNA_802_11I == uc_cipher_type)
    {
        puc_oui = g_auc_rsn_oui;
        puc_grp_policy  = &(pst_bss_80211i_info->uc_rsn_grp_policy);
        puc_pcip_policy =   pst_bss_80211i_info->auc_rsn_pairwise_policy;
        puc_auth_policy =   pst_bss_80211i_info->auc_rsn_auth_policy;
    }
    else
    {
        OAM_WARNING_LOG0(0, OAM_SF_SCAN, "{hmac_parse_cipher_suit::invalid OUI.}");
        return OAL_FALSE;
    }

    oal_memset(puc_pcip_policy, 0xFF, MAC_PAIRWISE_CIPHER_SUITES_NUM);
    oal_memset(puc_auth_policy, 0xFF, MAC_AUTHENTICATION_SUITE_NUM);

    /* 获取组播密钥套件,4个字节 */
    if(l_ie_len_left > 4)
    {
        if (0 != oal_memcmp(puc_oui, puc_ie + uc_index, MAC_OUI_LEN))
        {
            OAM_WARNING_LOG0(0, OAM_SF_SCAN, "{hmac_parse_cipher_suit::invalid group OUI.}");
            return OAL_FALSE;
        }
        *puc_grp_policy = puc_ie[uc_index + MAC_OUI_LEN];

        /* 忽略组播密钥套件长度 */
        uc_index += 4;
        l_ie_len_left -= 4;
    }
    else
    {
        OAM_WARNING_LOG1(0, OAM_SF_SCAN, "{hmac_parse_cipher_suit::ie too short group policy:%d.}", l_ie_len_left);
        return OAL_FALSE;
    }

    /* 获取成对密钥套件个数,最少2个字节长度 */
    if(l_ie_len_left > 2)
    {
        us_pcip_num = OAL_MAKE_WORD16(puc_ie[uc_index], puc_ie[uc_index+1]) & 0xFF;
        uc_index += 2;
        l_ie_len_left -= 2;
    }
    else
    {
        OAM_WARNING_LOG1(0, OAM_SF_SCAN, "{hmac_parse_cipher_suit::ie too short for pairwise key mgmt:%d.}", l_ie_len_left);
        return OAL_FALSE;
    }

    /* 确认单播套件个数是否与实际ie长度相匹配 */
    if((0 == us_pcip_num) || (us_pcip_num > l_ie_len_left / 4))
    {
        OAM_WARNING_LOG2(0, OAM_SF_SCAN, "{hmac_parse_cipher_suit::ie count botch pairwise:%d,left:%d}",
                                           us_pcip_num, l_ie_len_left);
        return OAL_FALSE;
    }

    us_suite_count = 0;
    for (us_temp = 0; us_temp < us_pcip_num; us_temp++)
    {
        if (0 != oal_memcmp(puc_oui, puc_ie + uc_index, MAC_OUI_LEN))
        {
            OAM_WARNING_LOG0(0, OAM_SF_SCAN, "{hmac_parse_cipher_suit::invalid pairwise OUI,ignore this ie.}");
            /* 对于不识别的成对密钥套件，忽略保存 */
            uc_index += 4;
            l_ie_len_left -= 4;
            continue;
        }
        if(us_suite_count < MAC_PAIRWISE_CIPHER_SUITES_NUM)
        {
            /* 成对密钥套件个数驱动最大为2，超过则不再继续保存 */
            puc_pcip_policy[us_suite_count++] = puc_ie[uc_index + MAC_OUI_LEN];
        }
        else
        {
            OAM_WARNING_LOG1(0, OAM_SF_SCAN, "{hmac_parse_cipher_suit::us_suite_count reach max,ignore this ie,us_pcip_num:%d.}",
                             us_pcip_num);
        }

        uc_index += 4;
        l_ie_len_left -= 4;
    }


    /* 获取认证套件计数,最少2个字节长度 */
    if(l_ie_len_left > 2)
    {
        us_auth_num = OAL_MAKE_WORD16(puc_ie[uc_index], puc_ie[uc_index+1]) & 0xFF;
        uc_index += 2;
        l_ie_len_left -= 2;
    }
    else
    {
        OAM_WARNING_LOG1(0, OAM_SF_SCAN, "{hmac_parse_cipher_suit::ie too short for aka suite:%d.}", l_ie_len_left);
        return OAL_FALSE;
    }

    /* 确认认证套件个数是否与实际ie长度相匹配 */
    if((0 == us_auth_num) || (us_auth_num > l_ie_len_left / 4))
    {
        OAM_WARNING_LOG2(0, OAM_SF_SCAN, "{hmac_parse_cipher_suit::ie count botch aka:%d,left:%d}",
                                           us_auth_num, l_ie_len_left);
        return OAL_FALSE;
    }

    /* 获取认证类型 */
    us_suite_count = 0;
    for (us_temp = 0; us_temp < us_auth_num; us_temp++)
    {
        if (0 != oal_memcmp(puc_oui, puc_ie + uc_index, MAC_OUI_LEN))
        {
            OAM_WARNING_LOG0(0, OAM_SF_SCAN, "{hmac_parse_cipher_suit::invalid WPA auth OUI,ignore this ie.}");
            /* 对于不识别的AKM套件，忽略保存 */
            uc_index += 4;
            l_ie_len_left -= 4;
            continue;
        }

        if(us_suite_count < WLAN_AUTHENTICATION_SUITES)
        {
            /* AKM套件个数驱动最大为2，超过则不再继续保存 */
            puc_auth_policy[us_suite_count++] = puc_ie[uc_index + MAC_OUI_LEN];
        }
        else
        {
            OAM_WARNING_LOG1(0, OAM_SF_SCAN, "{hmac_parse_cipher_suit::us_suite_count reach max,ignore this ie,us_auth_num:%d.}",
                             us_auth_num);
        }
        uc_index += 4;
        l_ie_len_left -= 4;
    }

    /* 获取 RSN 能力,2个字节 */
    if(DMAC_RSNA_802_11I == uc_cipher_type)
    {
        if(l_ie_len_left >=2)
        {
            pst_bss_80211i_info->auc_rsn_cap[0] = *(puc_ie + uc_index++);
            pst_bss_80211i_info->auc_rsn_cap[1] = *(puc_ie + uc_index++);

            l_ie_len_left -= 2;
        }
        else
        {
            /* 能力信息不关注失败情况,不返回失败,wpa_supplicant在解析时也不关注,可以正常关联 */
            OAM_WARNING_LOG1(0, OAM_SF_SCAN, "{hmac_scan_update_bss_list_rsn::ie too short for rsn cap:%d.}", l_ie_len_left);
        }
    }

    return OAL_TRUE;
}



OAL_STATIC oal_void hmac_scan_update_bss_list_security(mac_bss_dscr_stru *pst_bss_dscr,
                                                                oal_uint8 *puc_frame_body,
                                                                oal_uint16 us_frame_len,
                                                                oal_uint16 us_offset)
{
    oal_uint8           *puc_ie;
    oal_bool_enum_uint8  en_ret;

    /* 安全相关信息元素 */
    /* 清空当前 bss_info 结构中的安全信息 */
    oal_memset(&(pst_bss_dscr->st_bss_sec_info), 0xff, sizeof(mac_bss_80211i_info_stru));
    pst_bss_dscr->st_bss_sec_info.uc_bss_80211i_mode = 0;
    pst_bss_dscr->st_bss_sec_info.auc_rsn_cap[0]     = 0;
    pst_bss_dscr->st_bss_sec_info.auc_rsn_cap[1]     = 0;

#if defined (_PRE_WLAN_FEATURE_WPA2)

    puc_ie = mac_find_ie(MAC_EID_RSN, puc_frame_body + us_offset, (oal_int32)(us_frame_len - us_offset));
    if (OAL_PTR_NULL != puc_ie)
    {
        /* 更新从beacon 中收到的 RSN 安全相关信息到 pst_bss_dscr 中 */
        en_ret = hmac_scan_update_bss_list_rsn(pst_bss_dscr, puc_ie);
        if (OAL_FALSE == en_ret)
        {
            /* 如果获取RSN 信息失败，则上报帧体信息到SDT */
            oam_report_80211_frame(BROADCAST_MACADDR,
                                   puc_frame_body - MAC_80211_FRAME_LEN,
                                   MAC_80211_FRAME_LEN,
                                   puc_frame_body,
                                   us_frame_len + MAC_80211_FRAME_LEN,
                                   OAM_OTA_FRAME_DIRECTION_TYPE_RX);
        }
    }
#endif

#if defined (_PRE_WLAN_FEATURE_WPA)

    puc_ie = mac_find_vendor_ie(MAC_WLAN_OUI_MICROSOFT, MAC_OUITYPE_WPA, puc_frame_body + us_offset, (oal_int32)(us_frame_len - us_offset));
    if (OAL_PTR_NULL != puc_ie)
    {
        /* 更新从beacon 中收到的 WPA 安全相关信息到 pst_bss_dscr 中 */
        en_ret = hmac_scan_update_bss_list_wpa(pst_bss_dscr, puc_ie);
        if (OAL_FALSE == en_ret)
        {
            /* 如果获取安全信息失败，则上报帧体信息到SDT */
            oam_report_80211_frame(BROADCAST_MACADDR,
                                   puc_frame_body - MAC_80211_FRAME_LEN,
                                   MAC_80211_FRAME_LEN,
                                   puc_frame_body,
                                   us_frame_len + MAC_80211_FRAME_LEN,
                                   OAM_OTA_FRAME_DIRECTION_TYPE_RX);
        }
    }
#endif
}
#endif /* defined(_PRE_WLAN_FEATURE_WPA) || defined(_PRE_WLAN_FEATURE_WPA2) */


OAL_STATIC OAL_INLINE oal_void  hmac_scan_update_bss_list_wmm(mac_bss_dscr_stru   *pst_bss_dscr,
                                                                         oal_uint8           *puc_frame_body,
                                                                         oal_uint16           us_frame_len)
{
    oal_uint8  *puc_ie;

    pst_bss_dscr->uc_wmm_cap   = OAL_FALSE;
    pst_bss_dscr->uc_uapsd_cap = OAL_FALSE;

    if (us_frame_len <= (MAC_TIME_STAMP_LEN + MAC_BEACON_INTERVAL_LEN + MAC_CAP_INFO_LEN))
    {
        OAM_WARNING_LOG1(0,OAM_SF_ASSOC,"{hmac_scan_update_bss_list_wmm::us_frame_len[%d]!}",us_frame_len);
        return;
    }

    us_frame_len   -= (MAC_TIME_STAMP_LEN + MAC_BEACON_INTERVAL_LEN + MAC_CAP_INFO_LEN);
    puc_frame_body += (MAC_TIME_STAMP_LEN + MAC_BEACON_INTERVAL_LEN + MAC_CAP_INFO_LEN);

    puc_ie = mac_get_wmm_ie(puc_frame_body, us_frame_len);
    if (OAL_PTR_NULL != puc_ie)
    {
         pst_bss_dscr->uc_wmm_cap = OAL_TRUE;

         /* --------------------------------------------------------------------------------- */
         /* WMM Information/Parameter Element Format                                          */
         /* ----------------------------------------------------------------------------------*/
         /* EID | IE LEN | OUI | OUIType | OUISubtype | Version | QoSInfo | OUISubtype based |*/
         /* --------------------------------------------------------------------------------- */
         /* 1   |   1    |  3  | 1       | 1          | 1       | 1       | ---------------- |*/
         /* --------------------------------------------------------------------------------- */
         /* puc_ie[1] IE len 不包含EID和LEN字段,获取QoSInfo，uc_ie_len必须大于7字节长度 */
         /* Check if Bit 7 is set indicating U-APSD capability */
         if ((puc_ie[1] >= 7)&&(puc_ie[8] & BIT7))  /* wmm ie的第8个字节是QoS info字节 */
         {
            pst_bss_dscr->uc_uapsd_cap = OAL_TRUE;
         }
    }
    else
    {
        puc_ie = mac_find_ie(MAC_EID_HT_CAP, puc_frame_body, us_frame_len);
        if (OAL_PTR_NULL != puc_ie)
        {
            pst_bss_dscr->uc_wmm_cap = OAL_TRUE;
        }
    }
}

#ifdef _PRE_WLAN_FEATURE_11D

OAL_STATIC oal_void  hmac_scan_update_bss_list_country(mac_bss_dscr_stru   *pst_bss_dscr,
                                                                oal_uint8           *puc_frame_body,
                                                                oal_uint16           us_frame_len)
{
    oal_uint8 *puc_ie;
    oal_uint8  uc_offset;

    uc_offset = MAC_TIME_STAMP_LEN + MAC_BEACON_INTERVAL_LEN + MAC_CAP_INFO_LEN;

    puc_ie = mac_find_ie(MAC_EID_COUNTRY, puc_frame_body + uc_offset, us_frame_len - uc_offset);

    /* 国家码不存在, 全部标记为0 */
    if (OAL_PTR_NULL == puc_ie)
    {
        pst_bss_dscr->ac_country[0] = 0;
        pst_bss_dscr->ac_country[1] = 0;
        pst_bss_dscr->ac_country[2] = 0;

        return;
    }

    /* 国家码采用2个字节,IE LEN必须大于等于2 */
    if(puc_ie[1] >= 2)
    {
        pst_bss_dscr->ac_country[0] = (oal_int8)puc_ie[MAC_IE_HDR_LEN];
        pst_bss_dscr->ac_country[1] = (oal_int8)puc_ie[MAC_IE_HDR_LEN + 1];
        pst_bss_dscr->ac_country[2] = 0;
    }
}
#endif

#ifdef _PRE_WLAN_FEATURE_FTM

OAL_STATIC oal_void hmac_scan_update_bss_list_rrm(mac_bss_dscr_stru *pst_bss_dscr, oal_uint8 *puc_frame_body, oal_uint16 us_frame_len)
{
    oal_uint8 *puc_ie;

    /* 入参合法判断 */
    if (OAL_UNLIKELY((OAL_PTR_NULL == pst_bss_dscr) || (OAL_PTR_NULL == puc_frame_body)))
    {
        OAM_ERROR_LOG0(0, OAM_SF_ANY, "{hmac_scan_update_bss_list_rrm: input pointer is null!}");
        return;
    }

    puc_ie = mac_find_ie(MAC_EID_RRM, puc_frame_body, us_frame_len);
    if(OAL_PTR_NULL == puc_ie)
    {
        pst_bss_dscr->en_support_rrm = OAL_FALSE;
    }
    else
    {
        pst_bss_dscr->en_support_rrm = OAL_TRUE;
    }
}
#endif


OAL_STATIC oal_void  hmac_scan_update_bss_list_11n(mac_bss_dscr_stru   *pst_bss_dscr,
                                                            oal_uint8           *puc_frame_body,
                                                            oal_uint16           us_frame_len,
                                                            oal_uint16           us_offset)
{
    oal_uint8               *puc_ie;
    mac_ht_opern_stru       *pst_ht_op;
    oal_uint8                uc_sec_chan_offset;
    wlan_bw_cap_enum_uint8   en_ht_cap_bw = WLAN_BW_CAP_20M;
    wlan_bw_cap_enum_uint8   en_ht_op_bw  = WLAN_BW_CAP_20M;

    /* 11n */
    puc_ie = mac_find_ie(MAC_EID_HT_CAP, puc_frame_body + us_offset, us_frame_len - us_offset);
    if ((OAL_PTR_NULL != puc_ie) && (puc_ie[1] >= 2) && (OAL_TRUE == hmac_is_ht_mcs_set_valid(puc_ie, pst_bss_dscr->st_channel.en_band)))
    {
        /* puc_ie[2]是HT Capabilities Info的第1个字节 */
        pst_bss_dscr->en_ht_capable = OAL_TRUE;     /* 支持ht */
        pst_bss_dscr->en_ht_ldpc = (puc_ie[2] & BIT0);           /* 支持ldpc */
        en_ht_cap_bw = ((puc_ie[2] & BIT1) >> 1);                /* 取出支持的带宽 */
        pst_bss_dscr->en_ht_stbc = ((puc_ie[2] & BIT7) >> 7);
    }

    /* 默认20M,如果帧内容未携带HT_OPERATION则可以直接采用默认值 */
    pst_bss_dscr->en_channel_bandwidth = WLAN_BAND_WIDTH_20M;

    puc_ie = mac_find_ie(MAC_EID_HT_OPERATION, puc_frame_body + us_offset, us_frame_len - us_offset);
    if ((OAL_PTR_NULL != puc_ie) && (puc_ie[1] >= 2))   //增加ie长度异常检查
    {
        pst_ht_op  = (mac_ht_opern_stru *)(puc_ie + MAC_IE_HDR_LEN);

        /* 提取次信道偏移 */
        uc_sec_chan_offset = pst_ht_op->bit_secondary_chan_offset;

        /* 防止ap的channel width=0, 但channel offset = 1或者3 此时以channel width为主 */
        /* ht cap 20/40 enabled && ht operation 40 enabled */
        if ((0 != pst_ht_op->bit_sta_chan_width) && (en_ht_cap_bw > WLAN_BW_CAP_20M)) //cap > 20M才取channel bw
        {
            if (MAC_SCB == uc_sec_chan_offset)
            {
                pst_bss_dscr->en_channel_bandwidth =  WLAN_BAND_WIDTH_40MINUS;
                en_ht_op_bw = WLAN_BW_CAP_40M;
            }
            else if (MAC_SCA == uc_sec_chan_offset)
            {
                pst_bss_dscr->en_channel_bandwidth =  WLAN_BAND_WIDTH_40PLUS;
                en_ht_op_bw = WLAN_BW_CAP_40M;
            }
        }
    }

    /* 将AP带宽能力取声明能力的最小值，防止AP异常发送超过带宽能力数据，造成数据不通 */
    pst_bss_dscr->en_bw_cap = OAL_MIN(en_ht_cap_bw, en_ht_op_bw);

    puc_ie = mac_find_ie(MAC_EID_EXT_CAPS, puc_frame_body + us_offset, us_frame_len - us_offset);
    if ((OAL_PTR_NULL != puc_ie) && (puc_ie[1] >= 1))
    {
        /* Extract 20/40 BSS Coexistence Management Support */
        pst_bss_dscr->uc_coex_mgmt_supp = (puc_ie[2] & BIT0);
    }
}


OAL_STATIC oal_void  hmac_scan_update_bss_list_11ac(mac_bss_dscr_stru   *pst_bss_dscr,
                                                             oal_uint8           *puc_frame_body,
                                                             oal_uint16           us_frame_len,
                                                             oal_uint16           us_offset,
                                                             oal_uint             en_is_vendor_ie)
{
    oal_uint8   *puc_ie;
    oal_uint8    uc_vht_chan_width;
    oal_uint8    uc_chan_center_freq;
    oal_uint8    uc_supp_ch_width;


    puc_ie = mac_find_ie(MAC_EID_VHT_CAP, puc_frame_body + us_offset, us_frame_len - us_offset);
    if ((OAL_PTR_NULL != puc_ie) && (puc_ie[1] >= MAC_VHT_CAP_IE_LEN))
    {
        pst_bss_dscr->en_vht_capable = OAL_TRUE;     /* 支持vht */
        /* 说明epigram vendor中携带VHT ie，则设置标志位，assoc req中也需携带vendor+vht ie */
        if(OAL_TRUE == en_is_vendor_ie)
        {
            pst_bss_dscr->en_epigram_vht_capable = OAL_TRUE;
        }

        /* 提取Supported Channel Width Set */
        uc_supp_ch_width = ((puc_ie[2] & (BIT3|BIT2)) >> 2);

        if (0 == uc_supp_ch_width)
        {
            pst_bss_dscr->en_bw_cap = WLAN_BW_CAP_80M;   /* 80MHz */
        }
        else if (1 == uc_supp_ch_width)
        {
            pst_bss_dscr->en_bw_cap = WLAN_BW_CAP_160M;  /* 160MHz */
        }
    }
    else
    {
        /* 私有epigram vendor中不包含vht ie，适配5g 20M 私有协议 */
        if(OAL_TRUE == en_is_vendor_ie)
        {
            pst_bss_dscr->en_epigram_novht_capable = OAL_TRUE;
        }
    }


    puc_ie = mac_find_ie(MAC_EID_VHT_OPERN, puc_frame_body + us_offset, us_frame_len - us_offset);
    if ((OAL_PTR_NULL != puc_ie) && (puc_ie[1] >= MAC_VHT_OPERN_LEN))
    {
        uc_vht_chan_width   = puc_ie[2];
        uc_chan_center_freq = puc_ie[3];

        /* 更新带宽信息 */
        if (0 == uc_vht_chan_width)          /* 40MHz */
        {
            /* do nothing，en_channel_bandwidth已经在HT Operation IE中获取 */
        }
        else if (1 == uc_vht_chan_width)     /* 80MHz */
        {
            switch (uc_chan_center_freq - pst_bss_dscr->st_channel.uc_chan_number)
            {
                case 6:
                /***********************************************************************
                | 主20 | 从20 | 从40       |
                              |
                              |中心频率相对于主20偏6个信道
                ************************************************************************/
                    pst_bss_dscr->en_channel_bandwidth = WLAN_BAND_WIDTH_80PLUSPLUS;
                    break;

                case -2:
                /***********************************************************************
                | 从40        | 主20 | 从20 |
                              |
                              |中心频率相对于主20偏-2个信道
                ************************************************************************/
                    pst_bss_dscr->en_channel_bandwidth = WLAN_BAND_WIDTH_80PLUSMINUS;
                    break;

                case 2:
                /***********************************************************************
                | 从20 | 主20 | 从40       |
                              |
                              |中心频率相对于主20偏2个信道
                ************************************************************************/
                    pst_bss_dscr->en_channel_bandwidth = WLAN_BAND_WIDTH_80MINUSPLUS;
                    break;

                case -6:
                /***********************************************************************
                | 从40        | 从20 | 主20 |
                              |
                              |中心频率相对于主20偏-6个信道
                ************************************************************************/
                    pst_bss_dscr->en_channel_bandwidth = WLAN_BAND_WIDTH_80MINUSMINUS;
                    break;

                default:
                    break;

            }
        }
        else
        {
            /* Unsupported Channel BandWidth */
        }
    }
}

#ifdef _PRE_WLAN_FEATURE_BTCOEX


oal_void hmac_scan_btcoex_backlist_check_by_oui(
                                                  mac_bss_dscr_stru *pst_bss_dscr,
                                                  oal_uint8         *puc_frame_body,
                                                  oal_uint16         us_frame_len,
                                                  oal_uint16         us_offset)
{
    pst_bss_dscr->en_btcoex_blacklist_chip_oui = OAL_FALSE;
    if (OAL_PTR_NULL !=
            mac_find_vendor_ie(MAC_WLAN_CHIP_OUI_RALINK, MAC_WLAN_CHIP_OUI_TYPE_RALINK, puc_frame_body + us_offset, us_frame_len - us_offset) ||
        OAL_PTR_NULL !=
            mac_find_vendor_ie(MAC_WLAN_CHIP_OUI_RALINK, MAC_WLAN_CHIP_OUI_TYPE_RALINK1, puc_frame_body + us_offset, us_frame_len - us_offset) ||
        OAL_PTR_NULL !=
            mac_find_vendor_ie(MAC_WLAN_CHIP_OUI_SHENZHEN, MAC_WLAN_CHIP_OUI_TYPE_SHENZHEN, puc_frame_body + us_offset, us_frame_len - us_offset) ||
        OAL_PTR_NULL !=
            mac_find_vendor_ie(MAC_WLAN_CHIP_OUI_ATHEROS, MAC_WLAN_CHIP_OUI_TYPE_ATHEROS, puc_frame_body + us_offset, us_frame_len - us_offset) ||
        OAL_PTR_NULL !=
            mac_find_vendor_ie(MAC_WLAN_CHIP_OUI_BROADCOM, MAC_WLAN_CHIP_OUI_TYPE_BROADCOM, puc_frame_body + us_offset, us_frame_len - us_offset) ||
        OAL_PTR_NULL !=
            mac_find_vendor_ie(MAC_WLAN_CHIP_OUI_MARVELL, MAC_WLAN_CHIP_OUI_TYPE_MARVELL, puc_frame_body + us_offset, us_frame_len - us_offset))
    {
        pst_bss_dscr->en_btcoex_blacklist_chip_oui = OAL_TRUE;
    }

    if (OAL_PTR_NULL !=
            mac_find_vendor_ie(MAC_WLAN_CHIP_OUI_REALTEKS, MAC_WLAN_CHIP_OUI_TYPE_REALTEKS, puc_frame_body + us_offset, us_frame_len - us_offset))
    {
        pst_bss_dscr->en_is_tplink_oui = WLAN_AP_CHIP_OUI_REALTEKS;
    }
}
#endif

OAL_INLINE OAL_STATIC oal_void  hmac_scan_update_bss_list_protocol(mac_bss_dscr_stru *pst_bss_dscr,
                                                                            oal_uint8         *puc_frame_body,
                                                                            oal_uint16         us_frame_len)
{
    oal_uint16  us_offset = MAC_TIME_STAMP_LEN + MAC_BEACON_INTERVAL_LEN + MAC_CAP_INFO_LEN;
    oal_uint8 *puc_ie = OAL_PTR_NULL;
    oal_uint16 us_offset_vendor_vht = MAC_WLAN_OUI_VENDOR_VHT_HEADER + MAC_IE_HDR_LEN;

    /*************************************************************************/
    /*                       Beacon Frame - Frame Body                       */
    /* ----------------------------------------------------------------------*/
    /* |Timestamp|BcnInt|CapInfo|SSID|SupRates|DSParamSet|TIM  |CountryElem |*/
    /* ----------------------------------------------------------------------*/
    /* |8        |2     |2      |2-34|3-10    |3         |6-256|8-256       |*/
    /* ----------------------------------------------------------------------*/
    /* |PowerConstraint |Quiet|TPC Report|ERP |RSN  |WMM |Extended Sup Rates|*/
    /* ----------------------------------------------------------------------*/
    /* |3               |8    |4         |3   |4-255|26  | 3-257            |*/
    /* ----------------------------------------------------------------------*/
    /* |BSS Load |HT Capabilities |HT Operation |Overlapping BSS Scan       |*/
    /* ----------------------------------------------------------------------*/
    /* |7        |28              |24           |16                         |*/
    /* ----------------------------------------------------------------------*/
    /* |Extended Capabilities |                                              */
    /* ----------------------------------------------------------------------*/
    /* |3-8                   |                                              */
    /*************************************************************************/

    /* wmm */
    hmac_scan_update_bss_list_wmm(pst_bss_dscr, puc_frame_body, us_frame_len);

#if defined(_PRE_WLAN_FEATURE_WPA) || defined(_PRE_WLAN_FEATURE_WPA2)
    /* 11i */
    hmac_scan_update_bss_list_security(pst_bss_dscr, puc_frame_body, us_frame_len, us_offset);
#endif

#ifdef _PRE_WLAN_FEATURE_11D
    /* 11d */
    hmac_scan_update_bss_list_country(pst_bss_dscr, puc_frame_body, us_frame_len);
#endif

    /*rrm*/
#ifdef _PRE_WLAN_FEATURE_FTM
    hmac_scan_update_bss_list_rrm(pst_bss_dscr, puc_frame_body, us_frame_len);
#endif

    /* 11n */
    hmac_scan_update_bss_list_11n(pst_bss_dscr, puc_frame_body, us_frame_len, us_offset);

    /* 11ac */
    hmac_scan_update_bss_list_11ac(pst_bss_dscr,  puc_frame_body, us_frame_len, us_offset, OAL_FALSE);
        /* 查找私有vendor ie */
    puc_ie = mac_find_vendor_ie(MAC_WLAN_OUI_BROADCOM_EPIGRAM,
                                MAC_WLAN_OUI_VENDOR_VHT_TYPE,
                                puc_frame_body + us_offset,
                                us_frame_len - us_offset);
    if ((OAL_PTR_NULL != puc_ie) && (puc_ie[1] >= MAC_WLAN_OUI_VENDOR_VHT_HEADER))
    {
        hmac_scan_update_bss_list_11ac(pst_bss_dscr,  puc_ie + us_offset_vendor_vht, puc_ie[1] - MAC_WLAN_OUI_VENDOR_VHT_HEADER,0, OAL_TRUE);
    }
#ifdef _PRE_WLAN_FEATURE_BTCOEX
    hmac_scan_btcoex_backlist_check_by_oui(pst_bss_dscr, puc_frame_body, us_frame_len, us_offset);
#endif
#ifdef _PRE_WLAN_FEATURE_ROAM
    if (OAL_PTR_NULL !=
            mac_find_vendor_ie(MAC_WLAN_CHIP_OUI_BROADCOM, MAC_WLAN_CHIP_OUI_TYPE_BROADCOM, puc_frame_body + us_offset, us_frame_len - us_offset))
    {
        pst_bss_dscr->en_roam_blacklist_chip_oui = OAL_TRUE;
    }
    else
    {
        pst_bss_dscr->en_roam_blacklist_chip_oui = OAL_FALSE;
    }
#endif


#ifdef _PRE_WLAN_NARROW_BAND
    if(hitalk_status & NARROW_BAND_ON_MASK)
    {
        /* nb */
        hmac_scan_update_bss_list_nb(pst_bss_dscr, puc_frame_body + us_offset, us_frame_len - us_offset);
    }
#endif

    if (OAL_PTR_NULL !=
            mac_find_vendor_ie(MAC_WLAN_CHIP_OUI_ATHEROSC, MAC_WLAN_CHIP_OUI_TYPE_ATHEROSC, puc_frame_body + us_offset, us_frame_len - us_offset))
    {
        pst_bss_dscr->en_is_tplink_oui = WLAN_AP_CHIP_OUI_ATHEROS;
    }

    if (OAL_PTR_NULL !=
            mac_find_vendor_ie(MAC_WLAN_CHIP_OUI_SHENZHEN, MAC_WLAN_CHIP_OUI_TYPE_SHENZHEN, puc_frame_body + us_offset, us_frame_len - us_offset))
    {
        pst_bss_dscr->en_is_tplink_oui = WLAN_AP_CHIP_OUI_SHENZHEN;
    }

}


oal_uint8  hmac_scan_check_bss_supp_rates(mac_device_stru *pst_mac_dev,
                                                     oal_uint8 *puc_rate,
                                                     oal_uint8  uc_bss_rate_num,
                                                     oal_uint8 *puc_update_rate)
{
    mac_data_rate_stru            *pst_rates;
    oal_uint32                     i,j,k;
    oal_uint8                      uc_rate_num = 0;

    pst_rates   = mac_device_get_all_rates(pst_mac_dev);
    if(OAL_PTR_NULL != puc_rate)
    {
        for(i = 0; i < uc_bss_rate_num; i++)
        {
            for(j = 0; j < MAC_DATARATES_PHY_80211G_NUM; j++)
            {
                if ((IS_EQUAL_RATES(pst_rates[j].uc_mac_rate, puc_rate[i]))
                && (uc_rate_num < MAC_DATARATES_PHY_80211G_NUM))
                {
                    /* 去除重复速率 */
                    for(k = 0; k < uc_rate_num; k++)
                    {
                        if(IS_EQUAL_RATES(puc_update_rate[k], puc_rate[i]))
                        {
                            break;
                        }
                    }
                    /* 当不存在重复速率时，k等于uc_rate_num */
                    if(k == uc_rate_num)
                    {
                        puc_update_rate[uc_rate_num++] = puc_rate[i];
                    }
                    break;
                }
            }
        }
    }

    return uc_rate_num;
}


oal_uint8 hmac_scan_check_chan(oal_netbuf_stru *pst_netbuf, hmac_scanned_bss_info *pst_scanned_bss)
{
    dmac_rx_ctl_stru   *pst_rx_ctrl;
    oal_uint8           uc_curr_chan;
    oal_uint8          *puc_frame_body;
    oal_uint16          us_frame_body_len;
    oal_uint16          us_offset =  MAC_TIME_STAMP_LEN + MAC_BEACON_INTERVAL_LEN + MAC_CAP_INFO_LEN;
    oal_uint8          *puc_ie_start_addr;
    oal_uint8           uc_chan_num;

    pst_rx_ctrl = (dmac_rx_ctl_stru *)oal_netbuf_cb(pst_netbuf);
    uc_curr_chan = pst_rx_ctrl->st_rx_info.uc_channel_number;
    puc_frame_body = pst_scanned_bss->st_bss_dscr_info.auc_mgmt_buff + MAC_80211_FRAME_LEN;/*lint !e416*/
    us_frame_body_len = pst_scanned_bss->st_bss_dscr_info.ul_mgmt_len - MAC_80211_FRAME_LEN;

    /* 在DSSS Param set ie中解析chan num */
    puc_ie_start_addr = mac_find_ie(MAC_EID_DSPARMS, puc_frame_body + us_offset, us_frame_body_len - us_offset);/*lint !e416*/
    if ((OAL_PTR_NULL != puc_ie_start_addr) && (puc_ie_start_addr[1] == MAC_DSPARMS_LEN))
    {
        uc_chan_num = puc_ie_start_addr[2];
        if (OAL_SUCC != mac_is_channel_num_valid(mac_get_band_by_channel_num(uc_chan_num), uc_chan_num))
        {
            return  OAL_FALSE;
        }
    }

    /* 在HT operation ie中解析 chan num */
    puc_ie_start_addr = mac_find_ie(MAC_EID_HT_OPERATION, puc_frame_body + us_offset, us_frame_body_len - us_offset);/*lint !e416*/
    if ((OAL_PTR_NULL != puc_ie_start_addr) && (puc_ie_start_addr[1] >= 1))
    {
        uc_chan_num = puc_ie_start_addr[2];
        if (OAL_SUCC != mac_is_channel_num_valid(mac_get_band_by_channel_num(uc_chan_num), uc_chan_num))
        {
            return  OAL_FALSE;
        }
    }

    uc_chan_num = pst_scanned_bss->st_bss_dscr_info.st_channel.uc_chan_number;

#ifdef _PRE_WLAN_NARROW_BAND
    if(0 == (hitalk_status & NBFH_ON_MASK))
    {
        if(((uc_curr_chan > uc_chan_num) && (uc_curr_chan - uc_chan_num >= 3))
            || ((uc_curr_chan < uc_chan_num) && (uc_chan_num - uc_curr_chan >= 3)))
        {
            return OAL_FALSE;
        }
    }
#endif

    return OAL_TRUE;
}


oal_void hmac_scan_rm_repeat_sup_exsup_rates(mac_bss_dscr_stru *pst_bss_dscr,
                                                             oal_uint8        *puc_rates,
                                                             oal_uint8         uc_exrate_num)
{
    int i,j;
    for(i = 0; i < uc_exrate_num; i++)
    {
        /* 去除重复速率 */
        for(j = 0; j < pst_bss_dscr->uc_num_supp_rates; j++)
        {
            if(IS_EQUAL_RATES(puc_rates[i], pst_bss_dscr->auc_supp_rates[j]))
            {
                break;
            }
        }

        /* 只有不存在重复速率时，j等于pst_bss_dscr->uc_num_supp_rates */
        if(j == pst_bss_dscr->uc_num_supp_rates && WLAN_MAX_SUPP_RATES > pst_bss_dscr->uc_num_supp_rates)
        {
            pst_bss_dscr->auc_supp_rates[pst_bss_dscr->uc_num_supp_rates++] = puc_rates[i];
        }
    }
}


OAL_INLINE OAL_STATIC oal_uint32  hmac_scan_update_bss_list_rates(mac_bss_dscr_stru *pst_bss_dscr,
                                                                          oal_uint8         *puc_frame_body,
                                                                          oal_uint16         us_frame_len,
                                                                          mac_device_stru   *pst_mac_dev)
{
    oal_uint8   *puc_ie;
    oal_uint8    uc_num_rates    = 0;
    oal_uint8    uc_num_ex_rates = 0;
    oal_uint8    us_offset;
    oal_uint8    auc_rates[MAC_DATARATES_PHY_80211G_NUM] = {0};

    /* 设置Beacon帧的field偏移量 */
    us_offset = MAC_TIME_STAMP_LEN + MAC_BEACON_INTERVAL_LEN + MAC_CAP_INFO_LEN;


    puc_ie = mac_find_ie(MAC_EID_RATES, puc_frame_body + us_offset, us_frame_len - us_offset);
    if (OAL_PTR_NULL != puc_ie)
    {
        uc_num_rates = hmac_scan_check_bss_supp_rates(pst_mac_dev, puc_ie + MAC_IE_HDR_LEN, puc_ie[1], auc_rates);

        if (uc_num_rates > WLAN_MAX_SUPP_RATES)
        {
            OAM_WARNING_LOG1(0, OAM_SF_SCAN, "{hmac_scan_update_bss_list_rates::uc_num_rates=%d.}", uc_num_rates);
            uc_num_rates = WLAN_MAX_SUPP_RATES;
        }

        oal_memcopy(pst_bss_dscr->auc_supp_rates, auc_rates, uc_num_rates);
        pst_bss_dscr->uc_num_supp_rates = uc_num_rates;
    }


    puc_ie = mac_find_ie(MAC_EID_XRATES, puc_frame_body + us_offset, us_frame_len - us_offset);
    if (OAL_PTR_NULL != puc_ie)
    {
        uc_num_ex_rates = hmac_scan_check_bss_supp_rates(pst_mac_dev, puc_ie + MAC_IE_HDR_LEN, puc_ie[1], auc_rates);

        if (uc_num_rates + uc_num_ex_rates > WLAN_MAX_SUPP_RATES) /* 超出支持速率个数 */
        {
            OAM_WARNING_LOG2(0, OAM_SF_SCAN,
                            "{hmac_scan_update_bss_list_rates::number of rates too large, uc_num_rates=%d, uc_num_ex_rates=%d.}",
                            uc_num_rates, uc_num_ex_rates);
        }

        if (uc_num_ex_rates > 0)
        {
            /* support_rates和extended_rates去除重复速率，一并合入扫描结果的速率集中 */
            hmac_scan_rm_repeat_sup_exsup_rates(pst_bss_dscr, auc_rates, uc_num_ex_rates);

        }
    }

    return OAL_SUCC;
}

oal_uint8  *hmac_ie_find_vendor_vht_ie(oal_uint8 *puc_frame, oal_uint16 us_frame_len)
{
    oal_uint8    *puc_vendor_ie = OAL_PTR_NULL;
    oal_uint8    *puc_vht_ie = OAL_PTR_NULL;
    oal_uint16    us_offset_vendor_vht = MAC_WLAN_OUI_VENDOR_VHT_HEADER + MAC_IE_HDR_LEN;

    puc_vendor_ie = mac_find_vendor_ie(MAC_WLAN_OUI_BROADCOM_EPIGRAM,
                                MAC_WLAN_OUI_VENDOR_VHT_TYPE,
                                puc_frame,
                                us_frame_len);

    if ((OAL_PTR_NULL != puc_vendor_ie) && (puc_vendor_ie[1] >= MAC_WLAN_OUI_VENDOR_VHT_HEADER))
    {
        puc_vht_ie = mac_find_ie(MAC_EID_VHT_CAP, puc_vendor_ie + us_offset_vendor_vht, puc_vendor_ie[1] - MAC_WLAN_OUI_VENDOR_VHT_HEADER);
    }

    return puc_vht_ie;
}


wlan_nss_enum_uint8  hmac_scan_get_bss_max_nss(mac_vap_stru *pst_mac_vap, oal_uint8  *puc_frame_body,
                                                            oal_uint16 us_frame_len, oal_bool_enum_uint8 en_assoc_status)
{
    oal_uint8           *puc_ie = OAL_PTR_NULL;
    oal_uint8           *puc_ht_mcs_bitmask;
    wlan_nss_enum_uint8  en_nss = WLAN_SINGLE_NSS;
    oal_uint16           us_vht_mcs_map;
    oal_uint16           us_msg_idx = 0;
    oal_uint16           us_offset;

    /* 设置Beacon帧的field偏移量 */
    us_offset = MAC_TIME_STAMP_LEN + MAC_BEACON_INTERVAL_LEN + MAC_CAP_INFO_LEN;

    /* 查ht ie */
    puc_ie = mac_find_ie(MAC_EID_HT_CAP, puc_frame_body + us_offset, us_frame_len - us_offset);
    if (OAL_PTR_NULL != puc_ie)
    {
        us_msg_idx = MAC_IE_HDR_LEN + MAC_HT_CAPINFO_LEN + MAC_HT_AMPDU_PARAMS_LEN;
        puc_ht_mcs_bitmask = &puc_ie[us_msg_idx];
        for (en_nss = WLAN_SINGLE_NSS; en_nss < WLAN_NSS_BUTT; en_nss++)
        {
            if (0 == puc_ht_mcs_bitmask[en_nss])
            {
                break;
            }
        }
        if(WLAN_SINGLE_NSS != en_nss)
        {
            en_nss--;
        }

        /* 如果是考虑up状态，并且是ht协议时，按照此值返回 */
        if(OAL_TRUE == en_assoc_status &&
            (WLAN_HT_MODE == pst_mac_vap->en_protocol || WLAN_HT_ONLY_MODE == pst_mac_vap->en_protocol ||
             WLAN_HT_11G_MODE == pst_mac_vap->en_protocol))
        {
            return en_nss;
        }
    }

    /* 查vht ie */
    puc_ie = mac_find_ie(MAC_EID_VHT_CAP, puc_frame_body + us_offset, us_frame_len - us_offset);

    /*增加兼容性，还需从私有vendor ie里面查找vht ie*/
    if (OAL_PTR_NULL == puc_ie)
    {
        puc_ie = hmac_ie_find_vendor_vht_ie(puc_frame_body + us_offset, us_frame_len - us_offset);
    }

    if (OAL_PTR_NULL != puc_ie)
    {
        us_msg_idx = MAC_IE_HDR_LEN + MAC_VHT_CAP_INFO_FIELD_LEN;

        us_vht_mcs_map = OAL_MAKE_WORD16(puc_ie[us_msg_idx], puc_ie[us_msg_idx + 1]);

        for (en_nss = WLAN_SINGLE_NSS; en_nss < WLAN_NSS_BUTT; en_nss++)
        {
            if (WLAN_INVALD_VHT_MCS == WLAN_GET_VHT_MAX_SUPPORT_MCS(us_vht_mcs_map & 0x3))
            {
                break;
            }
            us_vht_mcs_map >>= 2;
        }
        if(WLAN_SINGLE_NSS != en_nss)
        {
            en_nss--;
        }

        /* 如果是考虑up状态，并且是vht协议时，按照此值返回 */
        if(OAL_TRUE == en_assoc_status &&
            (WLAN_VHT_MODE == pst_mac_vap->en_protocol || WLAN_VHT_ONLY_MODE == pst_mac_vap->en_protocol))
        {
            return en_nss;
        }
    }

    return en_nss;
}


oal_uint8 hmac_scan_get_num_sounding_dim(oal_uint8  *puc_frame_body, oal_uint16 us_frame_len)
{
    oal_uint8      *puc_vht_cap_ie = OAL_PTR_NULL;
    oal_uint16      us_offset;
    oal_uint16      us_vht_cap_filed_low;
    oal_uint16      us_vht_cap_filed_high;
    oal_uint32      ul_vht_cap_field;
    oal_uint16      us_msg_idx;
    oal_uint8       uc_num_sounding_dim = 0;

    /* 设置Beacon帧的field偏移量 */
    us_offset = MAC_TIME_STAMP_LEN + MAC_BEACON_INTERVAL_LEN + MAC_CAP_INFO_LEN;
    puc_vht_cap_ie = mac_find_ie(MAC_EID_VHT_CAP, puc_frame_body + us_offset, us_frame_len - us_offset);
    if (OAL_PTR_NULL == puc_vht_cap_ie)
    {
        puc_vht_cap_ie = hmac_ie_find_vendor_vht_ie(puc_frame_body + us_offset, us_frame_len - us_offset);
        if(OAL_PTR_NULL == puc_vht_cap_ie)
        {
            return uc_num_sounding_dim;
        }
    }
    us_msg_idx = MAC_IE_HDR_LEN;

    /* 解析VHT capablities info field */
    us_vht_cap_filed_low    = OAL_MAKE_WORD16(puc_vht_cap_ie[us_msg_idx], puc_vht_cap_ie[us_msg_idx + 1]);
    us_vht_cap_filed_high   = OAL_MAKE_WORD16(puc_vht_cap_ie[us_msg_idx + 2], puc_vht_cap_ie[us_msg_idx + 3]);
    ul_vht_cap_field        = OAL_MAKE_WORD32(us_vht_cap_filed_low, us_vht_cap_filed_high);
    /* 解析num_sounding_dim */
    uc_num_sounding_dim = ((ul_vht_cap_field & (BIT18 | BIT17 | BIT16)) >> 16);
    return uc_num_sounding_dim;
}

oal_void hmac_scan_get_11v_ability(mac_bss_dscr_stru          *pst_bss_dscr,oal_uint8 *puc_frame_body,oal_uint16 us_frame_body_len,oal_uint16 us_offset)
{
    mac_ext_cap_ie_stru                  *pst_ext_cap_ie;
    oal_uint8                            *puc_temp;
    puc_temp = mac_find_ie(MAC_EID_EXT_CAPS, puc_frame_body + us_offset, (oal_int32)(us_frame_body_len - us_offset));
    if (OAL_PTR_NULL != puc_temp)
    {
        puc_temp += MAC_IE_HDR_LEN;
        pst_ext_cap_ie = (mac_ext_cap_ie_stru*)puc_temp;
        pst_bss_dscr->en_11v_capable = (pst_ext_cap_ie->bit_bss_transition == 1)?OAL_TRUE:OAL_FALSE;
    }

}


oal_void hmac_scan_get_11k_ability(mac_bss_dscr_stru *pst_bss_dscr, oal_uint8 *puc_frame_body, oal_uint16 us_frame_body_len, oal_uint16 us_offset)
{
    oal_uint8 *puc_temp;
    puc_temp = mac_find_ie(MAC_EID_RRM, puc_frame_body + us_offset, (oal_int32)(us_frame_body_len - us_offset));
    if (puc_temp != OAL_PTR_NULL){
       pst_bss_dscr->en_11k_capable = OAL_TRUE;
    } else {
       pst_bss_dscr->en_11k_capable = OAL_FALSE;
    }
}


OAL_STATIC oal_uint32  hmac_scan_update_bss_dscr(hmac_scanned_bss_info   *pst_scanned_bss,
                                                          dmac_tx_event_stru      *pst_dtx_event,
                                                          oal_uint8                uc_device_id,
                                                          oal_uint8                uc_vap_id)
{
    oal_netbuf_stru                      *pst_netbuf    = pst_dtx_event->pst_netbuf;
    mac_scanned_result_extend_info_stru  *pst_scan_result_extend_info;
    mac_device_stru                      *pst_mac_device;
    hmac_vap_stru                        *pst_hmac_vap;
    mac_vap_stru                         *pst_mac_vap;
    mac_ieee80211_frame_stru             *pst_frame_header;
    oal_uint8                            *puc_frame_body;
    mac_bss_dscr_stru                    *pst_bss_dscr;
    oal_uint8                            *puc_ssid;                 /* 指向beacon帧中的ssid */
    oal_uint8                            *puc_mgmt_frame;
    dmac_rx_ctl_stru                     *pst_rx_ctrl;
    oal_uint16                            us_netbuf_len = pst_dtx_event->us_frame_len;
    oal_uint16                            us_frame_len;
    oal_uint16                            us_frame_body_len;
    oal_uint16                            us_offset =  MAC_TIME_STAMP_LEN + MAC_BEACON_INTERVAL_LEN + MAC_CAP_INFO_LEN;
    oal_uint8                             uc_ssid_len;
    oal_uint8                             uc_frame_channel;
#ifdef _PRE_WLAN_FEATURE_TXBF
    mac_11ntxbf_vendor_ie_stru           *pst_txbf_vendor_ie;
#endif

    /* 获取hmac vap */
    pst_hmac_vap = mac_res_get_hmac_vap(uc_vap_id);
    if (OAL_PTR_NULL == pst_hmac_vap)
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{hmac_scan_update_bss_dscr::pst_hmac_vap is null.}");
        return OAL_FAIL;
    }

    pst_mac_vap = &pst_hmac_vap->st_vap_base_info;

    /* 获取mac device */
    pst_mac_device  = mac_res_get_dev(uc_device_id);
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_mac_device))
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{hmac_scan_update_bss_dscr::pst_mac_device is null.}");
        return OAL_FAIL;
    }

    /* 获取device上报的扫描结果信息，并将其更新到bss描述结构体中 */
    us_frame_len   = us_netbuf_len - OAL_SIZEOF(mac_scanned_result_extend_info_stru);
    puc_mgmt_frame = (oal_uint8 *)OAL_NETBUF_DATA(pst_netbuf);
    pst_rx_ctrl    = (dmac_rx_ctl_stru *)oal_netbuf_cb(pst_netbuf);

    /* 指向netbuf中的上报的扫描结果的扩展信息的位置 */
    pst_scan_result_extend_info = (mac_scanned_result_extend_info_stru *)(puc_mgmt_frame + us_frame_len);

    /* 获取管理帧的帧头和帧体指针 */
    pst_frame_header  = (mac_ieee80211_frame_stru *)puc_mgmt_frame;
    puc_frame_body    = (oal_uint8 *)(puc_mgmt_frame + MAC_80211_FRAME_LEN);
    us_frame_body_len = us_frame_len - MAC_80211_FRAME_LEN;

    /* 获取管理帧中的信道 */
    uc_frame_channel = mac_ie_get_chan_num(puc_frame_body, us_frame_body_len, us_offset, pst_rx_ctrl->st_rx_info.uc_channel_number);

    /* 更新bss信息 */
    pst_bss_dscr = &(pst_scanned_bss->st_bss_dscr_info);

    /*****************************************************************************
        解析beacon/probe rsp帧，记录到pst_bss_dscr
    *****************************************************************************/
    /* 解析并保存ssid */
    puc_ssid = mac_get_ssid(puc_frame_body, (oal_int32)us_frame_body_len, &uc_ssid_len);
    if ((OAL_PTR_NULL != puc_ssid) && (0 != uc_ssid_len))
    {
        /* 将查找到的ssid保存到bss描述结构体中 */
        oal_memcopy(pst_bss_dscr->ac_ssid, puc_ssid, uc_ssid_len);
        pst_bss_dscr->ac_ssid[uc_ssid_len] = '\0';
    }

    /* 解析bssid */
    oal_set_mac_addr(pst_bss_dscr->auc_mac_addr, pst_frame_header->auc_address2);
    oal_set_mac_addr(pst_bss_dscr->auc_bssid, pst_frame_header->auc_address3);

    /* bss基本信息 */
    pst_bss_dscr->en_bss_type = pst_scan_result_extend_info->en_bss_type;

    pst_bss_dscr->us_cap_info = *((oal_uint16 *)(puc_frame_body + MAC_TIME_STAMP_LEN + MAC_BEACON_INTERVAL_LEN));

    pst_bss_dscr->c_rssi      =  (oal_int8)pst_scan_result_extend_info->l_rssi;

    /* 解析beacon周期与tim周期 */
    pst_bss_dscr->us_beacon_period = mac_get_beacon_period(puc_frame_body);
    pst_bss_dscr->uc_dtim_period   = mac_get_dtim_period(puc_frame_body, us_frame_body_len);
    pst_bss_dscr->uc_dtim_cnt      = mac_get_dtim_cnt(puc_frame_body, us_frame_body_len);

    /* 信道 */
    pst_bss_dscr->st_channel.uc_chan_number = uc_frame_channel;
    pst_bss_dscr->st_channel.en_band        = mac_get_band_by_channel_num(uc_frame_channel);

    /* 记录速率集 */
    hmac_scan_update_bss_list_rates(pst_bss_dscr, puc_frame_body, us_frame_body_len, pst_mac_device);

    /* 记录扫描结果 */
    pst_bss_dscr->en_support_max_nss = hmac_scan_get_bss_max_nss(pst_mac_vap, puc_frame_body,us_frame_body_len,OAL_FALSE);
    pst_bss_dscr->uc_num_sounding_dim = hmac_scan_get_num_sounding_dim(puc_frame_body, us_frame_body_len);

    /* 协议类相关信息元素的获取 */
    hmac_scan_update_bss_list_protocol(pst_bss_dscr, puc_frame_body, us_frame_body_len);
    /* 获取Ext Cap 11v能力 */
    hmac_scan_get_11v_ability(pst_bss_dscr,puc_frame_body,us_frame_body_len,us_offset);
    /* 获取Ext Cap 11k能力 */
    hmac_scan_get_11k_ability(pst_bss_dscr, puc_frame_body, us_frame_body_len, us_offset);


#if (_PRE_MULTI_CORE_MODE_OFFLOAD_DMAC == _PRE_MULTI_CORE_MODE)
    pst_bss_dscr->st_channel.en_bandwidth = pst_bss_dscr->en_channel_bandwidth;
    mac_get_channel_idx_from_num(pst_bss_dscr->st_channel.en_band,
        pst_bss_dscr->st_channel.uc_chan_number, &pst_bss_dscr->st_channel.uc_idx);
#endif

#ifdef _PRE_WLAN_FEATURE_TXBF
    pst_txbf_vendor_ie = (mac_11ntxbf_vendor_ie_stru *)mac_find_vendor_ie(MAC_HUAWEI_VENDER_IE, MAC_EID_11NTXBF, puc_frame_body + us_offset, (oal_int32)(us_frame_body_len - us_offset));
    if (OAL_PTR_NULL != pst_txbf_vendor_ie)
    {
        pst_bss_dscr->en_11ntxbf = (pst_txbf_vendor_ie->st_11ntxbf.bit_11ntxbf == 1) ? OAL_TRUE : OAL_FALSE;

    }
#endif

    /* 更新时间戳 */
    pst_bss_dscr->ul_timestamp = (oal_uint32)OAL_TIME_GET_STAMP_MS();

    pst_bss_dscr->ul_mgmt_len = us_frame_len;

    /* 拷贝管理帧内容 */
    oal_memcopy((oal_uint8 *)pst_scanned_bss->st_bss_dscr_info.auc_mgmt_buff, puc_mgmt_frame, (oal_uint32)us_frame_len);

    return OAL_SUCC;
}


OAL_STATIC oal_bool_enum_uint8 hmac_scan_need_update_timestamp(oal_uint8 uc_vap_id, hmac_scanned_bss_info *pst_new_bss, hmac_scanned_bss_info *pst_old_bss)
{
    if (('\0' == pst_new_bss->st_bss_dscr_info.ac_ssid[0]) &&
           ('\0' != pst_old_bss->st_bss_dscr_info.ac_ssid[0]))
    {
        return OAL_FALSE;
    }

    return OAL_TRUE;
}


oal_bool_enum_uint8 hmac_scan_need_update_old_scan_result(oal_uint8 uc_vap_id, hmac_scanned_bss_info *pst_new_bss, hmac_scanned_bss_info *pst_old_bss)
{
    
    if ((WLAN_PROBE_RSP == ((mac_ieee80211_frame_stru *)pst_old_bss->st_bss_dscr_info.auc_mgmt_buff)->st_frame_control.bit_sub_type)
        && (WLAN_BEACON == ((mac_ieee80211_frame_stru *)pst_new_bss->st_bss_dscr_info.auc_mgmt_buff)->st_frame_control.bit_sub_type)
        && (OAL_TRUE == pst_old_bss->st_bss_dscr_info.en_new_scan_bss))
    {
        return OAL_FALSE;
    }

    return OAL_TRUE;

}

#ifdef _PRE_WLAN_FEATURE_11V_ENABLE
oal_uint32 hmac_scan_11v_bsstreq_filter_switch(hmac_vap_stru *pst_hmac_vap, hmac_scanned_bss_info      * pst_new_scanned_bss)
{
    hmac_user_stru *pst_hmac_user;
    mac_vap_stru   *pst_vap = &(pst_hmac_vap->st_vap_base_info);
    hmac_user_11v_ctrl_stru *pst_11v_ctrl_info;

    /* 只有STA模式才支持11V */
    if(WLAN_VAP_MODE_BSS_STA != pst_vap->en_vap_mode)
    {
        return OAL_SUCC;
    }

    pst_hmac_user = mac_res_get_hmac_user((oal_uint16)pst_vap->uc_assoc_vap_id);
    if(OAL_PTR_NULL == pst_hmac_user)
    {
        return OAL_FAIL;
    }

    pst_11v_ctrl_info = &pst_hmac_user->st_11v_ctrl_info;

    /* 11v 过滤开启,如果下次扫描到target bss rssi大于-72dB(-78是开漫游门限，高6dB)关闭11v过滤 */
    if(0 == oal_memcmp(pst_11v_ctrl_info->auc_target_bss_addr,pst_new_scanned_bss->st_bss_dscr_info.auc_mac_addr,WLAN_MAC_ADDR_LEN)
       && OAL_TRUE == pst_11v_ctrl_info->en_bsstreq_filter
#ifdef _PRE_PLAT_FEATURE_CUSTOMIZE
       && pst_new_scanned_bss->st_bss_dscr_info.c_rssi >= wlan_customize.c_roam_trigger_a + ROAM_RSSI_DIFF_6_DB
#endif
       )
    {
        if(OAL_SUCC == hmac_config_filter_11v_bsstreq_switch(pst_vap,OAL_FALSE))
        {
            OAM_WARNING_LOG1(0,OAM_SF_ANY,"hmac_scan_11v_bsstreq_filter_switch: disable filter,RSSI[%d]",pst_new_scanned_bss->st_bss_dscr_info.c_rssi);

            pst_11v_ctrl_info->en_bsstreq_filter = OAL_FALSE;
        }
    }
    return OAL_SUCC;
}
#endif

oal_uint32 hmac_scan_proc_scanned_bss(frw_event_mem_stru *pst_event_mem)
{
    frw_event_stru             *pst_event;
    frw_event_hdr_stru         *pst_event_hdr;
    hmac_vap_stru              *pst_hmac_vap;
    oal_netbuf_stru            *pst_bss_mgmt_netbuf;
    hmac_device_stru           *pst_hmac_device;
    dmac_tx_event_stru         *pst_dtx_event;
    hmac_scanned_bss_info      *pst_new_scanned_bss;
    hmac_scanned_bss_info      *pst_old_scanned_bss;
    oal_uint32                  ul_ret;
    oal_uint16                  us_mgmt_len;
    oal_uint8                   uc_vap_id;
    hmac_bss_mgmt_stru         *pst_bss_mgmt;
    oal_uint32                  ul_curr_time_stamp;

    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_event_mem))
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{hmac_scan_proc_scanned_bss::pst_event_mem null.}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 获取事件头和事件结构体指针 */
    pst_event           = (frw_event_stru *)pst_event_mem->puc_data;
    pst_event_hdr       = &(pst_event->st_event_hdr);
    pst_dtx_event       = (dmac_tx_event_stru *)pst_event->auc_event_data;
    pst_bss_mgmt_netbuf = pst_dtx_event->pst_netbuf;

    pst_hmac_vap = mac_res_get_hmac_vap(pst_event_hdr->uc_vap_id);
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_hmac_vap))
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{hmac_scan_proc_scanned_bss::pst_hmac_vap null.}");

        /* 释放上报的bss信息和beacon或者probe rsp帧的内存 */
        oal_netbuf_free(pst_bss_mgmt_netbuf);
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 获取vap id */
    uc_vap_id = pst_hmac_vap->st_vap_base_info.uc_vap_id;

    /* 获取hmac device 结构 */
    pst_hmac_device = hmac_res_get_mac_dev(pst_event_hdr->uc_device_id);
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_hmac_device))
    {
        OAM_ERROR_LOG0(uc_vap_id, OAM_SF_SCAN, "{hmac_scan_proc_scanned_bss::pst_hmac_device null.}");

        /* 释放上报的bss信息和beacon或者probe rsp帧的内存 */
        oal_netbuf_free(pst_bss_mgmt_netbuf);
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 对dmac上报的netbuf内容进行解析，内容如下所示 */
    /***********************************************************************************************/
    /*            netbuf data域的上报的扫描结果的字段的分布                                        */
    /* ------------------------------------------------------------------------------------------  */
    /* beacon/probe rsp body  |     帧体后面附加字段(mac_scanned_result_extend_info_stru)          */
    /* -----------------------------------------------------------------------------------------   */
    /* 收到的beacon/rsp的body | rssi(4字节) | channel num(1字节)| band(1字节)|bss_tye(1字节)|填充  */
    /* ------------------------------------------------------------------------------------------  */
    /*                                                                                             */
    /***********************************************************************************************/

    /* 管理帧的长度等于上报的netbuf的长度减去上报的扫描结果的扩展字段的长度 */
    us_mgmt_len = pst_dtx_event->us_frame_len - OAL_SIZEOF(mac_scanned_result_extend_info_stru);

    /* 申请存储扫描结果的内存 */
    pst_new_scanned_bss = hmac_scan_alloc_scanned_bss(us_mgmt_len);
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_new_scanned_bss))
    {
        OAM_ERROR_LOG0(uc_vap_id, OAM_SF_SCAN, "{hmac_scan_proc_scanned_bss::alloc memory failed for storing scanned result.}");

        /* 释放上报的bss信息和beacon或者probe rsp帧的内存 */
        oal_netbuf_free(pst_bss_mgmt_netbuf);
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 更新描述扫描结果的bss dscr结构体 */
    ul_ret = hmac_scan_update_bss_dscr(pst_new_scanned_bss, pst_dtx_event, pst_event_hdr->uc_device_id, pst_event_hdr->uc_vap_id);
    if (OAL_UNLIKELY(OAL_SUCC != ul_ret))
    {
        OAM_ERROR_LOG1(uc_vap_id, OAM_SF_SCAN, "{hmac_scan_proc_scanned_bss::hmac_scan_update_bss_dscr failed[%d].}", ul_ret);

        /* 释放上报的bss信息和beacon或者probe rsp帧的内存 */
        oal_netbuf_free(pst_bss_mgmt_netbuf);

        /* 释放申请的存储bss信息的内存 */
        oal_free(pst_new_scanned_bss);
        return ul_ret;
    }

    /* 获取管理扫描的bss结果的结构体 */
    pst_bss_mgmt = &(pst_hmac_device->st_scan_mgmt.st_scan_record_mgmt.st_bss_mgmt);
    /* 对链表删操作前加锁 */
    oal_spin_lock(&(pst_bss_mgmt->st_lock));
    /* 判断相同bssid的bss是否已经扫描到 */
    pst_old_scanned_bss = hmac_scan_find_scanned_bss_by_bssid(pst_bss_mgmt, pst_new_scanned_bss->st_bss_dscr_info.auc_bssid);
    if (OAL_PTR_NULL == pst_old_scanned_bss)
    {
        /* 解锁 */
        oal_spin_unlock(&(pst_bss_mgmt->st_lock));

        /*lint -e801*/
        goto add_bss;
        /*lint +e801*/
    }

    /* 如果老的扫描的bss的信号强度大于当前扫描到的bss的信号强度，更新当前扫描到的信号强度为最强的信号强度 */
    if (pst_old_scanned_bss->st_bss_dscr_info.c_rssi > pst_new_scanned_bss->st_bss_dscr_info.c_rssi)
    {
        /* 1s中以内就采用之前的BSS保存的RSSI信息，否则就采用新的RSSI信息 */
        ul_curr_time_stamp = (oal_uint32)OAL_TIME_GET_STAMP_MS();
        if (oal_time_after32((ul_curr_time_stamp), (pst_old_scanned_bss->st_bss_dscr_info.ul_timestamp + HMAC_SCAN_MAX_SCANNED_RSSI_EXPIRE)) == OAL_FALSE)
        {
            OAM_INFO_LOG0(uc_vap_id, OAM_SF_SCAN, "{hmac_scan_proc_scanned_bss::update signal strength value.}");
            pst_new_scanned_bss->st_bss_dscr_info.c_rssi = pst_old_scanned_bss->st_bss_dscr_info.c_rssi;
        }
    }
    if (OAL_FALSE == hmac_scan_need_update_timestamp(uc_vap_id, pst_new_scanned_bss, pst_old_scanned_bss))
    {
        /*解锁*/
        oal_spin_unlock(&(pst_bss_mgmt->st_lock));

        /* 释放申请的存储bss信息的内存 */
        oal_free(pst_new_scanned_bss);

        /* 释放上报的bss信息和beacon或者probe rsp帧的内存 */
        oal_netbuf_free(pst_bss_mgmt_netbuf);

        return OAL_SUCC;
    }
    if (OAL_FALSE == hmac_scan_need_update_old_scan_result(uc_vap_id, pst_new_scanned_bss, pst_old_scanned_bss)
        || OAL_FALSE == hmac_scan_check_chan(pst_bss_mgmt_netbuf, pst_new_scanned_bss))
    {
        pst_old_scanned_bss->st_bss_dscr_info.ul_timestamp = (oal_uint32)OAL_TIME_GET_STAMP_MS();
        pst_old_scanned_bss->st_bss_dscr_info.c_rssi = pst_new_scanned_bss->st_bss_dscr_info.c_rssi;

        /* 解锁 */
        oal_spin_unlock(&(pst_bss_mgmt->st_lock));

        /* 释放申请的存储bss信息的内存 */
        oal_free(pst_new_scanned_bss);

        /* 释放上报的bss信息和beacon或者probe rsp帧的内存 */
        oal_netbuf_free(pst_bss_mgmt_netbuf);

        return OAL_SUCC;
    }

    /* 从链表中将原先扫描到的相同bssid的bss节点删除 */
    hmac_scan_del_bss_from_list_nolock(pst_old_scanned_bss, pst_hmac_device);
    /* 解锁 */
    oal_spin_unlock(&(pst_bss_mgmt->st_lock));
    /* 释放内存 */
    oal_free(pst_old_scanned_bss);


add_bss:
    /* 将扫描结果添加到链表中 */
    hmac_scan_add_bss_to_list(pst_new_scanned_bss, pst_hmac_device);
#ifdef _PRE_WLAN_FEATURE_11V_ENABLE
    /* 根据扫描结果关闭11v filter功能 */
    hmac_scan_11v_bsstreq_filter_switch(pst_hmac_vap, pst_new_scanned_bss);
#endif
    /* 释放上报的bss信息和beacon或者probe rsp帧的内存 */
    oal_netbuf_free(pst_bss_mgmt_netbuf);


    return OAL_SUCC;
}


OAL_STATIC oal_void  hmac_scan_print_channel_statistics_info(hmac_scan_record_stru   *pst_scan_record)
{
    mac_scan_chan_stats_stru    *pst_chan_stats = pst_scan_record->ast_chan_results;
    oal_uint8                    uc_vap_id = pst_scan_record->uc_vap_id;
    oal_uint8                    uc_idx = 0;

    /* 检测本次扫描是否开启了信道测量，如果没有直接返回 */
    if (0 == pst_chan_stats[0].uc_stats_valid)
    {
        OAM_INFO_LOG0(uc_vap_id, OAM_SF_SCAN, "{hmac_scan_print_channel_statistics_info:: curr scan don't enable channel measure.\n}");
        return;
    }

    /* 打印信道测量结果 */
    OAM_INFO_LOG0(uc_vap_id, OAM_SF_SCAN, "{hmac_scan_print_channel_statistics_info:: The chan measure result: \n}");

    for (uc_idx = 0; uc_idx < pst_scan_record->uc_chan_numbers; uc_idx++)
    {
        OAM_INFO_LOG1(uc_vap_id, OAM_SF_SCAN, "{hmac_scan_print_channel_statistics_info::Chan num      : %d\n}", pst_chan_stats[uc_idx].uc_channel_number);
        OAM_INFO_LOG1(uc_vap_id, OAM_SF_SCAN, "{hmac_scan_print_channel_statistics_info::Stats cnt     : %d\n}", pst_chan_stats[uc_idx].uc_stats_cnt);
        OAM_INFO_LOG1(uc_vap_id, OAM_SF_SCAN, "{hmac_scan_print_channel_statistics_info::Stats valid   : %d\n}", pst_chan_stats[uc_idx].uc_stats_valid);
        OAM_INFO_LOG1(uc_vap_id, OAM_SF_SCAN, "{hmac_scan_print_channel_statistics_info::Stats time us : %d\n}", pst_chan_stats[uc_idx].ul_total_stats_time_us);
        OAM_INFO_LOG1(uc_vap_id, OAM_SF_SCAN, "{hmac_scan_print_channel_statistics_info::Free time 20M : %d\n}", pst_chan_stats[uc_idx].ul_total_free_time_20M_us);
        OAM_INFO_LOG1(uc_vap_id, OAM_SF_SCAN, "{hmac_scan_print_channel_statistics_info::Free time 40M : %d\n}", pst_chan_stats[uc_idx].ul_total_free_time_40M_us);
        OAM_INFO_LOG1(uc_vap_id, OAM_SF_SCAN, "{hmac_scan_print_channel_statistics_info::Free time 80M : %d\n}", pst_chan_stats[uc_idx].ul_total_free_time_80M_us);
        OAM_INFO_LOG1(uc_vap_id, OAM_SF_SCAN, "{hmac_scan_print_channel_statistics_info::Send time     : %d\n}", pst_chan_stats[uc_idx].ul_total_send_time_us);
        OAM_INFO_LOG1(uc_vap_id, OAM_SF_SCAN, "{hmac_scan_print_channel_statistics_info::Recv time     : %d\n}", pst_chan_stats[uc_idx].ul_total_recv_time_us);
        OAM_INFO_LOG1(uc_vap_id, OAM_SF_SCAN, "{hmac_scan_print_channel_statistics_info::Free power cnt: %d\n}", pst_chan_stats[uc_idx].uc_free_power_cnt);
        OAM_INFO_LOG1(uc_vap_id, OAM_SF_SCAN, "{hmac_scan_print_channel_statistics_info::Free power 20M: %d\n}", (oal_int32)pst_chan_stats[uc_idx].s_free_power_stats_20M);
        OAM_INFO_LOG1(uc_vap_id, OAM_SF_SCAN, "{hmac_scan_print_channel_statistics_info::Free power 40M: %d\n}", (oal_int32)pst_chan_stats[uc_idx].s_free_power_stats_40M);
        OAM_INFO_LOG1(uc_vap_id, OAM_SF_SCAN, "{hmac_scan_print_channel_statistics_info::Free power 80M: %d\n}", (oal_int32)pst_chan_stats[uc_idx].s_free_power_stats_80M);
        OAM_INFO_LOG1(uc_vap_id, OAM_SF_SCAN, "{hmac_scan_print_channel_statistics_info::Radar detected: %d\n}", pst_chan_stats[uc_idx].uc_radar_detected);
        OAM_INFO_LOG1(uc_vap_id, OAM_SF_SCAN, "{hmac_scan_print_channel_statistics_info::Radar bw      : %d\n}", pst_chan_stats[uc_idx].uc_radar_bw);
    }

    return;
}


OAL_STATIC oal_void  hmac_scan_print_scan_record_info(hmac_vap_stru *pst_hmac_vap, hmac_scan_record_stru *pst_scan_record)
{
#if (_PRE_OS_VERSION_LINUX == _PRE_OS_VERSION) && (defined(_PRE_PRODUCT_ID_HI110X_HOST))
    oal_time_t_stru   st_timestamp_diff;

    /* 获取扫描间隔时间戳 */
    st_timestamp_diff  = oal_ktime_sub(oal_ktime_get(), pst_scan_record->st_scan_start_time);

    /* 调用内核接口，打印此次扫描耗时 */
    OAM_WARNING_LOG4(pst_scan_record->uc_vap_id, OAM_SF_SCAN,
                     "{hmac_scan_print_scan_record_info::scan comp, scan_status[%d],vap ch_num:%d, cookie[%x], duration time is: [%lu]ms.}",
                     pst_scan_record->en_scan_rsp_status,
                     pst_hmac_vap->st_vap_base_info.st_channel.uc_chan_number,
                     pst_scan_record->ull_cookie,
                     ktime_to_ms(st_timestamp_diff));
#endif

    /* 打印扫描到的bss信息 */
    hmac_scan_print_scanned_bss_info(pst_scan_record->uc_device_id);

    /* 信道测量结果 */
    hmac_scan_print_channel_statistics_info(pst_scan_record);

    return;
}
OAL_STATIC oal_void hmac_scan_complete_result_to_sme(hmac_scan_stru *pst_scan_mgmt, hmac_vap_stru  *pst_hmac_vap, mac_scan_rsp_stru *pst_d2h_scan_rsp_info)
{
    oal_bool_enum_uint8                 en_scan_abort_sync_state = OAL_FALSE;

    if ((WLAN_VAP_MODE_BSS_STA == pst_hmac_vap->st_vap_base_info.en_vap_mode)
        && (MAC_SCAN_ABORT_SYNC == pst_scan_mgmt->st_scan_record_mgmt.en_scan_rsp_status))
    {
        /* MAC_SCAN_ABORT_SYNC只作为一个host侧的临时状态，下面会被d2h扫描状态覆盖 */
        en_scan_abort_sync_state = OAL_TRUE;
    }

    if ((WLAN_VAP_MODE_BSS_AP == pst_hmac_vap->st_vap_base_info.en_vap_mode)
        && (MAC_VAP_STATE_BUTT != pst_scan_mgmt->st_scan_record_mgmt.en_vap_last_state))
    {
        hmac_fsm_change_state(pst_hmac_vap, pst_scan_mgmt->st_scan_record_mgmt.en_vap_last_state);
        pst_scan_mgmt->st_scan_record_mgmt.en_vap_last_state = MAC_VAP_STATE_BUTT;
    }

    /* 根据device上报的扫描结果，上报sme */
    /* 将扫描执行情况(扫描执行成功、还是失败等返回结果)记录到扫描运行记录结构体中 */
    pst_scan_mgmt->st_scan_record_mgmt.en_scan_rsp_status = pst_d2h_scan_rsp_info->en_scan_rsp_status;
    pst_scan_mgmt->st_scan_record_mgmt.ull_cookie         = pst_d2h_scan_rsp_info->ull_cookie;

    hmac_scan_print_scan_record_info(pst_hmac_vap, &(pst_scan_mgmt->st_scan_record_mgmt));

#ifdef _PRE_WLAN_FEATURE_ROAM
    /* STA背景扫描时，需要提前识别漫游场景 */
    hmac_roam_check_bkscan_result(pst_hmac_vap, &(pst_scan_mgmt->st_scan_record_mgmt));
#endif //_PRE_WLAN_FEATURE_ROAM

    /* 如果扫描回调函数不为空，则调用回调函数 */
    if (OAL_PTR_NULL != pst_scan_mgmt->st_scan_record_mgmt.p_fn_cb)
    {
        /* 终止扫描无需调用回调,防止终止扫描结束对随后发起PNO扫描的影响 */
        pst_scan_mgmt->st_scan_record_mgmt.p_fn_cb(&(pst_scan_mgmt->st_scan_record_mgmt));
    }
    else
    {
        if(OAL_TRUE == en_scan_abort_sync_state)
        {
            hmac_cfg80211_scan_comp_cb(&(pst_scan_mgmt->st_scan_record_mgmt));
        }
    }

}


oal_uint32  hmac_scan_proc_scan_comp_event(frw_event_mem_stru *pst_event_mem)
{
    frw_event_stru                     *pst_event;
    frw_event_hdr_stru                 *pst_event_hdr;
    hmac_vap_stru                      *pst_hmac_vap;
    hmac_device_stru                   *pst_hmac_device;
    mac_scan_rsp_stru                  *pst_d2h_scan_rsp_info;
    hmac_scan_stru                     *pst_scan_mgmt;

    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_event_mem))
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{hmac_scan_proc_scan_comp_event::pst_event_mem null.}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 获取事件头和事件结构体指针 */
    pst_event     = (frw_event_stru *)pst_event_mem->puc_data;
    pst_event_hdr = &(pst_event->st_event_hdr);

    /* 获取hmac device */
    pst_hmac_device = hmac_res_get_mac_dev(pst_event_hdr->uc_device_id);
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_hmac_device))
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{hmac_scan_proc_scan_comp_event::pst_hmac_device null.}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    pst_d2h_scan_rsp_info = (mac_scan_rsp_stru *)(pst_event->auc_event_data);
    pst_scan_mgmt = &(pst_hmac_device->st_scan_mgmt);

    if ((pst_event_hdr->uc_vap_id != pst_scan_mgmt->st_scan_record_mgmt.uc_vap_id) ||
        (pst_d2h_scan_rsp_info->ull_cookie != pst_scan_mgmt->st_scan_record_mgmt.ull_cookie))
    {
        OAM_WARNING_LOG4(pst_event_hdr->uc_vap_id, OAM_SF_SCAN,
                "{hmac_scan_proc_scan_comp_event::Report vap(%d) Scan_complete(cookie %d), but there have anoter vap(%d) scaning(cookie %d) !}",
                pst_event_hdr->uc_vap_id,
                pst_d2h_scan_rsp_info->ull_cookie,
                pst_scan_mgmt->st_scan_record_mgmt.uc_vap_id,
                pst_scan_mgmt->st_scan_record_mgmt.ull_cookie);
        return OAL_SUCC;
    }

    OAM_WARNING_LOG1(pst_event_hdr->uc_vap_id, OAM_SF_SCAN, "{hmac_scan_proc_scan_comp_event::scan status:%d !}",
                     pst_d2h_scan_rsp_info->en_scan_rsp_status);

    /* 删除扫描超时保护定时器 */
    if ((OAL_TRUE == pst_scan_mgmt->st_scan_timeout.en_is_registerd)
        && (MAC_SCAN_PNO != pst_d2h_scan_rsp_info->en_scan_rsp_status))
    {
        /* PNO没有启动扫描定时器,考虑到取消PNO扫描,立即下发普通扫描,PNO扫描结束事件对随后的普通扫描的影响 */
        FRW_TIMER_IMMEDIATE_DESTROY_TIMER(&(pst_scan_mgmt->st_scan_timeout));
    }

    /* 获取hmac vap */
    pst_hmac_vap = mac_res_get_hmac_vap(pst_event_hdr->uc_vap_id);
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_hmac_vap))
    {
        OAM_ERROR_LOG0(pst_event_hdr->uc_vap_id, OAM_SF_SCAN, "{hmac_scan_proc_scan_comp_event::pst_hmac_vap null.}");

        /* 设置当前处于非扫描状态 */
        pst_scan_mgmt->en_is_scanning = OAL_FALSE;
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 根据当前扫描的类型和当前vap的状态，决定切换vap的状态，如果是前景扫描，才需要切换vap的状态 */
    if ((WLAN_VAP_MODE_BSS_STA == pst_hmac_vap->st_vap_base_info.en_vap_mode)
        && (MAC_SCAN_PNO != pst_d2h_scan_rsp_info->en_scan_rsp_status))
    {
        if (MAC_VAP_STATE_STA_WAIT_SCAN == pst_hmac_vap->st_vap_base_info.en_vap_state)
        {
            /* 改变vap状态到SCAN_COMP */
            hmac_fsm_change_state(pst_hmac_vap, MAC_VAP_STATE_STA_SCAN_COMP);
        }
        else if (MAC_VAP_STATE_UP == pst_hmac_vap->st_vap_base_info.en_vap_state)
        {
            /* 背景扫描时需要进行帧过滤的配置 */
            hmac_set_rx_filter_value(&(pst_hmac_vap->st_vap_base_info));
        }
    }

    hmac_scan_complete_result_to_sme(pst_scan_mgmt, pst_hmac_vap, pst_d2h_scan_rsp_info);

    /* 设置当前处于非扫描状态 */
    if(MAC_SCAN_PNO != pst_d2h_scan_rsp_info->en_scan_rsp_status)
    {
        /* PNO扫描没有置此位为OAL_TRUE,PNO扫描结束,不能影响随后的常规扫描 */
        pst_scan_mgmt->en_is_scanning = OAL_FALSE;
    }
#ifdef _PRE_WLAN_FEATURE_P2P
    if (MAC_VAP_STATE_STA_LISTEN == pst_hmac_vap->st_vap_base_info.en_vap_state)
    {
        hmac_p2p_listen_timeout(pst_hmac_vap, &pst_hmac_vap->st_vap_base_info);
    }
    if (OAL_TRUE == pst_hmac_vap->en_wait_roc_end)
    {
        OAM_WARNING_LOG1(pst_event_hdr->uc_vap_id, OAM_SF_SCAN, "{hmac_scan_proc_scan_comp_event::scan rsp status[%d]}", pst_d2h_scan_rsp_info->en_scan_rsp_status);
        OAL_COMPLETE(&(pst_hmac_vap->st_roc_end_ready));
        pst_hmac_vap->en_wait_roc_end = OAL_FALSE;
    }
#endif

    return OAL_SUCC;
}

oal_uint32  hmac_scan_proc_scan_req_event_exception(hmac_vap_stru *pst_hmac_vap, oal_void *p_params)
{
    frw_event_mem_stru         *pst_event_mem;
    frw_event_stru             *pst_event;
    hmac_scan_rsp_stru          st_scan_rsp;
    hmac_scan_rsp_stru         *pst_scan_rsp;

    if (OAL_UNLIKELY((OAL_PTR_NULL == pst_hmac_vap) || (OAL_PTR_NULL == p_params)))
    {
        OAM_ERROR_LOG2(0, OAM_SF_SCAN, "{hmac_scan_proc_scan_req_event_exception::param null, %x %x.}", (uintptr_t)pst_hmac_vap, (uintptr_t)p_params);
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 不支持发起扫描的状态发起了扫描 */
    OAM_WARNING_LOG1(pst_hmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN, "{hmac_scan_proc_scan_req_event_exception::vap state is=%x.}",
                     pst_hmac_vap->st_vap_base_info.en_vap_state);

    OAL_MEMZERO(&st_scan_rsp, OAL_SIZEOF(hmac_scan_rsp_stru));

    /* 抛扫描完成事件到WAL, 执行SCAN_DONE , 释放扫描请求内存 */
    pst_event_mem = FRW_EVENT_ALLOC(OAL_SIZEOF(hmac_scan_rsp_stru));
    if (OAL_PTR_NULL == pst_event_mem)
    {
        OAM_ERROR_LOG0(pst_hmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN, "{hmac_scan_proc_scan_req_event_exception::pst_event_mem null.}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    st_scan_rsp.uc_result_code = MAC_SCAN_REFUSED;
    st_scan_rsp.uc_num_dscr    = 0;
#ifdef _PRE_WLAN_FEATURE_ROAM
    /* When STA is roaming, scan req return success instead of failure,
       in case roaming failure which will cause UI scan list null  */
    if (pst_hmac_vap->st_vap_base_info.en_vap_state == MAC_VAP_STATE_ROAMING)
    {
        st_scan_rsp.uc_result_code = MAC_SCAN_SUCCESS;
    }
#endif

    /* 填写事件 */
    pst_event = (frw_event_stru *)pst_event_mem->puc_data;

    FRW_EVENT_HDR_INIT(&(pst_event->st_event_hdr),
                       FRW_EVENT_TYPE_HOST_CTX,
                       HMAC_HOST_CTX_EVENT_SUB_TYPE_SCAN_COMP_STA,
                       OAL_SIZEOF(hmac_scan_rsp_stru),
                       FRW_EVENT_PIPELINE_STAGE_0,
                       pst_hmac_vap->st_vap_base_info.uc_chip_id,
                       pst_hmac_vap->st_vap_base_info.uc_device_id,
                       pst_hmac_vap->st_vap_base_info.uc_vap_id);

    pst_scan_rsp = (hmac_scan_rsp_stru *)pst_event->auc_event_data;

    oal_memcopy(pst_scan_rsp, (oal_void *)(&st_scan_rsp), OAL_SIZEOF(hmac_scan_rsp_stru));

    /* 分发事件 */
    frw_event_dispatch_event(pst_event_mem);
    FRW_EVENT_FREE(pst_event_mem);

    return OAL_SUCC;
}



oal_void  hmac_scan_set_sour_mac_addr_in_probe_req(hmac_vap_stru        *pst_hmac_vap,
                                                   oal_uint8            *puc_sour_mac_addr,
                                                   oal_bool_enum_uint8   en_is_rand_mac_addr_scan,
                                                   oal_bool_enum_uint8   en_is_p2p0_scan)
{
    mac_device_stru          *pst_mac_device  = OAL_PTR_NULL;
    hmac_device_stru         *pst_hmac_device = OAL_PTR_NULL;

    if((OAL_PTR_NULL == pst_hmac_vap) || (OAL_PTR_NULL == puc_sour_mac_addr))
    {
        OAM_ERROR_LOG2(0, OAM_SF_CFG, "{hmac_scan_set_sour_mac_addr_in_probe_req::param null,pst_hmac_vap:%p,puc_sour_mac_addr:%p.}",
                       (uintptr_t)pst_hmac_vap, (uintptr_t)puc_sour_mac_addr);
        return;
    }

    pst_mac_device = mac_res_get_dev(pst_hmac_vap->st_vap_base_info.uc_device_id);
    if (OAL_PTR_NULL == pst_mac_device)
    {
        OAM_WARNING_LOG0(0, OAM_SF_CFG, "{hmac_scan_set_sour_mac_addr_in_probe_req::pst_mac_device is null.}");
        return;
    }
    pst_hmac_device = hmac_res_get_mac_dev(pst_hmac_vap->st_vap_base_info.uc_device_id);
    if (OAL_PTR_NULL == pst_hmac_device)
    {
        OAM_WARNING_LOG1(0, OAM_SF_CFG, "{hmac_scan_set_sour_mac_addr_in_probe_req::pst_hmac_device is null. device_id %d}",
                        pst_hmac_vap->st_vap_base_info.uc_device_id);
        return;
    }

#ifdef _PRE_WLAN_FEATURE_P2P
    /* WLAN/P2P 特性情况下，p2p0 和p2p-p2p0 cl 扫描时候，需要使用不同设备 */
    if (OAL_TRUE == en_is_p2p0_scan)
    {
        oal_set_mac_addr(puc_sour_mac_addr, pst_hmac_vap->st_vap_base_info.pst_mib_info->st_wlan_mib_sta_config.auc_p2p0_dot11StationID);
    }
    else
#endif /* _PRE_WLAN_FEATURE_P2P */
    {
        /* 如果随机mac addr扫描特性开启且非P2P场景，设置随机mac addr到probe req帧中 */
        if ((OAL_TRUE == en_is_rand_mac_addr_scan) && (IS_LEGACY_VAP(&(pst_hmac_vap->st_vap_base_info)))
            && ((pst_mac_device->auc_mac_oui[0] != 0) || (pst_mac_device->auc_mac_oui[1] != 0) || (pst_mac_device->auc_mac_oui[2] != 0)))
        {
            /* 系统会在wps扫描或hilink连接的场景中,将mac oui清0 */
            /* 更新随机mac 地址,使用下发随机MAC OUI 生成的随机mac 地址到本次扫描 */
            oal_set_mac_addr(puc_sour_mac_addr, pst_hmac_device->st_scan_mgmt.auc_random_mac);
        }
        else
        {
            /* 设置地址为自己的MAC地址 */
            oal_set_mac_addr(puc_sour_mac_addr, pst_hmac_vap->st_vap_base_info.pst_mib_info->st_wlan_mib_sta_config.auc_dot11StationID);
        }
    }

    return;
}


OAL_STATIC oal_uint32  hmac_scan_update_scan_params(hmac_vap_stru        *pst_hmac_vap,
                                                    mac_scan_req_stru    *pst_scan_params,
                                                    oal_bool_enum_uint8   en_is_random_mac_addr_scan)
{
    mac_device_stru             *pst_mac_device;
    mac_vap_stru                *pst_mac_vap_temp;
    oal_uint32                   ul_ret;
    wlan_vap_mode_enum_uint8     en_vap_mode;

    /* 获取mac device */
    pst_mac_device = mac_res_get_dev(pst_hmac_vap->st_vap_base_info.uc_device_id);
    if (OAL_PTR_NULL == pst_mac_device)
    {
        OAM_WARNING_LOG0(pst_hmac_vap->st_vap_base_info.uc_device_id, OAM_SF_SCAN,
                         "{hmac_scan_update_scan_params::pst_mac_device null.}");
        return OAL_ERR_CODE_MAC_DEVICE_NULL;
    }

    /* 1.记录发起扫描的vap id到扫描参数 */
    pst_scan_params->uc_vap_id    = pst_hmac_vap->st_vap_base_info.uc_vap_id;
    pst_scan_params->en_scan_mode = WLAN_SCAN_MODE_FOREGROUND;
    pst_scan_params->en_need_switch_back_home_channel = OAL_FALSE;

    /* 2.修改扫描模式和信道扫描次数: 根据是否存在up状态下的vap，如果是，则是背景扫描，如果不是，则是前景扫描 */
    ul_ret = mac_device_find_up_vap(pst_mac_device, &pst_mac_vap_temp);
    if ((OAL_SUCC == ul_ret) && (OAL_PTR_NULL != pst_mac_vap_temp))
    {
        /* 判断vap的类型，如果是sta则为sta的背景扫描，如果是ap，则是ap的背景扫描，其它类型的vap暂不支持背景扫描 */
        en_vap_mode = pst_hmac_vap->st_vap_base_info.en_vap_mode;
        if (WLAN_VAP_MODE_BSS_STA == en_vap_mode)
        {
            /* 修改扫描参数为sta的背景扫描 */
            pst_scan_params->en_scan_mode = WLAN_SCAN_MODE_BACKGROUND_STA;
        }
        else if (WLAN_VAP_MODE_BSS_AP == en_vap_mode)
        {
            /* 修改扫描参数为sta的背景扫描 */
            pst_scan_params->en_scan_mode = WLAN_SCAN_MODE_BACKGROUND_AP;
        }
        else
        {
            OAM_ERROR_LOG1(0, OAM_SF_SCAN, "{hmac_scan_update_scan_params::vap mode[%d], not support bg scan.}", en_vap_mode);
            return OAL_FAIL;
        }
        pst_scan_params->en_need_switch_back_home_channel = OAL_TRUE;

        if (1 == mac_device_calc_up_vap_num(pst_mac_device) && !IS_LEGACY_VAP(pst_mac_vap_temp) && IS_LEGACY_VAP(&pst_hmac_vap->st_vap_base_info))
        {
            /* 修改扫描信道间隔(2)和回工作信道工作时间(60ms):仅仅针对P2P处于关联状态，wlan处于去关联状态,wlan发起的扫描 */
            pst_scan_params->uc_scan_channel_interval       = MAC_SCAN_CHANNEL_INTERVAL_PERFORMANCE;
            pst_scan_params->us_work_time_on_home_channel   = MAC_WORK_TIME_ON_HOME_CHANNEL_PERFORMANCE;

            if((pst_scan_params->us_scan_time > WLAN_DEFAULT_ACTIVE_SCAN_TIME)
               && (WLAN_SCAN_TYPE_ACTIVE == pst_scan_params->en_scan_type))
            {
                /* 指定SSID扫描超过3个,会修改每次扫描时间为40ms(默认是20ms) */
                /* P2P关联但wlan未关联场景,考虑到扫描时间增加对p2p wfd场景的影响,设置每信道扫描次数为1次(默认为2次) */
                pst_scan_params->uc_max_scan_count_per_channel = 1;
            }
        }
        else
        {
            /* 其他情况默认扫描6个信道回home信道工作100ms */
            pst_scan_params->uc_scan_channel_interval       = MAC_SCAN_CHANNEL_INTERVAL_DEFAULT;
            pst_scan_params->us_work_time_on_home_channel   = MAC_WORK_TIME_ON_HOME_CHANNEL_DEFAULT;
        }
    }
    /* 3.设置发送的probe req帧中源mac addr */
    pst_scan_params->en_is_random_mac_addr_scan = en_is_random_mac_addr_scan;
    hmac_scan_set_sour_mac_addr_in_probe_req(pst_hmac_vap, pst_scan_params->auc_sour_mac_addr,
                                             en_is_random_mac_addr_scan, pst_scan_params->bit_is_p2p0_scan);

    /* do foreground scan and switch back while requested by main proxysta */
#ifdef _PRE_WLAN_FEATURE_PROXYSTA
    if (mac_vap_is_msta(&pst_hmac_vap->st_vap_base_info)
        && (pst_hmac_vap->st_vap_base_info.en_vap_state != MAC_VAP_STATE_UP))
    {
        pst_scan_params->en_need_switch_back_home_channel = OAL_TRUE;
        pst_scan_params->en_scan_mode = WLAN_SCAN_MODE_FOREGROUND;
    }
#endif

    return OAL_SUCC;
}


OAL_STATIC oal_uint32  hmac_scan_check_can_enter_scan_state(mac_vap_stru    *pst_mac_vap)
{
    /* p2p有可能进行监听动作，但是和scan req的优先级一样，因此当上层发起的是扫描请求时，统一可使用下面的接口判断 */
    return hmac_p2p_check_can_enter_state(pst_mac_vap, HMAC_FSM_INPUT_SCAN_REQ);
}


OAL_STATIC oal_uint32  hmac_scan_check_is_dispatch_scan_req(hmac_vap_stru       *pst_hmac_vap,
                                                            hmac_device_stru    *pst_hmac_device)
{
    oal_uint32  ul_ret;

    /* 1.先检测其它vap的状态从而判断是否可进入扫描状态，使得扫描尽量不打断其它的入网流程 */
    ul_ret = hmac_scan_check_can_enter_scan_state(&(pst_hmac_vap->st_vap_base_info));
    if (OAL_SUCC != ul_ret)
    {
        OAM_WARNING_LOG1(pst_hmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                         "{hmac_scan_check_is_dispatch_scan_req::Because of err_code[%d], can't enter into scan state.}", ul_ret);
        return ul_ret;
    }

    /* 2.判断当前扫描是否正在执行 */
    if (OAL_TRUE == pst_hmac_device->st_scan_mgmt.en_is_scanning)
    {
        OAM_WARNING_LOG0(pst_hmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN, "{hmac_scan_check_is_dispatch_scan_req::the scan request is rejected.}");
        return OAL_FAIL;
    }

#ifdef _PRE_WLAN_FEATURE_ROAM
    /* 3.判断当前是否正在执行漫游 */
    if (MAC_VAP_STATE_ROAMING == pst_hmac_vap->st_vap_base_info.en_vap_state)
    {
        OAM_WARNING_LOG0(pst_hmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN, "{hmac_scan_check_is_dispatch_scan_req:: roam reject new scan.}");
        return OAL_FAIL;
    }
#endif //_PRE_WLAN_FEATURE_ROAM

    return OAL_SUCC;
}


OAL_STATIC oal_void  hmac_scan_proc_last_scan_record(hmac_vap_stru       *pst_hmac_vap,
                                                              hmac_device_stru    *pst_hmac_device)
{
    /* 如果是proxysta发起的扫描无需清楚扫描结果 */
#ifdef _PRE_WLAN_FEATURE_PROXYSTA
    if (mac_is_proxysta_enabled(pst_hmac_device->pst_device_base_info))
    {
        if (mac_vap_is_vsta(&pst_hmac_vap->st_vap_base_info))
        {
            /* proxysta do nothing */
            OAM_INFO_LOG0(0, OAM_SF_SCAN, "{hmac_scan_proc_scan_req_event:: proxysta don't need clean scan record.}");
            return;
        }
    }
#endif

    OAM_INFO_LOG0(0, OAM_SF_SCAN, "{hmac_scan_proc_scan_req_event:: start clean last scan record.}");





    /* 本次扫描请求发起时，清除上一次扫描结果中过期的bss信息 */
    hmac_scan_clean_expire_scanned_bss(pst_hmac_vap, &(pst_hmac_device->st_scan_mgmt.st_scan_record_mgmt));


    return;
}



OAL_STATIC oal_uint32  hmac_scan_proc_scan_timeout_fn(void *p_arg)
{
    hmac_device_stru                   *pst_hmac_device = (hmac_device_stru *)p_arg;
    hmac_vap_stru                      *pst_hmac_vap    = OAL_PTR_NULL;
    hmac_scan_record_stru              *pst_scan_record = OAL_PTR_NULL;
    oal_uint32                          ul_pedding_data = 0;

    /* 获取扫描记录信息 */
    pst_scan_record = &(pst_hmac_device->st_scan_mgmt.st_scan_record_mgmt);

    /* 获取hmac vap */
    pst_hmac_vap = mac_res_get_hmac_vap(pst_scan_record->uc_vap_id);
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_hmac_vap))
    {
        OAM_ERROR_LOG0(pst_scan_record->uc_vap_id, OAM_SF_SCAN, "{hmac_scan_proc_scan_timeout_fn::pst_hmac_vap null.}");

        /* 扫描状态恢复为未在执行的状态 */
        pst_hmac_device->st_scan_mgmt.en_is_scanning = OAL_FALSE;
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 根据当前扫描的类型和当前vap的状态，决定切换vap的状态，如果是前景扫描，才需要切换vap的状态 */
    if (WLAN_VAP_MODE_BSS_STA == pst_hmac_vap->st_vap_base_info.en_vap_mode)
    {
        if (MAC_VAP_STATE_STA_WAIT_SCAN == pst_hmac_vap->st_vap_base_info.en_vap_state)
        {
            /* 改变vap状态到SCAN_COMP */
            hmac_fsm_change_state(pst_hmac_vap, MAC_VAP_STATE_STA_SCAN_COMP);
        }
        else if (MAC_VAP_STATE_UP == pst_hmac_vap->st_vap_base_info.en_vap_state)
        {
            /* 背景扫描时需要进行帧过滤的配置 */
            hmac_set_rx_filter_value(&(pst_hmac_vap->st_vap_base_info));
        }
    }

    if ((WLAN_VAP_MODE_BSS_AP == pst_hmac_vap->st_vap_base_info.en_vap_mode)
        && (MAC_VAP_STATE_BUTT != pst_scan_record->en_vap_last_state))
    {
        hmac_fsm_change_state(pst_hmac_vap, pst_scan_record->en_vap_last_state);
        pst_scan_record->en_vap_last_state = MAC_VAP_STATE_BUTT;
    }

    /* 设置扫描响应状态为超时 */
    pst_scan_record->en_scan_rsp_status = MAC_SCAN_TIMEOUT;
    OAM_WARNING_LOG1(pst_scan_record->uc_vap_id, OAM_SF_SCAN, "{hmac_scan_proc_scan_timeout_fn::scan time out cookie [%x].}", pst_scan_record->ull_cookie);

    /* 如果扫描回调函数不为空，则调用回调函数 */
    if (OAL_PTR_NULL != pst_scan_record->p_fn_cb)
    {
        OAM_WARNING_LOG0(pst_scan_record->uc_vap_id, OAM_SF_SCAN, "{hmac_scan_proc_scan_timeout_fn::scan callback func proc.}");
        pst_scan_record->p_fn_cb(pst_scan_record);
    }

    /* DMAC 超时未上报扫描完成，HMAC 下发扫描结束命令，停止DMAC 扫描 */
    hmac_config_scan_abort(&pst_hmac_vap->st_vap_base_info, OAL_SIZEOF(oal_uint32), (oal_uint8 *)&ul_pedding_data);

    /* 扫描状态恢复为未在执行的状态 */
    pst_hmac_device->st_scan_mgmt.en_is_scanning = OAL_FALSE;

    CHR_EXCEPTION(CHR_WIFI_DRV(CHR_WIFI_DRV_EVENT_SCAN,CHR_WIFI_DRV_ERROR_SCAN_TIMEOUT));

    return OAL_SUCC;

}


oal_uint32  hmac_scan_proc_scan_req_event(hmac_vap_stru *pst_hmac_vap, oal_void *p_params)
{
    frw_event_mem_stru         *pst_event_mem;
    frw_event_stru             *pst_event;
    mac_scan_req_stru          *pst_h2d_scan_req_params;     /* hmac发送到dmac的扫描请求参数 */
    mac_scan_req_stru          *pst_scan_params;
    hmac_device_stru           *pst_hmac_device;
    hmac_scan_record_stru      *pst_scan_record;
    oal_uint32                  ul_scan_timeout;
    oal_uint32                  ul_ret;

    /* 参数合法性检查 */
    if (OAL_UNLIKELY(OAL_ANY_NULL_PTR2(pst_hmac_vap,p_params)))
    {
        OAM_ERROR_LOG2(0, OAM_SF_SCAN, "{hmac_scan_proc_scan_req_event::param null, %p %p.}", (uintptr_t)pst_hmac_vap, (uintptr_t)p_params);
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 扫描停止模块测试 */
    if (((HMAC_BGSCAN_DISABLE == g_en_bgscan_enable_flag) && (MAC_VAP_STATE_UP == pst_hmac_vap->st_vap_base_info.en_vap_state))
        ||(HMAC_SCAN_DISABLE == g_en_bgscan_enable_flag))
    {
        OAM_WARNING_LOG1(0, OAM_SF_SCAN, "hmac_scan_proc_scan_req_event: g_en_bgscan_enable_flag= %d.", g_en_bgscan_enable_flag);
        return OAL_FAIL;
    }

    pst_scan_params = (mac_scan_req_stru *)p_params;

    /* 异常判断: 扫描的信道个数为0 */
    if (0 == pst_scan_params->uc_channel_nums)
    {
        OAM_WARNING_LOG0(pst_hmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN, "{hmac_scan_proc_scan_req_event::channel_nums=0.}");
        return OAL_FAIL;
    }

    /* 获取hmac device */
    pst_hmac_device = hmac_res_get_mac_dev(pst_hmac_vap->st_vap_base_info.uc_device_id);
    if (OAL_PTR_NULL == pst_hmac_device)
    {
        OAM_WARNING_LOG1(pst_hmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN, "{hmac_scan_proc_scan_req_event::pst_hmac_device[%d] null.}",
                        pst_hmac_vap->st_vap_base_info.uc_device_id);
        return OAL_ERR_CODE_MAC_DEVICE_NULL;
    }

    /* 更新此次扫描请求的扫描参数 */
    if (pst_scan_params->uc_scan_func == MAC_SCAN_FUNC_P2P_LISTEN)
    {
        ul_ret = hmac_scan_update_scan_params(pst_hmac_vap, pst_scan_params, OAL_FALSE);
    }
    else
    {
#ifdef _PRE_PLAT_FEATURE_CUSTOMIZE
        ul_ret = hmac_scan_update_scan_params(pst_hmac_vap, pst_scan_params, wlan_customize.uc_random_mac_addr_scan);
#else
        ul_ret = hmac_scan_update_scan_params(pst_hmac_vap, pst_scan_params, pst_hmac_device->st_scan_mgmt.en_is_random_mac_addr_scan);
#endif
    }
    if (OAL_SUCC != ul_ret)
    {
        OAM_WARNING_LOG1(pst_hmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                         "{hmac_scan_proc_scan_req_event::update scan mode failed, error[%d].}", ul_ret);
        return ul_ret;
    }

    /* 检测是否符合发起扫描请求的条件，如果不符合，直接返回 */
    ul_ret = hmac_scan_check_is_dispatch_scan_req(pst_hmac_vap, pst_hmac_device);
    if (OAL_SUCC != ul_ret)
    {
        if (MAC_SCAN_FUNC_P2P_LISTEN == pst_scan_params->uc_scan_func)
        {
            mac_vap_state_change(&pst_hmac_vap->st_vap_base_info, pst_hmac_device->pst_device_base_info->st_p2p_info.en_last_vap_state);
        }

        OAM_WARNING_LOG1(pst_hmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                         "{hmac_scan_proc_scan_req_event::Because of error[%d], can't dispatch scan req.}", ul_ret);
        return ul_ret;

    }

    /* 设置扫描模块处于扫描状态，其它扫描请求将丢弃 */
    pst_hmac_device->st_scan_mgmt.en_is_scanning = OAL_TRUE;

    /* 处理上一次扫描记录，目前直接清楚上一次结果，后续可能需要老化时间处理 */
    hmac_scan_proc_last_scan_record(pst_hmac_vap, pst_hmac_device);

    /* 记录扫描发起者的信息，某些模块回调函数使用 */
    pst_scan_record = &(pst_hmac_device->st_scan_mgmt.st_scan_record_mgmt);
    pst_scan_record->uc_chip_id      = pst_hmac_device->pst_device_base_info->uc_chip_id;
    pst_scan_record->uc_device_id    = pst_hmac_device->pst_device_base_info->uc_device_id;
    pst_scan_record->uc_vap_id       = pst_scan_params->uc_vap_id;
    pst_scan_record->uc_chan_numbers = pst_scan_params->uc_channel_nums;
    pst_scan_record->p_fn_cb         = pst_scan_params->p_fn_cb;

    if (WLAN_VAP_MODE_BSS_AP == pst_hmac_vap->st_vap_base_info.en_vap_mode)
    {
        OAM_WARNING_LOG1(pst_hmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                         "{hmac_scan_proc_scan_req_event::save last en_vap_state:%d}", pst_hmac_vap->st_vap_base_info.en_vap_state);

        pst_scan_record->en_vap_last_state = pst_hmac_vap->st_vap_base_info.en_vap_state;
    }

    pst_scan_record->ull_cookie      = pst_scan_params->ull_cookie;

    /* 记录扫描开始时间 */
    pst_scan_record->st_scan_start_time = oal_ktime_get();

    /* 抛扫描请求事件到DMAC, 申请事件内存 */
    pst_event_mem = FRW_EVENT_ALLOC(OAL_SIZEOF(mac_scan_req_stru));
    if (OAL_PTR_NULL == pst_event_mem)
    {
        OAM_ERROR_LOG1(pst_hmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                       "{hmac_scan_proc_scan_req_event::alloc memory(%u) failed.}", OAL_SIZEOF(mac_scan_req_stru));

        /* 恢复扫描状态为非运行状态 */
        pst_hmac_device->st_scan_mgmt.en_is_scanning = OAL_FALSE;
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 如果发起扫描的vap的模式为sta，并且，其关联状态为非up状态，且非p2p监听状态，则切换其扫描状态 */
    if ((WLAN_VAP_MODE_BSS_STA == pst_hmac_vap->st_vap_base_info.en_vap_mode) &&
        (MAC_SCAN_FUNC_P2P_LISTEN != pst_scan_params->uc_scan_func))
    {
        if (MAC_VAP_STATE_UP != pst_hmac_vap->st_vap_base_info.en_vap_state)
        {
            /* 切换vap的状态为WAIT_SCAN状态 */
            hmac_fsm_change_state(pst_hmac_vap, MAC_VAP_STATE_STA_WAIT_SCAN);
        }
        else
        {
            /* 背景扫描时需要进行帧过滤的配置 */
            pst_hmac_vap->st_vap_base_info.en_vap_state = MAC_VAP_STATE_STA_WAIT_SCAN;
            hmac_set_rx_filter_value(&(pst_hmac_vap->st_vap_base_info));
            pst_hmac_vap->st_vap_base_info.en_vap_state = MAC_VAP_STATE_UP;
        }
    }
    else if (WLAN_VAP_MODE_BSS_AP == pst_hmac_vap->st_vap_base_info.en_vap_mode)
    {
        mac_vap_state_enum_uint8 en_state_bak = pst_hmac_vap->st_vap_base_info.en_vap_state;
        pst_hmac_vap->st_vap_base_info.en_vap_state = MAC_VAP_STATE_AP_WAIT_START;
        hmac_set_rx_filter_value(&(pst_hmac_vap->st_vap_base_info));
        pst_hmac_vap->st_vap_base_info.en_vap_state = en_state_bak;
    }

    /* AP的启动扫描做特殊处理，当hostapd下发扫描请求时，VAP还处于INIT状态 */
    if ( (WLAN_VAP_MODE_BSS_AP == pst_hmac_vap->st_vap_base_info.en_vap_mode) &&
         (MAC_VAP_STATE_INIT == pst_hmac_vap->st_vap_base_info.en_vap_state) )
    {
        OAM_WARNING_LOG0(pst_hmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
            "{hmac_scan_proc_scan_req_event::ap startup scan.}");
        hmac_fsm_change_state(pst_hmac_vap, MAC_VAP_STATE_AP_WAIT_START);
    }

    /* 填写事件 */
    pst_event = (frw_event_stru *)pst_event_mem->puc_data;

    FRW_EVENT_HDR_INIT(&(pst_event->st_event_hdr),
                       FRW_EVENT_TYPE_WLAN_CTX,
                       DMAC_WLAN_CTX_EVENT_SUB_TYPE_SCAN_REQ,
                       OAL_SIZEOF(mac_scan_req_stru),
                       FRW_EVENT_PIPELINE_STAGE_1,
                       pst_hmac_vap->st_vap_base_info.uc_chip_id,
                       pst_hmac_vap->st_vap_base_info.uc_device_id,
                       pst_hmac_vap->st_vap_base_info.uc_vap_id);

    /* 直接传内容，屏蔽多产品的差异 */
    pst_h2d_scan_req_params = (mac_scan_req_stru *)(pst_event->auc_event_data);

    /* 拷贝扫描请求参数到事件data区域 */
    oal_memcopy(pst_h2d_scan_req_params, pst_scan_params, OAL_SIZEOF(mac_scan_req_stru));

    /* 打印扫描参数，测试使用 */
    /* 如果是P2P 发起监听，则设置HMAC 扫描超时时间为P2P 监听时间 */
    if (MAC_SCAN_FUNC_P2P_LISTEN == pst_scan_params->uc_scan_func)
    {
        ul_scan_timeout = pst_scan_params->us_scan_time * 2;
    }
    else
    {
        ul_scan_timeout = WLAN_DEFAULT_MAX_TIME_PER_SCAN;
    }
    hmac_scan_print_scan_params(pst_h2d_scan_req_params, &pst_hmac_vap->st_vap_base_info);

    /* 启动扫描保护定时器，防止因拋事件、核间通信失败等情况下的异常保护，定时器初步的超时时间为4.5秒 */
    FRW_TIMER_CREATE_TIMER(&(pst_hmac_device->st_scan_mgmt.st_scan_timeout),
                           hmac_scan_proc_scan_timeout_fn,
                           ul_scan_timeout,
                           pst_hmac_device,
                           OAL_FALSE,
                           OAM_MODULE_ID_HMAC,
                           pst_hmac_device->pst_device_base_info->ul_core_id);

    /* 分发事件 */
    frw_event_dispatch_event(pst_event_mem);
    FRW_EVENT_FREE(pst_event_mem);

    return OAL_SUCC;
}


oal_uint32  hmac_scan_proc_sched_scan_req_event(hmac_vap_stru *pst_hmac_vap, oal_void *p_params)
{
    frw_event_mem_stru         *pst_event_mem;
    frw_event_stru             *pst_event;
    hmac_device_stru           *pst_hmac_device;
    hmac_scan_record_stru      *pst_scan_record;
    mac_pno_scan_stru          *pst_pno_scan_params;
    oal_uint32                  ul_ret;

    /* 参数合法性检查 */
    if (OAL_UNLIKELY((OAL_PTR_NULL == pst_hmac_vap) || (OAL_PTR_NULL == p_params)))
    {
        OAM_ERROR_LOG2(0, OAM_SF_SCAN, "{hmac_scan_proc_sched_scan_req_event::param null, %p %p.}", (uintptr_t)pst_hmac_vap, (uintptr_t)p_params);
        return OAL_ERR_CODE_PTR_NULL;
    }

    pst_pno_scan_params = (mac_pno_scan_stru *)p_params;

    /* 判断PNO调度扫描下发的过滤的ssid个数小于等于0 */
    if (pst_pno_scan_params->l_ssid_count <= 0)
    {
        OAM_WARNING_LOG0(pst_hmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN, "{hmac_scan_proc_sched_scan_req_event::ssid_count <=0.}");
        return OAL_FAIL;
    }

    /* 获取hmac device */
    pst_hmac_device = hmac_res_get_mac_dev(pst_hmac_vap->st_vap_base_info.uc_device_id);
    if (OAL_PTR_NULL == pst_hmac_device)
    {
        OAM_WARNING_LOG1(pst_hmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN, "{hmac_scan_proc_sched_scan_req_event::pst_hmac_device[%d] null.}",
                        pst_hmac_vap->st_vap_base_info.uc_device_id);
        return OAL_ERR_CODE_MAC_DEVICE_NULL;
    }

    /* 检测是否符合发起扫描请求的条件，如果不符合，直接返回 */
    ul_ret = hmac_scan_check_is_dispatch_scan_req(pst_hmac_vap, pst_hmac_device);
    if (OAL_SUCC != ul_ret)
    {
        OAM_WARNING_LOG1(pst_hmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                         "{hmac_scan_proc_sched_scan_req_event::Because of error[%d], can't dispatch scan req.}", ul_ret);
        return ul_ret;
    }

    /* 清空上一次的扫描结果 */
    hmac_scan_proc_last_scan_record(pst_hmac_vap, pst_hmac_device);

    /* 记录扫描发起者的信息，某些模块回调函数使用 */
    pst_scan_record = &(pst_hmac_device->st_scan_mgmt.st_scan_record_mgmt);
    pst_scan_record->uc_chip_id   = pst_hmac_device->pst_device_base_info->uc_chip_id;
    pst_scan_record->uc_device_id = pst_hmac_device->pst_device_base_info->uc_device_id;
    pst_scan_record->uc_vap_id    = pst_hmac_vap->st_vap_base_info.uc_vap_id;
    pst_scan_record->p_fn_cb      = pst_pno_scan_params->p_fn_cb;

    /* 抛扫描请求事件到DMAC, 申请事件内存 */
    pst_event_mem = FRW_EVENT_ALLOC(OAL_SIZEOF(mac_pno_scan_stru *));
    if (OAL_PTR_NULL == pst_event_mem)
    {
        OAM_ERROR_LOG0(pst_hmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN, "{hmac_scan_proc_sched_scan_req_event::pst_event_mem null.}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 填写事件 */
    pst_event = (frw_event_stru *)pst_event_mem->puc_data;

    FRW_EVENT_HDR_INIT(&(pst_event->st_event_hdr),
                       FRW_EVENT_TYPE_WLAN_CTX,
                       DMAC_WLAN_CTX_EVENT_SUB_TYPE_SCHED_SCAN_REQ,
                       OAL_SIZEOF(mac_pno_scan_stru *),
                       FRW_EVENT_PIPELINE_STAGE_1,
                       pst_hmac_vap->st_vap_base_info.uc_chip_id,
                       pst_hmac_vap->st_vap_base_info.uc_device_id,
                       pst_hmac_vap->st_vap_base_info.uc_vap_id);

    /* 事件data域内携带PNO扫描请求参数 */
    oal_memcopy(frw_get_event_payload(pst_event_mem), (oal_uint8 *)&pst_pno_scan_params, OAL_SIZEOF(mac_pno_scan_stru *));

    /* 分发事件 */
    frw_event_dispatch_event(pst_event_mem);
    FRW_EVENT_FREE(pst_event_mem);

    return OAL_SUCC;
}


oal_uint32  hmac_scan_process_chan_result_event(frw_event_mem_stru *pst_event_mem)
{
    frw_event_stru                     *pst_event;
    frw_event_hdr_stru                 *pst_event_hdr;
    hmac_device_stru                   *pst_hmac_device;
    dmac_crx_chan_result_stru          *pst_chan_result_param;
    hmac_scan_record_stru              *pst_scan_record;
    oal_uint8                           uc_scan_idx;

    /* 获取事件信息 */
    pst_event               = (frw_event_stru *)pst_event_mem->puc_data;
    pst_event_hdr           = &(pst_event->st_event_hdr);
    pst_chan_result_param   = (dmac_crx_chan_result_stru *)(pst_event->auc_event_data);
    uc_scan_idx             = pst_chan_result_param->uc_scan_idx;

    /* 获取hmac device */
    pst_hmac_device = hmac_res_get_mac_dev(pst_event_hdr->uc_device_id);
    if (OAL_PTR_NULL == pst_hmac_device)
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{hmac_scan_process_chan_result_event::pst_hmac_device is null.}");
        return OAL_FAIL;
    }

    pst_scan_record = &(pst_hmac_device->st_scan_mgmt.st_scan_record_mgmt);

    /* 检查上报的索引是否合法 */
    if (uc_scan_idx >= pst_scan_record->uc_chan_numbers)
    {
        /* dmac上报的扫描结果超出了要扫描的信道个数 */
        OAM_WARNING_LOG2(0, OAM_SF_SCAN,
                         "{hmac_scan_process_chan_result_event::result from dmac error! scan_idx[%d], chan_numbers[%d].}",
                         uc_scan_idx, pst_scan_record->uc_chan_numbers);

        return OAL_FAIL;
    }

    pst_scan_record->ast_chan_results[uc_scan_idx] = pst_chan_result_param->st_chan_result;

    return OAL_SUCC;
}
#ifdef _PRE_WLAN_FEATURE_11K

oal_uint32 hmac_scan_rrm_proc_save_bss(mac_vap_stru *pst_mac_vap, oal_uint8 uc_len, oal_uint8 *puc_param)
{
    oal_netbuf_stru                 *pst_action_table_bcn_rpt;
    oal_uint8                       *puc_data;
    mac_user_stru                   *pst_mac_user;
    oal_uint16                       us_index = 0;
    mac_meas_rpt_ie_stru            *pst_meas_rpt_ie;
    mac_bcn_rpt_stru                *pst_bcn_rpt;
    mac_tx_ctl_stru                 *pst_tx_ctl;
    mac_vap_rrm_trans_req_info_stru *pst_trans_req_info;
    oal_uint32                       ul_ret;
    hmac_device_stru                *pst_hmac_device;
    hmac_bss_mgmt_stru              *pst_bss_mgmt;
    oal_dlist_head_stru             *pst_entry;
    hmac_scanned_bss_info           *pst_scanned_bss;
    oal_uint8                        uc_bss_idx = 0;
    hmac_vap_stru                   *pst_hmac_vap;
    oal_uint16                       us_len = 0;

    if (OAL_PTR_NULL == puc_param)
    {
        OAM_ERROR_LOG0(pst_mac_vap->uc_vap_id, OAM_SF_ASSOC, "{hmac_scan_rrm_proc_save_bss::puc_param null.}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 获取hmac vap */
    pst_hmac_vap = mac_res_get_hmac_vap(pst_mac_vap->uc_vap_id);
    if (OAL_PTR_NULL == pst_hmac_vap)
    {
        OAM_ERROR_LOG0(pst_mac_vap->uc_vap_id, OAM_SF_SCAN, "{hmac_scan_rrm_proc_save_bss_etc::pst_hmac_vap null.}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    pst_hmac_device = hmac_res_get_mac_dev(pst_mac_vap->uc_device_id);
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_hmac_device))
    {
        OAM_ERROR_LOG0(pst_mac_vap->uc_vap_id, OAM_SF_SCAN, "{hmac_scan_rrm_proc_save_bss::pst_hmac_device null.}");

        return OAL_ERR_CODE_PTR_NULL;
    }

    pst_mac_user = mac_res_get_mac_user(pst_mac_vap->uc_assoc_vap_id);
    if (OAL_PTR_NULL == pst_mac_user)
    {
        OAM_ERROR_LOG1(0, OAM_SF_TX, "{hmac_scan_rrm_proc_save_bss::pst_mac_user[%d] null.", pst_mac_vap->uc_assoc_vap_id);

        return OAL_ERR_CODE_PTR_NULL;
    }

    pst_trans_req_info = (mac_vap_rrm_trans_req_info_stru *)puc_param;

    pst_action_table_bcn_rpt = (oal_netbuf_stru *)OAL_MEM_NETBUF_ALLOC(OAL_NORMAL_NETBUF, WLAN_MEM_NETBUF_SIZE2, OAL_NETBUF_PRIORITY_MID);
    if(OAL_PTR_NULL == pst_action_table_bcn_rpt)
    {
        OAM_ERROR_LOG0(pst_mac_vap->uc_vap_id, OAM_SF_ASSOC, "{hmac_scan_rrm_proc_save_bss::pst_action_table_bcn_rpt null.}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    OAL_MEMZERO(oal_netbuf_cb(pst_action_table_bcn_rpt), OAL_NETBUF_CB_SIZE());

    puc_data = (oal_uint8 *)OAL_NETBUF_HEADER(pst_action_table_bcn_rpt);

    /*************************************************************************/
    /*                        Management Frame Format                        */
    /* --------------------------------------------------------------------  */
    /* |Frame Control|Duration|DA|SA|BSSID|Sequence Control|Frame Body|FCS|  */
    /* --------------------------------------------------------------------  */
    /* | 2           |2       |6 |6 |6    |2               |0 - 2312  |4  |  */
    /* --------------------------------------------------------------------  */
    /*                                                                       */
    /*************************************************************************/

    /*************************************************************************/
    /*                Set the fields in the frame header                     */
    /*************************************************************************/

    /* All the fields of the Frame Control Field are set to zero. Only the   */
    /* Type/Subtype field is set.                                            */
    mac_hdr_set_frame_control(puc_data, WLAN_PROTOCOL_VERSION| WLAN_FC0_TYPE_MGT | WLAN_FC0_SUBTYPE_ACTION);

    /* duration */
    puc_data[2] = 0;
    puc_data[3] = 0;

    /* DA is address of STA requesting association */
    oal_set_mac_addr(puc_data + 4, pst_mac_user->auc_user_mac_addr);

    /* SA is the dot11MACAddress */
    oal_set_mac_addr(puc_data + 10, pst_mac_vap->pst_mib_info->st_wlan_mib_sta_config.auc_dot11StationID);

    oal_set_mac_addr(puc_data + 16, pst_mac_vap->auc_bssid);

    /* seq control */
    puc_data[22] = 0;
    puc_data[23] = 0;

    /*************************************************************************/
    /*                    Radio Measurement Report Frame - Frame Body        */
    /* --------------------------------------------------------------------- */
    /* |Category |Action |Dialog Token| Measurement Report Elements         |*/
    /* --------------------------------------------------------------------- */
    /* |1        |1      | 1          |  var                                 */
    /* --------------------------------------------------------------------- */
    /*                                                                       */
    /*************************************************************************/

    /* Initialize index and the frame data pointer */
    us_index = MAC_80211_FRAME_LEN;

    /* Category */
    puc_data[us_index++] = MAC_ACTION_CATEGORY_RADIO_MEASURMENT;

    /* Action */
    puc_data[us_index++] = MAC_RM_ACTION_RADIO_MEASUREMENT_REPORT;

    /* Dialog Token  */
    puc_data[us_index++]  = pst_trans_req_info->uc_action_dialog_token;

    us_len = us_index;

    /* 获取管理扫描的bss结果的结构体 */
    pst_bss_mgmt = &(pst_hmac_device->st_scan_mgmt.st_scan_record_mgmt.st_bss_mgmt);

    /* 11k table模式skb 内存使用可能存在越界*/
    pst_meas_rpt_ie = (mac_meas_rpt_ie_stru *)(puc_data + us_index);

    if ((WLAN_MEM_NETBUF_SIZE2 - us_len) < (MAC_MEASUREMENT_RPT_FIX_LEN + MAC_BEACON_RPT_FIX_LEN))
    {
        /* 释放bcn_rpt内存 */
        oal_netbuf_free(pst_action_table_bcn_rpt);
        return OAL_SUCC;
    }

    /* 对链表删操作前加锁 */
    oal_spin_lock(&(pst_bss_mgmt->st_lock));

    OAL_DLIST_SEARCH_FOR_EACH(pst_entry, &(pst_bss_mgmt->st_bss_list_head))
    {
        pst_scanned_bss = OAL_DLIST_GET_ENTRY(pst_entry, hmac_scanned_bss_info, st_dlist_head);
        if (0 == oal_memcmp(pst_scanned_bss->st_bss_dscr_info.ac_ssid, pst_trans_req_info->auc_ssid, pst_trans_req_info->us_ssid_len))
        {
            /*************************************************************************/
            /*                   Measurement Report IE - Frame Body                  */
            /* --------------------------------------------------------------------- */
            /* |Element ID |Length |Meas Token| Meas Rpt Mode | Meas Type | Meas Rpt|*/
            /* --------------------------------------------------------------------- */
            /* |1          |1      | 1        |  1            | 1         | var      */
            /* --------------------------------------------------------------------- */
            /*                                                                       */
            /*************************************************************************/
            pst_meas_rpt_ie->uc_eid = MAC_EID_MEASREP;
            pst_meas_rpt_ie->uc_len = 3 + MAC_BEACON_RPT_FIX_LEN;
            pst_meas_rpt_ie->uc_token = pst_trans_req_info->uc_meas_token;
            OAL_MEMZERO(&(pst_meas_rpt_ie->st_rptmode), OAL_SIZEOF(mac_meas_rpt_mode_stru));
            pst_meas_rpt_ie->uc_rpttype = 5;

            pst_bcn_rpt = (mac_bcn_rpt_stru *)pst_meas_rpt_ie->auc_meas_rpt;
            OAL_MEMZERO(pst_bcn_rpt, OAL_SIZEOF(mac_bcn_rpt_stru));
            oal_memcopy(pst_bcn_rpt->auc_bssid, pst_scanned_bss->st_bss_dscr_info.auc_bssid, WLAN_MAC_ADDR_LEN);
            pst_bcn_rpt->uc_channum = pst_scanned_bss->st_bss_dscr_info.st_channel.uc_chan_number;
            if(0 != pst_hmac_vap->bit_11k_auth_oper_class)
            {
                pst_bcn_rpt->uc_optclass = pst_hmac_vap->bit_11k_auth_oper_class;
            }
            else
            {
                pst_bcn_rpt->uc_optclass = pst_trans_req_info->uc_oper_class;
            }

            us_len += (MAC_MEASUREMENT_RPT_FIX_LEN + MAC_BEACON_RPT_FIX_LEN);
            uc_bss_idx++;
            if ((WLAN_MEM_NETBUF_SIZE2 - us_len) < (MAC_MEASUREMENT_RPT_FIX_LEN + MAC_BEACON_RPT_FIX_LEN))
            {
                break;
            }
            pst_meas_rpt_ie = (mac_meas_rpt_ie_stru *)pst_bcn_rpt->auc_subelm;
        }
    }
    oal_spin_unlock(&(pst_bss_mgmt->st_lock));

    OAM_WARNING_LOG3(pst_mac_vap->uc_vap_id, OAM_SF_ANY, "{hmac_scan_rrm_proc_save_bss:: Find BSS Num = [%d],us_len=[%d], buf_size=[%d].}",uc_bss_idx, us_len, WLAN_MEM_NETBUF_SIZE2);

    pst_tx_ctl = (mac_tx_ctl_stru *)oal_netbuf_cb(pst_action_table_bcn_rpt);
    pst_tx_ctl->us_mpdu_len  = us_len;
    /* 获取发送user_idex */
    ul_ret = mac_vap_set_cb_tx_user_idx(pst_mac_vap, pst_tx_ctl, pst_mac_user->auc_user_mac_addr);
    if (OAL_SUCC != ul_ret)
    {
        OAM_WARNING_LOG3(pst_mac_vap->uc_vap_id, OAM_SF_ANY, "(hmac_scan_rrm_proc_save_bss_etc::fail to find user by xx:xx:xx:0x:0x:0x.}",
        pst_mac_user->auc_user_mac_addr[3],
        pst_mac_user->auc_user_mac_addr[4],
        pst_mac_user->auc_user_mac_addr[5]);
    }

    if (MAC_GET_CB_MPDU_LEN(pst_tx_ctl) > WLAN_MEM_NETBUF_SIZE2)
    {
        oal_netbuf_free(pst_action_table_bcn_rpt);
        OAM_WARNING_LOG1(pst_mac_vap->uc_vap_id, OAM_SF_ANY, "{hmac_scan_rrm_proc_save_bss_etc::invalid us_len=[%d].}",
        MAC_GET_CB_MPDU_LEN(pst_tx_ctl));
        return OAL_SUCC;
    }

    oal_netbuf_put(pst_action_table_bcn_rpt, pst_tx_ctl->us_mpdu_len);

    ul_ret = hmac_tx_mgmt_send_event(pst_mac_vap, pst_action_table_bcn_rpt, pst_tx_ctl->us_mpdu_len);
    if (OAL_SUCC != ul_ret)
    {
        oal_netbuf_free(pst_action_table_bcn_rpt);
        OAM_WARNING_LOG1(pst_mac_vap->uc_vap_id, OAM_SF_ANY, "{hmac_scan_rrm_proc_save_bss::hmac_tx_mgmt_send_event failed[%d].}", ul_ret);
    }

    return OAL_SUCC;
}
#endif

oal_void  hmac_scan_init(hmac_device_stru *pst_hmac_device)
{
    hmac_scan_stru      *pst_scan_mgmt;
    hmac_bss_mgmt_stru  *pst_bss_mgmt;

    /* 初始化扫描管理结构体信息 */
    pst_scan_mgmt = &(pst_hmac_device->st_scan_mgmt);
    OAL_MEMZERO(pst_scan_mgmt, OAL_SIZEOF(hmac_scan_stru));
    pst_scan_mgmt->en_is_scanning = OAL_FALSE;
    pst_scan_mgmt->st_scan_record_mgmt.en_vap_last_state = MAC_VAP_STATE_BUTT;

    /* 初始化bss管理结果链表和锁 */
    pst_bss_mgmt = &(pst_scan_mgmt->st_scan_record_mgmt.st_bss_mgmt);
    oal_dlist_init_head(&(pst_bss_mgmt->st_bss_list_head));
    oal_spin_lock_init(&(pst_bss_mgmt->st_lock));

    /* 初始化内核下发扫描request资源锁 */
    oal_spin_lock_init(&(pst_scan_mgmt->st_scan_request_spinlock));

    /* 初始化 st_wiphy_mgmt 结构 */
    OAL_WAIT_QUEUE_INIT_HEAD(&(pst_scan_mgmt->st_wait_queue));
    /* 初始化扫描生成随机MAC 地址 */
    oal_random_ether_addr(pst_hmac_device->st_scan_mgmt.auc_random_mac);

#if  defined(_PRE_WLAN_CHIP_TEST_ALG) && (_PRE_OS_VERSION_LINUX == _PRE_OS_VERSION) && defined(_PRE_DEBUG_MODE)
    hmac_scan_ct_init();
#endif

    return;
}


oal_void  hmac_scan_exit(hmac_device_stru *pst_hmac_device)
{
    hmac_scan_stru      *pst_scan_mgmt;

    /* 清空扫描记录信息 */
    hmac_scan_clean_scan_record(&(pst_hmac_device->st_scan_mgmt.st_scan_record_mgmt));

    /* 清除扫描管理结构体信息 */
    pst_scan_mgmt = &(pst_hmac_device->st_scan_mgmt);
    OAL_MEMZERO(pst_scan_mgmt, OAL_SIZEOF(hmac_scan_stru));
    pst_scan_mgmt->en_is_scanning = OAL_FALSE;

#if  defined(_PRE_WLAN_CHIP_TEST_ALG) && (_PRE_OS_VERSION_LINUX == _PRE_OS_VERSION) && defined(_PRE_DEBUG_MODE)
    hmac_scan_ct_exit();
#endif

    return;
}


#ifdef _PRE_DEBUG_MODE

oal_void  hmac_scan_test_cb(void *p_scan_record)
{
    hmac_scan_record_stru           *pst_scan_record;
    hmac_bss_mgmt_stru              *pst_bss_mgmt;
    mac_bss_dscr_stru               *pst_bss_dscr;
    hmac_scanned_bss_info           *pst_scanned_bss;
    oal_dlist_head_stru             *pst_entry;
    oal_time_us_stru                 st_curr_time;
    oal_uint8                        uc_idx;

    if (OAL_PTR_NULL == p_scan_record)
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "pst_scan_record is null ptr.");
        return;
    }

    pst_scan_record = (hmac_scan_record_stru *)p_scan_record;

    /* 打印信道测量结果 */
    OAL_IO_PRINT("The chan measure result: \n");
    for (uc_idx = 0; uc_idx < pst_scan_record->uc_chan_numbers; uc_idx++)
    {
        OAL_IO_PRINT("[channel_result]Chan num      : %d\n", pst_scan_record->ast_chan_results[uc_idx].uc_channel_number);
        OAL_IO_PRINT("[channel_result]Stats cnt     : %d\n", pst_scan_record->ast_chan_results[uc_idx].uc_stats_cnt);
        OAL_IO_PRINT("[channel_result]Stats valid   : %d\n", pst_scan_record->ast_chan_results[uc_idx].uc_stats_valid);
        OAL_IO_PRINT("[channel_result]Stats time us : %d\n", pst_scan_record->ast_chan_results[uc_idx].ul_total_stats_time_us);
        OAL_IO_PRINT("[channel_result]Free time 20M : %d\n", pst_scan_record->ast_chan_results[uc_idx].ul_total_free_time_20M_us);
        OAL_IO_PRINT("[channel_result]Free time 40M : %d\n", pst_scan_record->ast_chan_results[uc_idx].ul_total_free_time_40M_us);
        OAL_IO_PRINT("[channel_result]Free time 80M : %d\n", pst_scan_record->ast_chan_results[uc_idx].ul_total_free_time_80M_us);
        OAL_IO_PRINT("[channel_result]Send time     : %d\n", pst_scan_record->ast_chan_results[uc_idx].ul_total_send_time_us);
        OAL_IO_PRINT("[channel_result]Recv time     : %d\n", pst_scan_record->ast_chan_results[uc_idx].ul_total_recv_time_us);
        OAL_IO_PRINT("[channel_result]Free power cnt: %d\n", pst_scan_record->ast_chan_results[uc_idx].uc_free_power_cnt);
        OAL_IO_PRINT("[channel_result]Free power 20M: %d\n", pst_scan_record->ast_chan_results[uc_idx].s_free_power_stats_20M);
        OAL_IO_PRINT("[channel_result]Free power 40M: %d\n", pst_scan_record->ast_chan_results[uc_idx].s_free_power_stats_40M);
        OAL_IO_PRINT("[channel_result]Free power 80M: %d\n", pst_scan_record->ast_chan_results[uc_idx].s_free_power_stats_80M);
        OAL_IO_PRINT("[channel_result]Radar detected: %d\n", pst_scan_record->ast_chan_results[uc_idx].uc_radar_detected);
        OAL_IO_PRINT("[channel_result]Radar bw      : %d\n\n", pst_scan_record->ast_chan_results[uc_idx].uc_radar_bw);
    }

    /* 打印BSS结果 */
    OAL_IO_PRINT("The bss result: \n");

    /* 获取扫描结果的管理结构地址 */
    pst_bss_mgmt = &(pst_scan_record->st_bss_mgmt);

    /* 获取锁 */
    oal_spin_lock(&(pst_bss_mgmt->st_lock));

    /* 遍历扫描到的bss信息 */
    OAL_DLIST_SEARCH_FOR_EACH(pst_entry, &(pst_bss_mgmt->st_bss_list_head))
    {
        pst_scanned_bss = OAL_DLIST_GET_ENTRY(pst_entry, hmac_scanned_bss_info, st_dlist_head);
        pst_bss_dscr    = &(pst_scanned_bss->st_bss_dscr_info);
        if ((OAL_PTR_NULL == pst_bss_dscr) || (OAL_PTR_NULL == pst_bss_dscr->auc_mgmt_buff))
        {
            continue;
        }

        OAL_IO_PRINT("Chan num      : %d\n", pst_bss_dscr->st_channel.uc_chan_number);
        OAL_IO_PRINT("BSSID         : 0x%x:0x%x:0x%x:0x%x:0x%x:0x%x\n\n", pst_bss_dscr->auc_bssid[0],
                                                                          pst_bss_dscr->auc_bssid[1],
                                                                          pst_bss_dscr->auc_bssid[2],
                                                                          pst_bss_dscr->auc_bssid[3],
                                                                          pst_bss_dscr->auc_bssid[4],
                                                                          pst_bss_dscr->auc_bssid[5]);
    }

	/* 释放锁 */
    oal_spin_unlock(&(pst_bss_mgmt->st_lock));

    /* 打印此次扫描耗时 */
    oal_time_get_stamp_us(&st_curr_time);
    OAL_IO_PRINT("Scan start time: %d %d\n", (oal_uint32)pst_scan_record->st_scan_start_timestamp.i_sec, (oal_uint32)pst_scan_record->st_scan_start_timestamp.i_usec);
    OAL_IO_PRINT("Scan end   time: %d %d\n\n", (oal_uint32)st_curr_time.i_sec, (oal_uint32)st_curr_time.i_usec);

    return;
}


oal_uint32  hmac_scan_test(mac_vap_stru *pst_mac_vap, oal_uint16 us_len, oal_uint8 *puc_param)
{
    mac_scan_req_stru   st_scan_req;
    oal_uint8           uc_chan_idx;
    oal_uint32          ul_ret;
    oal_uint8           uc_2g_chan_num = 0;
    oal_uint8           uc_5g_chan_num = 0;
    oal_uint8           uc_chan_number = 0;
    oal_int8            ac_param[15] = {0};
    oal_bool_enum_uint8 en_2g_all = OAL_FALSE;
    oal_bool_enum_uint8 en_5g_all = OAL_FALSE;
    mac_ioctl_scan_test_config_stru *pst_scan_test;
    hmac_vap_stru                   *pst_hmac_vap;

    pst_scan_test = (mac_ioctl_scan_test_config_stru *)puc_param;

    oal_memcopy(ac_param, pst_scan_test->ac_scan_type, sizeof(pst_scan_test->ac_scan_type));
    OAL_MEMZERO(&st_scan_req, OAL_SIZEOF(st_scan_req));

    if (0 == oal_strcmp("2g", ac_param))
    {
        en_2g_all = OAL_TRUE;
    }
    else if (0 == oal_strcmp("5g", ac_param))
    {
        en_5g_all = OAL_TRUE;
    }
    else if (0 == oal_strcmp("all", ac_param))
    {
        en_2g_all = OAL_TRUE;
        en_5g_all = OAL_TRUE;
    }
    else
    {
        /* 指定单个信道 */
        uc_chan_number = (oal_uint8)oal_atoi(ac_param);
    }

    if (en_2g_all)
    {
        /* 2G全信道 */
        for (uc_chan_idx = 0; uc_chan_idx < MAC_CHANNEL_FREQ_2_BUTT; uc_chan_idx++)
        {
            ul_ret = mac_is_channel_idx_valid(WLAN_BAND_2G, uc_chan_idx);
            if (OAL_SUCC == ul_ret)
            {
                mac_get_channel_num_from_idx(WLAN_BAND_2G, uc_chan_idx, &uc_chan_number);

                st_scan_req.ast_channel_list[uc_2g_chan_num].uc_chan_number = uc_chan_number;
                st_scan_req.ast_channel_list[uc_2g_chan_num].en_band        = WLAN_BAND_2G;
                st_scan_req.ast_channel_list[uc_2g_chan_num].uc_idx         = uc_chan_idx;
                st_scan_req.ast_channel_list[uc_2g_chan_num].en_bandwidth   = pst_scan_test->en_bandwidth;
                st_scan_req.uc_channel_nums++;
                uc_2g_chan_num++;
            }
        }
    }

    if (en_5g_all)
    {
        /* 5G全信道 */
        for (uc_chan_idx = 0; uc_chan_idx < MAC_CHANNEL_FREQ_5_BUTT; uc_chan_idx++)
        {
            ul_ret = mac_is_channel_idx_valid(WLAN_BAND_5G, uc_chan_idx);
            if (OAL_SUCC == ul_ret)
            {
                mac_get_channel_num_from_idx(WLAN_BAND_5G, uc_chan_idx, &uc_chan_number);

                st_scan_req.ast_channel_list[uc_2g_chan_num + uc_5g_chan_num].uc_chan_number = uc_chan_number;
                st_scan_req.ast_channel_list[uc_2g_chan_num + uc_5g_chan_num].en_band        = WLAN_BAND_5G;
                st_scan_req.ast_channel_list[uc_2g_chan_num + uc_5g_chan_num].uc_idx         = uc_chan_idx;
                st_scan_req.ast_channel_list[uc_2g_chan_num + uc_5g_chan_num].en_bandwidth   = pst_scan_test->en_bandwidth;
                st_scan_req.uc_channel_nums++;
                uc_5g_chan_num++;
            }
        }
    }

    if (OAL_FALSE == en_2g_all && OAL_FALSE == en_5g_all)
    {
        if (uc_chan_number < 15)
        {
            ul_ret = mac_is_channel_num_valid(WLAN_BAND_2G, uc_chan_number);
            if (OAL_SUCC != ul_ret)
            {
                OAL_IO_PRINT("Invalid chan num: %d. return!\n", uc_chan_number);
                return OAL_FAIL;
            }
            st_scan_req.ast_channel_list[0].en_band        = WLAN_BAND_2G;
        }
        else
        {
            ul_ret = mac_is_channel_num_valid(WLAN_BAND_5G, uc_chan_number);
            if (OAL_SUCC != ul_ret)
            {
                OAL_IO_PRINT("Invalid chan num: %d. return!\n", uc_chan_number);
                return OAL_FAIL;
            }
            st_scan_req.ast_channel_list[0].en_band        = WLAN_BAND_5G;
        }

        st_scan_req.ast_channel_list[0].uc_chan_number = uc_chan_number;
        st_scan_req.ast_channel_list[0].en_bandwidth   = pst_scan_test->en_bandwidth;

        mac_get_channel_idx_from_num(st_scan_req.ast_channel_list[0].en_band,
                                     st_scan_req.ast_channel_list[0].uc_chan_number,
                                     &(st_scan_req.ast_channel_list[0].uc_idx));

        st_scan_req.uc_channel_nums                    = 1;
    }

    st_scan_req.en_bss_type  = WLAN_MIB_DESIRED_BSSTYPE_INFRA;
    st_scan_req.en_scan_type = WLAN_SCAN_TYPE_ACTIVE;
    st_scan_req.uc_bssid_num = 0;
    st_scan_req.uc_ssid_num  = 0;

    oal_set_mac_addr(st_scan_req.auc_bssid[0], BROADCAST_MACADDR);
    st_scan_req.uc_bssid_num = 1;

    st_scan_req.us_scan_time = WLAN_DEFAULT_ACTIVE_SCAN_TIME;
    st_scan_req.en_scan_mode = WLAN_SCAN_MODE_FOREGROUND;
    st_scan_req.uc_scan_func = MAC_SCAN_FUNC_ALL;

    st_scan_req.p_fn_cb = hmac_scan_test_cb;

    pst_hmac_vap = mac_res_get_hmac_vap(pst_mac_vap->uc_vap_id);

    return hmac_scan_proc_scan_req_event(pst_hmac_vap, &st_scan_req);
}
#endif


oal_uint32  hmac_bgscan_enable(mac_vap_stru *pst_mac_vap, oal_uint16 us_len, oal_uint8 *puc_param)
{
    hmac_scan_state_enum_uint8    pen_bgscan_enable_flag;

    pen_bgscan_enable_flag =  *puc_param;                 /*背景扫描停止使能位*/

    /* 背景扫描停止命令 */
    switch (pen_bgscan_enable_flag)
    {
        case 0:
            g_en_bgscan_enable_flag = HMAC_BGSCAN_DISABLE;
            break;
        case 1:
            g_en_bgscan_enable_flag = HMAC_BGSCAN_ENABLE;
            break;
        case 2:
            g_en_bgscan_enable_flag = HMAC_SCAN_DISABLE;
            break;
        default:
            g_en_bgscan_enable_flag = HMAC_BGSCAN_ENABLE;
            break;
    }

    OAM_WARNING_LOG1(0, OAM_SF_SCAN, "hmac_bgscan_enable: g_en_bgscan_enable_flag= %d.", g_en_bgscan_enable_flag);

    return OAL_SUCC;
}

#ifdef _PRE_WLAN_FEATURE_DBAC

oal_uint32   hmac_scan_start_dbac(mac_device_stru *pst_dev)
{
    oal_uint8   auc_cmd[32];
    oal_uint16  us_len;
    oal_uint32  ul_ret = OAL_FAIL;
    oal_uint8   uc_idx;
#define DBAC_START_STR     " dbac start"
#define DBAC_START_STR_LEN 11
    mac_vap_stru  *pst_mac_vap = OAL_PTR_NULL;

    mac_ioctl_alg_config_stru  *pst_alg_config = (mac_ioctl_alg_config_stru *)auc_cmd;

    oal_memcopy(auc_cmd + OAL_SIZEOF(mac_ioctl_alg_config_stru), (const oal_int8 *)DBAC_START_STR, 11);
    auc_cmd[OAL_SIZEOF(mac_ioctl_alg_config_stru) + DBAC_START_STR_LEN ] = 0;

    pst_alg_config->uc_argc = 2;
    pst_alg_config->auc_argv_offset[0] = 1;
    pst_alg_config->auc_argv_offset[1] = 6;

    for(uc_idx = 0; uc_idx < pst_dev->uc_vap_num; uc_idx++)
    {
        pst_mac_vap = mac_res_get_mac_vap(pst_dev->auc_vap_id[uc_idx]);
        if(OAL_PTR_NULL != pst_mac_vap &&
        WLAN_VAP_MODE_BSS_AP == pst_mac_vap->en_vap_mode)
        {
            break;
        }
    }
    if(pst_mac_vap)
    {
        us_len = OAL_SIZEOF(mac_ioctl_alg_config_stru) + DBAC_START_STR_LEN + 1;
        ul_ret = hmac_config_send_event(pst_mac_vap, WLAN_CFGID_ALG, us_len, auc_cmd);
        if (OAL_UNLIKELY(OAL_SUCC != ul_ret))
        {
            OAM_WARNING_LOG1(pst_mac_vap->uc_vap_id, OAM_SF_CFG, "{hmac_config_alg::hmac_config_send_event failed[%d].}", ul_ret);
        }
        OAL_IO_PRINT("start dbac\n");
    }
    else
    {
        OAL_IO_PRINT("no vap found to start dbac\n");
    }

    return ul_ret;
}
#endif

oal_uint32 hmac_start_all_bss_of_device(hmac_device_stru *pst_hmac_dev)
{
    oal_uint8      uc_idx;
    hmac_vap_stru *pst_hmac_vap;
    mac_device_stru *pst_dev = pst_hmac_dev->pst_device_base_info;

    OAM_WARNING_LOG1(0, OAM_SF_ACS, "{hmac_start_all_bss_of_device:device id=%d}", pst_hmac_dev->pst_device_base_info->uc_device_id);
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_dev))
    {
        OAM_ERROR_LOG0(0, OAM_SF_ANY, "{hmac_start_all_bss_of_device::pst_device_base_info null!}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    for(uc_idx = 0; uc_idx < pst_dev->uc_vap_num; uc_idx++)
    {
        pst_hmac_vap = mac_res_get_hmac_vap(pst_dev->auc_vap_id[uc_idx]);
        if (!pst_hmac_vap)
        {
            OAM_WARNING_LOG2(0, OAM_SF_ACS, "hmac_start_all_bss_of_device:null ap, idx=%d id=%d", uc_idx, pst_dev->auc_vap_id[uc_idx]);
            continue;
        }

        if(OAL_PTR_NULL != pst_hmac_vap &&
            (MAC_VAP_STATE_AP_WAIT_START == pst_hmac_vap->st_vap_base_info.en_vap_state
#ifdef _PRE_WLAN_FEATURE_DBAC
            || (mac_is_dbac_enabled(pst_dev) && MAC_VAP_STATE_PAUSE== pst_hmac_vap->st_vap_base_info.en_vap_state)
#endif
            || MAC_VAP_STATE_UP == pst_hmac_vap->st_vap_base_info.en_vap_state))
        {
            if(OAL_SUCC == hmac_chan_start_bss(pst_hmac_vap, OAL_PTR_NULL, WLAN_PROTOCOL_BUTT))
            {
                OAM_WARNING_LOG4(0, OAM_SF_ACS, "start vap %d on ch=%d band=%d bw=%d\n",
                    pst_dev->auc_vap_id[uc_idx],
                    pst_hmac_vap->st_vap_base_info.st_channel.uc_chan_number,
                    pst_hmac_vap->st_vap_base_info.st_channel.en_band,
                    pst_hmac_vap->st_vap_base_info.st_channel.en_bandwidth);
            }
        }
        else
        {
            OAM_WARNING_LOG0(0, OAM_SF_ACS, "start vap error\n");
            continue;
        }
    }

#ifdef _PRE_WLAN_FEATURE_DBAC
    if (mac_is_dbac_enabled(pst_dev))
    {
        return hmac_scan_start_dbac(pst_dev);
    }
#endif

    return OAL_SUCC;
}

/* TBD 此函数随着scan模块上移打桩于此，需要ACS重写此接口 放到hmac_acs文件中 */
#ifdef _PRE_WLAN_FEATURE_20_40_80_COEXIST

OAL_STATIC oal_uint32 hmac_get_pri_sec_chan(mac_bss_dscr_stru *pst_bss_dscr, oal_uint32 *pul_pri_chan, oal_uint32 *pul_sec_chan)
{
    *pul_pri_chan = *pul_sec_chan = 0;

    *pul_pri_chan = pst_bss_dscr->st_channel.uc_chan_number;

    OAM_INFO_LOG1(0, OAM_SF_2040, "hmac_get_pri_sec_chan:pst_bss_dscr->st_channel.en_bandwidth = %d\n", pst_bss_dscr->en_channel_bandwidth);

    if (WLAN_BAND_WIDTH_40PLUS == pst_bss_dscr->en_channel_bandwidth)
    {
        *pul_sec_chan = *pul_pri_chan + 4;
    }
    else if (WLAN_BAND_WIDTH_40MINUS == pst_bss_dscr->en_channel_bandwidth)
    {
        *pul_sec_chan = *pul_pri_chan - 4;
    }
    else
    {
        OAM_WARNING_LOG1(0, OAM_SF_2040, "hmac_get_pri_sec_chan: pst_bss_dscr is not support 40Mhz, *pul_sec_chan = %d\n", *pul_sec_chan);
    }

    OAM_INFO_LOG2(0, OAM_SF_2040, "*pul_pri_chan = %d, *pul_sec_chan = %d\n\n", *pul_pri_chan, *pul_sec_chan);

    return OAL_SUCC;
}


OAL_STATIC oal_void hmac_switch_pri_sec(mac_vap_stru *pst_mac_vap)
{
    if (WLAN_BAND_WIDTH_40PLUS == pst_mac_vap->st_channel.en_bandwidth)
    {
        pst_mac_vap->st_channel.uc_chan_number = pst_mac_vap->st_channel.uc_chan_number + 4;
        pst_mac_vap->st_channel.en_bandwidth = WLAN_BAND_WIDTH_40MINUS;
    }
    else if (WLAN_BAND_WIDTH_40MINUS == pst_mac_vap->st_channel.en_bandwidth)
    {
        pst_mac_vap->st_channel.uc_chan_number = pst_mac_vap->st_channel.uc_chan_number - 4;
        pst_mac_vap->st_channel.en_bandwidth = WLAN_BAND_WIDTH_40PLUS;
    }
    else
    {
        OAM_WARNING_LOG1(pst_mac_vap->uc_vap_id, OAM_SF_2040, "hmac_switch_pri_sec:en_bandwidth = %d\n not need obss scan\n",
                        pst_mac_vap->st_channel.en_bandwidth);
    }
}


OAL_STATIC oal_bool_enum_uint8 hmac_obss_check_40mhz_2g(mac_vap_stru *pst_mac_vap,
                                                            hmac_scan_record_stru *pst_scan_record)
{
    oal_uint32                  ul_pri_freq;
    oal_uint32                  ul_sec_freq;
    oal_uint32                  ul_affected_start;
    oal_uint32                  ul_affected_end;

    oal_uint32                  ul_pri;
    oal_uint32                  ul_sec;
    oal_uint32                  ul_sec_chan, ul_pri_chan;

    hmac_bss_mgmt_stru          *pst_bss_mgmt;
    mac_bss_dscr_stru           *pst_bss_dscr;
    hmac_scanned_bss_info       *pst_scanned_bss;
    oal_dlist_head_stru         *pst_entry;

    /* 获取主信道、次信道中心频点 */
    ul_pri_freq = (oal_int32)g_ast_freq_map_2g[pst_mac_vap->st_channel.uc_chan_number - 1].us_freq;//2412 + (pst_mac_vap->st_channel.uc_chan_number - 1) * 5;

    if (WLAN_BAND_WIDTH_40PLUS == pst_mac_vap->st_channel.en_bandwidth)
    {
        ul_sec_freq = ul_pri_freq + 20;
    }
    else if (WLAN_BAND_WIDTH_40MINUS == pst_mac_vap->st_channel.en_bandwidth)
    {
        ul_sec_freq = ul_pri_freq - 20;
    }
    else
    {
        OAM_WARNING_LOG1(pst_mac_vap->uc_vap_id, OAM_SF_2040, "hmac_obss_check_40mhz_2g:en_bandwidth = %d not need obss\n",
                        pst_mac_vap->st_channel.en_bandwidth);
        return OAL_TRUE;
    }

    /* 2.4G共存检测,检测范围是40MHz带宽中心频点为中心,左右各25MHZ */
    ul_affected_start   = ((ul_pri_freq + ul_sec_freq) >> 1) - 25;
    ul_affected_end     = ((ul_pri_freq + ul_sec_freq) >> 1) + 25;

    OAM_INFO_LOG2(pst_mac_vap->uc_vap_id, OAM_SF_2040, "hmac_obss_check_40mhz_2g:40 MHz affected channel range: [%d, %d] MHz",
                    ul_affected_start, ul_affected_end);

    /* 获取扫描结果的管理结构地址 */
    pst_bss_mgmt = &(pst_scan_record->st_bss_mgmt);

    /* 获取锁 */
    oal_spin_lock(&(pst_bss_mgmt->st_lock));

    /* 遍历扫描到的bss信息 */
    OAL_DLIST_SEARCH_FOR_EACH(pst_entry, &(pst_bss_mgmt->st_bss_list_head))
    {
        pst_scanned_bss = OAL_DLIST_GET_ENTRY(pst_entry, hmac_scanned_bss_info, st_dlist_head);
        pst_bss_dscr    = &(pst_scanned_bss->st_bss_dscr_info);
        if (OAL_PTR_NULL == pst_bss_dscr)
        {
            OAM_WARNING_LOG0(0,OAM_SF_2040,"{hmac_obss_check_40mhz_2g::pst_bss_dscr is NULL}");
            continue;
        }

        ul_pri = (oal_int32)g_ast_freq_map_2g[pst_bss_dscr->st_channel.uc_chan_number - 1].us_freq;//2412 + (st_chan_result.uc_channel_number - 1) * 5;
        ul_sec = ul_pri;

        OAM_INFO_LOG2(pst_mac_vap->uc_vap_id, OAM_SF_2040, "pst_bss_dscr->st_channel.uc_chan_number = %d, ul_pri = %d\n",
                        pst_bss_dscr->st_channel.uc_chan_number, ul_pri);

        /* 获取扫描到的BSS的信道、频点信息 */
        hmac_get_pri_sec_chan(pst_bss_dscr, &ul_pri_chan, &ul_sec_chan);

        /* 该BSS为40MHz带宽,计算次信道频点 */
        if (ul_sec_chan)
        {
            if (ul_sec_chan < ul_pri_chan)
                ul_sec = ul_pri - 20;
            else
                ul_sec = ul_pri + 20;
        }

        if ((ul_pri < ul_affected_start || ul_pri > ul_affected_end) &&
            (ul_sec < ul_affected_start || ul_sec > ul_affected_end))
            continue; /* not within affected channel range */

        /*  */
        if (ul_sec_chan)
        {
            if (ul_pri_freq != ul_pri || ul_sec_freq != ul_sec)
            {
                OAM_INFO_LOG4(0, OAM_SF_2040, "hmac_obss_check_40mhz_2g:40 MHz pri/sec <%d, %d >mismatch with BSS <%d, %d>\n",
                                ul_pri_freq, ul_sec_freq, ul_pri, ul_sec);
                /* 解除锁 */
                oal_spin_unlock(&(pst_bss_mgmt->st_lock));
                return OAL_FALSE;
            }
        }
    }

    /* 解除锁 */
    oal_spin_unlock(&(pst_bss_mgmt->st_lock));

    return OAL_TRUE;
}


OAL_STATIC oal_bool_enum_uint8 hmac_obss_check_40mhz_5g(mac_vap_stru *pst_mac_vap,
                                                            hmac_scan_record_stru *pst_scan_record)
{
    oal_uint32                  ul_pri_chan;
    oal_uint32                  ul_sec_chan;
    oal_uint32                  ul_pri_bss;
    oal_uint32                  ul_sec_bss;
    oal_uint32                  ul_bss_pri_chan;
    oal_uint32                  ul_bss_sec_chan;
    oal_uint8                   uc_match;
    hmac_bss_mgmt_stru          *pst_bss_mgmt;
    mac_bss_dscr_stru           *pst_bss_dscr;
    hmac_scanned_bss_info       *pst_scanned_bss;
    oal_dlist_head_stru         *pst_entry;

    OAM_INFO_LOG1(pst_mac_vap->uc_vap_id, OAM_SF_2040, "hmac_obss_check_40mhz_5g:pst_mac_vap->st_channel.uc_chan_number = %d\n", pst_mac_vap->st_channel.uc_chan_number);

    /* 获取主信道和次信道 */
    ul_pri_chan = pst_mac_vap->st_channel.uc_chan_number;
    if (WLAN_BAND_WIDTH_40PLUS == pst_mac_vap->st_channel.en_bandwidth)
    {
        ul_sec_chan = ul_pri_chan + 4;
    }
    else if (WLAN_BAND_WIDTH_40MINUS == pst_mac_vap->st_channel.en_bandwidth)
    {
        ul_sec_chan = ul_pri_chan - 4;
    }
    else
    {
        OAM_WARNING_LOG1(pst_mac_vap->uc_vap_id, OAM_SF_2040, "hmac_obss_check_40mhz_5g: en_bandwidth = %d not need obss scan",
                        pst_mac_vap->st_channel.en_bandwidth);
        return OAL_TRUE;
    }

    /* 获取扫描结果的管理结构地址 */
    pst_bss_mgmt = &(pst_scan_record->st_bss_mgmt);

    /* 获取锁 */
    oal_spin_lock(&(pst_bss_mgmt->st_lock));

    /* 若在次信道检测到Beacon, 但是主信道上没有, 则需要交换主次信道 */
    ul_pri_bss = ul_sec_bss = 0;
    OAL_DLIST_SEARCH_FOR_EACH(pst_entry, &(pst_bss_mgmt->st_bss_list_head))
    {
        pst_scanned_bss = OAL_DLIST_GET_ENTRY(pst_entry, hmac_scanned_bss_info, st_dlist_head);
        pst_bss_dscr    = &(pst_scanned_bss->st_bss_dscr_info);

        if (OAL_PTR_NULL == pst_bss_dscr)
        {
            OAM_WARNING_LOG0(0,OAM_SF_2040,"{hmac_obss_check_40mhz_5g::pst_bss_dscr is NULL}");
            continue;
        }

        OAM_INFO_LOG1(pst_mac_vap->uc_vap_id, OAM_SF_2040, "hmac_obss_check_40mhz_5g:bss uc_channel_number = %d\n", pst_bss_dscr->st_channel.uc_chan_number);
        if (pst_bss_dscr->st_channel.uc_chan_number == ul_pri_chan)
            ul_pri_bss++;
        else if (pst_bss_dscr->st_channel.uc_chan_number == ul_sec_chan)
            ul_sec_bss++;
    }

    if (ul_sec_bss && !ul_pri_bss)
    {
        OAM_INFO_LOG0(pst_mac_vap->uc_vap_id, OAM_SF_2040, "hmac_obss_check_40mhz_5g:Switch own primary and secondary channel \
                        to get secondary channel with no Beacons from other BSSes\n");

        hmac_switch_pri_sec(pst_mac_vap);

        /* 此处主次交换完后直接返回即可, 按hostapd-2.4.0版本修改 */
		/* 释放锁 */
        oal_spin_unlock(&(pst_bss_mgmt->st_lock));
	    return OAL_TRUE;
    }

    /*
     * Match PRI/SEC channel with any existing HT40 BSS on the same
     * channels that we are about to use (if already mixed order in
     * existing BSSes, use own preference).
     */
    uc_match = OAL_FALSE;
    OAL_DLIST_SEARCH_FOR_EACH(pst_entry, &(pst_bss_mgmt->st_bss_list_head))
    {
        pst_scanned_bss = OAL_DLIST_GET_ENTRY(pst_entry, hmac_scanned_bss_info, st_dlist_head);
        pst_bss_dscr    = &(pst_scanned_bss->st_bss_dscr_info);
        if (OAL_PTR_NULL == pst_bss_dscr)
        {
            OAM_WARNING_LOG0(0,OAM_SF_2040,"{hmac_obss_check_40mhz_5g::pst_bss_dscr is NULL}");
            continue;
        }

        hmac_get_pri_sec_chan(pst_bss_dscr, &ul_bss_pri_chan, &ul_bss_sec_chan);
        if (ul_pri_chan == ul_bss_pri_chan &&
            ul_sec_chan == ul_bss_sec_chan) {
            uc_match = OAL_TRUE;
            break;
        }
    }

    if (!uc_match) {
        OAL_DLIST_SEARCH_FOR_EACH(pst_entry, &(pst_bss_mgmt->st_bss_list_head))
        {
            pst_scanned_bss = OAL_DLIST_GET_ENTRY(pst_entry, hmac_scanned_bss_info, st_dlist_head);
            pst_bss_dscr    = &(pst_scanned_bss->st_bss_dscr_info);
            if (OAL_PTR_NULL == pst_bss_dscr)
            {
                OAM_WARNING_LOG0(0,OAM_SF_2040,"{hmac_obss_check_40mhz_5g::pst_bss_dscr is NULL}");
                continue;
            }

            hmac_get_pri_sec_chan(pst_bss_dscr, &ul_bss_pri_chan, &ul_bss_sec_chan);

            if ((ul_pri_chan == ul_bss_sec_chan) && (ul_sec_chan == ul_bss_pri_chan))
            {
                OAM_INFO_LOG0(0, OAM_SF_2040, "hmac_obss_check_40mhz_5g:switch own primary and secondary channel due to BSS overlap with\n");

                hmac_switch_pri_sec(pst_mac_vap);
                break;
            }
        }
    }

    OAM_INFO_LOG1(pst_mac_vap->uc_vap_id, OAM_SF_2040, "hmac_obss_check_40mhz_5g:After check, pst_mac_vap->st_channel.uc_chan_number = %d\n", pst_mac_vap->st_channel.uc_chan_number);

    /* 解除锁 */
    oal_spin_unlock(&(pst_bss_mgmt->st_lock));

    return OAL_TRUE;
}


oal_uint32  hmac_obss_init_scan_hook(hmac_scan_record_stru   *pst_scan_record,
                                     hmac_device_stru        *pst_dev)
{
    /*
    TODO: ACS未运行，需要根据扫描结果进行OBSS共存性判断
    1、当前VAP的信道即为目标信道
    2、判断之后按照需要更新VAP的信道和带宽
    */
    oal_uint8                           uc_idx;
    oal_bool_enum_uint8                 en_bandwidth_40MHz = OAL_FALSE;
    oal_bool_enum_uint8                 en_2g_first_vap_bandwidth_40MHz = OAL_FALSE;
    mac_vap_stru                        *pst_mac_vap;
    oal_uint8                           uc_2g_check_band = OAL_FALSE;
    oal_uint8                           uc_5g_check_band = OAL_FALSE;
    oal_uint8                           uc_5g_first_vap_chan_number = 0;
    wlan_channel_bandwidth_enum_uint8   en_5g_first_vap_bandwidth = WLAN_BAND_WIDTH_20M;

    OAM_WARNING_LOG0(0, OAM_SF_ACS, "hmac_obss_init_scan_hook run\n");

    /* 遍历每个vap,检测信道和带宽 */
    for(uc_idx = 0; uc_idx < pst_dev->pst_device_base_info->uc_vap_num; uc_idx++)
    {
        pst_mac_vap = mac_res_get_mac_vap(pst_dev->pst_device_base_info->auc_vap_id[uc_idx]);

        if (pst_mac_vap && WLAN_VAP_MODE_BSS_AP == pst_mac_vap->en_vap_mode)
        {
            OAM_INFO_LOG2(pst_mac_vap->uc_vap_id, OAM_SF_2040, "hmac_obss_init_scan_hook:original channel=%d, bandwidth=%d.\n",
                pst_mac_vap->st_channel.uc_chan_number, pst_mac_vap->st_channel.en_bandwidth);

            if (WLAN_BAND_2G == pst_mac_vap->st_channel.en_band)
            {
                if (WLAN_BAND_WIDTH_40PLUS == pst_mac_vap->st_channel.en_bandwidth ||
                        WLAN_BAND_WIDTH_40MINUS == pst_mac_vap->st_channel.en_bandwidth)
                {
                    /* 只检测第一个vap，后面的跟随第一个vap的带宽和信道 */
                    if (OAL_FALSE == uc_2g_check_band)
                    {
                        /* 2G vap 20/40共存检测 */
                        en_bandwidth_40MHz = hmac_obss_check_40mhz_2g(pst_mac_vap, pst_scan_record);

                        OAM_INFO_LOG1(pst_mac_vap->uc_vap_id, OAM_SF_2040, "hmac_obss_init_scan_hook:2.4G first vap, en_bandwidth_40MHz=%d\n", en_bandwidth_40MHz);
                        uc_2g_check_band = OAL_TRUE;
                        en_2g_first_vap_bandwidth_40MHz = en_bandwidth_40MHz;
                    }
                    else
                    {
                        /* 第一个vap检测完成之后,后续的同频段vap信道和带宽都跟随第一个 */
                        OAM_INFO_LOG0(pst_mac_vap->uc_vap_id, OAM_SF_2040, "hmac_obss_init_scan_hook:follow bandwidth of first 2g vap.\n");
                        en_bandwidth_40MHz = en_2g_first_vap_bandwidth_40MHz;
                    }
                }

                /* 设置带宽 */
                if (OAL_FALSE == en_bandwidth_40MHz)
                {
                    OAM_INFO_LOG0(pst_mac_vap->uc_vap_id, OAM_SF_2040, "hmac_obss_init_scan_hook:cann't set 40MHz bandwidth,change to 20MHz.\n");
                    pst_mac_vap->st_channel.en_bandwidth = WLAN_BAND_WIDTH_20M;
                }
            }
            else if (WLAN_BAND_5G == pst_mac_vap->st_channel.en_band)
            {
                if (WLAN_BAND_WIDTH_40PLUS == pst_mac_vap->st_channel.en_bandwidth ||
                        WLAN_BAND_WIDTH_40MINUS == pst_mac_vap->st_channel.en_bandwidth)
                {
                    if (OAL_FALSE == uc_5g_check_band)
                    {
                        /* 5G vap 20/40共存检测 */
                        en_bandwidth_40MHz = hmac_obss_check_40mhz_5g(pst_mac_vap, pst_scan_record);
                        uc_5g_check_band = OAL_TRUE;
                        uc_5g_first_vap_chan_number = pst_mac_vap->st_channel.uc_chan_number;
                        en_5g_first_vap_bandwidth = pst_mac_vap->st_channel.en_bandwidth;
                    }
                    else
                    {
                        /* 第一个vap检测完成之后,后续的同频段vap信道和带宽都跟随第一个 */
                        OAM_INFO_LOG0(pst_mac_vap->uc_vap_id, OAM_SF_2040, "hmac_obss_init_scan_hook:follow bandwidth of first 5g vap.\n");
                        pst_mac_vap->st_channel.uc_chan_number = uc_5g_first_vap_chan_number;
                        pst_mac_vap->st_channel.en_bandwidth = en_5g_first_vap_bandwidth;
                    }
                }
            }
            else
            {
                OAM_WARNING_LOG1(pst_mac_vap->uc_vap_id, OAM_SF_2040, "hmac_obss_init_scan_hook: en_band = %d not support!",
                            pst_mac_vap->st_channel.en_band);
                continue;
            }

            /* 记录信道和带宽 */
            pst_dev->ast_best_channel[pst_mac_vap->st_channel.en_band].uc_chan_number = pst_mac_vap->st_channel.uc_chan_number;
            pst_dev->ast_best_channel[pst_mac_vap->st_channel.en_band].en_bandwidth = pst_mac_vap->st_channel.en_bandwidth;
            pst_dev->pst_device_base_info->uc_max_channel = pst_mac_vap->st_channel.uc_chan_number;
            pst_mac_vap->st_ch_switch_info.en_user_pref_bandwidth = pst_mac_vap->st_channel.en_bandwidth;
            OAM_INFO_LOG2(pst_mac_vap->uc_vap_id, OAM_SF_2040, "hmac_obss_init_scan_hook:After adjust, channel=%d, bandwidth=%d.\n",
                            pst_mac_vap->st_channel.uc_chan_number, pst_mac_vap->st_channel.en_bandwidth);
        }
    }

    return OAL_SUCC;
}
#endif

#if defined(_PRE_SUPPORT_ACS) || defined(_PRE_WLAN_FEATURE_DFS) || defined(_PRE_WLAN_FEATURE_20_40_80_COEXIST)

oal_bool_enum_uint8 hmac_device_in_init_scan(mac_device_stru *pst_mac_device)
{
    hmac_device_stru    *pst_hmac_device = OAL_PTR_NULL;

    pst_hmac_device = hmac_res_get_mac_dev(pst_mac_device->uc_device_id);

    if (OAL_PTR_NULL == pst_hmac_device)
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "hmac_res_get_mac_dev:pst_hmac_device is null\n");
        return OAL_FALSE;
    }
    else
    {
        return pst_hmac_device->en_in_init_scan;
    }

}


mac_need_init_scan_res_enum_uint8 hmac_need_init_scan(hmac_device_stru *pst_hmac_device, mac_vap_stru *pst_in_mac_vap, mac_try_init_scan_type en_type)
{
    mac_need_init_scan_res_enum_uint8 en_result = MAC_INIT_SCAN_NOT_NEED;
    mac_device_stru    *pst_mac_device = pst_hmac_device->pst_device_base_info;

    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_mac_device))
    {
        OAM_ERROR_LOG0(0, OAM_SF_ANY, "{hmac_need_init_scan::pst_device_base_info null!}");
        return OAL_ERR_CODE_PTR_NULL;
    }

#ifdef _PRE_WLAN_FEATURE_PROXYSTA
    {
        mac_vap_stru *pst_main_sta = mac_find_main_proxysta(pst_mac_device);

        // proxysta模式下仅在以下情况下进行初始信道选择
        // 1、main sta不存在
        // 2、main sta存在，但是其状态为INIT
        if (!(!pst_main_sta|| pst_main_sta->en_vap_state == MAC_VAP_STATE_INIT))
        {
            return MAC_INIT_SCAN_NOT_NEED;
        }
    }
#endif
    if (pst_in_mac_vap->en_vap_mode != WLAN_VAP_MODE_BSS_AP && pst_in_mac_vap->en_vap_mode != WLAN_VAP_MODE_CONFIG)
    {
        return MAC_INIT_SCAN_NOT_NEED;
    }

    if (MAC_TRY_INIT_SCAN_VAP_UP == en_type && !pst_hmac_device->en_start_via_priv)
    {
        return  MAC_INIT_SCAN_NOT_NEED;
    }

    if(pst_hmac_device->en_in_init_scan)
    {
        return MAC_TRY_INIT_SCAN_RESCAN == en_type ? MAC_INIT_SCAN_NOT_NEED : MAC_INIT_SCAN_IN_SCAN;
    }

#ifdef _PRE_SUPPORT_ACS
    en_result |=  ((mac_get_acs_switch(pst_mac_device) == MAC_ACS_SW_INIT
                  || mac_get_acs_switch(pst_mac_device) == MAC_ACS_SW_BOTH ? OAL_TRUE : OAL_FALSE));
    if (MAC_TRY_INIT_SCAN_RESCAN == en_type)
    {
        return en_result;
    }
#endif

#ifdef _PRE_WLAN_FEATURE_DFS
    /*lint -e514*/
    en_result |= mac_dfs_get_dfs_enable(pst_mac_device) && mac_dfs_get_cac_enable(pst_hmac_device->pst_device_base_info);
    /*lint +e514*/

#endif

#ifdef _PRE_WLAN_FEATURE_20_40_80_COEXIST
    en_result |= mac_get_2040bss_switch(pst_mac_device);
#endif


    return en_result;

}


oal_uint32  hmac_init_scan_timeout(void *p_arg)
{
    hmac_device_stru *pst_dev = (hmac_device_stru *)p_arg;

    if (!pst_dev->en_init_scan)
    {
        return OAL_SUCC;
    }

    pst_dev->en_init_scan      = OAL_FALSE;
    pst_dev->en_in_init_scan   = OAL_FALSE;
    hmac_start_all_bss_of_device(pst_dev);

    return OAL_SUCC;
}


oal_uint32  hmac_init_scan_cancel_timer(hmac_device_stru *pst_hmac_dev)
{
    if (pst_hmac_dev && pst_hmac_dev->st_scan_mgmt.st_init_scan_timeout.en_is_enabled)
    {
        /* 关闭超时定时器 */
        FRW_TIMER_IMMEDIATE_DESTROY_TIMER(&pst_hmac_dev->st_scan_mgmt.st_init_scan_timeout);
    }

    return OAL_SUCC;
}


oal_void hmac_init_scan_cb(void *p_scan_record)
{
    hmac_scan_record_stru           *pst_scan_record = (hmac_scan_record_stru *)p_scan_record;
    oal_uint8                        uc_device_id    = pst_scan_record->uc_device_id;
    hmac_device_stru                 *pst_hmac_dev    = hmac_res_get_mac_dev(uc_device_id);
    mac_device_stru                  *pst_mac_dev;

    OAM_WARNING_LOG0(0, OAM_SF_ACS, "{hmac_init_scan_cb called}");

    if (OAL_PTR_NULL == pst_hmac_dev)
    {
        OAM_ERROR_LOG1(0, OAM_SF_ACS, "{hmac_init_scan_cb:pst_hmac_dev=NULL, device_id=%d}", uc_device_id);
        return;
    }

    pst_mac_dev = pst_hmac_dev->pst_device_base_info;
    if (OAL_PTR_NULL == pst_hmac_dev || OAL_PTR_NULL == pst_mac_dev)
    {
        OAM_ERROR_LOG1(0, OAM_SF_ACS, "{hmac_init_scan_cb:pst_mac_dev=NULL, device_id=%d}", uc_device_id);
        return;
    }

#ifdef _PRE_SUPPORT_ACS
    if (MAC_ACS_SW_INIT == mac_get_acs_switch(pst_mac_dev) || MAC_ACS_SW_BOTH == mac_get_acs_switch(pst_mac_dev))
    {
        OAM_WARNING_LOG0(0, OAM_SF_ACS, "acs  enable, post and return\n");
        /* 若ACS在运行中，则等待ACS APP返回响应结果 */
        if(OAL_SUCC == hmac_acs_init_scan_hook(pst_scan_record, pst_hmac_dev))
        {
            return;
        }
    }
#endif

    OAM_WARNING_LOG0(0, OAM_SF_ACS, "acs not enable, cancel timer\n");
    /* ACS未执行，超时保护至此结束 */
    hmac_init_scan_cancel_timer(pst_hmac_dev);

    /* 若ACS未执行或者执行失败，继续执行后续操作 */
#ifdef _PRE_WLAN_FEATURE_20_40_80_COEXIST
    if (mac_get_2040bss_switch(pst_mac_dev))
    {
        hmac_obss_init_scan_hook(pst_scan_record, pst_hmac_dev);
    }
#endif

  OAM_WARNING_LOG3(0, OAM_SF_ACS, "<<< rsp=5:scan report ch=%d bss=%d init=%d\n", pst_scan_record->uc_chan_numbers,
                    pst_scan_record->st_bss_mgmt.ul_bss_num, pst_hmac_dev->en_init_scan);
#ifdef _PRE_WLAN_FEATURE_DFS
    if (mac_dfs_get_dfs_enable(pst_mac_dev))
    {
        /* 若成功开始了CAC或者已经开始CAC，返回，由CAC超时函数处理VAP START */
        if(OAL_SUCC == hmac_dfs_init_scan_hook(pst_scan_record, pst_hmac_dev))
        {
            return;
        }
    }
#endif

    /* ACS未运行、DFS未运行，直接启动BSS */
    hmac_init_scan_timeout(pst_hmac_dev);
}


oal_uint32 hmac_init_scan_do(hmac_device_stru *pst_hmac_dev, mac_vap_stru *pst_mac_vap, mac_init_scan_req_stru *pst_cmd)
{
    oal_uint8                           uc_idx, uc_cnt;
    mac_scan_req_stru                   st_scan_req;
    wlan_channel_band_enum_uint8        en_band;
    wlan_channel_bandwidth_enum_uint8   en_bandwidth;
    oal_uint32                          ul_ret = OAL_FAIL;
    hmac_vap_stru                      *pst_hmac_vap = mac_res_get_hmac_vap(pst_mac_vap->uc_vap_id);

    if(pst_hmac_vap == OAL_PTR_NULL)
    {
        OAM_ERROR_LOG1(0, OAM_SF_ACS, "{hmac_init_scan_do:get hmac vap=NULL, vapid=%d}", pst_mac_vap->uc_vap_id);
        return OAL_FAIL;
    }

    OAL_MEMZERO(&st_scan_req, OAL_SIZEOF(st_scan_req));
    st_scan_req.en_scan_mode  = pst_cmd->auc_arg[0];
    st_scan_req.en_scan_type  = pst_cmd->auc_arg[1];
    st_scan_req.uc_scan_func  = pst_cmd->auc_arg[2];
    st_scan_req.en_bss_type   = WLAN_MIB_DESIRED_BSSTYPE_INFRA;
    st_scan_req.uc_bssid_num  = 0;
    st_scan_req.uc_ssid_num   = 0;

    st_scan_req.uc_max_scan_count_per_channel           = 1;
    st_scan_req.uc_max_send_probe_req_count_per_channel = st_scan_req.en_scan_mode == WLAN_SCAN_MODE_FOREGROUND ? 2 : 1;

    st_scan_req.us_scan_time        = st_scan_req.en_scan_mode == WLAN_SCAN_MODE_FOREGROUND ? 120 : 30;
    st_scan_req.uc_probe_delay      = 0;
    st_scan_req.uc_vap_id           = pst_mac_vap->uc_vap_id; /* 其实是该device下的vap_id[0] */
    st_scan_req.p_fn_cb             = hmac_init_scan_cb;

    st_scan_req.uc_channel_nums     = 0;        /* 信道列表中信道的个数 */
    uc_cnt = 0;
    for (uc_idx = 0; uc_idx < pst_cmd->ul_cmd_len; uc_idx += 2)
    {
        en_band      = pst_cmd->auc_data[uc_idx] & 0X0F;
        en_bandwidth = (pst_cmd->auc_data[uc_idx] >> 4) & 0x0F;
        if (OAL_SUCC != mac_is_channel_num_valid(en_band, pst_cmd->auc_data[uc_idx+1]))
        {
            OAM_WARNING_LOG3(0, OAM_SF_ACS, "{hmac_init_scan_do:invalid channel number, ch=%d, band=%d bw=%d}",
                                        pst_cmd->auc_data[uc_idx+1], en_band, en_bandwidth);
            OAL_IO_PRINT("{hmac_init_scan_do:invalid channel number, ch=%d, band=%d bw=%d}\n",
                                        pst_cmd->auc_data[uc_idx+1], en_band, en_bandwidth);

            continue;
        }

        st_scan_req.ast_channel_list[uc_cnt].uc_chan_number = pst_cmd->auc_data[uc_idx + 1];
        st_scan_req.ast_channel_list[uc_cnt].en_band        = en_band;
        st_scan_req.ast_channel_list[uc_cnt].en_bandwidth   = en_bandwidth;
        ul_ret = mac_get_channel_idx_from_num(st_scan_req.ast_channel_list[uc_cnt].en_band,
                                     st_scan_req.ast_channel_list[uc_cnt].uc_chan_number,
                                    &st_scan_req.ast_channel_list[uc_cnt].uc_idx);
        if (OAL_SUCC != ul_ret)
        {
            OAM_WARNING_LOG2(0, OAM_SF_ACS, "{hmac_init_scan_do:mac_get_channel_idx_from_num failed en_band:[%d],chan_number:[%d]}",
                st_scan_req.ast_channel_list[uc_cnt].en_band,st_scan_req.ast_channel_list[uc_cnt].uc_chan_number);
        }
        uc_cnt++;
    }

    OAL_IO_PRINT(">>> got=5:do scan mode=%d func=0x%x type=%d ch_cnt=%d\n",
        st_scan_req.en_scan_mode,
        st_scan_req.uc_scan_func,st_scan_req.en_scan_type, uc_cnt);

    if (uc_cnt != 0)
    {
        st_scan_req.uc_channel_nums = uc_cnt;

        /* 直接调用扫描模块扫描请求处理函数 */
        ul_ret = hmac_scan_proc_scan_req_event(pst_hmac_vap, &st_scan_req);
        if(OAL_SUCC != ul_ret)
        {
            OAM_WARNING_LOG1(0, OAM_SF_SCAN, "hmac_init_scan_do:hmac_scan_add_req failed, ret=%d", ul_ret);
        }
    }
    else
    {
        OAM_WARNING_LOG0(0, OAM_SF_SCAN, "hmac_init_scan_do:no valid channel found, not scan");
    }

    return ul_ret;
}

oal_bool_enum_uint8 hmac_init_scan_skip_channel(hmac_device_stru              *pst_hmac_dev,
                                                wlan_channel_band_enum_uint8   en_band,
                                                oal_uint8                      uc_idx)
{
    /* skip any illegal channel */
    if (OAL_SUCC != mac_is_channel_idx_valid(en_band, uc_idx))
    {
        return OAL_TRUE;
    }
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_hmac_dev->pst_device_base_info))
    {
        OAM_ERROR_LOG0(0, OAM_SF_ANY, "{hmac_init_scan_skip_channel::pst_device_base_info null!}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    if (pst_hmac_dev->pst_device_base_info
     && pst_hmac_dev->pst_device_base_info->en_max_band != en_band)
    {
        return  OAL_TRUE;
    }

    return OAL_FALSE;
}

oal_uint32 hmac_init_scan_process(hmac_device_stru *pst_hmac_dev, mac_vap_stru *pst_mac_vap, mac_scan_op_enum_uint8 en_op)
{
    oal_uint8         ast_buf[OAL_SIZEOF(mac_acs_cmd_stru) - 4 + WLAN_MAX_CHANNEL_NUM * 2];
    oal_uint8         uc_idx;
    oal_uint8        *puc_dat, uc_tot, uc_chan_num;
    oal_uint32        ul_ret;

    mac_init_scan_req_stru *pst_cmd = (mac_init_scan_req_stru *)ast_buf;

    puc_dat = pst_cmd->auc_data;

    pst_hmac_dev->en_init_scan    = (en_op ==  MAC_SCAN_OP_INIT_SCAN) ? OAL_TRUE : OAL_FALSE;
    pst_hmac_dev->en_in_init_scan = OAL_TRUE;

    pst_cmd->uc_cmd     = 5; //DMAC_ACS_CMD_DO_SCAN;
    pst_cmd->auc_arg[0] =   (en_op ==  MAC_SCAN_OP_FG_SCAN_ONLY
                          || en_op ==  MAC_SCAN_OP_INIT_SCAN) ? WLAN_SCAN_MODE_FOREGROUND : WLAN_SCAN_MODE_BACKGROUND_AP;

    pst_cmd->auc_arg[1] = WLAN_SCAN_TYPE_ACTIVE;
    pst_cmd->auc_arg[2] = MAC_SCAN_FUNC_MEAS | MAC_SCAN_FUNC_STATS | MAC_SCAN_FUNC_BSS;
    pst_cmd->ul_cmd_cnt = 0;

    uc_tot = 0;
    for (uc_idx = 0; uc_idx < MAC_CHANNEL_FREQ_2_BUTT; uc_idx++)
    {
        if(!hmac_init_scan_skip_channel(pst_hmac_dev, WLAN_BAND_2G, uc_idx))
        {
            mac_get_channel_num_from_idx(WLAN_BAND_2G, uc_idx, &uc_chan_num);
            *puc_dat++ = ((WLAN_BAND_WIDTH_20M) << 4) | WLAN_BAND_2G;
            *puc_dat++ = uc_chan_num;
             uc_tot++;
        }
    }
    for (uc_idx = 0; uc_idx < MAC_CHANNEL_FREQ_5_BUTT; uc_idx++)
    {
        if(!hmac_init_scan_skip_channel(pst_hmac_dev, WLAN_BAND_5G, uc_idx))
        {
            mac_get_channel_num_from_idx(WLAN_BAND_5G, uc_idx, &uc_chan_num);
            *puc_dat++ = ((WLAN_BAND_WIDTH_20M) << 4) | WLAN_BAND_5G;
            *puc_dat++ = uc_chan_num;
             uc_tot++;
        }
    }

    pst_cmd->ul_cmd_len = uc_tot * 2;

    /* best信道清空为0，在start bss作为合法性判断依据 */
    if(pst_hmac_dev->en_init_scan)
    {
        oal_memset(pst_hmac_dev->ast_best_channel, 0, OAL_SIZEOF(pst_hmac_dev->ast_best_channel));
    }

    /* 启动定时器, 超时后强制启动BSS: 保护时间包括扫描时长与APP交互时长 */
    {
        FRW_TIMER_CREATE_TIMER(&pst_hmac_dev->st_scan_mgmt.st_init_scan_timeout,
                               hmac_init_scan_timeout,
                               HMAC_INIT_SCAN_TIMEOUT_MS,
                               pst_hmac_dev,
                               OAL_FALSE,
                               OAM_MODULE_ID_HMAC,
                               pst_hmac_dev->pst_device_base_info->ul_core_id);
    }

    ul_ret = hmac_init_scan_do(pst_hmac_dev, pst_mac_vap, pst_cmd);
    if(OAL_SUCC != ul_ret)
    {
        pst_hmac_dev->en_init_scan    = OAL_FALSE;
        pst_hmac_dev->en_in_init_scan = OAL_FALSE;
    }

    return ul_ret;
}

oal_uint32  hmac_init_scan_try(mac_device_stru *pst_mac_device, mac_vap_stru *pst_in_mac_vap, mac_try_init_scan_type en_type)
{
    oal_uint8        uc_idx;
    mac_vap_stru    *pst_mac_vap, *pst_mac_vap_scan = NULL;
    hmac_vap_stru   *pst_hmac_vap;
    oal_uint8        en_scan_type;
    hmac_device_stru*pst_hmac_device = hmac_res_get_mac_dev(pst_mac_device->uc_device_id);
    oal_bool_enum_uint8  en_need_do_init_scan = OAL_FALSE;

    if (OAL_PTR_NULL == pst_hmac_device)
    {
        OAM_WARNING_LOG1(0, OAM_SF_SCAN, "{hmac_init_scan_try:mac_res_get_hmac_vap failed. device_id:%d.}",pst_mac_device->uc_device_id);
        return OAL_FAIL;
    }
    en_scan_type = hmac_need_init_scan(pst_hmac_device, pst_in_mac_vap, en_type);
    if(en_scan_type == MAC_INIT_SCAN_NOT_NEED)
    {
        return OAL_FAIL;
    }
    else if(en_scan_type == MAC_INIT_SCAN_IN_SCAN)
    {
        OAL_IO_PRINT("just in init scan\n");

        mac_vap_init_rates(pst_in_mac_vap);
        pst_hmac_vap = mac_res_get_hmac_vap(pst_in_mac_vap->uc_vap_id);
        if (OAL_PTR_NULL == pst_hmac_vap)
        {
            OAM_WARNING_LOG1(0, OAM_SF_SCAN, "{hmac_init_scan_try:mac_res_get_hmac_vap failed vap_id:%d.}",pst_in_mac_vap->uc_vap_id);
            return OAL_FAIL;
        }
        if (pst_hmac_vap->st_vap_base_info.en_vap_state == MAC_VAP_STATE_UP)
        {
            hmac_fsm_change_state(pst_hmac_vap, MAC_VAP_STATE_AP_WAIT_START);
        }

        return OAL_SUCC;
    }
    else
    {
        OAM_WARNING_LOG0(0, OAM_SF_SCAN, "hmac_init_scan_try: need init scan");
    }

    for (uc_idx = 0; uc_idx < pst_hmac_device->pst_device_base_info->uc_vap_num; uc_idx++)
    {
        pst_mac_vap = mac_res_get_mac_vap(pst_hmac_device->pst_device_base_info->auc_vap_id[uc_idx]);
        if (!pst_mac_vap)
        {
            continue;
        }

        if(WLAN_VAP_MODE_BSS_AP == pst_mac_vap->en_vap_mode)
        {
            mac_vap_init_rates(pst_mac_vap);

            /* 强制设置AP侧状态机为 WAIT_START，因为需要执行初始信道检查 */
            pst_hmac_vap = mac_res_get_hmac_vap(pst_mac_vap->uc_vap_id);
            if (OAL_PTR_NULL == pst_hmac_vap)
            {
                OAM_WARNING_LOG1(0, OAM_SF_SCAN, "{hmac_init_scan_try::need init scan fail.vap_id = %u}",pst_mac_vap->uc_vap_id);
                continue;
            }
            switch (pst_hmac_vap->st_vap_base_info.en_vap_state)
            {
                case MAC_VAP_STATE_UP :
                case MAC_VAP_STATE_PAUSE : // dbac
                        hmac_fsm_change_state(pst_hmac_vap, MAC_VAP_STATE_AP_WAIT_START);
                        // no break here!
                case MAC_VAP_STATE_AP_WAIT_START :
                        en_need_do_init_scan = OAL_TRUE;
                        pst_mac_vap_scan = pst_mac_vap;
                        break;
                default :
                     break;
            }
        }
    }

    if (en_need_do_init_scan)
    {
        return hmac_init_scan_process(pst_hmac_device, pst_mac_vap_scan, MAC_SCAN_OP_INIT_SCAN);
    }

    return OAL_SUCC;
}

#endif
/*lint -e578*//*lint -e19*/
oal_module_symbol(hmac_scan_find_scanned_bss_dscr_by_index);
oal_module_symbol(hmac_scan_find_scanned_bss_by_bssid);
/*lint -e578*//*lint -e19*/

#ifdef __cplusplus
    #if __cplusplus
        }
    #endif
#endif

