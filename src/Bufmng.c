/******************************************************************************/
/*                                                                            */
/* src/Bufmng.c                                                               */
/*                                                                 2020/07/18 */
/* Copyright (C) 2019-2020 Mochi.                                             */
/*                                                                            */
/******************************************************************************/
/******************************************************************************/
/* インクルード                                                               */
/******************************************************************************/
/* 標準ヘッダ */
#include <stdint.h>
#include <string.h>

/* ライブラリヘッダ */
#include <libmvfs.h>
#include <MLib/MLibRingBuffer.h>

/* モジュール内ヘッダ */
#include "config.h"
#include "ns16550.h"
#include "Bufmng.h"
#include "Debug.h"


/******************************************************************************/
/* グローバル変数定義                                                         */
/******************************************************************************/
/* バッファ */
static MLibRingBuffer_t gBuffer[ NS16550_COM_NUM ][ BUFMNG_ID_NUM ];


/******************************************************************************/
/* グローバル関数定義                                                         */
/******************************************************************************/
/******************************************************************************/
/**
 * @brief       バッファ管理初期化
 * @details     バッファを初期化する。
 */
/******************************************************************************/
void BufmngInit( void )
{
    MLibErr_t      errMLib; /* MLibエラー要因   */
    MLibRet_t      retMLib; /* MLib戻り値       */
    BufmngId_t     id;      /* バッファID       */
    NS16550ComNo_t comNo;   /* デバイス識別番号 */

    /* 初期化 */
    errMLib = MLIB_ERR_NONE;
    retMLib = MLIB_RET_FAILURE;
    id      = BUFMNG_ID_MIN;
    comNo   = NS16550_COM_MIN;

    /* デバイス識別番号毎に繰り返し */
    for ( comNo = NS16550_COM_MIN; comNo <= NS16550_COM_MAX; comNo++ ) {
        /* バッファ種別毎に繰り返し */
        for ( id = BUFMNG_ID_MIN; id <= BUFMNG_ID_NUM; id++ ) {
            /* バッファ初期化 */
            retMLib = MLibRingBufferInit( &( gBuffer[ comNo ][ id ] ),
                                          1,
                                          CONFIG_BUFFER_SIZE,
                                          &errMLib                     );

            /* 初期化結果判定 */
            if ( retMLib != MLIB_RET_SUCCESS ) {
                /* 失敗 */

                DEBUG_LOG_ERR(
                    "MLibRingBufferInit(): comNo=%u, id=%u, ret=%X, err=%X",
                    comNo,
                    id,
                    retMLib,
                    errMLib
                );
            }
        }
    }

    return;
}


/******************************************************************************/
/**
 * @brief       バッファクリア
 * @details     バッファをクリアする。
 *
 * @param[in]   comNo デバイス識別番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 * @param[in]   id    バッファID
 *                  - BUFMNG_ID_RX 受信バッファ
 *                  - BUFMNG_ID_TX 転送バッファ
 */
/******************************************************************************/
void BufmngClear( NS16550ComNo_t comNo,
                  BufmngId_t     id     )
{
    MLibErr_t errMLib;  /* MLIBエラー要因 */
    MLibRet_t retMLib;  /* MLIB戻り値     */

    /* 初期化 */
    errMLib = MLIB_ERR_NONE;
    retMLib = MLIB_RET_SUCCESS;

    /* バッファクリア */
    retMLib = MLibRingBufferClear( &( gBuffer[ comNo ][ id ] ), &errMLib );

    /* クリア結果判定 */
    if ( retMLib != MLIB_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR(
            "MLibRingBufferClear(): ret=%X, err=%X",
            retMLib,
            errMLib
        );
    }

    return;
}


/******************************************************************************/
/**
 * @brief       バッファ読込み
 * @details     バッファからデータを読み込む。
 *
 * @param[in]   comNo  デバイス識別番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 * @param[in]   id     バッファID
 *                  - BUFFER_ID_RECEIVE  受信バッファ
 *                  - BUFFER_ID_TRANSMIT 転送バッファ
 * @param[out]  *pData 格納先データ
 * @param[in]   size   読込みサイズ
 *
 * @return      読込んだデータサイズを返す。
 */
/******************************************************************************/
size_t BufmngRead( NS16550ComNo_t comNo,
                   BufmngId_t     id,
                   uint8_t        *pData,
                   size_t         size    )
{
    uint32_t  idx;      /* インデックス   */
    MLibErr_t errMLib;  /* MLIBエラー要因 */
    MLibRet_t retMLib;  /* MLIB戻り値     */

    /* 初期化 */
    idx     = 0;
    errMLib = MLIB_ERR_NONE;
    retMLib = MLIB_RET_FAILURE;

    /* 読込みサイズ分繰り返し */
    for ( idx = 0; idx < size; idx++ ) {
        /* バッファ取出し */
        retMLib = MLibRingBufferPop( &( gBuffer[ comNo ][ id ] ),
                                     &( pData[ idx ] ),
                                     &errMLib                     );

        /* 取出し結果判定 */
        if ( retMLib != MLIB_RET_SUCCESS ) {
            /* 失敗 */

            break;
        }
    }

    return idx;
}


/******************************************************************************/
/**
 * @brief       バッファ書込み
 * @details     バッファにデータを書き込む。
 *
 * @param[in]   comNo  デバイス識別番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 * @param[in]   id     バッファID
 *                  - BUFFER_ID_RECEIVE  受信バッファ
 *                  - BUFFER_ID_TRANSMIT 転送バッファ
 * @param[in]   *pData 書込みデータ
 * @param[in]   size   書込みサイズ
 *
 * @return      書き込んだデータサイズを返す。
 */
/******************************************************************************/
size_t BufmngWrite( NS16550ComNo_t comNo,
                    BufmngId_t     id,
                    uint8_t        *pData,
                    size_t         size    )
{
    uint32_t  idx;      /* 書込みインデックス */
    MLibErr_t errMLib;  /* MLIBエラー要因     */
    MLibRet_t retMLib;  /* MLIB戻り値         */

    /* 初期化 */
    idx     = 0;
    errMLib = MLIB_ERR_NONE;
    retMLib = MLIB_RET_FAILURE;

    /* バッファ種別判定 */
    if ( id == BUFMNG_ID_TX ) {
        /* 転送バッファ */

        /* 書込みサイズ分繰り返し */
        for ( idx = 0; idx < size; idx++ ) {
            /* バッファ追加 */
            retMLib = MLibRingBufferPush( &( gBuffer[ comNo ][ id ] ),
                                          &( pData[ idx ] ),
                                          &errMLib                     );

            /* 追加結果判定 */
            if ( retMLib != MLIB_RET_SUCCESS ) {
                /* 失敗 */

                break;
            }
        }

    } else {
        /* 受信バッファ */

        /* 書込みサイズ分繰り返し */
        for ( idx = 0; idx < size; idx++ ) {
            /* バッファ追加 */
            MLibRingBufferPushOW( &( gBuffer[ comNo ][ id ] ),
                                  &( pData[ idx ] ),
                                  &errMLib                     );
        }
    }

    return idx;
}


/******************************************************************************/
