/******************************************************************************/
/*                                                                            */
/* src/Txctrl.c                                                               */
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

/* ライブラリヘッダ */
#include <MLib/MLibSpin.h>

/* モジュール内ヘッダ */
#include "ns16550.h"
#include "Bufmng.h"
#include "Debug.h"
#include "Filemng.h"
#include "Ioctrl.h"
#include "Txctrl.h"


/******************************************************************************/
/* 定義                                                                       */
/******************************************************************************/
/* 転送状態 */
#define TXSTATE_STARTED ( 0 ) /**< 転送中状態     */
#define TXSTATE_STOPED  ( 1 ) /**< 転送停止中状態 */


/******************************************************************************/
/* グローバル変数定義                                                         */
/******************************************************************************/
/** スピンロック */
static MLibSpin_t gLock[ NS16550_COM_NUM ];

/** 転送状態 */
static uint32_t gTxState;


/******************************************************************************/
/* グローバル関数定義                                                         */
/******************************************************************************/
/******************************************************************************/
/**
 * @brief       転送
 * @details     転送バッファからデータを取り出し、デバイスに書き込むことでデー
 *              タを転送する。転送するデータが無い場合は、THR割込みを禁止して転
 *              送を停止状態にする。また、転送バッファに書込みが可能であること
 *              をデバイスファイル管理モジュールに通知する。
 *
 * @param[in]   comNo デバイス識別番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 */
/******************************************************************************/
void TxctrlDo( NS16550ComNo_t comNo )
{
    size_t  size;                                   /* 転送サイズ */
    uint8_t data[ NS16550_TRANSMIT_BUFFER_SIZE ];   /* 転送データ */

    /* 初期化 */
    size = 0;

    /* スピンロック */
    MLibSpinLock( &( gLock[ comNo ] ), NULL );

    /* バッファ取出 */
    size = BufmngRead( comNo,
                       BUFMNG_ID_TX,
                       data,
                       NS16550_TRANSMIT_BUFFER_SIZE );

    /* データ有無判定 */
    if ( size != 0 ) {
        /* データ有り */

        /* データ書き込み */
        IoctrlOutTHR( comNo, data, size );

    } else {
        /* データ無し */

        /* THR割込み禁止 */
        IoctrlSetIER( comNo, NS16550_IER_THR, NS16550_IER_THR_DISABLE );

        /* 転送状態設定 */
        gTxState = TXSTATE_STOPED;
    }

    /* スピンアンロック */
    MLibSpinUnlock( &( gLock[ comNo ] ), NULL );

    /* 書込レディ状態更新 */
    FilemngUpdateReadyWrite( comNo );

    return;
}


/******************************************************************************/
/**
 * @brief       転送制御初期化
 * @details     スピンロックを初期化する。
 */
/******************************************************************************/
void TxctrlInit( void )
{
    MLibErr_t errMLib;  /* MLIBエラー要因 */
    MLibRet_t retMLib;  /* MLIB戻り値     */

    /* 初期化 */
    errMLib = MLIB_ERR_NONE;
    retMLib = MLIB_RET_FAILURE;

    /* COM1用スピンロック初期化 */
    retMLib = MLibSpinInit( &gLock[ NS16550_COM1 ], &errMLib );

    /* 初期化結果判定 */
    if ( retMLib != MLIB_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "MLibSpinInit(): ret=%u, err=%X", retMLib, errMLib );
    }

    /* COM2用スピンロック初期化 */
    retMLib = MLibSpinInit( &gLock[ NS16550_COM2 ], &errMLib );

    /* 初期化結果判定 */
    if ( retMLib != MLIB_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "MLibSpinInit(): ret=%u, err=%X", retMLib, errMLib );
    }

    /* 転送状態初期化 */
    gTxState = TXSTATE_STARTED;

    return;
}


/******************************************************************************/
/**
 * @brief       転送要求
 * @details     転送状態が転送停止中の場合はTHR割込みを許可して転送中状態に変更
 *              する。
 *
 * @param[in]   comNo デバイス識別M番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 */
/******************************************************************************/
void TxctrlRequest( NS16550ComNo_t comNo )
{
    /* スピンロック */
    MLibSpinLock( &( gLock[ comNo ] ), NULL );

    /* 転送状態判定 */
    if ( gTxState == TXSTATE_STOPED ) {
        /* 転送停止中 */

        /* THR割込み許可 */
        IoctrlSetIER( comNo, NS16550_IER_THR, NS16550_IER_THR_ENABLE );

        /* 転送状態設定 */
        gTxState = TXSTATE_STARTED;
    }

    /* スピンアンロック */
    MLibSpinUnlock( &( gLock[ comNo ] ), NULL );

    return;
}


/******************************************************************************/
