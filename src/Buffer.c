/******************************************************************************/
/*                                                                            */
/* src/Buffer.c                                                               */
/*                                                                 2019/09/01 */
/* Copyright (C) 2019 Mochi.                                                  */
/*                                                                            */
/******************************************************************************/
/******************************************************************************/
/* インクルード                                                               */
/******************************************************************************/
/* 標準ヘッダ */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* モジュール内ヘッダ */
#include "ns16550.h"
#include "Buffer.h"
#include "Debug.h"


/******************************************************************************/
/* 定義                                                                       */
/******************************************************************************/
/** バッファサイズ */
#define BUFFER_SIZE ( 4096 )

/** バッファ */
typedef struct {
    char     *pBuffer;  /* バッファ   */
    uint32_t readIdx;   /* 読込み位置 */
    uint32_t writeIdx;  /* 書込み位置 */
} Buffer_t;


/******************************************************************************/
/* ローカル関数宣言                                                           */
/******************************************************************************/
/* 読込み位置更新 */
static void UpdateReadIdx( NS16550ComNo_t comNo,
                           BufferId_t     id     );
/* 書込み位置更新 */
static void UpdateWriteIdx( NS16550ComNo_t comNo,
                            BufferId_t     id     );


/******************************************************************************/
/* グローバル変数定義                                                         */
/******************************************************************************/
/* バッファ */
static Buffer_t gBuffer[ NS16550_COM_NUM ][ BUFFER_ID_NUM ];


/******************************************************************************/
/* グローバル関数定義                                                         */
/******************************************************************************/
/******************************************************************************/
/**
 * @brief       バッファ管理初期化
 * @details     バッファを初期化する。
 */
/******************************************************************************/
void BufferInit( void )
{
    BufferId_t     id;      /* バッファID */
    NS16550ComNo_t comNo;   /* COM番号    */

    DEBUG_LOG_FNC( "%s(): start.", __func__ );

    /* バッファ管理情報初期化 */
    memset( gBuffer, 0, sizeof ( gBuffer ) );

    /* COM番号毎に繰り返す */
    for ( comNo = NS16550_COM_MIN; comNo <= NS16550_COM_MAX; comNo++ ) {
        /* バッファ種別毎に繰り返す */
        for ( id = BUFFER_ID_MIN; id <= BUFFER_ID_NUM; id++ ) {
            /* バッファ割当て */
            gBuffer[ comNo ][ id ].pBuffer = malloc( BUFFER_SIZE );

            /* 割当結果判定 */
            if ( gBuffer[ comNo ][ id ].pBuffer == NULL ) {
                /* 失敗 */

                DEBUG_LOG_ERR( "malloc(): comNo=%d, id=%d", comNo, id );
                DEBUG_ABORT();
            }

            /* バッファ初期化 */
            memset( gBuffer[ comNo ][ id ].pBuffer, 0, BUFFER_SIZE );
        }
    }

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       バッファ読込み
 * @details     引数idのバッファの読込み位置の値を引数*pValueに設定し、読込み位
 *              置を更新する。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16550_COM3 COM3
 *                  - NS16550_COM4 COM4
 * @param[in]   id    バッファID
 *                  - BUFFER_ID_RECEIVE  受信バッファ
 *                  - BUFFER_ID_TRANSMIT 転送バッファ
 * @param[out]  value 読込み値
 */
/******************************************************************************/
bool BufferRead( NS16550ComNo_t comNo,
                 BufferId_t     id,
                 uint8_t        *pValue )
{
    uint8_t  *pBuffer;      /* バッファ   */
    uint32_t *pReadIdx;     /* 読込み位置 */
    uint32_t *pWriteIdx;    /* 書込み位置 */

    DEBUG_LOG_FNC(
        "%s(): start. comNo=%d, id=%d, pValue=%p",
        __func__,
        comNo,
        id,
        pValue
    );

    /* 初期化 */
    pBuffer   = gBuffer[ comNo ][ id ].pBuffer;
    pReadIdx  = &( gBuffer[ comNo ][ id ].readIdx  );
    pWriteIdx = &( gBuffer[ comNo ][ id ].writeIdx );

    /* 読込み位置判定 */
    if ( *pReadIdx == *pWriteIdx ) {
        /* バッファ無し */

        DEBUG_LOG_FNC( "%s(): end.", __func__ );
        return false;
    }

    /* バッファ読込み */
    *pValue = pBuffer[ *pReadIdx ];

    /* 読込み位置更新 */
    UpdateReadIdx( comNo, id );

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return true;
}


/******************************************************************************/
/**
 * @brief       バッファ書込み
 * @details     引数idのバッファの書込み位置に引数valueを書き込み、書込み位置を
 *              更新する。書込み位置が読込み位置に追いついてしまった場合は、最
 *              古のバッファを破棄して読込み位置を更新する。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16550_COM3 COM3
 *                  - NS16550_COM4 COM4
 * @param[in]   id    バッファID
 *                  - BUFFER_ID_RECEIVE  受信バッファ
 *                  - BUFFER_ID_TRANSMIT 転送バッファ
 * @param[in]   value 書込み値
 */
/******************************************************************************/
void BufferWrite( NS16550ComNo_t comNo,
                  BufferId_t     id,
                  uint8_t        value  )
{
    uint8_t  *pBuffer;      /* バッファ   */
    uint32_t *pReadIdx;     /* 読込み位置 */
    uint32_t *pWriteIdx;    /* 書込み位置 */

    DEBUG_LOG_FNC(
        "%s(): start. comNo=%d, id=%d, value=0x%02X",
        __func__,
        comNo,
        id,
        value
    );

    /* 初期化 */
    pBuffer   = gBuffer[ comNo ][ id ].pBuffer;
    pReadIdx  = &( gBuffer[ comNo ][ id ].readIdx  );
    pWriteIdx = &( gBuffer[ comNo ][ id ].writeIdx );

    /* バッファ書込み */
    pBuffer[ *pWriteIdx ] = value;

    /* 書込み位置更新 */
    UpdateWriteIdx( comNo, id );

    /* 読込み位置判定 */
    if ( *pWriteIdx == *pReadIdx ) {
        /* バッファオーバーフロー */

        /* 最古バッファ破棄 */
        UpdateReadIdx( comNo, id );
    }

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/* ローカル関数定義                                                           */
/******************************************************************************/
/******************************************************************************/
/**
 * @brief       読込み位置更新
 * @details     引数idのバッファの読込み位置を次の位置に更新する。読込み位置が
 *              バッファの最後であった場合は、バッファの先頭に設定する。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16550_COM3 COM3
 *                  - NS16550_COM4 COM4
 * @param[in]   id    バッファID
 *                  - BUFFER_ID_RECEIVE  受信バッファ
 *                  - BUFFER_ID_TRANSMIT 転送バッファ
 */
/******************************************************************************/
static void UpdateReadIdx( NS16550ComNo_t comNo,
                           BufferId_t     id     )
{
    DEBUG_LOG_FNC( "%s(): start. comNo=%d, id=%d", __func__, comNo, id );

    /* 読込み位置更新 */
    ( gBuffer[ comNo ][ id ].readIdx )++;

    /* 読込み位置判定 */
    if ( gBuffer[ comNo ][ id ].readIdx >= BUFFER_SIZE ) {
        /* オーバー */

        /* 読込み位置を先頭に設定 */
        gBuffer[ comNo ][ id ].readIdx = 0;
    }

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       書込み位置更新
 * @details     引数idのバッファの書込み位置を次の位置に更新する。書込み位置が
 *              バッファの最後であった場合は、バッファの先頭に設定する。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16550_COM3 COM3
 *                  - NS16550_COM4 COM4
 * @param[in]   id    バッファID
 *                  - BUFFER_ID_RECEIVE  受信バッファ
 *                  - BUFFER_ID_TRANSMIT 転送バッファ
 */
/******************************************************************************/
static void UpdateWriteIdx( NS16550ComNo_t comNo,
                            BufferId_t     id     )
{
    DEBUG_LOG_FNC( "%s(): start. comNo=%d, id=%d", __func__, comNo, id );

    /* 書込み位置更新 */
    ( gBuffer[ comNo ][ id ].writeIdx )++;

    /* 書込み位置判定 */
    if ( gBuffer[ comNo ][ id ].writeIdx >= BUFFER_SIZE ) {
        /* オーバー */

        /* 書込み位置を先頭に設定 */
        gBuffer[ comNo ][ id ].writeIdx = 0;
    }

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
