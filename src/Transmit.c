/******************************************************************************/
/*                                                                            */
/* src/Transmit.c                                                             */
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
#include <string.h>

/* カーネルヘッダ */
#include <kernel/types.h>

/* ライブラリヘッダ */
#include <libmk.h>

/* モジュール内ヘッダ */
#include "msg.h"
#include "ns16550.h"
#include "Buffer.h"
#include "Ctrl.h"
#include "Debug.h"
#include "Main.h"
#include "Transmit.h"
#include "Receive.h"


/******************************************************************************/
/* 定義                                                                       */
/******************************************************************************/
/** 転送制御スレッドスタックサイズ */
#define STACK_SIZE ( 4096 )

/** COM制御情報 */
typedef struct {
    int remain;     /* 送信バッファ残り */
} comCtrlInfo_t;


/******************************************************************************/
/* ローカル関数宣言                                                           */
/******************************************************************************/
/* 転送バッファ空通知メッセージ処理 */
static void ProcMsgTxBufferEmpty( MkTaskId_t         src,
                                  MsgTxBufferEmpty_t *pMsg,
                                  size_t             size   );
/* 転送要求メッセージ処理 */
static void ProcMsgTxReq( MkTaskId_t src,
                          MsgTxReq_t *pMsg,
                          size_t     size   );
/* データ転送 */
static void Transmit( NS16550ComNo_t comNo );
/* 転送制御スレッド */
static void Transmitter( void *pArg );


/******************************************************************************/
/* グローバル変数定義                                                         */
/******************************************************************************/
/** COM制御情報 */
static comCtrlInfo_t gComCtrlInfo[ NS16550_COM_NUM ];
/** 転送スレッドタスクID */
static MkTaskId_t gTaskId;
/** 転送要求メッセージシーケンス番号 */
static uint32_t gSeqNoTxReq;
/** 転送バッファ空メッセージシーケンス番号 */
static uint32_t gSeqNoTxBufferEmpty;


/******************************************************************************/
/* グローバル関数定義                                                         */
/******************************************************************************/
/******************************************************************************/
/**
 * @brief       転送制御初期化
 * @details     転送制御スレッドを生成する。
 */
/******************************************************************************/
void TransmitInit( void )
{
    void    *pStack;    /* スレッドスタック */
    MkRet_t retMk;      /* カーネル戻り値   */
    MkErr_t err;        /* カーネルエラー   */

    DEBUG_LOG_FNC( "%s(): start.", __func__ );

    /* COM制御情報初期化 */
    gComCtrlInfo[ NS16550_COM1 ].remain = 0;
    gComCtrlInfo[ NS16550_COM2 ].remain = 0;
    gComCtrlInfo[ NS16550_COM3 ].remain = 0;
    gComCtrlInfo[ NS16550_COM4 ].remain = 0;

    /* シーケンス番号初期化 */
    gSeqNoTxReq         = 0;
    gSeqNoTxBufferEmpty = 0;

    /* スタック割当て */
    pStack = malloc( STACK_SIZE );

    /* 割当て結果判定 */
    if ( pStack == NULL ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "malloc()" );
        DEBUG_ABORT();
    }

    /* スレッド生成 */
    retMk = LibMkThreadCreate( &Transmitter,
                               NULL,
                               pStack,
                               STACK_SIZE,
                               &gTaskId,
                               &err          );

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
 * @brief       転送バッファ空きメッセージ送信
 * @details     転送バッファ空きメッセージを転送制御に送信する。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16550_COM3 COM3
 *                  - NS16550_COM4 COM4
 */
/******************************************************************************/
void TransmitSendMsgTxBufferEmpty( NS16550ComNo_t comNo )
{
    MkRet_t            retMk;   /* カーネル戻り値 */
    MkErr_t            err;     /* カーネルエラー */
    MsgTxBufferEmpty_t msg;     /* メッセージ     */

    DEBUG_LOG_FNC( "%s(): start. comNo=%d", __func__, comNo );

    /* 初期化 */
    retMk = MK_RET_FAILURE;
    err   = MK_ERR_NONE;
    memset( &msg, 0, sizeof ( MsgTxBufferEmpty_t ) );

    /* メッセージ設定 */
    msg.header.msgId = MSG_ID_TXBUFFEREMPTY;
    msg.header.type  = MSG_TYPE_NTC;
    msg.header.seqNo = ++gSeqNoTxBufferEmpty;
    msg.comNo        = comNo;

    DEBUG_LOG_TRC(
        "%s(): gTaskId=0x%X, seqNo=%u, comNo=%d",
        __func__,
        gTaskId,
        gSeqNoTxBufferEmpty,
        comNo
    );

    /* メッセージ送信 */
    retMk = LibMkMsgSend( gTaskId, &msg, sizeof ( MsgTxBufferEmpty_t ), &err );

    /* 送信結果判定 */
    if ( retMk != MK_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "LibMkMsgSend(): ret=%d, err=0x%X", retMk, err );
    }

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       転送要求メッセージ送信
 * @details     転送要求メッセージを転送制御に送信する。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16550_COM3 COM3
 *                  - NS16550_COM4 COM4
 */
/******************************************************************************/
void TransmitSendMsgTxReq( NS16550ComNo_t comNo )
{
    MkRet_t    retMk;   /* カーネル戻り値 */
    MkErr_t    err;     /* カーネルエラー */
    MsgTxReq_t msg;     /* メッセージ     */

    DEBUG_LOG_FNC( "%s(): start. comNo=%d", __func__, comNo );

    /* 初期化 */
    retMk = MK_RET_FAILURE;
    err   = MK_ERR_NONE;
    memset( &msg, 0, sizeof ( MsgTxReq_t ) );

    /* メッセージ設定 */
    msg.header.msgId = MSG_ID_TXREQ;
    msg.header.type  = MSG_TYPE_NTC;
    msg.header.seqNo = ++gSeqNoTxReq;
    msg.comNo        = comNo;

    DEBUG_LOG_TRC(
        "%s(): gTaskId=0x%X, seqNo=%u, comNo=%d",
        __func__,
        gTaskId,
        gSeqNoTxReq,
        comNo
    );

    /* メッセージ送信 */
    retMk = LibMkMsgSend( gTaskId, &msg, sizeof ( MsgTxReq_t ), &err );

    /* 送信結果判定 */
    if ( retMk != MK_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "LibMkMsgSend(): ret=%d, err=0x%X", retMk, err );
    }

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/* ローカル関数定義                                                           */
/******************************************************************************/
/******************************************************************************/
/**
 * @brief       転送バッファ空通知メッセージ処理
 * @details     転送バッファ空き残量を初期化し、データ転送を開始する。
 *
 * @param[in]   src   送信元タスクID
 * @param[in]   *pMsg メッセージ
 * @param[in]   size  メッセージサイズ
 */
/******************************************************************************/
static void ProcMsgTxBufferEmpty( MkTaskId_t         src,
                                  MsgTxBufferEmpty_t *pMsg,
                                  size_t             size   )
{
    DEBUG_LOG_FNC( "%s(): start.", __func__ );

    /* 送信元タスクIDチェック */
    if ( src != ReceiveGetTaskId() ) {
        /* 受信制御スレッドでない */

        DEBUG_LOG_ERR( "invalid src: 0x%X", src );
        return;
    }

    /* メッセージサイズチェック */
    if ( size < sizeof ( MsgTxBufferEmpty_t ) ) {
        /* 不正 */

        DEBUG_LOG_ERR(
            "invalid size: %u < %u",
            size,
            sizeof ( MsgTxBufferEmpty_t )
        );
        return;
    }

    /* COM番号チェック */
    if ( ( pMsg->comNo < NS16550_COM_MIN               ) &&
         (               NS16550_COM_MAX < pMsg->comNo )    ) {
        /* 範囲外 */

        DEBUG_LOG_ERR( "invalid comNo: %d", pMsg->comNo );
        DEBUG_LOG_FNC( "%s(): end.", __func__ );
        return;
    }

    /* 転送バッファ残量初期化 */
    gComCtrlInfo[ pMsg->comNo ].remain = NS16550_TRANSMIT_BUFFER_SIZE;

    /* データ転送 */
    Transmit( pMsg->comNo );

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       転送要求メッセージ処理
 * @details     データ転送を開始する。
 *
 * @param[in]   src   送信元タスクID
 * @param[in]   *pMsg メッセージ
 * @param[in]   size  メッセージサイズ
 */
/******************************************************************************/
static void ProcMsgTxReq( MkTaskId_t src,
                          MsgTxReq_t *pMsg,
                          size_t     size   )
{
    DEBUG_LOG_FNC( "%s(): start.", __func__ );

    /* 送信元タスクIDチェック */
    if ( src != MainGetTaskId() ) {
        /* メイン制御タスクIDでない */

        DEBUG_LOG_ERR( "invalid src: 0x%X", src );
        return;
    }

    /* メッセージサイズチェック */
    if ( size < sizeof ( MsgTxReq_t ) ) {
        /* 不正 */

        DEBUG_LOG_ERR( "invalid size: %u < %u", size, sizeof ( MsgTxReq_t ) );
        return;
    }

    /* COM番号チェック */
    if ( ( pMsg->comNo < NS16550_COM_MIN               ) &&
         (               NS16550_COM_MAX < pMsg->comNo )    ) {
        /* 範囲外 */

        DEBUG_LOG_ERR( "invalid comNo: %d", pMsg->comNo );
        DEBUG_LOG_FNC( "%s(): end.", __func__ );
        return;
    }

    /* データ転送 */
    Transmit( pMsg->comNo );

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       データ転送
 * @details     1byte毎に転送レジスタにデータを書き込みデータ転送を行う。転送
 *              バッファの空き残量が0になるまで、または、転送データが無くなるま
 *              で繰り返し行う。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16550_COM3 COM3
 *                  - NS16550_COM4 COM4
 */
/******************************************************************************/
static void Transmit( NS16550ComNo_t comNo )
{
    bool    retBool;    /* 転送データ取出結果 */
    uint8_t data;       /* 転送データ         */

    DEBUG_LOG_FNC( "%s(): start. comNo=%d", __func__, comNo );

    /* 初期化 */
    retBool = false;
    data    = 0;

    /* デバイス側転送バッファに空きがある限り繰り返す */
    while ( gComCtrlInfo[ comNo ].remain != 0 ) {
        /* 転送データ取り出し */
        retBool = BufferRead( comNo, BUFFER_ID_TRANSMIT, &data );

        /* 取り出し結果判定 */
        if ( retBool == false ) {
            /* データ無し */

            DEBUG_LOG_FNC( "%s(): end.", __func__ );
            return;
        }

        /* データ書込 */
        CtrlOutTHR( comNo, data );

        /* 転送バッファ残量更新 */
        gComCtrlInfo[ comNo ].remain--;
    };

    /* 転送割り込み有効化 */
    CtrlEnableInterrupt( comNo, NS16550_IER_THR );

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       転送制御スレッド
 * @details     メッセージ受信を待ち合わせて、受信メッセージに対応した処理を呼
 *              び出す。
 *
 * @param[in]   *pArg 未使用
 */
/******************************************************************************/
static void Transmitter( void *pArg )
{
    void       *pBuffer;    /* メッセージ受信バッファ   */
    size_t     size;        /* 受信メッセージサイズ     */
    MkRet_t    retMk;       /* カーネル戻り値           */
    MkErr_t    err;         /* カーネルエラー           */
    MsgHdr_t   *pMsgHdr;    /* メッセージヘッダ         */
    MkTaskId_t src;         /* メッセージ送信元タスクID */

    DEBUG_LOG_FNC( "%s(): start!", __func__ );

    /* 初期化 */
    pBuffer = NULL;
    size    = 0;
    retMk   = MK_RET_FAILURE;
    err     = MK_ERR_NONE;
    pMsgHdr = NULL;
    src     = MK_TASKID_NULL;

    /* メッセージ受信バッファ確保 */
    pBuffer = malloc( MK_MSG_SIZE_MAX );
    pMsgHdr = pBuffer;

    /* 確保結果判定 */
    if ( pBuffer == NULL ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "malloc()" );
        DEBUG_ABORT();
    }

    /* メインループ */
    while ( true ) {
        /* メッセージ受信 */
        retMk = LibMkMsgReceive(
                    MK_TASKID_NULL,   /* 受信タスクID   */
                    pMsgHdr,          /* バッファ       */
                    MK_MSG_SIZE_MAX,  /* バッファサイズ */
                    &src,             /* 送信元タスクID */
                    &size,            /* 受信サイズ     */
                    &err              /* エラー内容     */
                );

        /* 受信結果判定 */
        if ( retMk != MK_RET_SUCCESS ) {
            /* 失敗 */

            DEBUG_LOG_ERR( "LibMkMsgReceive(): ret=%d, err=0x%X", retMk, err );
            continue;
        }

        /* 受信メッセージサイズ判定 */
        if ( size < sizeof ( MsgHdr_t ) ) {
            /* 不正 */

            DEBUG_LOG_ERR( "invalid size: %u < %u", size, sizeof ( MsgHdr_t ) );
            continue;
        }

        /* メッセージID判定 */
        if ( pMsgHdr->msgId == MSG_ID_TXBUFFEREMPTY ) {
            /* 転送バッファ空 */
            ProcMsgTxBufferEmpty( src,
                                  ( MsgTxBufferEmpty_t * ) pMsgHdr,
                                  size                              );

        } else if ( pMsgHdr->msgId == MSG_ID_TXREQ ) {
            /* 転送要求 */
            ProcMsgTxReq( src,
                          ( MsgTxReq_t * ) pMsgHdr,
                          size                      );

        } else {
            /* 不正 */

            DEBUG_LOG_ERR( "invalid msgId: 0x%X", pMsgHdr->msgId );
        }
    }
}


/******************************************************************************/

