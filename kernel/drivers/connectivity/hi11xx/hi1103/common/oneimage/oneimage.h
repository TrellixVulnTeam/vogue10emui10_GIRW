

#ifndef __ONEIMAGE_H__
#define __ONEIMAGE_H__

/* ����ͷ�ļ����� */
#include "plat_type.h"

/* �궨�� */
#define DTS_COMP_HW_CONNECTIVITY_NAME "hisilicon,hisi_wifi"

/* 2015-12-15 modify by ouyangxiaoyu for nfc one image beg */
#define DTS_COMP_HISI_NFC_NAME "hisilicon,hisi_nfc"

/* 2015-12-15 modify by ouyangxiaoyu for nfc one image end */
#define DTS_COMP_HW_HISI_SUPP_CONFIG_NAME     "hisi,wifi_supp"
#define DTS_COMP_HW_HISI_P2P_CONFIG_NAME      "hisi,wifi_p2p"
#define DTS_COMP_HW_HISI_HOSTAPD_CONFIG_NAME  "hisi,wifi_hostapd"
#define DTS_COMP_HW_HISI_FIRMWARE_CONFIG_NAME "hisi,wifi_firmware"

/* 2015-12-15 modify by ouyangxiaoyu for nfc one image beg */
#define DTS_COMP_HW_HISI_NFC_CONFIG_NAME "nfc_hisi_conf_name"
#define DTS_COMP_HW_BRCM_NFC_CONFIG_NAME "nfc_default_hisi_conf_name"

/* 2015-12-15 modify by ouyangxiaoyu for nfc one image end */
#define PROC_CONN_DIR "/proc/connectivity"

#define HW_CONN_PROC_DIR "connectivity"

#define HW_CONN_PROC_CHIPTYPE_FILE "chiptype"
#define HW_CONN_PROC_SUPP_FILE     "supp_config_template"
#define HW_CONN_PROC_P2P_FILE      "p2p_config_template"
#define HW_CONN_PROC_HOSTAPD_FILE  "hostapd_bin_file"
#define HW_CONN_PROC_FRIMWARE      "firmware_type_num"

#define BUFF_LEN      129
#define NODE_PATH_LEN 256

/* STRUCT DEFINE */
typedef struct hisi_proc_info {
    int proc_type;
    char *proc_node_name;
    char *proc_pro_name;
} HISI_PROC_INFO_STRU;

typedef enum proc_enum {
    HW_PROC_CHIPTYPE = 0,
    HW_PROC_SUPP,
    HW_PROC_P2P,
    HW_PROC_HOSTAPD,
    HW_PROC_FIRMWARE,
    HW_PROC_BUTT,
} HISI_PROC_ENUM;

/* EXTERN FUNCTION */
#if (_PRE_OS_VERSION_LINUX == _PRE_OS_VERSION)
extern bool is_my_chip_etc(void);
extern bool is_hisi_chiptype_etc(int32 chip);
extern bool is_my_nfc_chip_etc(void);
extern int read_nfc_conf_name_from_dts_etc(char *buf, int buf_len, char *node_name, char *property_name);
extern int hw_misc_connectivity_init_etc(void);
extern void hw_misc_connectivity_exit_etc(void);
#endif
extern int create_hwconn_proc_file(void);
#endif