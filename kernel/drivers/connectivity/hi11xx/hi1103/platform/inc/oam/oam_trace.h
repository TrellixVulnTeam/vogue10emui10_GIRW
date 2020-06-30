

#ifndef __OAM_TRACE_H__
#define __OAM_TRACE_H__

/* ����ͷ�ļ����� */
#include "oal_ext_if.h"
#include "oam_ext_if.h"

#undef THIS_FILE_ID
#define THIS_FILE_ID OAM_FILE_ID_OAM_TRACE_H
/* �궨�� */
/* profiling��غ궨�� */
#define OAM_PROFILING_TX_PACKET_MAX_COUNT 100 /* ͳ�Ʒ����������� */
#define OAM_PROFILING_TX_OFFSET_COUNT     5   /* ͳ�Ʒ����Ķ�����tx��Ϊ5�� */
#define OAM_PROFILING_TX_MSG_COUNT        2   /* txÿ�ΰ����ı��ĸ��� */

#define OAM_PROFILING_RX_PACKET_MAX_COUNT 100 /* ͳ�Ʒ����������� */
#define OAM_PROFILING_RX_OFFSET_COUNT     3   /* ͳ���հ��Ķ�����rx��Ϊ3�� */
#define OAM_PROFILING_RX_MSG_COUNT        2   /* rxÿ�ΰ����ı��ĸ��� */
#define OAM_PROFILING_MAX_POINT_NUM       40  /* tx rx �����ڵ��� */

#define OAM_PROFILING_ALG_PACKET_MAX_COUNT 2 /* ͳ��ALG�������� */

#define OAM_PROFILING_PACKET_INVALID_VALUE 1010 /* ��������skb id����ʼ��Ĭ��ֵΪ1010 */
#define OAM_PROFILING_PACKET_ABNORMAL_GAP  2000

#define OAM_PROFILING_STATISTIC_RX_DEBUG  0x01 /* RX��������λ:1Ϊenbale��0Ϊdisable */
#define OAM_PROFILING_STATISTIC_TX_DEBUG  0x02 /* RX��������λ:1Ϊenbale��0Ϊdisable */
#define OAM_PROFILING_STATISTIC_ALG_DEBUG 0x04 /* ALG��������λ:1Ϊenbale��0Ϊdisable */

/* trace��غ궨�� */
#define OAM_TRACE_PRINT_DATA_LENGTH   128 /* TRACE��ӡ����󳤶� */
#define OAM_TRACE_PRINT_FORMAT_LENGTH 128 /* TRACE�����ʽ����󳤶� */

/* ö�ٶ��� */
/* profiling���ö�ٶ��� */
/* ���ڶ�Ӧ��������������������� */
typedef enum {
    OAM_PROFILING_SWITCH_DEBUG_CONFIG = 1,

    OAM_PROFILING_SWITCH_CONFIG_BUTT
} oam_profiling_switch_config_enum;
typedef oal_uint8 oam_profiling_switch_config_enum_uint8;

/* profiling���STRUCT���� */
/* tx����ͳ�ƽṹ�� */
typedef struct {
    oal_spin_lock_stru st_spin_lock; /* ��д�� */
    oal_uint16 us_abnormal_index;    /* ����������ͳ�� */
    oal_uint8 uc_idx_for_array;      /* �ڷ���������ں�������ֵ */
    oal_uint8 uc_skb_id_idx;         /* ����ͳ�ƴ��� */

    oal_uint16 aus_tx_skb_id_list[OAM_PROFILING_TX_PACKET_MAX_COUNT];                                     /* ���ڽ��ڼ������͵İ���offset֮�ͣ����浽��Ӧ����������Ԫ���� */
    oal_uint16 aus_tx_pass_count[OAM_PROFILING_TX_PACKET_MAX_COUNT][OAM_PROFILING_TX_FUNC_BUTT];          /* ������������ͳ�� */
    oal_uint32 ast_tx_func_stand_time[OAM_PROFILING_TX_PACKET_MAX_COUNT][OAM_PROFILING_MAX_POINT_NUM];    /* ����ÿ���ڵ�Ļ�׼ʱ�� */
    oal_uint32 ast_tx_func_time_offset[OAM_PROFILING_TX_PACKET_MAX_COUNT][OAM_PROFILING_TX_OFFSET_COUNT]; /* ÿ�ε�offsetʱ�� */
} oam_profiling_tx_statistic_stru;

/* rx����ͳ�ƽṹ�� */
typedef struct {
    oal_spin_lock_stru st_spin_lock; /* ��д�� */
    oal_uint16 us_abnormal_index;    /* ����������ͳ�� */
    oal_uint8 uc_idx_for_array;      /* �ڷ���������ں�������ֵ */
    oal_uint8 uc_skb_id_idx;         /* �ڷ���������ں�������ֵ */

    oal_uint16 aus_rx_skb_id_list[OAM_PROFILING_RX_PACKET_MAX_COUNT];                                     /* ���ڽ��ڼ������͵İ���offset֮�ͣ����浽��Ӧ����������Ԫ���� */
    oal_uint16 aus_rx_pass_count[OAM_PROFILING_RX_PACKET_MAX_COUNT][OAM_PROFILING_RX_FUNC_BUTT];          /* ������������ͳ�� */
    oal_uint32 ast_rx_func_stand_time[OAM_PROFILING_RX_PACKET_MAX_COUNT][OAM_PROFILING_MAX_POINT_NUM];    /* ����ÿ���ڵ�Ļ�׼ʱ�� */
    oal_uint32 ast_rx_func_time_offset[OAM_PROFILING_RX_PACKET_MAX_COUNT][OAM_PROFILING_RX_OFFSET_COUNT]; /* ÿ�ε�offsetʱ�� */
} oam_profiling_rx_statistic_stru;

/* alg����ͳ�ƽṹ�� */
typedef struct {
    oal_spin_lock_stru st_spin_lock; /* ��д�� */
    oal_uint16 us_abnormal_index;    /* ����������ͳ�� */
    oal_uint8 uc_idx_for_array;      /* �ڷ���������ں�������ֵ */
    oal_uint8 uc_skb_id_idx;         /* �ڷ���������ں�������ֵ */

    oal_uint16 aus_alg_skb_id_list[OAM_PROFILING_ALG_PACKET_MAX_COUNT];                                   /* ���ڽ��ڼ������͵İ������浽��Ӧ����������Ԫ���� */
    oal_uint16 aus_alg_pass_count[OAM_PROFILING_ALG_PACKET_MAX_COUNT][OAM_PROFILING_ALG_FUNC_BUTT];       /* ������������ͳ�� */
    oal_uint32 ast_alg_func_stand_time[OAM_PROFILING_ALG_PACKET_MAX_COUNT][OAM_PROFILING_MAX_POINT_NUM];  /* ����������׼ʱ�� */
    oal_uint32 ast_alg_func_time_offset[OAM_PROFILING_ALG_PACKET_MAX_COUNT][OAM_PROFILING_ALG_FUNC_BUTT]; /* �������ʱ��� */
} oam_profiling_alg_statistic_stru;

/* оƬ����ʱ��ͳ�ƽṹ�� */
typedef struct {
    oal_uint32 aul_timestamp[OAM_PROFILING_STARTTIME_FUNC_BUTT];
} oam_profiling_starttime_statistic_stru;

/* �ŵ��л�ʱ��ͳ�� */
typedef struct {
    oal_uint32 aul_timestamp[OAM_PROFILING_CHSWITCH_FUNC_BUTT];
} oam_profiling_chswitch_statistic_stru;

/* OAM PROFILING�Ŀ��ƽṹ */
typedef struct {
    oal_int i_profiling_tx_debuging_enable;
    oal_int i_profiling_rx_debuging_enable;
    oal_int i_profiling_alg_debuging_enable;
} oam_profiling_statistic_debug_stru;

/* �Զ���skb cb��Ӧ�Ľṹ�壬�������ò�����Ϣ */
typedef struct {
    oal_uint8 auc_resv[44]; /* ��ping����ͳ�������壬��Ӧ��dmac_rx_ctrl_stru */

    oal_uint16 us_skb_id;       /* skb id�� */
    oal_uint8 uc_is_broad_cast; /* ���ݰ��Ƿ�Ϊ�㲥�� */
    oal_uint8 uc_is_used;
} oam_profiling_skb_cb_stru;

/* trace��Ӧ�������� */
extern oal_uint32 oam_trace_init(oal_void);
extern oal_uint32 oam_trace_set_switch(oal_switch_enum_uint8 en_switch);

#endif /* end of oam_trace.h */