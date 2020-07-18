/******************************************************************************/
/*                                                                            */
/* src/Ioctrl.c                                                               */
/*                                                                 2020/07/18 */
/* Copyright (C) 2019-2020 Mochi.                                             */
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
#include "Debug.h"
#include "Ioctrl.h"
#include "ns16550.h"


/******************************************************************************/
/* 定義                                                                       */
/******************************************************************************/
/** COM制御情報 */
typedef struct {
    uint16_t dl;    /**< Divisor Latch設定値 */
    uint8_t  ier;   /**< IER設定値           */
    uint8_t  fcr;   /**< FCR設定値           */
    uint8_t  lcr;   /**< LCR設定値           */
    uint8_t  mcr;   /**< MCR設定値           */
} ctrlInfo_t;


/******************************************************************************/
/* ローカル関数宣言                                                           */
/******************************************************************************/
/* デバイス初期化 */
static void Init( NS16550ComNo_t comNo );


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
 * @brief       入出力制御初期化
 * @details     NS16550またはその互換デバイスを初期化する。
 */
/******************************************************************************/
void IoctrlInit( void )
{
    /* COM1初期化 */
    Init( NS16550_COM1 );

    /* COM2初期化 */
    Init( NS16550_COM2 );

    return;
}


/******************************************************************************/
/**
 * @brief       IIR読込み
 * @details     Interrupt Identifing Registerを読み込む。
 *
 * @param[in]   comNo デバイス識別番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *
 * @return      レジスタ値を返す。
 */
/******************************************************************************/
uint8_t IoctrlInIIR( NS16550ComNo_t comNo )
{
    uint8_t iir;    /* レジスタ値 */

    /* 初期化 */
    iir = 0;

    /* IIR読込み */
    LibMkIoPortInByte( gIoBase[ comNo ] + NS16550_IIR, &iir, 1, NULL );

    DEBUG_LOG_TRC( "%s(): COM%u, 0x%02x", __func__, comNo + 1, iir );

    return iir;
}


/******************************************************************************/
/**
 * @brief       LSR読込み
 * @details     Line Status Registerを読み込む。
 *
 * @param[in]   comNo デバイス識別番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *
 * @return      レジスタ値を返す。
 */
/******************************************************************************/
uint8_t IoctrlInLSR( NS16550ComNo_t comNo )
{
    uint8_t lsr;    /* レジスタ値 */

    /* 初期化 */
    lsr = 0;

    /* LSR読込み */
    LibMkIoPortInByte( gIoBase[ comNo ] + NS16550_LSR, &lsr, 1, NULL );

    DEBUG_LOG_TRC( "%s(): COM%u, 0x%02X", __func__, comNo + 1, lsr );

    return lsr;
}


/******************************************************************************/
/**
 * @brief       MSR読込み
 * @details     Modem Status Registerを読み込む。
 *
 * @param[in]   comNo デバイス識別番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *
 * @return      レジスタ値を返す。
 */
/******************************************************************************/
uint8_t IoctrlInMSR( NS16550ComNo_t comNo )
{
    uint8_t msr;    /* レジスタ値 */

    /* 初期化 */
    msr = 0;

    /* MSR読込み */
    LibMkIoPortInByte( gIoBase[ comNo ] + NS16550_MSR, &msr, 1, NULL );

    DEBUG_LOG_TRC( "%s(): COM%u, 0x%02X", __func__, comNo + 1, msr );

    return msr;
}


/******************************************************************************/
/**
 * @brief       RBR読込み
 * @details     Receiver Buffer Registerを読み込む。
 *
 * @param[in]   comNo デバイス識別番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *
 * @return      レジスタ値を返す。
 */
/******************************************************************************/
uint8_t IoctrlInRBR( NS16550ComNo_t comNo )
{
    uint8_t rbr;    /* レジスタ値 */

    /* 初期化 */
    rbr = 0;

    /* RBR読込み */
    LibMkIoPortInByte( gIoBase[ comNo ] + NS16550_RBR, &rbr, 1, NULL );

    DEBUG_LOG_TRC( "%s(): COM%u, 0x%02X", __func__, comNo + 1, rbr );

    return rbr;
}


/******************************************************************************/
/**
 * @brief       THR書込み
 * @details     Transmit Holding Registerに値を書き込む。
 *
 * @param[in]   comNo   デバイス識別番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 * @param[in]   *pValue 書込み値
 * @param[in]   size    書込みサイズ
 */
/******************************************************************************/
void IoctrlOutTHR( NS16550ComNo_t comNo,
                   uint8_t        *pValue,
                   size_t         size     )
{
    /* THR書込み */
    LibMkIoPortOutByte( gIoBase[ comNo ] + NS16550_THR, pValue, size, NULL );

    DEBUG_LOG_TRC( "%s(): COM%u, size=%u", __func__, comNo + 1, size );

    return;
}


/******************************************************************************/
/**
 * @brief       DivisorLatch設定
 * @details     Divisor Latchを設定する。
 *
 * @param[in]   comNo デバイス識別番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 * @param[in]   value ディバイザラッチ値
 */
/******************************************************************************/
void IoctrlSetDivisorLatch( NS16550ComNo_t comNo,
                            uint16_t       value  )
{
    uint8_t dll;    /* ディバイザラッチ（最下位byte） */
    uint8_t dlm;    /* ディバイザラッチ（最上位byte） */

    /* 初期化 */
    dll = ( uint8_t ) ( ( value      ) & 0x00FFu );
    dlm = ( uint8_t ) ( ( value >> 8 ) & 0x00FFu );

    /* 設定値保存 */
    gCtrlInfo[ comNo ].dl = value;

    /* Divisor Latchアクセス設定 */
    IoctrlSetLCR( comNo, NS16550_LCR_DLAB, NS16550_LCR_DLAB_ON );

    /* Divisor Latch（最下位byte）書込み */
    LibMkIoPortOutByte( gIoBase[ comNo ] + NS16550_DLL, &dll, 1, NULL );

    /* Divisor Latch（最上位byte）書込み */
    LibMkIoPortOutByte( gIoBase[ comNo ] + NS16550_DLM, &dlm, 1, NULL );

    DEBUG_LOG_TRC( "%s(): COM%u, 0x%02X_%02X", __func__, comNo + 1, dlm, dll );

    /* 非Divisor Latchアクセス設定 */
    IoctrlSetLCR( comNo, NS16550_LCR_DLAB, NS16550_LCR_DLAB_OFF );

    return;
}


/******************************************************************************/
/**
 * @brief       FCR設定
 * @details     FIFO Control Registerを設定する。
 *
 * @param[in]   comNo デバイス識別番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 * @param[in]   mask  bitフィールドマスク値
 *                  - NS16550_FCR_FIFO   FIFO有効フラグ
 *                  - NS16550_FCR_RXFIFO 受信FIFO有効フラグ
 *                  - NS16550_FCR_TXFIFO 転送FIFO有効フラグ
 *                  - NS16550_FCR_TRG    受信トリガレベル
 * @param[in]   value 設定値
 */
/******************************************************************************/
void IoctrlSetFCR( NS16550ComNo_t comNo,
                   uint8_t        mask,
                   uint8_t        value  )
{
    /* 書込値設定 */
    value |= gCtrlInfo[ comNo ].fcr & ~mask;

    /* FCR書込み */
    LibMkIoPortOutByte( gIoBase[ comNo ] + NS16550_FCR, &value, 1, NULL );

    DEBUG_LOG_TRC( "%s() COM%u, 0x%02X", __func__, comNo + 1, value );

    /* 設定値保存 */
    gCtrlInfo[ comNo ].fcr = value &
                             ~( NS16550_FCR_TXFIFO | NS16550_FCR_RXFIFO );

    return;
}


/******************************************************************************/
/**
 * @brief       IER設定
 * @details     Interrupt Enable Registerを設定する。
 *
 * @param[in]   comNo デバイス識別番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 * @param[in]   mask  bitフィールドマスク値
 *                  - NS16550_IER_RBR データ受信割込み可否フラグ
 *                  - NS16550_IER_THR THR空割込み可否フラグ
 *                  - NS16550_IER_LSR LSR要因割込み可否フラグ
 *                  - NS16550_IER_MSR MSR要因割込み可否フラグ
 * @param[in]   value 設定値
 */
/******************************************************************************/
void IoctrlSetIER( NS16550ComNo_t comNo,
                   uint8_t        mask,
                   uint8_t        value   )
{
    /* 書込値設定 */
    value |= gCtrlInfo[ comNo ].ier & ~mask;

    /* IER書込み */
    LibMkIoPortOutByte( gIoBase[ comNo ] + NS16550_IER, &value, 1, NULL );

    DEBUG_LOG_TRC( "%s(): COM%u, 0x%02X", __func__, comNo + 1, value );

    /* 設定値保存 */
    gCtrlInfo[ comNo ].ier = value;

    return;
}


/******************************************************************************/
/**
 * @brief       LCR設定
 * @details     Line Control Registerを設定する。
 *
 * @param[in]   comNo デバイス識別番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
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
void IoctrlSetLCR( NS16550ComNo_t comNo,
                   uint8_t        mask,
                   uint8_t        value  )
{
    /* 書込値設定 */
    value |= gCtrlInfo[ comNo ].lcr & ~mask;

    /* LCR書込み */
    LibMkIoPortOutByte( gIoBase[ comNo ] + NS16550_LCR, &value, 1, NULL );

    DEBUG_LOG_TRC( "%s(): COM%u, 0x%02X", __func__, comNo + 1, value );

    /* 設定値保存 */
    gCtrlInfo[ comNo ].lcr = value;

    return;
}


/******************************************************************************/
/**
 * @brief       MCR設定
 * @details     Modem Control Registerを設定する。
 *
 * @param[in]   comNo デバイス識別番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 * @param[in]   mask  bitフィールドマスク値
 *                  - NS16550_MCR_DTR  DTRピン出力
 *                  - NS16550_MCR_RTS  RTSピン出力
 *                  - NS16550_MCR_OUT1 OUT1ピン出力
 *                  - NS16550_MCR_OUT2 OUT2ピン出力
 *                  - NS16550_MCR_LOOP ループバックモード
 * @param[in]   value 設定値
 */
/******************************************************************************/
void IoctrlSetMCR( NS16550ComNo_t comNo,
                   uint8_t        mask,
                   uint8_t        value  )
{
    /* 書込値設定 */
    value |= gCtrlInfo[ comNo ].mcr & ~mask;

    /* MCR書込み */
    LibMkIoPortOutByte( gIoBase[ comNo ] + NS16550_MCR, &value, 1, NULL );

    DEBUG_LOG_TRC( "%s(): COM%u, 0x%02X", __func__, comNo + 1, value );

    /* 設定値保存 */
    gCtrlInfo[ comNo ].mcr = value;

    return;
}


/******************************************************************************/
/* ローカル関数定義                                                           */
/******************************************************************************/
/******************************************************************************/
/**
 * @brief       デバイス初期化
 * @details     NS16550またはその互換デバイスを初期化する。
 *
 * @param[in]   comNo デバイス識別番号
 *              - NS16550_COM1 COM1
 *              - NS16550_COM2 COM2
 */
/******************************************************************************/
static void Init( NS16550ComNo_t comNo )
{
    /* COM制御情報初期化 */
    memset( &gCtrlInfo[ comNo ], 0, sizeof ( ctrlInfo_t ) );

    /* IER初期化 */
    IoctrlSetIER( comNo,
                  NS16550_IER_ALL,                  /* 全有効ビット設定  */
                  ( NS16550_IER_MSR_DISABLE |       /* MSR要因割込み禁止 */
                    NS16550_IER_LSR_DISABLE |       /* LSR要因割込み禁止 */
                    NS16550_IER_THR_DISABLE |       /* THR要因割込み禁止 */
                    NS16550_IER_RBR_DISABLE   ) );  /* RBR要因割込み禁止 */

    /* MCR初期化 */
    IoctrlSetMCR( comNo,
                  NS16550_MCR_ALL,                  /* 全有効ビット設定     */
                  ( NS16550_MCR_LOOP_OFF |          /* 非ループバックモード */
                    NS16550_MCR_OUT2_L   |          /* Not(OUT2)=H          */
                    NS16550_MCR_OUT1_H   |          /* Not(OUT1)=L          */
                    NS16550_MCR_RTS_H    |          /* Not(RTS)=L           */
                    NS16550_MCR_DTR_H      ) );     /* Not(DTR)=L           */

    /* LCR初期化 */
    IoctrlSetLCR( comNo,
                  NS16550_LCR_ALL,                  /* 全有効ビット設定       */
                  ( NS16550_LCR_DLAB_OFF      |     /* 非DivisorLatchアクセス */
                    NS16550_LCR_BREAK_DISABLE |     /* Break信号送出無し      */
                    NS16550_LCR_STICK_DISABLE |     /* 固定パリティしない     */
                    NS16550_LCR_EPS_ODD       |     /* 奇数パリティ           */
                    NS16550_LCR_PEN_DISABLE   |     /* パリティbit無し        */
                    NS16550_LCR_STB_1         |     /* ストップbit長1         */
                    NS16550_LCR_WLS_8           ) );/* データbit長1           */

    /* Divisor Latch初期化 */
    IoctrlSetDivisorLatch( comNo, NS16550_DIVISOR_LATCH_DEFAULT );

    /* FCR初期化 */
    IoctrlSetFCR( comNo,
                  NS16550_FCR_ALL,                  /* 全有効ビット設定     */
                  ( NS16550_FCR_TRG_14      |       /* 受信FIFOトリガ14byte */
                    NS16550_FCR_TXFIFO_RST  |       /* 転送FIFOリセット     */
                    NS16550_FCR_RXFIFO_RST  |       /* 受信FIFOリセット     */
                    NS16550_FCR_FIFO_ENABLE   ) );  /* FIFO有効             */

    return;
}


/******************************************************************************/
