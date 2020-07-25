/******************************************************************************/
/*                                                                            */
/* src/Intmng.c                                                               */
/*                                                                 2020/07/25 */
/* Copyright (C) 2020 Mochi.                                                  */
/*                                                                            */
/******************************************************************************/
/******************************************************************************/
/* インクルード                                                               */
/******************************************************************************/
/* 標準ヘッダ */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/* ライブラリヘッダ */
#include <libmk.h>

/* モジュール内ヘッダ */
#include "config.h"
#include "ns16550.h"
#include "Debug.h"
#include "Intmng.h"
#include "Ioctrl.h"
#include "Rxctrl.h"
#include "Txctrl.h"


/******************************************************************************/
/* ローカル関数宣言                                                           */
/******************************************************************************/
/* 割込み設定初期化 */
static void InitInterrupt( NS16550ComNo_t comNo,
                           uint8_t        irqNo  );
/* 割込み処理 */
static void ProcInterrupt( NS16550ComNo_t comNo,
                           uint8_t        irqNo  );
/* 受信割込み処理 */
static void ProcInterruptRx( NS16550ComNo_t comNo,
                             uint8_t        irqNo  );
/* 転送割込み処理 */
static void ProcInterruptTx( NS16550ComNo_t comNo,
                             uint8_t        irqNo  );
/* 割込み管理スレッド */
static void StartThread( void *pArg );
/* 割込み待ち合わせ */
static void WaitInterrupt( void );


/******************************************************************************/
/* 静的グローバル変数定義                                                     */
/******************************************************************************/
/* スレッドスタック */
static void *gpStack;


/******************************************************************************/
/* グローバル関数定義                                                         */
/******************************************************************************/
/******************************************************************************/
/**
 * @brief       割込み管理初期化
 * @details     割込み管理スレッドを生成する。
 */
/******************************************************************************/
void IntmngInit( void )
{
    MkErr_t errMk;  /* カーネルエラー要因 */
    MkRet_t retMk;  /* カーネル戻り値     */

    /* 初期化 */
    errMk = MK_ERR_NONE;
    retMk = MK_RET_FAILURE;

    /* スタック領域割当 */
    gpStack = malloc( CONFIG_STACK_SIZE );

    /* 割当結果判定 */
    if ( gpStack == NULL ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "No resource." );
        DEBUG_ABORT();
    }

    /* スレッド生成 */
    retMk = LibMkThreadCreate( &StartThread,
                               NULL,
                               gpStack,
                               CONFIG_STACK_SIZE,
                               NULL,
                               &errMk             );

    /* 生成結果判定 */
    if ( retMk != MK_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "LibMkThreadCreate(): ret=%X, err=%X", retMk, errMk );
        DEBUG_ABORT();
    }

    return;
}


/******************************************************************************/
/* ローカル関数定義                                                           */
/******************************************************************************/
/******************************************************************************/
/**
 * @brief       割込み設定初期化
 * @details     デバイスからの割込み監視の開始とカーネルの割込み有効化、および
 *              デバイスの割込み有効化を行う。
 *
 * @param[in]   comNo デバイス識別番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 * @param[in]   irqNo 割込み番号
 *                  - LIBMK_INT_IRQ3 IRQ3
 *                  - LIBMK_INT_IRQ4 IRQ4
 */
/******************************************************************************/
static void InitInterrupt( NS16550ComNo_t comNo,
                           uint8_t        irqNo  )
{
    MkErr_t errMk;  /* カーネルエラー要因 */
    MkRet_t retMk;  /* カーネル戻り値     */

    /* 初期化 */
    errMk = MK_ERR_NONE;
    retMk = MK_RET_FAILURE;

    /* 割込み監視開始 */
    retMk = LibMkIntStartMonitoring( irqNo, &errMk );

    /* 開始結果判定 */
    if ( retMk != MK_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR(
            "LibMkIntStartMonitoring(): ret=%X, err=%X",
            retMk,
            errMk
        );
        DEBUG_ABORT();
    }

    /* 割込み有効化(カーネル) */
    retMk = LibMkIntEnable( irqNo, &errMk );

    /* 有効化結果判定 */
    if ( retMk != MK_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "LibMkIntEnable(): ret=%X, err=%X", retMk, errMk );
        DEBUG_ABORT();
    }

    /* 割込み有効化(デバイス) */
    IoctrlSetIER( comNo,
                  NS16550_IER_ALL,
                  ( NS16550_IER_MSR_ENABLE |
                    NS16550_IER_LSR_ENABLE |
                    NS16550_IER_THR_ENABLE |
                    NS16550_IER_RBR_ENABLE   ) );
    IoctrlSetMCR( comNo,
                  NS16550_MCR_OUT2,
                  NS16550_MCR_OUT2_H );

    return;
}


/******************************************************************************/
/**
 * @brief       割込み処理
 * @details     割込み要因を特定し、要因毎に対応した処理を呼び出す。
 *
 * @param[in]   comNo デバイス識別番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 * @param[in]   irqNo 割込み番号
 *                  - LIBMK_INT_IRQ3 IRQ3
 *                  - LIBMK_INT_IRQ4 IRQ4
 */
/******************************************************************************/
static void ProcInterrupt( NS16550ComNo_t comNo,
                           uint8_t        irqNo  )
{
    uint8_t iir;    /* 割込み要因レジスタ値 */
    MkErr_t errMk;  /* カーネルエラー要因   */
    MkRet_t retMk;  /* カーネル戻り値       */

    /* 初期化 */
    errMk = MK_ERR_NONE;
    retMk = MK_RET_FAILURE;

    /* 割込み要因読込み */
    iir = IoctrlInIIR( comNo );

    /* 割込み要因判定 */
    switch ( iir & NS16550_IIR_ID ) {
        case NS16550_IIR_ID_THR:
            /* 転送バッファ空 */

            ProcInterruptTx( comNo, irqNo );
            break;

        case NS16550_IIR_ID_RBR:
            /* データ受信 */
        case NS16550_IIR_ID_RBR_TO:
            /* データ受信タイムアウト割込み */
        case NS16550_IIR_ID_LSR:
            /* エラー */

            ProcInterruptRx( comNo, irqNo );
            break;

        case NS16550_IIR_ID_MSR:
            /* MSR要因割込み *//* TODO */
        default:
            /* 不明 */

            DEBUG_LOG_ERR( "Invalid IIR: %X", iir & NS16550_IIR_ID );

            /* 割込み完了通知 */
            retMk = LibMkIntComplete( irqNo, &errMk );

            /* 通知結果判定 */
            if ( retMk != MK_RET_SUCCESS ) {
                /* 失敗 */

                DEBUG_LOG_ERR(
                    "LibMkIntComplete(): ret=%X, err=%X",
                    retMk,
                    errMk
                );
            }

            break;
    }

    return;
}


/******************************************************************************/
/**
 * @brief       受信割込み処理
 * @details     受信処理を行い、割込み完了をカーネルに通知する。
 *
 * @param[in]   comNo デバイス識別番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 * @param[in]   irqNo 割込み番号
 *                  - LIBMK_INT_IRQ3 IRQ3
 *                  - LIBMK_INT_IRQ4 IRQ4
 */
/******************************************************************************/
static void ProcInterruptRx( NS16550ComNo_t comNo,
                             uint8_t        irqNo  )
{
    MkErr_t errMk;  /* カーネルエラー要因 */
    MkRet_t retMk;  /* カーネル戻り値     */

    /* 初期化 */
    errMk = MK_ERR_NONE;
    retMk = MK_RET_FAILURE;

    /* 受信 */
    RxctrlDo( comNo );

    /* 割込み完了通知 */
    retMk = LibMkIntComplete( irqNo, &errMk );

    /* 通知結果判定 */
    if ( retMk != MK_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "LibMkIntComplete(): ret=%X, err=%X", retMk, errMk );
    }

    return;
}


/******************************************************************************/
/**
 * @brief       転送割込み処理
 * @details     割込み完了をカーネルに通知し、転送処理を行う。
 *
 * @param[in]   comNo デバイス識別番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 * @param[in]   irqNo 割込み番号
 *                  - LIBMK_INT_IRQ3 IRQ3
 *                  - LIBMK_INT_IRQ4 IRQ4
 */
/******************************************************************************/
static void ProcInterruptTx( NS16550ComNo_t comNo,
                             uint8_t        irqNo  )
{
    MkErr_t errMk;  /* カーネルエラー要因 */
    MkRet_t retMk;  /* カーネル戻り値     */

    /* 初期化 */
    errMk = MK_ERR_NONE;
    retMk = MK_RET_FAILURE;

    /* 割込み完了通知 */
    retMk = LibMkIntComplete( irqNo, &errMk );

    /* 通知結果判定 */
    if ( retMk != MK_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "LibMkIntComplete(): ret=%X, err=%X", retMk, errMk );
    }

    /* 転送 */
    TxctrlDo( comNo );

    return;
}


/******************************************************************************/
/**
 * @brief       割込み管理スレッド
 * @details     デバイス毎に割込み設定を行い、割込みを待ち合わせる。
 *
 * @param[in]   *pArg 未使用
 */
/******************************************************************************/
static void StartThread( void *pArg )
{
    /* 割込み設定 */
    InitInterrupt( NS16550_COM1, LIBMK_INT_IRQ4 );
    InitInterrupt( NS16550_COM2, LIBMK_INT_IRQ3 );

    /* 割込み待ち合わせ */
    WaitInterrupt();

    /* TODO */
    DEBUG_ABORT();
}


/******************************************************************************/
/**
 * @brief       割込み待ち合わせ
 * @details     割込みを待ち合わせて割込み発生時に割り込み処理を行う。
 */
/******************************************************************************/
static void WaitInterrupt( void )
{
    MkErr_t  errMk;     /* カーネルエラー要因  */
    MkRet_t  retMk;     /* カーネル戻り値      */
    uint8_t  irqNo;     /* 割込み番号          */
    uint32_t irqNoList; /* 割込み番号リスト    */

    /* 初期化 */
    errMk     = MK_ERR_NONE;
    retMk     = MK_RET_FAILURE;
    irqNo     = 0;
    irqNoList = 0;

    /* 無限ループ */
    while ( true ) {
        /* 割込み待ち合わせ */
        retMk = LibMkIntWait( &irqNoList, &errMk );

        /* 待ち合わせ結果判定 */
        if ( retMk != MK_RET_SUCCESS ) {
            /* 失敗 */

            DEBUG_LOG_ERR( "LibMkIntWait(): ret=%X, err=%X", retMk, errMk );

            /* 継続 */
            continue;
        }

        /* 割込み毎に繰り返し */
        LIBMK_INT_FOREACH( irqNoList, irqNo ) {
            /* 割込み番号判定 */
            if ( irqNo == LIBMK_INT_IRQ4 ) {
                /* IRQ4 */

                /* COM1割込み処理 */
                ProcInterrupt( NS16550_COM1, irqNo );

            } else if ( irqNo == LIBMK_INT_IRQ3 ) {
                /* IRQ3 */

                /* COM2割込み処理 */
                ProcInterrupt( NS16550_COM2, irqNo );

            } else {
                /* 不正 */

                DEBUG_LOG_ERR( "Invalid IRQ: %d", irqNo );
            }
        }
    }
}


/******************************************************************************/
