

#ifndef __OAL_WINDOWS_TYPES_H__
#define __OAL_WINDOWS_TYPES_H__

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/*****************************************************************************
  1 ����ͷ�ļ�����
*****************************************************************************/


/*****************************************************************************
  2 �����������Ͷ���
*****************************************************************************/
typedef char                    oal_int8;           /* ����ǰ׺:c */
typedef short                   oal_int16;          /* ����ǰ׺:s */
typedef int                     oal_int32;          /* ����ǰ׺:l */
typedef long long               oal_int64;          /* ����ǰ׺:ll */
typedef unsigned char           oal_uint8;          /* ����ǰ׺:uc */
typedef unsigned short          oal_uint16;         /* ����ǰ׺:us */
typedef unsigned int            oal_uint32;         /* ����ǰ׺:ul */
typedef unsigned long long      oal_uint64;         /* ����ǰ׺:ull */
typedef void                    oal_void;

/*****************************************************************************
  3 �궨��
*****************************************************************************/
#ifdef INLINE_TO_FORCEINLINE
#define OAL_INLINE      __forceinline
#else
#define OAL_INLINE      __inline
#endif

#if ((_PRE_TEST_MODE_UT == _PRE_TEST_MODE) || (_PRE_TEST_MODE_ST == _PRE_TEST_MODE) || defined(_PRE_WIFI_DMT))
#define OAL_STATIC
#else
#define OAL_STATIC          static
#endif

#define OAL_VOLATILE

#define LINUX_VERSION_CODE    0x30000 /* TBD  Hi1102 ��Ҫ�޸� */
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))


/*****************************************************************************
  4 ö�ٶ���
*****************************************************************************/


/*****************************************************************************
  5 ȫ�ֱ�������
*****************************************************************************/


/*****************************************************************************
  6 ��Ϣͷ����
*****************************************************************************/


/*****************************************************************************
  7 ��Ϣ����
*****************************************************************************/


/*****************************************************************************
  8 STRUCT����
*****************************************************************************/


/*****************************************************************************
  9 UNION����
*****************************************************************************/


/*****************************************************************************
  10 ��������
*****************************************************************************/


#ifdef __cplusplus
    #if __cplusplus
        }
    #endif
#endif

#endif /* end of oal_types.h */
