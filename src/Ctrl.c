/******************************************************************************/
/*                                                                            */
/* src/Ctrl.c                                                                 */
/*                                                                 2019/09/01 */
/* Copyright (C) 2019 Mochi.                                                  */
/*                                                                            */
/******************************************************************************/
/******************************************************************************/
/* インクルード                                                               */
/******************************************************************************/
/* 標準ヘッダ */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* ライブラリヘッダ */
#include <libmk.h>

/* モジュール内ヘッダ */
#include "ns16550.h"
#include "Ctrl.h"
#include "Debug.h"


/******************************************************************************/
/* 定義                                                                       */
/******************************************************************************/
/** COM制御情報 */
typedef struct {
    uint16_t dl;    /**< ディバイザラッチ設定値 */
    uint8_t  ier;   /**< IER設定値              */
    uint8_t  fcr;   /**< FCR設定値              */
    uint8_t  lcr;   /**< LCR設定値              */
    uint8_t  mcr;   /**< MCR設定値              */
} ctrlInfo_t;


/******************************************************************************/
/* グローバル変数定義                                                         */
/******************************************************************************/
/** I/Oポートベースアドレス変換 */
const uint16_t gIoBase[] = {
    NS16550_COM1_IOBASE,    /* COM1I/Oポートベース */
    NS16550_COM2_IOBASE,    /* COM2I/Oポートベース */
    NS16550_COM3_IOBASE,    /* COM3I/Oポートベース */
    NS16550_COM4_IOBASE  }; /* COM4I/Oポートベース */

/** COM制御情報 */
static ctrlInfo_t gCtrlInfo[ NS16550_COM_NUM ];


/******************************************************************************/
/* グローバル関数定義                                                         */
/******************************************************************************/
/******************************************************************************/
/**
 * @brief       割込み無効化
 * @details     NS16550の特定の割込みを無効にする。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16550_COM3 COM3
 *                  - NS16550_COM4 COM4
 * @param[in]   flag  無効化割込みフラグ
 *                  - NS16550_IER_RBR データ受信割込み可否フラグ
 *                  - NS16550_IER_THR THR空割込み可否フラグ
 *                  - NS16550_IER_LSR LSR要因割込み可否フラグ
 *                  - NS16550_IER_MSR MSR要因割込み可否フラグ
 */
/******************************************************************************/
void CtrlDisableInterrupt( NS16550ComNo_t comNo,
                           uint8_t        flag   )
{
    DEBUG_LOG_FNC(
        "%s(): start. comNo=%d, flag=0x%02X",
        __func__,
        comNo,
        flag
    );

    /* 割込み設定値計算 */
    gCtrlInfo[ comNo ].ier &= ~flag;

    /* 割込み設定 */
    LibMkIoPortOutByte(
        gIoBase[ comNo ] + NS16550_IER,
        &( gCtrlInfo[ comNo ].ier ),
        1,
        NULL
    );

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       割込み有効化
 * @details     NS16550の特定の割込みを有効にする。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16550_COM3 COM3
 *                  - NS16550_COM4 COM4
 * @param[in]   flag  有効化割込みフラグ
 *                  - NS16550_IER_RBR データ受信割込み可否フラグ
 *                  - NS16550_IER_THR THR空割込み可否フラグ
 *                  - NS16550_IER_LSR LSR要因割込み可否フラグ
 *                  - NS16550_IER_MSR MSR要因割込み可否フラグ
 */
/******************************************************************************/
void CtrlEnableInterrupt( NS16550ComNo_t comNo,
                          uint8_t        flag   )
{
    DEBUG_LOG_FNC(
        "%s(): start. comNo=%d, flag=0x%02X",
        __func__,
        comNo,
        flag
    );

    /* 割込み設定値計算 */
    gCtrlInfo[ comNo ].ier |= flag;

    /* 割込み設定 */
    LibMkIoPortOutByte(
        gIoBase[ comNo ] + NS16550_IER,
        &( gCtrlInfo[ comNo ].ier ),
        1,
        NULL
    );

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       割込み要因レジスタ読込み
 * @details     引数comNoの割込み要因レジスタを読み込む。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16550_COM3 COM3
 *                  - NS16550_COM4 COM4
 *
 * @return      割込み要因レジスタ値を返す。
 */
/******************************************************************************/
uint8_t CtrlInIIR( NS16550ComNo_t comNo )
{
    uint8_t iir;    /* レジスタ値 */

    DEBUG_LOG_FNC( "%s(): start. comNo=%d", __func__, comNo );

    /* 初期化 */
    iir = 0;

    /* 割込み要因レジスタ読込み */
    LibMkIoPortInByte(
        gIoBase[ comNo ] + NS16550_IIR,
        &iir,
        1,
        NULL
    );

    DEBUG_LOG_FNC( "%s(): end. iir=0x%02X", __func__, iir );
    return iir;
}


/******************************************************************************/
/**
 * @brief       ラインステータスレジスタ読込み
 * @details     引数comNoのラインステータスレジスタを読み込む。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16550_COM3 COM3
 *                  - NS16550_COM4 COM4
 *
 * @return      ラインステータスレジスタ値を返す。
 */
/******************************************************************************/
uint8_t CtrlInLSR( NS16550ComNo_t comNo )
{
    uint8_t lsr;    /* レジスタ値 */

    DEBUG_LOG_FNC( "%s(): start. comNo=%d", __func__, comNo );

    /* 初期化 */
    lsr = 0;

    /* ラインステータスレジスタ読込み */
    LibMkIoPortInByte(
        gIoBase[ comNo ] + NS16550_LSR,
        &lsr,
        1,
        NULL
    );

    DEBUG_LOG_FNC( "%s(): end. lsr=0x%02X", __func__, lsr );
    return lsr;
}


/******************************************************************************/
/**
 * @brief       モデムステータスレジスタ読込み
 * @details     引数comNoのモデムステータスレジスタを読み込む。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16550_COM3 COM3
 *                  - NS16550_COM4 COM4
 *
 * @return      モデムステータスレジスタ値を返す。
 */
/******************************************************************************/
uint8_t CtrlInMSR( NS16550ComNo_t comNo )
{
    uint8_t msr;    /* レジスタ値 */

    DEBUG_LOG_FNC( "%s(): start. comNo=%d", __func__, comNo );

    /* 初期化 */
    msr = 0;

    /* モデムステータスレジスタ読込み */
    LibMkIoPortInByte(
        gIoBase[ comNo ] + NS16550_MSR,
        &msr,
        1,
        NULL
    );

    DEBUG_LOG_FNC( "%s(): end. msr=0x%02X", __func__, msr );
    return msr;
}


/******************************************************************************/
/**
 * @brief       受信バッファレジスタ読込み
 * @details     引数comNoの受信バッファレジスタを読み込む。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16550_COM3 COM3
 *                  - NS16550_COM4 COM4
 *
 * @return      受信バッファレジスタ値を返す。
 */
/******************************************************************************/
uint8_t CtrlInRBR( NS16550ComNo_t comNo )
{
    uint8_t rbr;    /* レジスタ値 */

    DEBUG_LOG_FNC( "%s(): start. comNo=%d", __func__, comNo );

    /* 初期化 */
    rbr = 0;

    /* 受信バッファレジスタ読込み */
    LibMkIoPortInByte(
        gIoBase[ comNo ] + NS16550_RBR,
        &rbr,
        1,
        NULL
    );

    DEBUG_LOG_FNC( "%s(): end. rbr=0x%02X", __func__, rbr );
    return rbr;
}


/******************************************************************************/
/**
 * @brief       NS16550初期化
 * @details     指定したCOMポートのNS16550を初期化する。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16550_COM3 COM3
 *                  - NS16550_COM4 COM4
 */
/******************************************************************************/
void CtrlInit( NS16550ComNo_t comNo )
{
    DEBUG_LOG_FNC( "%s(): start. comNo=%d", __func__, comNo );

    /* COM制御情報初期化 */
    memset( &gCtrlInfo[ comNo ], 0, sizeof ( ctrlInfo_t ) );
    gCtrlInfo[ comNo ].ier = 0;
    gCtrlInfo[ comNo ].lcr = NS16550_LCR_WLS_8         |
                             NS16550_LCR_STB_1         |
                             NS16550_LCR_PEN_DISABLE   |
                             NS16550_LCR_EPS_ODD       |
                             NS16550_LCR_STICK_DISABLE |
                             NS16550_LCR_BREAK_DISABLE |
                             NS16550_LCR_DLAB_OFF;
    gCtrlInfo[ comNo ].fcr = NS16550_FCR_FIFO_ENABLE  |
                             NS16550_FCR_RFIFO_ENABLE |
                             NS16550_FCR_TFIFO_ENABLE |
                             NS16550_FCR_TRG_14;
    gCtrlInfo[ comNo ].mcr = NS16550_MCR_DTR_H    |
                             NS16550_MCR_RTS_H    |
                             NS16550_MCR_OUT1_L   |
                             NS16550_MCR_OUT2_L   |   /* 割込み禁止 */
                             NS16550_MCR_LOOP_OFF;

    /* 割込み無効化 */
    CtrlEnableInterrupt( comNo,
                         ( NS16550_IER_RBR |
                           NS16550_IER_THR |
                           NS16550_IER_LSR |
                           NS16550_IER_MSR   ) );

    /* ディバイザラッチ設定 */
    CtrlSetDivisorLatch( comNo, NS16550_DIVISOR_LATCH_DEFAULT );

    /* FIFOコントロールレジスタ設定 */
    LibMkIoPortOutByte(
        gIoBase[ comNo ] + NS16550_FCR,
        &( gCtrlInfo[ comNo ].fcr ),
        1,
        NULL
    );

    /* モデムコントロールレジスタ設定 */
    LibMkIoPortOutByte(
        gIoBase[ comNo ] + NS16550_MCR,
        &( gCtrlInfo[ comNo ].mcr ),
        1,
        NULL
    );

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       THR書込み
 * @details     THRに値を書き込む
 *
 * @param[in]   comNo COM番号
 * @param[in]   value 書込み値
 */
/******************************************************************************/
void CtrlOutTHR( NS16550ComNo_t comNo,
                 uint8_t        value  )
{
    DEBUG_LOG_FNC(
        "%s(): start. comNo=%d, value=0x%02X",
        __func__,
        comNo,
        value
    );

    /* Transmit Holding Register書込 */
    LibMkIoPortOutByte(
        gIoBase[ comNo ] + NS16550_THR,
        &value,
        1,
        NULL
    );

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       ディバイザラッチ設定
 * @details     NS16550のディバイザラッチを設定する。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16550_COM3 COM3
 *                  - NS16550_COM4 COM4
 * @param[in]   value ディバイザラッチ値
 */
/******************************************************************************/
void CtrlSetDivisorLatch( NS16550ComNo_t comNo,
                          uint16_t       value  )
{
    uint8_t dll;    /* ディバイザラッチ（最下位byte） */
    uint8_t dlm;    /* ディバイザラッチ（最上位byte） */

    DEBUG_LOG_FNC(
        "%s(): start, comNo=%d, value=0x%02X",
        __func__,
        comNo,
        value
    );

    /* 初期化 */
    dll = ( uint8_t ) ( ( value      ) & 0x00FFu );
    dlm = ( uint8_t ) ( ( value >> 8 ) & 0x00FFu );

    /* 設定値保存 */
    gCtrlInfo[ comNo ].dl = value;

    /* ディバイザラッチアクセス設定 */
    CtrlSetLCR( comNo, NS16550_LCR_DLAB, NS16550_LCR_DLAB_ON );

    /* ディバイザラッチ（最下位byte）設定 */
    LibMkIoPortOutByte(
        gIoBase[ comNo ] + NS16550_DLL,
        &dll,
        1,
        NULL
    );

    /* ディバイザラッチ（最上位byte）設定 */
    LibMkIoPortOutByte(
        gIoBase[ comNo ] + NS16550_DLM,
        &dlm,
        1,
        NULL
    );

    /* ディバイザラッチアクセス解除 */
    CtrlSetLCR( comNo, NS16550_LCR_DLAB, NS16550_LCR_DLAB_OFF );

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       FIFOコントロールレジスタ設定
 * @details     FIFOコントロールレジスタの特定bitフィールドを設定する。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16500_COM3 COM3
 *                  - NS16550_COM4 COM4
 * @param[in]   mask  bitフィールドマスク値
 *                  - NS16550_FCR_FIFO  FIFO有効フラグ
 *                  - NS16550_FCR_RFIFO 受信FIFO有効フラグ
 *                  - NS16550_FCR_TFIFO 転送FIFO有効フラグ
 *                  - NS16550_FCR_TRG   受信トリガレベル
 * @param[in]   value 設定値
 */
/******************************************************************************/
void CtrlSetFCR( NS16550ComNo_t comNo,
                 uint8_t        mask,
                 uint8_t        value  )
{
    DEBUG_LOG_FNC(
        "%s(): start. comNo=%d, mask=0x%02X, value=0x%02X",
        __func__,
        comNo,
        mask,
        value
    );

    /* 書込値設定 */
    gCtrlInfo[ comNo ].fcr &= ~mask;
    gCtrlInfo[ comNo ].fcr |= value;

    /* FIFOコントロールレジスタ設定 */
    LibMkIoPortOutByte(
        gIoBase[ comNo ] + NS16550_FCR,
        &( gCtrlInfo[ comNo ].fcr ),
        1,
        NULL
    );

    DEBUG_LOG_FNC( "%s() end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       ラインコントロールレジスタ設定
 * @details     ラインコントロールレジスタの特定bitフィールドを設定する。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16500_COM3 COM3
 *                  - NS16550_COM4 COM4
 * @param[in]   mask  bitフィールドマスク値
 *                  - NS16550_LCR_WLS   ワード長
 *                  - NS16550_LCR_STB   ストップビット数
 *                  - NS16550_LCR_PEN   パリティ許可フラグ
 *                  - NS16550_LCR_EPS   パリティ選択フラグ
 *                  - NS16550_LCR_STICK スティックパリティフラグ
 *                  - NS16550_LCR_BREAK ブレイク制御フラグ
 *                  - NS16550_LCR_DLAB  ディバイザラッチアクセス
 * @param[in]   value 設定値
 */
/******************************************************************************/
void CtrlSetLCR( NS16550ComNo_t comNo,
                 uint8_t        mask,
                 uint8_t        value  )
{
    DEBUG_LOG_FNC(
        "%s(): start. comNo=%d, mask=0x%02X, value=0x%02X",
        __func__,
        comNo,
        mask,
        value
    );

    /* 書込値設定 */
    gCtrlInfo[ comNo ].lcr &= ~mask;
    gCtrlInfo[ comNo ].lcr |= value;

    /* ラインコントロールレジスタ設定 */
    LibMkIoPortOutByte(
        gIoBase[ comNo ] + NS16550_LCR,
        &( gCtrlInfo[ comNo ].lcr ),
        1,
        NULL
    );

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       モデムコントロールレジスタ設定
 * @details     モデムコントロールレジスタの特定bitフィールドを設定する。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16500_COM3 COM3
 *                  - NS16550_COM4 COM4
 * @param[in]   mask  bitフィールドマスク値
 *                  - NS16550_MCR_DTR  DTRピン出力
 *                  - NS16550_MCR_RTS  RTSピン出力
 *                  - NS16550_MCR_OUT1 OUT1ピン出力
 *                  - NS16550_MCR_OUT2 OUT2ピン出力
 *                  - NS16550_MCR_LOOP ループバックモード
 * @param[in]   value 設定値
 */
/******************************************************************************/
void CtrlSetMCR( NS16550ComNo_t comNo,
                 uint8_t        mask,
                 uint8_t        value  )
{
    DEBUG_LOG_FNC(
        "%s(): start. comNo=%d, mask=0x%02X, value=0x%02X",
        __func__,
        comNo,
        mask,
        value
    );

    /* 書込値設定 */
    gCtrlInfo[ comNo ].mcr &= ~mask;
    gCtrlInfo[ comNo ].mcr |= value;

    /* モデムコントロールレジスタ設定 */
    LibMkIoPortOutByte(
        gIoBase[ comNo ] + NS16550_MCR,
        &( gCtrlInfo[ comNo ].mcr ),
        1,
        NULL
    );

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
