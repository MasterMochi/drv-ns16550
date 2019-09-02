/******************************************************************************/
/*                                                                            */
/* src/Receive.c                                                              */
/*                                                                 2019/09/01 */
/* Copyright (C) 2019 Mochi.                                                  */
/*                                                                            */
/******************************************************************************/
/******************************************************************************/
/* インクルード                                                               */
/******************************************************************************/
/* 標準ヘッダ */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

/* ライブラリヘッダ */
#include <libmk.h>

/* モジュール内ヘッダ */
#include "ns16550.h"
#include "Buffer.h"
#include "Ctrl.h"
#include "Debug.h"
#include "Main.h"
#include "Receive.h"
#include "Transmit.h"


/******************************************************************************/
/* 定義                                                                       */
/******************************************************************************/
/* スレッドスタックサイズ */
#define STACK_SIZE ( 8192 )


/******************************************************************************/
/* ローカル関数宣言                                                           */
/******************************************************************************/
/* 割込み有効化 */
static void EnableInterrupt( NS16550ComNo_t comNo );
/* 割込み初期設定 */
static void InitInterrupt( uint8_t irqNo );
/* 割込み処理 */
static void ProcInterrupt( NS16550ComNo_t comNo );
/* LSR要因割込み処理 */
static void ProcInterruptLineStatus( NS16550ComNo_t comNo );
/* MSR要因割込み処理 */
static void ProcInterruptModemStatus( NS16550ComNo_t comNo );
/* THR空割込み処理 */
static void ProcInterruptTx( NS16550ComNo_t comNo );
/* データ受信割込み処理 */
static void ProcInterruptRx( NS16550ComNo_t comNo );
/* 受信制御スレッド */
static void Receiver( void *pArg );


/******************************************************************************/
/* グローバル変数宣言                                                         */
/******************************************************************************/
/* 受信スレッドタスクID */
static MkTaskId_t gTaskId;


/******************************************************************************/
/* グローバル関数宣言                                                         */
/******************************************************************************/
/******************************************************************************/
/**
 * @brief       受信制御初期化
 * @details     受信制御スレッドを生成する。
 */
/******************************************************************************/
void ReceiveInit( void )
{
    void    *pStack;    /* スレッドスタック */
    MkRet_t retMk;      /* カーネル戻り値   */
    MkErr_t err;        /* カーネルエラー   */

    DEBUG_LOG_FNC( "%s(): start.", __func__ );

    /* スタック割当て */
    pStack = malloc( STACK_SIZE );

    /* 割当て結果判定 */
    if ( pStack == NULL ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "malloc()" );
        DEBUG_ABORT();
    }

    /* スレッド生成 */
    retMk = LibMkThreadCreate( &Receiver,
                               NULL,
                               pStack,
                               STACK_SIZE,
                               &gTaskId,
                               &err        );

    /* 生成結果判定 */
    if ( retMk != MK_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "LibMkThreadCreate(): ret=%d, err=0x%X", retMk, err );
        DEBUG_ABORT();
    }

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       受信制御スレッドタスクID取得
 * @details     受信制御スレッドのタスクIDを取得する。
 *
 * @return      タスクIDを返す。
 */
/******************************************************************************/
MkTaskId_t ReceiveGetTaskId( void )
{
    return gTaskId;
}


/******************************************************************************/
/* ローカル関数定義                                                           */
/******************************************************************************/
/******************************************************************************/
/**
 * @brief       割込み有効化
 * @details     NS16550の割込みを有効化する。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16550_COM3 COM3
 *                  - NS16550_COM4 COM4
 */
/******************************************************************************/
static void EnableInterrupt( NS16550ComNo_t comNo )
{
    uint8_t data;

    DEBUG_LOG_FNC( "%s(): start. comNo=%d", __func__, comNo );

    /* 割込み許可レジスタ設定 */
    CtrlEnableInterrupt( comNo,
                         ( NS16550_IER_RBR |
                           NS16550_IER_THR |
                           NS16550_IER_LSR |
                           NS16550_IER_MSR   ) );

    /* モデムコントロールレジスタ設定 */
    CtrlSetMCR( comNo, NS16550_MCR_OUT2, NS16550_MCR_OUT2_H );

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       割込み設定初期化
 * @details     割込み番号irqNoについて、割込み監視の開始と割込み有効化を行う。
 *
 * @param[in]   irqNo 割込み番号
 *                  - LIBMK_INT_IRQ3 IRQ3
 *                  - LIBMK_INT_IRQ4 IRQ4
 */
/******************************************************************************/
static void InitInterrupt( uint8_t irqNo )
{
    MkRet_t retMk;  /* カーネル戻り値 */
    MkErr_t err;    /* カーネルエラー */

    DEBUG_LOG_FNC( "%s(): start. irqNo=%u", __func__, irqNo );

    /* 初期化 */
    retMk = MK_RET_FAILURE;
    err   = MK_ERR_NONE;

    /* 割込み監視開始 */
    retMk = LibMkIntStartMonitoring( irqNo, &err );

    /* 開始結果判定 */
    if ( retMk != MK_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR(
            "LibMkIntStartMonitoring(): ret=%d, err=0x%X",
            retMk,
            err
        );
        DEBUG_ABORT();
    }

    /* 割込み有効化 */
    retMk = LibMkIntEnable( irqNo, &err );

    /* 有効化結果判定 */
    if ( retMk != MK_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "LibMkIntEnable(): ret=%d, err=0x%X", retMk, err );
        DEBUG_ABORT();
    }

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       割込み処理
 * @details     割込み要因を特定し、要因毎に対応した処理を呼び出す。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16550_COM3 COM3
 *                  - NS16550_COM4 COM4
 */
/******************************************************************************/
static void ProcInterrupt( NS16550ComNo_t comNo )
{
    uint8_t iir;    /* 割込み要因レジスタ値 */

    DEBUG_LOG_FNC( "%s(): start. comNo=%d", __func__, comNo );

    /* 割込み要因レジスタ読込 */
    iir = CtrlInIIR( comNo );

    /* 割込み保留判定 */
    if ( ( iir & NS16550_IIR_PENDING ) == NS16550_IIR_PENDING_NO ) {
        /* 割込み保留無し */

        DEBUG_LOG_TRC( "no pending: comNo=%d", comNo );
        DEBUG_LOG_FNC( "%s(): end.", __func__ );
        return;
    }

    /* 割込み要因判定 */
    switch ( iir & NS16550_IIR_ID ) {
        case NS16550_IIR_ID_MSR:
            /* MSR要因割込み */

            ProcInterruptModemStatus( comNo );
            break;

        case NS16550_IIR_ID_THR:
            /* THR空割込み */

            ProcInterruptTx( comNo );
            break;

        case NS16550_IIR_ID_RBR:
            /* データ受信割込み */
        case NS16550_IIR_ID_RBR_TO:
            /* データ受信タイムアウト割込み */

            ProcInterruptRx( comNo );
            break;

        case NS16550_IIR_ID_LSR:
            /* LSR要因割込み */

            ProcInterruptLineStatus( comNo );
            break;

        default:
            /* 不明 */

            break;
    }

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       LSR要因割込み処理
 * @details     LSR(Line Status Register)を読み込む。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16550_COM3 COM3
 *                  - NS16550_COM4 COM4
 */
/******************************************************************************/
static void ProcInterruptLineStatus( NS16550ComNo_t comNo )
{
    uint8_t lsr;    /* LSR値 */

    /* 初期化 */
    lsr = 0;

    /* LSR読込み */
    lsr = CtrlInLSR( comNo );

    DEBUG_LOG_TRC( "%s(): comNo=%d, lsr=0x%02X", __func__, comNo, lsr );
    return;
}

/******************************************************************************/
/**
 * @brief       MSR要因割込み処理
 * @details     MSR(Modem Status Register)を読み込み。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16550_COM3 COM3
 *                  - NS16550_COM4 COM4
 */
/******************************************************************************/
static void ProcInterruptModemStatus( NS16550ComNo_t comNo )
{
    uint8_t msr;    /* MSR値 */

    /* 初期化 */
    msr = 0;

    /* MSR読込み */
    msr = CtrlInMSR( comNo );

    DEBUG_LOG_TRC( "%s(): comNo=%d, msr=0x%02X", __func__, comNo, msr );
    return;
}

/******************************************************************************/
/**
 * @brief       THR空割込み処理
 * @details     THR空割込みを禁止して転送制御に転送開始メッセージを送信する。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16550_COM3 COM3
 *                  - NS16550_COM4 COM4
 */
/******************************************************************************/
static void ProcInterruptTx( NS16550ComNo_t comNo )
{
    DEBUG_LOG_FNC( "%s(): start. comNo=%d", __func__, comNo );

    /* THR空割込み禁止 */
    CtrlDisableInterrupt( comNo, NS16550_IER_THR );

    /* 転送開始メッセージ送信 */
    TransmitSendMsgTxBufferEmpty( comNo );

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       データ受信割込み処理
 * @details     受信データの有無をラインステータスレジスタを読み込んでチェック
 *              し、受信データが有る場合は受信データを読み込み、バッファに書き
 *              込む。これを受信データが無くなるまで繰り返し行った後、メイン制
 *              御に受信通知メッセージを送信する。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16550_COM3 COM3
 *                  - NS16550_COM4 COM4
 */
/******************************************************************************/
static void ProcInterruptRx( NS16550ComNo_t comNo )
{
    uint8_t lsr;    /* LSR値               */
    uint8_t rbr;    /* RBR値（受信データ） */

    DEBUG_LOG_FNC( "%s(): start. comNo=%d", __func__, comNo );

    /* 初期化 */
    lsr = 0;
    rbr = 0;

    /* ラインステータスレジスタ読込み */
    lsr = CtrlInLSR( comNo );

    /* 受信データ有無チェック */
    if ( ( lsr & NS16550_LSR_DR ) == NS16550_LSR_DR_NO ) {
        /* 受信データ無し */

        DEBUG_LOG_TRC( "no data: comNo=%d, lsr=0x%02X", comNo, lsr );
        DEBUG_LOG_FNC( "%s(): end.", __func__ );
        return;
    }

    /* 受信データ1byte毎に繰り返す */
    while ( true ) {
        /* 受信データ読込み */
        rbr = CtrlInRBR( comNo );

        /* バッファ書込み */
        BufferWrite( comNo, BUFFER_ID_RECEIVE, rbr );

        /* ラインステータスレジスタ読込み */
        lsr = CtrlInLSR( comNo );

        /* 受信データ有無チェック */
        if ( ( lsr & NS16550_LSR_DR ) == NS16550_LSR_DR_NO ) {
            /* 受信データ無し */

            DEBUG_LOG_FNC( "%s(): end.", __func__ );

            break;
        }
    }

    /* 受信通知メッセージ送信 */
    MainSendMsgRxNtc( comNo );

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       受信制御スレッド
 * @details     割込み初期設定、および、割込み受信待ちを行う。
 *
 * @param[in]   *pArg 未使用
 */
/******************************************************************************/
static void Receiver( void *pArg )
{
    uint8_t  irqNo;     /* 割込み番号       */
    uint32_t irqNoList; /* 割込み番号リスト */
    MkRet_t  retMk;     /* カーネル戻り値   */
    MkErr_t  err;       /* カーネルエラー   */

    DEBUG_LOG_TRC( "%s(): start!", __func__ );

    /* 初期化 */
    irqNo     = 0;
    irqNoList = 0;
    retMk     = MK_RET_FAILURE;
    err       = MK_ERR_NONE;

    /* 割込み初期設定 */
    InitInterrupt( LIBMK_INT_IRQ3 );    /* IRQ3(COM2,COM4) */
    InitInterrupt( LIBMK_INT_IRQ4 );    /* IRQ4(COM1,COM3) */

    /* 割込み許可設定 */
    EnableInterrupt( NS16550_COM1 );
    EnableInterrupt( NS16550_COM2 );

    /* 無限ループ */
    while ( true ) {
        /* 割込み待ち */
        retMk = LibMkIntWait( &irqNoList, &err );

        /* 割込み待ち結果判定 */
        if ( retMk != MK_RET_SUCCESS ) {
            /* 失敗 */

            DEBUG_LOG_ERR( "LibMkIntWait(): ret=%d, err=0x%X", retMk, err );

            /* 継続 */
            continue;
        }

        /* 割り込み毎に繰り返す */
        LIBMK_INT_FOREACH( irqNoList, irqNo ) {
            /* 割込み番号判定 */
            if ( irqNo == LIBMK_INT_IRQ3 ) {
                /* IRQ3 */

                /* COM2割込み処理 */
                ProcInterrupt( NS16550_COM2 );

            } else if ( irqNo == LIBMK_INT_IRQ4 ) {
                /* IRQ4 */

                /* COM1割込み処理 */
                ProcInterrupt( NS16550_COM1 );

            } else {
                /* 不正 */

                DEBUG_LOG_ERR( "invalid irqNo: %d", irqNo );
            }

            /* 割込み完了通知 */
            retMk = LibMkIntComplete( irqNo, &err );

            /* 通知結果判定 */
            if ( retMk != MK_RET_SUCCESS ) {
                /* 失敗 */

                DEBUG_LOG_ERR(
                    "LibMkIntComplete(): ret=%d, err=0x%X",
                    retMk,
                    err
                );
            }
        }
    }
}


/******************************************************************************/

