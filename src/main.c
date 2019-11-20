/******************************************************************************/
/*                                                                            */
/* src/main.c                                                                 */
/*                                                                 2019/10/13 */
/* Copyright (C) 2019 Mochi.                                                  */
/*                                                                            */
/******************************************************************************/
/******************************************************************************/
/* インクルード                                                               */
/******************************************************************************/
/* 標準ヘッダ */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* ライブラリヘッダ */
#include <libmk.h>
#include <libmvfs.h>
#include <MLib/MLibState.h>

/* モジュール内ヘッダ */
#include "msg.h"
#include "ns16550.h"
#include "Buffer.h"
#include "Ctrl.h"
#include "Debug.h"
#include "Receive.h"
#include "Transmit.h"


/******************************************************************************/
/* 定義                                                                       */
/******************************************************************************/
/* 状態 */
#define STATE_INIT ( 1 )    /**< 初期状態     */
#define STATE_OPEN ( 2 )    /**< OPEN中状態   */
#define STATE_READ ( 3 )    /**< READ待ち状態 */

/* イベント */
#define EVENT_VFSOPEN  ( 1 )    /**< VfsOpenイベント  */
#define EVENT_VFSREAD  ( 2 )    /**< VfsReadイベント  */
#define EVENT_VFSWRITE ( 3 )    /**< VfsWriteイベント */
#define EVENT_VFSCLOSE ( 4 )    /**< VfsCloseイベント */
#define EVENT_RX_NTC   ( 5 )    /**< 受信通知イベント */

/* ファイルパス */
#define FILE_PATH_SERIAL1 ( "/serial1" )    /**< serial1    */
#define FILE_PATH_SERIAL2 ( "/serial2" )    /**< serial2    */
#define FILE_NUM          ( 2 )             /**< ファイル数 */

/* ファイル管理情報 */
typedef struct {
    MLibStateHandle_t handle;       /**< 状態遷移ハンドル   */
    MkPid_t           pid;          /**< PID                */
    uint32_t          globalFd;     /**< グローバルFD       */
    char              *pPath;       /**< ファイルパス       */
} mngInfo_t;

/* 状態遷移タスクパラメータ(VfsOpen) */
typedef struct {
    NS16550ComNo_t comNo;       /**< COM番号      */
    MkPid_t        pid;         /**< PID          */
    uint32_t       globalFd;    /**< グローバルFD */
} stateParamVfsOpen_t;

/* 状態遷移タスクパラメータ(VfsRead) */
typedef struct {
    NS16550ComNo_t comNo;       /**< COM番号            */
    uint32_t       globalFd;    /**< グローバルFD       */
    uint64_t       readIdx;     /**< 読込みインデックス */
    size_t         size;        /**< 読込みサイズ       */
} stateParamVfsRead_t;

/* 状態遷移タスクパラメータ(VfsWrite) */
typedef struct {
    NS16550ComNo_t comNo;       /**< COM番号            */
    uint32_t       globalFd;    /**< グローバルFD       */
    uint64_t       writeIdx;    /**< 書込みインデックス */
    uint8_t        *pBuffer;    /**< 書込みバッファ     */
    size_t         size;        /**< 書込みサイズ       */
} stateParamVfsWrite_t;

/* 状態遷移タスクパラメータ(VfsClose) */
typedef struct {
    NS16550ComNo_t comNo;       /**< COM番号      */
    uint32_t       globalFd;    /**< グローバルFD */
} stateParamVfsClose_t;

/* 状態遷移タスクパラメータ(RxNtc) */
typedef struct {
    NS16550ComNo_t comNo;       /**< COM番号 */
} stateParamRxNtc_t;


/******************************************************************************/
/* ローカル関数宣言                                                           */
/******************************************************************************/
/* 状態遷移タスク */
static MLibState_t DoTask0101( void *pArg );
static MLibState_t DoTask0201( void *pArg );
static MLibState_t DoTask0202( void *pArg );
static MLibState_t DoTask0203( void *pArg );
static MLibState_t DoTask0204( void *pArg );
static MLibState_t DoTask0205( void *pArg );

/* グローバルFD->COM番号変換 */
static NS16550ComNo_t ConvertGlobalFdToComNo( uint32_t globalFd );
/* VfsClose処理 */
static void DoVfsClose( uint32_t globalFd );
/* VfsOpen処理 */
static void DoVfsOpen( MkPid_t    pid,
                       uint32_t   globalFd,
                       const char *pPath    );
/* VfsRead処理 */
static void DoVfsRead( uint32_t globalFd,
                       uint64_t readIdx,
                       size_t   size      );
/* VfsWrite処理 */
static void DoVfsWrite( uint32_t globalFd,
                        uint64_t writeIdx,
                        void     *pBuffer,
                        size_t   size      );
/* 管理情報初期化 */
static void InitMngInfo( NS16550ComNo_t comNo,
                         char           *pPath );
/* ファイルマウント */
static void Mount( char *pPath );
/* メッセージ受信 */
static void RecvMsg( MkTaskId_t src,
                     void       *pMsg,
                     size_t     size   );
/* 受信通知メッセージ処理 */
static void RecvMsgRxNtc( MkTaskId_t src,
                          MsgRxNtc_t *pMsg,
                          size_t     size   );
/* VfsClose応答送信 */
static void SendVfsCloseResp( uint32_t globalFd,
                              uint32_t result    );
/* VfsOpen応答送信 */
static void SendVfsOpenResp( uint32_t globalFd,
                             uint32_t result    );
/* VfsRead応答送信 */
static void SendVfsReadResp( uint32_t globalFd,
                             uint32_t result,
                             uint32_t ready,
                             void     *pBuffer,
                             size_t   size      );
/* VfsReady通知送信 */
static void SendVfsReadyNtc( const char *pPath,
                             uint32_t   rw      );
/* VfsWrite応答送信 */
static void SendVfsWriteResp( uint32_t globalFd,
                              uint32_t result,
                              uint32_t ready,
                              size_t   size      );


/******************************************************************************/
/* グローバル変数定義                                                         */
/******************************************************************************/
/** ファイル管理情報 */
mngInfo_t gMngInfo[ FILE_NUM ];

/** 状態遷移表 */
static MLibStateTransition_t gStt[] =
    {
        /*-----------+----------------+------------+--------------------*/
        /* 状態      | イベント       | タスク     | 遷移先状態         */
        /*-----------+----------------+------------+--------------------*/
        { STATE_INIT , EVENT_VFSOPEN  , DoTask0101 , { STATE_OPEN , 0 } },
        { STATE_INIT , EVENT_RX_NTC   , NULL       , { STATE_INIT , 0 } },
        /*-----------+----------------+------------+--------------------*/
        /* 状態      | イベント       | タスク     | 遷移先状態         */
        /*-----------+----------------+------------+--------------------*/
        { STATE_OPEN , EVENT_VFSOPEN  , DoTask0201 , { STATE_OPEN , 0 } },
        { STATE_OPEN , EVENT_VFSREAD  , DoTask0202 , { STATE_OPEN , 0 } },
        { STATE_OPEN , EVENT_VFSWRITE , DoTask0203 , { STATE_OPEN , 0 } },
        { STATE_OPEN , EVENT_VFSCLOSE , DoTask0204 , { STATE_INIT , 0 } },
        { STATE_OPEN , EVENT_RX_NTC   , DoTask0205 , { STATE_OPEN , 0 } }
        /*-----------+----------------+------------+--------------------*/
    };

/** 受信通知メッセージシーケンス番号 */
static uint32_t gSeqNoMsgRxNtc;

/** メインスレッドタスクID */
static MkTaskId_t gTaskId;


/******************************************************************************/
/* グローバル関数定義                                                         */
/******************************************************************************/
/******************************************************************************/
/**
 * @brief       drv-ns16550メイン関数
 * @details     メインループを起動する。
 */
/******************************************************************************/
void main( void )
{
    MkRet_t            retMk;       /* カーネル戻り値    */
    uint32_t           errMk;       /* カーネルエラー    */
    uint32_t           errLibMvfs;  /* LibMvfsエラー     */
    LibMvfsRet_t       retLibMvfs;  /* LibMvfs関数戻り値 */
    LibMvfsSchedInfo_t schedInfo;   /* スケジューラ情報  */

    /* 初期化 */
    retMk      = MK_RET_FAILURE;
    errMk      = MK_ERR_NONE;
    errLibMvfs = LIBMVFS_ERR_NONE;
    retLibMvfs = LIBMVFS_RET_FAILURE;
    memset( &schedInfo, 0, sizeof ( schedInfo ) );

    DEBUG_LOG_TRC( "driver start!" );

    /* シーケンス番号初期化 */
    gSeqNoMsgRxNtc = 0;

    /* タスクID取得 */
    retMk = LibMkTaskGetId( &gTaskId, &errMk );

    /* 取得結果判定 */
    if ( retMk != MK_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "LibMkTaskGetId(): ret=%d, err=0x%X", retMk, errMk );
        DEBUG_ABORT();
    }

    /* 管理情報初期化 */
    InitMngInfo( NS16550_COM1, FILE_PATH_SERIAL1 );
    InitMngInfo( NS16550_COM2, FILE_PATH_SERIAL2 );

    /* NS16550初期化 */
    CtrlInit( NS16550_COM1 );
    CtrlInit( NS16550_COM2 );

    /* モジュール初期化 */
    BufferInit();
    TransmitInit();
    ReceiveInit();

    /* ファイルマウント */
    Mount( FILE_PATH_SERIAL1 );
    Mount( FILE_PATH_SERIAL2 );

    /* スケジュール情報設定 */
    schedInfo.callBack.pVfsOpen  = &DoVfsOpen;
    schedInfo.callBack.pVfsWrite = &DoVfsWrite;
    schedInfo.callBack.pVfsRead  = &DoVfsRead;
    schedInfo.callBack.pVfsClose = &DoVfsClose;
    schedInfo.callBack.pOther    = &RecvMsg;

    DEBUG_LOG_TRC( "schedule start!" );

    /* スケジューラ起動 */
    retLibMvfs = LibMvfsSchedStart( &schedInfo, &errLibMvfs );

    /* スケジューラ起動結果判定 */
    if ( retLibMvfs != LIBMVFS_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR(
            "LibMvfsSchedStart(): ret=%d, err=0x%X",
            retLibMvfs,
            errLibMvfs
        );
        DEBUG_ABORT();
    }

    /* TODO */
    DEBUG_ABORT();
}


/******************************************************************************/
/**
 * @brief       メイン制御スレッドタスクID取得
 * @details     メイン制御スレッドのタスクIDを取得する。
 *
 * @return      タスクIDを返す。
 */
/******************************************************************************/
MkTaskId_t MainGetTaskId( void )
{
    return gTaskId;
}


/******************************************************************************/
/**
 * @brief       受信通知
 * @details     メイン制御に受信通知メッセージを送信する。
 *
 * @param[in]   comNo COM番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 *                  - NS16550_COM3 COM3
 *                  - NS16550_COM4 COM4
 */
/******************************************************************************/
void MainSendMsgRxNtc( NS16550ComNo_t comNo )
{
    MkRet_t    retMk;   /* カーネル戻り値 */
    MkErr_t    err;     /* カーネルエラー */
    MsgRxNtc_t msg;     /* メッセージ     */

    DEBUG_LOG_FNC( "%s(): start. comNo=%d", __func__, comNo );

    /* 初期化 */
    retMk = MK_RET_FAILURE;
    err   = MK_ERR_NONE;
    memset( &msg, 0, sizeof ( MsgRxNtc_t ) );

    /* メッセージ設定 */
    msg.header.msgId = MSG_ID_RXNTC;
    msg.header.type  = MSG_TYPE_NTC;
    msg.header.seqNo = ++gSeqNoMsgRxNtc;
    msg.comNo        = comNo;

    DEBUG_LOG_TRC(
        "%s(): gTaskId=0x%X, seqNo=%u, comNo=%d",
        __func__,
        gTaskId,
        gSeqNoMsgRxNtc,
        comNo
    );

    /* メッセージ送信 */
    retMk = LibMkMsgSend( gTaskId, &msg, sizeof ( MsgRxNtc_t ), &err );

    /* 送信結果判定 */
    if ( retMk != MK_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "LibMkMsgSend(): ret=%d, err=0x%X", retMk, err );
        DEBUG_ABORT();
    }

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/* ローカル関数定義                                                           */
/******************************************************************************/
/******************************************************************************/
/**
 * @brief       タスク0101
 * @details     初期状態でVfsOpenイベントを入力した時に、グローバルFDを記録し
 *              ファイル操作を受け付ける様にし、VfsOpen応答(成功)を送信する。
 *
 * @param[in]   *pArg
 *
 * @return      遷移先状態を返す。
 * @retval      STATE_OPEN Open済み状態
 */
/******************************************************************************/
static MLibState_t DoTask0101( void *pArg )
{
    stateParamVfsOpen_t *pParam;    /* パラメータ */

    DEBUG_LOG_FNC( "%s(): start.", __func__ );

    /* 初期化 */
    pParam = ( stateParamVfsOpen_t * ) pArg;

    /* 管理情報設定 */
    gMngInfo[ pParam->comNo ].pid      = pParam->pid;
    gMngInfo[ pParam->comNo ].globalFd = pParam->globalFd;

    /* VfsOpen応答(成功)送信 */
    SendVfsOpenResp( pParam->globalFd, LIBMVFS_RET_SUCCESS );

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return STATE_OPEN;
}


/******************************************************************************/
/**
 * @brief       タスク0201
 * @details     受付不可状態にてVfsOpenイベントを入力した時に、VfsOpen応答
 *              (失敗)を送信する。
 *
 * @param[in]   *pArg
 *
 * @return      遷移先状態を返す。
 * @retval      STATE_OPEN Open済み状態（自状態）
 * @retval      STATE_READ Read待ち状態（自状態）
 */
/******************************************************************************/
static MLibState_t DoTask0201( void *pArg )
{
    MLibState_t         ret;        /* 遷移先状態 */
    stateParamVfsOpen_t *pParam;    /* パラメータ */

    DEBUG_LOG_FNC( "%s(): start.", __func__ );

    /* 初期化 */
    ret    = MLIB_STATE_NULL;
    pParam = ( stateParamVfsOpen_t * ) pArg;

    /* VfsOpen応答(失敗)送信 */
    SendVfsOpenResp( pParam->globalFd, LIBMVFS_RET_FAILURE );

    /* 自状態取得 */
    ret = MLibStateGet( &( gMngInfo[ pParam->comNo ].handle ), NULL );

    DEBUG_LOG_FNC( "%s(): end. ret=%u", __func__, ret );
    return ret;
}


/******************************************************************************/
/**
 * @brief       タスク0202
 * @details     Open済み状態にてVfsReadイベントを入力した時に、受信バッファから
 *              データを読み込む。
 *
 * @param[in]   *pArg
 *
 * @return      遷移先状態を返す。
 * @retval      STATE_OPEN Open済み状態
 */
/******************************************************************************/
static MLibState_t DoTask0202( void *pArg )
{
    bool                ret;        /* バッファ読込結果   */
    char                *pBuffer;   /* バッファ           */
    size_t              readSize;   /* 読込みサイズ       */
    uint32_t            ready;      /* 読込レディ状態     */
    stateParamVfsRead_t *pParam;    /* パラメータ         */

    DEBUG_LOG_FNC( "%s(): start.", __func__ );

    /* 初期化 */
    ret      = false;
    pBuffer  = NULL;
    pParam   = ( stateParamVfsRead_t * ) pArg;
    readSize = 0;

    /* レデイ状態取得 */
    ready = BufferGetReady( pParam->comNo, BUFFER_ID_RECEIVE );

    /* バッファ作成 */
    pBuffer = malloc( pParam->size );

    /* 確保結果判定 */
    if ( pBuffer == NULL ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "malloc(): size=%u", pParam->size );

        /* VfsRead応答(失敗)送信 */
        SendVfsReadResp( pParam->globalFd,
                         LIBMVFS_RET_FAILURE,
                         ready,
                         NULL,
                         0                    );

        DEBUG_LOG_FNC( "%s(): end.", __func__ );
        return STATE_OPEN;
    }

    /* 1byte毎に繰り返す */
    while ( readSize < pParam->size ) {
        /* バッファ読込 */
        ret = BufferRead( pParam->comNo,
                          BUFFER_ID_RECEIVE,
                          &pBuffer[ readSize ] );

        /* 読込み結果判定 */
        if ( ret == false ) {
            /* 失敗 */
            break;
        }

        /* 読込みサイズ更新 */
        readSize++;
    }

    /* VfsRead応答(成功)送信 */
    SendVfsReadResp( pParam->globalFd,
                     LIBMVFS_RET_SUCCESS,
                     ready,
                     pBuffer,
                     readSize             );

    /* バッファ解放 */
    free( pBuffer );

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return STATE_OPEN;
}


/******************************************************************************/
/**
 * @brief       タスク0203
 * @details     Open済み状態でVfsWriteイベントを入力した時に、書込みデータを転
 *              送バッファに書き込み転送を開始する。転送バッファに全てのデータ
 *              を書き込んだ後、VfsWrite応答(成功)を送信する。
 *
 * @param[in]   *pArg
 *
 * @return      遷移先状態を返す。
 * @retval      STATE_OPEN Open済み状態
 */
/******************************************************************************/
static MLibState_t DoTask0203( void *pArg )
{
    uint32_t             idx;       /* インデックス */
    uint32_t             ready;     /* レディ状態   */
    stateParamVfsWrite_t *pParam;   /* パラメータ   */

    DEBUG_LOG_FNC( "%s(): start.", __func__ );

    /* 初期化 */
    idx    = 0;
    pParam = ( stateParamVfsWrite_t * ) pArg;

    /* 書込レディ状態取得 */
    ready = BufferGetReady( pParam->comNo, BUFFER_ID_TRANSMIT );

    /* 1byte毎に繰り返す */
    while ( idx < pParam->size ) {
        /* 転送バッファ書込み */
        BufferWrite( pParam->comNo,
                     BUFFER_ID_TRANSMIT,
                     pParam->pBuffer[ idx ] );

        /* インデックス更新 */
        idx++;
    }

    /* 転送要求送信 */
    TransmitSendMsgTxReq( pParam->comNo );

    /* VfsWrite応答(成功)送信 */
    SendVfsWriteResp( pParam->globalFd,
                      LIBMVFS_RET_SUCCESS,
                      ready,
                      pParam->size         );

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return STATE_OPEN;
}


/******************************************************************************/
/**
 * @brief       タスク0204
 * @details     Open済み状態にてVfsCloseイベントを入力した時、管理情報を初期化
 *              しVfsClose応答(成功)を送信して、初期状態に遷移する。
 *
 * @param[in]   *pArg
 *
 * @return      遷移先状態を返す。
 * @retval      STATE_INIT 初期状態
 */
/******************************************************************************/
static MLibState_t DoTask0204( void *pArg )
{
    stateParamVfsClose_t *pParam;   /* パラメータ */

    DEBUG_LOG_FNC( "%s(): start.", __func__ );

   /* 初期化 */
    pParam = ( stateParamVfsClose_t * ) pArg;

     /* 管理情報初期化 */
    memset( &gMngInfo[ pParam->comNo ], 0, sizeof ( mngInfo_t ) );

    /* VfsClose応答(成功)送信 */
    SendVfsCloseResp( pParam->globalFd, LIBMVFS_RET_SUCCESS );

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return STATE_INIT;
}


/******************************************************************************/
/**
 * @brief       タスク0205
 * @details     Open済み状態にて受信通知イベントを入力した時、仮想ファイルサー
 *              バにVfsReady通知を送信する。
 *
 * @param[in]   *pArg
 *
 * @return      遷移先状態を返す。
 * @retval      STATE_OPEN Open済み状態
 */
/******************************************************************************/
static MLibState_t DoTask0205( void *pArg )
{
    stateParamRxNtc_t *pParam;  /* パラメータ */

    DEBUG_LOG_FNC( "%s(): start.", __func__ );

    /* 初期化 */
    pParam = ( stateParamRxNtc_t * ) pArg;

    /* VfsReady通知送信 */
    SendVfsReadyNtc( gMngInfo[ pParam->comNo ].pPath, MVFS_READY_READ );

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return STATE_OPEN;
}


/******************************************************************************/
/**
 * @brief       VfsClose要求
 * @details     引数globalFdから該当COM番号を判定し、VfsCloseイベントを状態遷移
 *              に入力する。
 *
 * @param[in]   globalFd グローバルFD
 */
/******************************************************************************/
static void DoVfsClose( uint32_t globalFd )
{
    uint32_t             err;       /* エラー         */
    MLibRet_t            retMLib;   /* MLib関数戻り値 */
    MLibState_t          prevState; /* 遷移前状態     */
    MLibState_t          nextState; /* 遷移後状態     */
    NS16550ComNo_t       comNo;     /* COM番号        */
    stateParamVfsClose_t param;     /* パラメータ     */

    DEBUG_LOG_FNC( "%s(): start. globalFd=%u", __func__, globalFd );

    /* 初期化 */
    err       = MLIB_STATE_ERR_NONE;
    retMLib   = MLIB_SUCCESS;
    prevState = MLIB_STATE_NULL;
    nextState = MLIB_STATE_NULL;
    comNo     = FILE_NUM;
    memset( &param, 0, sizeof ( stateParamVfsClose_t ) );

    /* COM番号変換 */
    comNo = ConvertGlobalFdToComNo( globalFd );

    /* 変換結果判定 */
    if ( comNo == FILE_NUM ) {
        /* 不正 */

        DEBUG_LOG_ERR( "ConvertGlobalFdToComNo(): globalFd=%u", globalFd );

        /* VfsClose応答(失敗)送信 */
        SendVfsCloseResp( globalFd, LIBMVFS_RET_FAILURE );

        DEBUG_LOG_FNC( "%s(): end.", __func__ );
        return;
    }

    /* パラメータ設定 */
    param.comNo    = comNo;
    param.globalFd = globalFd;

    /* 状態遷移実行 */
    retMLib = MLibStateExec(
                  &( gMngInfo[ comNo ].handle ),
                  EVENT_VFSCLOSE,
                  &param,
                  &prevState,
                  &nextState,
                  &err
              );

    /* 実行結果判定 */
    if ( retMLib != MLIB_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "MLibStateExec(): ret=%d, err=0x%X", retMLib, err );

        /* VfsClose応答(失敗)送信 */
        SendVfsCloseResp( globalFd, LIBMVFS_RET_FAILURE );
    }

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       VfsOpen要求
 * @details     引数*pPathから対応するCOM番号を判定し、VfsOpenイベントを状態遷
 *              移に入力する。
 *
 * @param[in]   pid      プロセスID
 * @param[in]   globalFd グローバルFD
 * @param[in]   *pPath   ファイルパス
 */
/******************************************************************************/
static void DoVfsOpen( MkPid_t    pid,
                       uint32_t   globalFd,
                       const char *pPath    )
{
    uint32_t            err;        /* エラー         */
    MLibRet_t           retMLib;    /* MLib関数戻り値 */
    MLibState_t         prevState;  /* 遷移前状態     */
    MLibState_t         nextState;  /* 遷移後状態     */
    NS16550ComNo_t      comNo;      /* COM番号        */
    stateParamVfsOpen_t param;      /* パラメータ     */

    DEBUG_LOG_FNC(
        "%s(): start. pid=%u, globalFd=%u, pPath=%s",
        __func__,
        pid,
        globalFd,
        pPath
    );

    /* 初期化 */
    err       = MLIB_STATE_ERR_NONE;
    retMLib   = MLIB_SUCCESS;
    prevState = MLIB_STATE_NULL;
    nextState = MLIB_STATE_NULL;
    comNo     = FILE_NUM;
    memset( &param, 0, sizeof ( stateParamVfsOpen_t ) );

    /* パス比較 */
    if ( strcmp( FILE_PATH_SERIAL1, pPath ) == 0 ) {
        /* COM1パス */
        comNo = NS16550_COM1;

    } else if ( strcmp( FILE_PATH_SERIAL2, pPath ) == 0 ) {
        /* COM2パス */
        comNo = NS16550_COM2;

    } else {
        /* 不正 */

        DEBUG_LOG_ERR( "invalid path: %s", pPath );

        /* VfsOpen応答(失敗)送信 */
        SendVfsOpenResp( globalFd, LIBMVFS_RET_FAILURE );

        DEBUG_LOG_FNC( "%s(): end.", __func__ );
        return;
    }

    /* パラメータ設定 */
    param.comNo    = comNo;
    param.pid      = pid;
    param.globalFd = globalFd;

    DEBUG_LOG_TRC(
        "%s(): comNo=%d, pid=0x%X, globalFd=%d, path=%s",
        __func__,
        comNo,
        pid,
        globalFd,
        pPath
    );

    /* 状態遷移実行 */
    retMLib = MLibStateExec(
                  &( gMngInfo[ comNo ].handle ),
                  EVENT_VFSOPEN,
                  &param,
                  &prevState,
                  &nextState,
                  &err
              );

    /* 実行結果判定 */
    if ( retMLib != MLIB_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "MLibStateExec(): ret=%d, err=0x%X", retMLib, err );

        /* VfsOpen応答(失敗)送信 */
        SendVfsOpenResp( globalFd, LIBMVFS_RET_FAILURE );
    }

    DEBUG_LOG_TRC( "state: %d -> %d", prevState, nextState );
    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       VfsRead要求
 * @details     引数globalFdから該当COM番号を判定し、VfsReadイベントを状態遷移
 *              に入力する。
 *
 * @param[in]   globalFd グローバルFD
 * @param[in]   readIdx  読込インデックス
 * @param[in]   size     読込サイズ
 */
/******************************************************************************/
static void DoVfsRead( uint32_t globalFd,
                       uint64_t readIdx,
                       size_t   size      )
{
    uint32_t            ready;      /* レディ状態     */
    uint32_t            err;        /* エラー         */
    MLibRet_t           retMLib;    /* MLib関数戻り値 */
    MLibState_t         prevState;  /* 遷移前状態     */
    MLibState_t         nextState;  /* 遷移後状態     */
    NS16550ComNo_t      comNo;      /* COM番号        */
    stateParamVfsRead_t param;      /* パラメータ     */

    DEBUG_LOG_FNC(
        "%s(): start. globalFd=%u, readIdx=%u, size=%u",
        __func__,
        globalFd,
        ( uint32_t ) readIdx,
        size
    );

    /* 初期化 */
    err       = MLIB_STATE_ERR_NONE;
    retMLib   = MLIB_SUCCESS;
    prevState = MLIB_STATE_NULL;
    nextState = MLIB_STATE_NULL;
    comNo     = FILE_NUM;
    memset( &param, 0, sizeof ( stateParamVfsRead_t ) );

    /* COM番号変換 */
    comNo = ConvertGlobalFdToComNo( globalFd );

    /* 変換結果判定 */
    if ( comNo == FILE_NUM ) {
        /* 不正 */

        DEBUG_LOG_ERR( "ConvertGlobalFdToComNo(): globalFd=%u", globalFd );

        /* レデイ状態取得 */
        ready = BufferGetReady( comNo, BUFFER_ID_RECEIVE );

        /* VfsRead応答(失敗)送信 */
        SendVfsReadResp( globalFd, LIBMVFS_RET_FAILURE, ready, NULL, 0 );

        DEBUG_LOG_FNC( "%s(): end.", __func__ );
        return;
    }

    /* パラメータ設定 */
    param.globalFd = globalFd;
    param.readIdx  = readIdx;
    param.size     = size;

    DEBUG_LOG_TRC(
        "%s(): globalFd=%u, readIdx=%u, size=%u",
        __func__,
        globalFd,
        ( uint32_t ) readIdx,
        size
    );

    /* 状態遷移実行 */
    retMLib = MLibStateExec(
                  &( gMngInfo[ comNo ].handle ),
                  EVENT_VFSREAD,
                  &param,
                  &prevState,
                  &nextState,
                  &err
              );

    /* 実行結果判定 */
    if ( retMLib != MLIB_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "MLibStateExec(): ret=%d, err=0x%X", retMLib, err );

        /* レデイ状態取得 */
        ready = BufferGetReady( comNo, BUFFER_ID_RECEIVE );

        /* VfsRead応答(失敗)送信 */
        SendVfsReadResp( globalFd, LIBMVFS_RET_FAILURE, ready, NULL, 0 );
    }

    DEBUG_LOG_TRC( "state: %u -> %u", prevState, nextState );
    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       VfsWrite要求
 * @details     引数globalFdから該当COM番号を判定し、VfsWriteイベントを状態遷移
 *              に入力する。
 *
 * @param[in]   globalFd グローバルFD
 * @param[in]   writeIdx 書込インデックス
 * @param[in]   *pBuffer 書込バッファ
 * @param[in]   size     書込サイズ
 */
/******************************************************************************/
static void DoVfsWrite( uint32_t globalFd,
                        uint64_t writeIdx,
                        void     *pBuffer,
                        size_t   size      )
{
    uint32_t             ready;     /* レディ状態     */
    uint32_t             err;       /* エラー         */
    MLibRet_t            retMLib;   /* MLib関数戻り値 */
    MLibState_t          prevState; /* 遷移前状態     */
    MLibState_t          nextState; /* 遷移後状態     */
    NS16550ComNo_t       comNo;     /* COM番号        */
    stateParamVfsWrite_t param;     /* パラメータ     */

    DEBUG_LOG_FNC(
        "%s(): start. globalFd=%u, writeIdx=%u, pBuffer=%p, size=%u",
        __func__,
        globalFd,
        ( uint32_t ) writeIdx,
        pBuffer,
        size
    );

    /* 初期化 */
    err       = MLIB_STATE_ERR_NONE;
    retMLib   = MLIB_SUCCESS;
    prevState = MLIB_STATE_NULL;
    nextState = MLIB_STATE_NULL;
    comNo     = FILE_NUM;
    memset( &param, 0, sizeof ( stateParamVfsWrite_t ) );

    /* パラメータ */
    param.globalFd = globalFd;
    param.writeIdx = writeIdx;
    param.pBuffer  = pBuffer;
    param.size     = size;

    /* COM番号変換 */
    comNo = ConvertGlobalFdToComNo( globalFd );

    /* 変換結果判定 */
    if ( comNo == FILE_NUM ) {
        /* 不正 */

        DEBUG_LOG_ERR( "ConvertGlobalFdToComNo(): globalFd=%u", globalFd );

        /* 書込レディ状態取得 */
        ready = BufferGetReady( comNo, BUFFER_ID_TRANSMIT );

        /* VfsWrite応答(失敗)送信 */
        SendVfsWriteResp( globalFd, LIBMVFS_RET_FAILURE, ready, 0 );

        DEBUG_LOG_FNC( "%s(): end.", __func__ );
        return;
    }

    DEBUG_LOG_TRC(
        "%s(): globalFd=%u, writeIdx=%u, size=%u",
        __func__,
        globalFd,
        ( uint32_t ) writeIdx,
        size
    );

    /* 状態遷移実行 */
    retMLib = MLibStateExec(
                  &( gMngInfo[ comNo ].handle ),
                  EVENT_VFSWRITE,
                  &param,
                  &prevState,
                  &nextState,
                  &err
              );

    /* 実行結果判定 */
    if ( retMLib != MLIB_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "MLibStateExec(): ret=%d, err=0x%X", retMLib, err );

        /* 書込レディ状態取得 */
        ready = BufferGetReady( comNo, BUFFER_ID_TRANSMIT );

        /* VfsWrite応答(失敗)送信 */
        SendVfsWriteResp( globalFd, LIBMVFS_RET_FAILURE, ready, 0 );
    }

    DEBUG_LOG_TRC( "state: %u -> %u", prevState, nextState );
    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       グローバルFD->COM番号変換
 * @details     管理情報からグローバルFDに対応するCOM番号を検索する。
 *
 * @param[in]   globalFD グローバルFD
 *
 * @return      COM番号を返す。
 * @retval      NS16550_COM1 COM1
 * @retval      NS16550_COM2 COM2
 * @retval      FILE_NUM     無効値
 */
/******************************************************************************/
static NS16550ComNo_t ConvertGlobalFdToComNo( uint32_t globalFd )
{
    uint32_t idx;   /* インデックス */

    DEBUG_LOG_FNC( "%s(): start. globalFd=%u", __func__, globalFd );

    /* 管理テーブル毎に繰り返す */
    for ( idx = 0; idx < FILE_NUM; idx++ ) {
        /* グローバルFD比較 */
        if ( gMngInfo[ idx ].globalFd == globalFd ) {
            /* 一致 */
            break;
        }
    }

    DEBUG_LOG_FNC( "%s(): end. idx=%d", __func__, idx );
    return idx;
}


/******************************************************************************/
/**
 * @brief       管理情報初期化
 * @details     ファイル毎の状態遷移を初期化する。
 *
 * @param[in]   comNo  COM番号
 * @param[in]   *pPath ファイルパス
 */
/******************************************************************************/
static void InitMngInfo( NS16550ComNo_t comNo,
                         char           *pPath )
{
    uint32_t  err;      /* MLibエラー     */
    MLibRet_t retMLib;  /* MLib関数戻り値 */

    DEBUG_LOG_FNC( "%s(): start. comNo=%d, pPath=%s", __func__, comNo, pPath );

    /* 初期化 */
    err     = MLIB_STATE_ERR_NONE;
    retMLib = MLIB_FAILURE;

    DEBUG_LOG_TRC( "%s(): comNo=%u, pPath=%s", __func__, comNo, pPath );

    /* 管理情報初期化 */
    memset( &gMngInfo[ comNo ], 0, sizeof ( mngInfo_t ) );

    /* ファイルパス設定 */
    gMngInfo[ comNo ].pPath = pPath;

    /* 状態遷移初期化 */
    retMLib = MLibStateInit(
                  &( gMngInfo[ comNo ].handle ),
                  gStt,
                  sizeof ( gStt ),
                  STATE_INIT,
                  &err
              );

    /* 初期化結果判定 */
    if ( retMLib != MLIB_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "MLibStateInit(): ret=%d, err=0x%X", retMLib, err );
        DEBUG_ABORT();
    }

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       ファイルマウント
 * @details     ファイルをマウントする。
 *
 * @param[in]   *pPath ファイルパス
 */
/******************************************************************************/
static void Mount( char *pPath )
{
    uint32_t     err;           /* LibMvfsエラー     */
    LibMvfsRet_t retLibMvfs;    /* LibMvfs関数戻り値 */

    DEBUG_LOG_FNC( "%s(): start. pPath=%s", __func__, pPath );

    /* 初期化 */
    err        = LIBMVFS_ERR_NONE;
    retLibMvfs = LIBMVFS_RET_FAILURE;

    DEBUG_LOG_TRC( "%s(): pPath=%s", __func__, pPath );

    /* ファイルマウント */
    retLibMvfs = LibMvfsMount( pPath, &err );

    /* マウント結果判定 */
    if ( retLibMvfs != LIBMVFS_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "LibMvfsMount(): ret=%d, err=0x%X", retLibMvfs, err );
    }

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       メッセージ受信
 * @details     メッセージIDを判定し対応する受信処理関数を呼び出す。
 *
 * @param[in]   src   送信元タスクID
 * @param[in]   *pMsg 受信メッセージ
 * @param[in]   size  受信メッセージサイズ
 */
/******************************************************************************/
static void RecvMsg( MkTaskId_t src,
                     void       *pMsg,
                     size_t     size   )
{
    MsgHdr_t *pMsgHdr;  /* メッセージヘッダ */

    DEBUG_LOG_FNC(
        "%s(): start. src=0x%X, pMsg=%p, size=%u",
        __func__,
        src,
        pMsg,
        size
    );

    /* 初期化 */
    pMsgHdr = ( MsgHdr_t * ) pMsg;

    /* メッセージサイズ判定 */
    if ( size < sizeof ( MsgHdr_t ) ) {
        /* 不正 */

        DEBUG_LOG_ERR( "invalid size: %u < %u", size, sizeof ( MsgHdr_t ) );
        DEBUG_LOG_FNC( "%s(): end.", __func__ );
        return;
    }

    /* メッセージID判定 */
    switch ( pMsgHdr->msgId ) {
        case MSG_ID_RXNTC:
            /* 受信通知 */
            RecvMsgRxNtc( src, ( MsgRxNtc_t * ) pMsg, size );
            break;

        default:
            /* 不正 */

            DEBUG_LOG_ERR( "invalid msgId: %u", pMsgHdr->msgId );
            break;
    }

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       受信通知メッセージ処理
 * @details     メッセージの正当性チェックを行い、受信通知イベントを状態遷移に
 *              入力する。
 *
 * @param[in]   src   送信元タスクID
 * @param[in]   *pMsg メッセージ
 * @param[in]   size  メッセージサイズ
 */
/******************************************************************************/
static void RecvMsgRxNtc( MkTaskId_t src,
                          MsgRxNtc_t *pMsg,
                          size_t     size   )
{
    uint32_t          err;          /* エラー         */
    MLibRet_t         retMLib;      /* MLib関数戻り値 */
    MLibState_t       prevState;    /* 遷移前状態     */
    MLibState_t       nextState;    /* 遷移後状態     */
    stateParamRxNtc_t param;        /* パラメータ     */

    DEBUG_LOG_FNC(
        "%s(): start. src=0x%X, pMsg=%p, size=%u",
        __func__,
        src,
        pMsg,
        size
    );

    /* 初期化 */
    err       = MLIB_STATE_ERR_NONE;
    retMLib   = MLIB_SUCCESS;
    prevState = MLIB_STATE_NULL;
    nextState = MLIB_STATE_NULL;
    memset( &param, 0, sizeof ( stateParamRxNtc_t ) );

    /* メッセージサイズチェック */
    if ( size != sizeof ( MsgRxNtc_t ) ) {
        /* 不正 */

        DEBUG_LOG_ERR( "invalid size: %u != %u", size, sizeof ( MsgRxNtc_t ) );
        DEBUG_LOG_FNC( "%s(): end.", __func__ );
        return;
    }

    /* 送信元タスクID判定 */
    if ( src != ReceiveGetTaskId() ) {
        /* 受信制御スレッドでない */

        DEBUG_LOG_ERR( "invalid src: 0x%X", src );
        return;
    }

    /* パラメータ設定 */
    param.comNo = pMsg->comNo;

    DEBUG_LOG_TRC( "%s(): comNo=%u", __func__, pMsg->comNo );

    /* 状態遷移実行 */
    retMLib = MLibStateExec(
                  &( gMngInfo[ pMsg->comNo ].handle ),
                  EVENT_RX_NTC,
                  &param,
                  &prevState,
                  &nextState,
                  &err
              );

    /* 実行結果判定 */
    if ( retMLib != MLIB_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "MLibStateExec(): ret=%d, err=0x%X", retMLib, err );
    }

    DEBUG_LOG_TRC( "state: %u -> %u", prevState, nextState );
    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       VfsClose応答送信
 * @details     MVFSにVfsClose応答を送信する。
 *
 * @param[in]   globalFd グローバルFD
 * @param[in]   result   処理結果
 *                  - LIBMVFS_RET_SUCCESS 成功
 *                  - LIBMVFS_RET_FAILURE 失敗
 */
/******************************************************************************/
static void SendVfsCloseResp( uint32_t globalFd,
                              uint32_t result    )
{
    uint32_t     err;           /* LibMvfs関数エラー */
    LibMvfsRet_t retLibMvfs;    /* LibMvfs関数戻り値 */

    DEBUG_LOG_FNC(
        "%s(): start. globalFd=%u, result=%d",
        __func__,
        globalFd,
        result
    );

    /* 初期化 */
    err        = LIBMVFS_ERR_NONE;
    retLibMvfs = LIBMVFS_RET_FAILURE;

    DEBUG_LOG_TRC(
        "%s(): globalFd=%u, result=%u",
        __func__,
        globalFd,
        result
    );

    /* VfsClose応答送信 */
    retLibMvfs = LibMvfsSendVfsCloseResp( globalFd, result, &err );

    /* 送信結果判定 */
    if ( retLibMvfs != LIBMVFS_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR(
            "LibMvfsSendVfsCloseResp(): ret=%d, err=0x%X",
            retLibMvfs,
            err
        );
    }

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       VfsOpen応答送信
 * @details     MVFSにVfsClose応答を送信する。
 *
 * @param[in]   globalFd グローバルFD
 * @param[in]   result   処理結果
 *                  - LIBMVFS_RET_SUCCESS 成功
 *                  - LIBMVFS_RET_FAILURE 失敗
 */
/******************************************************************************/
static void SendVfsOpenResp( uint32_t globalFd,
                             uint32_t result    )
{
    uint32_t     err;           /* LibMvfs関数エラー */
    LibMvfsRet_t retLibMvfs;    /* LibMvfs関数戻り値 */

    DEBUG_LOG_FNC( "%s(): start. result=%d", __func__, result );

    /* 初期化 */
    err        = LIBMVFS_ERR_NONE;
    retLibMvfs = LIBMVFS_RET_FAILURE;

    DEBUG_LOG_TRC( "%s(): result=%u", __func__, result );

    /* VfsOpen応答送信 */
    retLibMvfs = LibMvfsSendVfsOpenResp( globalFd, result, &err );

    /* 送信結果判定 */
    if ( retLibMvfs != LIBMVFS_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR(
            "LibMvfsSendVfsOpenResp(): ret=%d, err=0x%X",
            retLibMvfs,
            err
        );
    }

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       VfsRead応答送信
 * @details     MVFSにVfsRead応答を送信する。
 *
 * @param[in]   globalFd グローバルFD
 * @param[in]   result   処理結果
 *                  - LIBMVFS_RET_SUCCESS 成功
 *                  - LIBMVFS_RET_FAILURE 失敗
 * @param[in]   ready    レディ状態
 *                  - 0               非レディ
 *                  - MVFS_READY_READ 読込レディ
 * @param[in]   *pBuffer バッファ
 * @param[in]   size     読込サイズ
 */
/******************************************************************************/
static void SendVfsReadResp( uint32_t globalFd,
                             uint32_t result,
                             uint32_t ready,
                             void     *pBuffer,
                             size_t   size      )
{
    uint32_t     err;           /* LibMvfs関数エラー */
    LibMvfsRet_t retLibMvfs;    /* LibMvfs関数戻り値 */

    DEBUG_LOG_FNC(
        "%s(): start. globalFd=%u, result=%d, pBuffer=%p, size=%u",
        __func__,
        globalFd,
        result,
        pBuffer,
        size
    );

    /* 初期化 */
    err        = LIBMVFS_ERR_NONE;
    retLibMvfs = LIBMVFS_RET_FAILURE;

    DEBUG_LOG_TRC(
        "%s(): globalFd=%u, result=%d, ready=%#X, size=%u",
        __func__,
        globalFd,
        result,
        ready,
        size
    );

    /* VfsRead応答送信 */
    retLibMvfs = LibMvfsSendVfsReadResp( globalFd,
                                         result,
                                         ready,
                                         pBuffer,
                                         size,
                                         &err      );

    /* 送信結果判定 */
    if ( retLibMvfs != LIBMVFS_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR(
            "LibMvfsSendVfsReadResp(): ret=%d, err=0x%X",
            retLibMvfs,
            err
        );
    }

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       VfsReady通知送信
 * @details     MVFSにVfsReady通知を送信する。
 *
 * @param[in]   *pPath ファイルパス
 * @param[in]   rw     RWフラグ
 *                  - MVFS_READY_READ  Readレディ
 *                  - MVFS_READY_WRITE Writeレディ
 */
/******************************************************************************/
static void SendVfsReadyNtc( const char *pPath,
                             uint32_t   rw      )
{
    uint32_t     err;           /* LibMvfs関数エラー */
    LibMvfsRet_t retLibMvfs;    /* LibMvfs関数戻り値 */

    DEBUG_LOG_TRC( "%s(): start. globalFd=%s, rw=%u", __func__, pPath, rw );

    /* 初期化 */
    err        = LIBMVFS_ERR_NONE;
    retLibMvfs = LIBMVFS_RET_FAILURE;

    /* VfsReady通知送信 */
    retLibMvfs = LibMvfsSendVfsReadyNtc( pPath, rw, &err );

    /* 送信結果判定 */
    if ( retLibMvfs != LIBMVFS_RET_SUCCESS ) {
        /* 失敗 */
        DEBUG_LOG_ERR(
            "LibMvfsSendVfsReadyNtc(): ret=%d, err=0x%X",
            retLibMvfs,
            err
        );
    }

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
/**
 * @brief       VfsWrite応答送信
 * @details     MVFSにVfsWrite応答を送信する。
 *
 * @param[in]   globalFd グローバルFD
 * @param[in]   result   処理結果
 *                  - LIBMVFS_RET_SUCCESS 成功
 *                  - LIBMVFS_RET_FAILURE 失敗
 * @param[in]   ready    レディ状態
 *                  - 0                非レディ
 *                  - MVFS_READY_WRITE 書込レディ
 * @param[in]   size     書込サイズ
 */
/******************************************************************************/
static void SendVfsWriteResp( uint32_t globalFd,
                              uint32_t result,
                              uint32_t ready,
                              size_t   size      )
{
    uint32_t     err;           /* LibMvfs関数エラー */
    LibMvfsRet_t retLibMvfs;    /* LibMvfs関数戻り値 */

    DEBUG_LOG_FNC(
        "%s(): start. globalFd=%u, result=%d, size=%u",
        __func__,
        globalFd,
        result,
        size
    );

    /* 初期化 */
    err        = LIBMVFS_ERR_NONE;
    retLibMvfs = LIBMVFS_RET_FAILURE;

    DEBUG_LOG_TRC(
        "%s(): globalFd=%u, result=%d, ready=%#X, size=%u",
        __func__,
        globalFd,
        result,
        ready,
        size
    );

    /* VfsWrite応答送信 */
    retLibMvfs = LibMvfsSendVfsWriteResp( globalFd,
                                          result,
                                          ready,
                                          size,
                                          &err      );

    /* 送信結果判定 */
    if ( retLibMvfs != LIBMVFS_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR(
            "LibMvfsSendVfsWriteResp(): ret=%d, err=0x%X",
            retLibMvfs,
            err
        );
    }

    DEBUG_LOG_FNC( "%s(): end.", __func__ );
    return;
}


/******************************************************************************/
