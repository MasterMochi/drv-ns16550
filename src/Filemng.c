/******************************************************************************/
/*                                                                            */
/* src/Filemng.c                                                              */
/*                                                                 2020/07/19 */
/* Copyright (C) 2020 Mochi.                                                  */
/*                                                                            */
/******************************************************************************/
/******************************************************************************/
/* インクルード                                                               */
/******************************************************************************/
/* 標準ヘッダ */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ライブラリヘッダ */
#include <libmvfs.h>
#include <kernel/types.h>
#include <MLib/MLibState.h>
#include <MLib/MLibSpin.h>

/* モジュール内ヘッダ */
#include "config.h"
#include "ns16550.h"
#include "Bufmng.h"
#include "Debug.h"
#include "Filemng.h"
#include "Txctrl.h"


/******************************************************************************/
/* 定義                                                                       */
/******************************************************************************/
/* 状態 */
#define STATE_INIT   ( 1 )      /**< 未open状態(初期状態) */
#define STATE_OPENED ( 2 )      /**< open中状態           */

/* イベント */
#define EVENT_VFSOPEN  ( 1 )    /**< open要求イベント  */
#define EVENT_VFSREAD  ( 2 )    /**< read要求イベント  */
#define EVENT_VFSWRITE ( 3 )    /**< write要求イベント */
#define EVENT_VFSCLOSE ( 4 )    /**< close要求イベント */

/* VfsCloseイベントパラメータ */
typedef struct {
    NS16550ComNo_t comNo;       /**< デバイス識別番号 */
    uint32_t       globalFD;    /**< グローバルFD     */
} paramVfsClose_t;

/* VfsOpenイベントパラメータ */
typedef struct {
    NS16550ComNo_t comNo;       /**< デバイス識別番号 */
    MkPid_t        pid;         /**< 要求元PID        */
    uint32_t       globalFD;    /**< グローバルFD     */
} paramVfsOpen_t;

/* VfsReadイベントパラメータ */
typedef struct {
    NS16550ComNo_t comNo;       /**< デバイス識別番号   */
    uint32_t       globalFD;    /**< グローバルFD       */
    uint64_t       readIdx;     /**< 読込みインデックス */
    size_t         size;        /**< 読込みサイズ       */
} paramVfsRead_t;

/* VfsWriteイベントパラメータ */
typedef struct {
    NS16550ComNo_t comNo;       /**< デバイス識別番号   */
    uint32_t       globalFD;    /**< グローバルFD       */
    uint64_t       writeIdx;    /**< 書込みインデックス */
    uint8_t        *pBuffer;    /**< 書込みバッファ     */
    size_t         size;        /**< 書込みサイズ       */
} paramVfsWrite_t;


/******************************************************************************/
/* ローカル関数宣言                                                           */
/******************************************************************************/
/* グローバルFDデバイス識別番号変換 */
static NS16550ComNo_t ConvertGlobalFD( uint32_t globalFD );
/* デバイスファイル作成 */
static void CreateFile( char *pPath );
/* 状態遷移タスク */
static MLibStateNo_t DoTask11( void *pArg );
static MLibStateNo_t DoTask12( void *pArg );
static MLibStateNo_t DoTask22( void *pArg );
static MLibStateNo_t DoTask32( void *pArg );
static MLibStateNo_t DoTask42( void *pArg );
/* デバイスファイルclose要求 */
static void DoVfsClose( uint32_t globalFD );
/* デバイスファイルopen要求 */
static void DoVfsOpen( MkPid_t    pid,
                       uint32_t   globalFD,
                       const char *pPath    );
/* デバイスファイルread要求 */
static void DoVfsRead( uint32_t globalFD,
                       uint64_t readIdx,
                       size_t   size      );
/* デバイスファイルwrite要求 */
static void DoVfsWrite( uint32_t globalFD,
                        uint64_t writeIdx,
                        void     *pBuffer,
                        size_t   size      );
/* デバイスファイル毎初期化 */
static void Init( NS16550ComNo_t comNo );
/* デバイスファイルclose応答送信 */
static void SendVfsCloseResp( uint32_t globalFD,
                              uint32_t result    );
/* デバイスファイルopen応答送信 */
static void SendVfsOpenResp( uint32_t globalFD,
                             uint32_t result    );
/* デバイスファイルread応答送信 */
static void SendVfsReadResp( uint32_t globalFD,
                             uint32_t result,
                             uint32_t ready,
                             void     *pBuffer,
                             size_t   size      );
/* 読書レディ状態通知送信 */
static void SendVfsReadyNtc( const char *pPath,
                             uint32_t   ready   );
/* デバイスファイルwrite応答送信 */
static void SendVfsWriteResp( uint32_t globalFD,
                              uint32_t result,
                              uint32_t ready,
                              size_t   size      );


/******************************************************************************/
/* 静的グローバル変数宣言                                                     */
/******************************************************************************/
/** デバイスファイルパス */
static char *gpPath[ NS16550_COM_NUM ] = { CONFIG_FILEPATH_SERIAL1,
                                           CONFIG_FILEPATH_SERIAL2  };
/** スピンロック */
static MLibSpin_t gLock[ NS16550_COM_NUM ];
/** 状態遷移 */
static MLibState_t gState[ NS16550_COM_NUM ];
/** 状態遷移表 */
static const MLibStateTransition_t gStt[] =
    {
        /*-------------+----------------+----------+----------------------*/
        /* 状態        | イベント       | タスク   | 遷移先状態           */
        /*-------------+----------------+----------+----------------------*/
        { STATE_INIT   , EVENT_VFSOPEN  , DoTask11 , { STATE_OPENED , 0 } },
        /*-------------+----------------+----------+----------------------*/
        { STATE_OPENED , EVENT_VFSOPEN  , DoTask12 , { STATE_OPENED , 0 } },
        { STATE_OPENED , EVENT_VFSREAD  , DoTask22 , { STATE_OPENED , 0 } },
        { STATE_OPENED , EVENT_VFSWRITE , DoTask32 , { STATE_OPENED , 0 } },
        { STATE_OPENED , EVENT_VFSCLOSE , DoTask42 , { STATE_INIT   , 0 } }
        /*-------------+----------------+----------+----------------------*/
    };
/** グローバルFD */
static uint32_t gGlobalFD[ NS16550_COM_NUM ];
/** レディ通知状態更新 */
static uint32_t gReady[ NS16550_COM_NUM ];


/******************************************************************************/
/* グローバル関数定義                                                         */
/******************************************************************************/
/******************************************************************************/
/**
 * @brief       デバイスファイル管理初期化
 * @details     デバイスファイル毎に初期化を行う。
 */
/******************************************************************************/
void FilemngInit( void )
{
    /* 初期化 */
    Init( NS16550_COM1 );
    Init( NS16550_COM2 );

    return;
}


/******************************************************************************/
/**
 * @brief       読込レディ状態更新
 * @details     読込レディ未通知の場合は読込レディ状態であることを通知するため
 *              に読書レディ状態通知を行う。
 *
 * @param[in]   comNo デバイス識別番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 */
/******************************************************************************/
void FilemngUpdateReadyRead( NS16550ComNo_t comNo )
{
    /* スピンロック */
    MLibSpinLock( &( gLock[ comNo ] ), NULL );

    /* レディ通知状態判定 */
    if ( ( gReady[ comNo ] & MVFS_READY_READ ) == 0 ) {
        /* 読込レディ未通知 */

        /* レディ通知状態更新 */
        gReady[ comNo ] |= MVFS_READY_READ;

        /* 読書レディ状態通知 */
        SendVfsReadyNtc( gpPath[ comNo ], gReady[ comNo ] );
    }

    /* スピンアンロック */
    MLibSpinUnlock( &( gLock[ comNo ] ), NULL );

    return;
}


/******************************************************************************/
/**
 * @brief       書込レディ状態更新
 * @details     書込レディ未通知の場合は書込レディ状態であることを通知するため
 *              に読書レディ状態通知を行う。
 *
 * @param[in]   comNo デバイス識別番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 */
/******************************************************************************/
void FilemngUpdateReadyWrite( NS16550ComNo_t comNo )
{
    /* スピンロック */
    MLibSpinLock( &( gLock[ comNo ] ), NULL );

    /* レディ通知状態判定 */
    if ( ( gReady[ comNo ] & MVFS_READY_WRITE ) == 0 ) {
        /* 書込レディ未通知 */

        /* レディ通知状態更新 */
        gReady[ comNo ] |= MVFS_READY_WRITE;

        /* 読書レディ状態通知 */
        SendVfsReadyNtc( gpPath[ comNo ], gReady[ comNo ] );
    }

    /* スピンアンロック */
    MLibSpinUnlock( &( gLock[ comNo ] ), NULL );

    return;
}


/******************************************************************************/
/**
 * @brief       デバイスファイル操作要求待ち合わせ
 * @details     スケジューラを起動してデバイスファイル操作要求を待ち合わせる。
 */
/******************************************************************************/
void FilemngWaitRequest( void )
{
    LibMvfsErr_t       errLibMvfs;  /* LibMvfsエラー要因 */
    LibMvfsRet_t       retLibMvfs;  /* LibMvfs戻り値     */
    LibMvfsSchedInfo_t schedInfo;   /* スケジューラ情報  */

    /* 初期化 */
    errLibMvfs = LIBMVFS_ERR_NONE;
    retLibMvfs = LIBMVFS_RET_FAILURE;
    memset( &schedInfo, 0, sizeof ( schedInfo ) );

    /* スケジュール情報設定 */
    schedInfo.callBack.pVfsOpen  = &DoVfsOpen;
    schedInfo.callBack.pVfsWrite = &DoVfsWrite;
    schedInfo.callBack.pVfsRead  = &DoVfsRead;
    schedInfo.callBack.pVfsClose = &DoVfsClose;
    schedInfo.callBack.pOther    = NULL;

    /* スケジューラ起動 */
    retLibMvfs = LibMvfsSchedStart( &schedInfo, &errLibMvfs );

    /* 起動結果判定 */
    if ( retLibMvfs != LIBMVFS_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR(
            "LibMvfsSchedStart(): ret=%X, err=%X",
            retLibMvfs,
            errLibMvfs
        );
    }

    return;
}


/******************************************************************************/
/* ローカル関数定義                                                           */
/******************************************************************************/
/******************************************************************************/
/**
 * @brief       グローバルFDデバイス識別番号変換
 * @details     デバイス識別番号毎に記録してあるグローバルFDに該当するデバイス
 *              識別番号を返す。
 *
 * @param[in]   globalFD グローバルFD
 *
 * @return      デバイス識別番号を返す。
 * @retval      NS16550_COM1     COM1
 * @retval      NS16550_COM2     COM2
 * @retval      NS16550_COM_NULL 変換失敗
 */
/******************************************************************************/
static NS16550ComNo_t ConvertGlobalFD( uint32_t globalFD )
{
    NS16550ComNo_t comNo;   /* デバイス識別番号 */

    /* デバイス識別番号毎に繰り返し */
    for ( comNo = NS16550_COM_MIN; comNo <= NS16550_COM_MAX; comNo++ ) {
        /* グローバルFD比較 */
        if ( gGlobalFD[ comNo ] == globalFD ) {
            /* 一致 */

            return comNo;
        }
    }

    return NS16550_COM_NULL;
}


/******************************************************************************/
/**
 * @brief       デバイスファイル作成
 * @details     ファイルをマウントしてデバイスファイルを作成する。
 *
 * @param[in]   *pPath ファイルパス
 */
/******************************************************************************/
static void CreateFile( char *pPath )
{
    LibMvfsErr_t errLibMvfs;    /* LibMvfsエラー要因 */
    LibMvfsRet_t retLibMvfs;    /* LibMvfs戻り値     */

    /* 初期化 */
    errLibMvfs = LIBMVFS_ERR_NONE;
    retLibMvfs = LIBMVFS_RET_FAILURE;

    DEBUG_LOG_TRC( "%s(): path=%s", __func__, pPath );

    /* マウント */
    retLibMvfs = LibMvfsMount( pPath, &errLibMvfs );

    /* 結果判定 */
    if ( retLibMvfs != LIBMVFS_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR(
            "LibMvfsMount(): ret=%X, err=%X, path=%s",
            retLibMvfs,
            errLibMvfs,
            pPath
        );
    }

    return;
}


/******************************************************************************/
/**
 * @brief       状態遷移Task11
 * @details     グローバルFDの記録と受信バッファをクリアし、デバイスファイル
 *              open応答(成功)を行う。
 *
 * @param[in]   *pArg パラメータ
 *
 * @return      状態遷移先を返す。
 * @retval      STATE_OPENED open中状態
 */
/******************************************************************************/
static MLibStateNo_t DoTask11( void *pArg )
{
    paramVfsOpen_t *pParam; /* パラメータ */

    /* 初期化 */
    pParam = ( paramVfsOpen_t * ) pArg;

    /* グローバルFD記録 */
    gGlobalFD[ pParam->comNo ] = pParam->globalFD;

    /* 受信バッファクリア */
    BufmngClear( pParam->comNo, BUFMNG_ID_RX );

    /* デバイスファイルopen応答(成功) */
    SendVfsOpenResp( pParam->globalFD, LIBMVFS_RET_SUCCESS );

    return STATE_OPENED;
}


/******************************************************************************/
/**
 * @brief       状態遷移Task12
 * @details     デバイスファイルopen応答(失敗)を行う。
 *
 * @param[in]   *pArg パラメータ
 *
 * @return      状態遷移先を返す。
 * @retval      STATE_OPENED open中状態
 */
/******************************************************************************/
static MLibStateNo_t DoTask12( void *pArg )
{
    paramVfsOpen_t *pParam; /* パラメータ */

    /* 初期化 */
    pParam = ( paramVfsOpen_t * ) pArg;

    /* デバイスファイルopen応答(失敗) */
    SendVfsOpenResp( pParam->globalFD, LIBMVFS_RET_FAILURE );

    return STATE_OPENED;
}


/******************************************************************************/
/**
 * @brief       状態遷移Task22
 * @details     バッファからデータを取り出し、デバイスファイルread応答を行う。
 *              本処理はスピンロックにより排他制御する。
 *
 * @param[in]   *pArg パラメータ
 *
 * @return      状態遷移先を返す。
 * @retval      STATE_OPENED open中状態
 */
/******************************************************************************/
static MLibStateNo_t DoTask22( void *pArg )
{
    size_t         size;        /* 読込み実施サイズ */
    uint8_t        *pBuffer;    /* バッファ         */
    paramVfsRead_t *pParam;     /* パラメータ       */

    /* 初期化 */
    size    = 0;
    pBuffer = 0;
    pParam  = ( paramVfsRead_t * ) pArg;

    /* スピンロック */
    MLibSpinLock( &( gLock[ pParam->comNo ] ), NULL );

    /* バッファ領域確保 */
    pBuffer = malloc( pParam->size );

    /* 確保結果判定 */
    if ( pBuffer == NULL ) {
        /* 失敗 */

        DEBUG_LOG_ERR(
            "No resource: comNo=%u, globalFD=%u, size=%u",
            pParam->comNo,
            pParam->globalFD,
            pParam->size
        );

        /* デバイスファイルread応答(失敗) */
        SendVfsReadResp( pParam->globalFD,
                         LIBMVFS_RET_FAILURE,
                         MVFS_READY_READ,
                         NULL,
                         0                    );

        /* レディ通知状態更新 */
        gReady[ pParam->comNo ] |= MVFS_READY_READ;

    } else {
        /* 成功 */

        /* バッファ取出 */
        size = BufmngRead( pParam->comNo, BUFMNG_ID_RX, pBuffer, pParam->size );

        /* 取出結果判定 */
        if ( size == pParam->size ) {
            /* 要求サイズ読込み成功 */

            /* デバイスファイルread応答(成功) */
            SendVfsReadResp( pParam->globalFD,
                             LIBMVFS_RET_SUCCESS,
                             MVFS_READY_READ,
                             pBuffer,
                             size                 );

            /* レディ通知状態更新 */
            gReady[ pParam->comNo ] |= MVFS_READY_READ;

        } else if ( size != 0 ) {
            /* 一部読込み成功 */

            /* デバイスファイルread応答(成功) */
            SendVfsReadResp( pParam->globalFD,
                             LIBMVFS_RET_SUCCESS,
                             0,
                             pBuffer,
                             size                 );

            /* レディ通知状態更新 */
            gReady[ pParam->comNo ] &= ~MVFS_READY_READ;

        } else {
            /* 読込み失敗 */

            /* デバイスファイルread応答(失敗) */
            SendVfsReadResp( pParam->globalFD,
                             LIBMVFS_RET_FAILURE,
                             0,
                             NULL,
                             0                    );

            /* レディ通知状態更新 */
            gReady[ pParam->comNo ] &= ~MVFS_READY_READ;
        }

        /* バッファ領域解放 */
        free( pBuffer );
    }

    /* スピンアンロック */
    MLibSpinUnlock( &( gLock[ pParam->comNo ] ), NULL );

    return STATE_OPENED;
}


/******************************************************************************/
/**
 * @brief       状態遷移Task32
 * @details     バッファにデータを追加し、デバイスファイルwrite応答を行う。ま
 *              た、転送要求を行う。本処理はスピンロックにより排他制御する。
 *
 * @param[in]   *pArg パラメータ
 *
 * @return      状態遷移先を返す。
 * @retval      STATE_OPENED open中状態
 */
/******************************************************************************/
static MLibStateNo_t DoTask32( void *pArg )
{
    size_t          size;       /* 書込み実施サイズ */
    paramVfsWrite_t *pParam;    /* パラメータ       */

    /* 初期化 */
    size   = 0;
    pParam = ( paramVfsWrite_t * ) pArg;

    /* スピンロック */
    MLibSpinLock( &( gLock[ pParam->comNo ] ), NULL );

    /* バッファ追加 */
    size = BufmngWrite( pParam->comNo,
                        BUFMNG_ID_TX,
                        pParam->pBuffer,
                        pParam->size     );

    /* 追加サイズ判定 */
    if ( size == pParam->size ) {
        /* 要求サイズ分全追加 */

        /* デバイスファイルwrite応答(成功) */
        SendVfsWriteResp( pParam->globalFD,
                          LIBMVFS_RET_SUCCESS,
                          MVFS_READY_WRITE,
                          size                 );

        /* レディ通知状態更新 */
        gReady[ pParam->comNo ] |= MVFS_READY_WRITE;

    } else if ( size != 0 ) {
        /* 一部追加 */

        /* デバイスファイルwrite応答(成功) */
        SendVfsWriteResp( pParam->globalFD,
                          LIBMVFS_RET_SUCCESS,
                          0,
                          size                 );

        /* レディ通知状態更新 */
        gReady[ pParam->comNo ] &= ~MVFS_READY_WRITE;

    } else {
        /* 追加失敗 */

        /* デバイスファイルwrite応答(失敗) */
        SendVfsWriteResp( pParam->globalFD, LIBMVFS_RET_FAILURE, 0, 0 );

        /* レディ通知状態更新 */
        gReady[ pParam->comNo ] &= ~MVFS_READY_WRITE;
    }

    /* スピンアンロック */
    MLibSpinUnlock( &( gLock[ pParam->comNo ] ), NULL );

    /* 転送要求 */
    TxctrlRequest( pParam->comNo );

    return STATE_OPENED;
}


/******************************************************************************/
/**
 * @brief       状態遷移Task42
 * @details     デバイスファイルclose応答(成功)を行い、記録していたグローバルFD
 *              を削除する。
 *
 * @param[in]   *pArg パラメータ
 *
 * @return      状態遷移先を返す。
 * @retval      STATE_INIT 未open状態(初期状態)
 */
/******************************************************************************/
static MLibStateNo_t DoTask42( void *pArg )
{
    paramVfsClose_t *pParam;    /* パラメータ */

    /* 初期化 */
    pParam = ( paramVfsClose_t * ) pArg;

    /* デバイスファイルclose応答(成功) */
    SendVfsCloseResp( pParam->globalFD, LIBMVFS_RET_SUCCESS );

    /* グローバルFD削除 */
    gGlobalFD[ pParam->comNo ] = 0;

    /* レディ通知状態初期化 */
    gReady[ pParam->comNo ] = 0;

    return STATE_INIT;
}


/******************************************************************************/
/**
 * @brief       デバイスファイルclose要求
 * @details     グローバルFDから対象デバイスを識別し状態遷移を実行する。不正な
 *              (未openな)グローバルFDの場合はデバイスファイルclose応答(失敗)を
 *              送信する。
 *
 * @param[in]   globalFD グローバルFD
 */
/******************************************************************************/
static void DoVfsClose( uint32_t globalFD )
{
    MLibErr_t       errMLib;    /* MLibエラー要因   */
    MLibRet_t       retMLib;    /* MLib関数戻り値   */
    MLibStateNo_t   prevState;  /* 遷移前状態       */
    MLibStateNo_t   nextState;  /* 遷移後状態       */
    NS16550ComNo_t  comNo;      /* デバイス識別番号 */
    paramVfsClose_t param;      /* パラメータ       */

    /* 初期化 */
    errMLib   = MLIB_ERR_NONE;
    retMLib   = MLIB_RET_FAILURE;
    prevState = MLIB_STATE_NULL;
    nextState = MLIB_STATE_NULL;
    comNo     = NS16550_COM_NULL;
    memset( &param, 0, sizeof ( paramVfsClose_t ) );

    /* グローバルFDデバイス識別番号変換 */
    comNo = ConvertGlobalFD( globalFD );

    DEBUG_LOG_TRC( "%s(): comNo=%d, globalFD=%d", __func__, comNo, globalFD );

    /* 変換結果判定 */
    if ( comNo == NS16550_COM_NULL ) {
        /* 不正 */

        DEBUG_LOG_ERR( "Invalaid globalFD: %u", globalFD );

        /* デバイスファイルclose応答(失敗) */
        SendVfsCloseResp( globalFD, LIBMVFS_RET_FAILURE );

        return;
    }

    /* パラメータ設定 */
    param.comNo    = comNo;
    param.globalFD = globalFD;

    /* 状態遷移実行 */
    retMLib = MLibStateExec( &( gState[ comNo ] ),
                             EVENT_VFSCLOSE,
                             &param,
                             &prevState,
                             &nextState,
                             &errMLib              );

    /* 実行結果判定 */
    if ( retMLib != MLIB_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "MLibStateExec(): ret=%X, err=%X", retMLib, errMLib );

        /* デバイスファイルclose応答(失敗) */
        SendVfsCloseResp( globalFD, LIBMVFS_RET_FAILURE );
    }

    DEBUG_LOG_TRC(
        "%s(): state chg. %d -> %d.",
        __func__,
        prevState,
        nextState
    );

    return;
}


/******************************************************************************/
/**
 * @brief       デバイスファイルopen要求
 * @details     ファイルパスから対象デバイスを識別し状態遷移を実行する。不正な
 *              ファイルパスの場合はデバイスファイルopen応答(失敗)を送信する。
 *
 * @param[in]   pid      要求元プロセスID
 * @param[in]   globalFD グローバルFD
 * @param[in]   *pPath   ファイルパス
 */
/******************************************************************************/
static void DoVfsOpen( MkPid_t    pid,
                       uint32_t   globalFD,
                       const char *pPath    )
{
    MLibErr_t      errMLib;     /* MLibエラー要因   */
    MLibRet_t      retMLib;     /* MLib戻り値       */
    MLibStateNo_t  prevState;   /* 遷移前状態       */
    MLibStateNo_t  nextState;   /* 遷移後状態       */
    NS16550ComNo_t comNo;       /* デバイス識別番号 */
    paramVfsOpen_t param;       /* パラメータ       */

    /* 初期化 */
    errMLib   = MLIB_ERR_NONE;
    retMLib   = MLIB_RET_FAILURE;
    prevState = MLIB_STATE_NULL;
    nextState = MLIB_STATE_NULL;
    comNo     = NS16550_COM_NULL;
    memset( &param, 0, sizeof ( param ) );

    /* パス比較 */
    if ( strcmp( CONFIG_FILEPATH_SERIAL1, pPath ) == 0 ) {
        /* serial1 */
        comNo = NS16550_COM1;

    } else if ( strcmp( CONFIG_FILEPATH_SERIAL2, pPath ) == 0 ) {
        /* serial2 */
        comNo = NS16550_COM2;

    } else {
        /* 不正 */

        DEBUG_LOG_ERR( "Invalid path: %s", pPath );

        /* デバイスファイルopen応答(失敗) */
        SendVfsOpenResp( globalFD, LIBMVFS_RET_FAILURE );

        return;
    }

    DEBUG_LOG_TRC(
        "%s(): comNo=%d, pid=0x%X, globalFD=%d, path=%s",
        __func__,
        comNo,
        pid,
        globalFD,
        pPath
    );

    /* パラメータ設定 */
    param.comNo    = comNo;
    param.pid      = pid;
    param.globalFD = globalFD;

    /* 状態遷移実行 */
    retMLib = MLibStateExec( &( gState[ comNo ] ),
                             EVENT_VFSOPEN,
                             &param,
                             &prevState,
                             &nextState,
                             &errMLib              );

    /* 実行結果判定 */
    if ( retMLib != MLIB_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "MLibStateExec(): ret=%X, err=%X", retMLib, errMLib );

        /* デバイスファイルopen応答(失敗) */
        SendVfsOpenResp( globalFD, LIBMVFS_RET_FAILURE );
    }

    DEBUG_LOG_TRC(
        "%s(): state chg. %d -> %d.",
        __func__,
        prevState,
        nextState
    );

    return;
}


/******************************************************************************/
/**
 * @brief       デバイスファイルread要求
 * @details     グローバルFDから対象デバイスを識別し状態遷移を実行する。不正な
 *              (未openな)グローバルFDの場合はデバイスファイルread応答(失敗)を
 *              送信する。
 *
 * @param[in]   globalFD グローバルFD
 * @param[in]   readIdx  読込みインデックス
 * @param[in]   size     読込みサイズ
 */
/******************************************************************************/
static void DoVfsRead( uint32_t globalFD,
                       uint64_t readIdx,
                       size_t   size      )
{
    MLibErr_t      errMLib;     /* MLibエラー要因   */
    MLibRet_t      retMLib;     /* MLib関数戻り値   */
    MLibStateNo_t  prevState;   /* 遷移前状態       */
    MLibStateNo_t  nextState;   /* 遷移後状態       */
    NS16550ComNo_t comNo;       /* デバイス識別番号 */
    paramVfsRead_t param;       /* パラメータ       */

    /* 初期化 */
    errMLib   = MLIB_ERR_NONE;
    retMLib   = MLIB_RET_FAILURE;
    prevState = MLIB_STATE_NULL;
    nextState = MLIB_STATE_NULL;
    comNo     = NS16550_COM_NULL;
    memset( &param, 0, sizeof ( paramVfsRead_t ) );

    /* グローバルFDデバイス識別番号変換 */
    comNo = ConvertGlobalFD( globalFD );

    DEBUG_LOG_TRC(
        "%s(): comNo=%d, globalFD=%d, readIdx=%u, size=%u",
        __func__,
        comNo,
        globalFD,
        ( uint32_t ) readIdx,
        size
    );

    /* 変換結果判定 */
    if ( comNo == NS16550_COM_NULL ) {
        /* 不正 */

        DEBUG_LOG_ERR( "Invalaid globalFD: %u", globalFD );

        /* デバイスファイルread応答(失敗) */
        SendVfsReadResp( globalFD, LIBMVFS_RET_FAILURE, 0, NULL, 0 );

        return;
    }

    /* パラメータ設定 */
    param.comNo    = comNo;
    param.globalFD = globalFD;
    param.readIdx  = readIdx;
    param.size     = size;

    /* 状態遷移実行 */
    retMLib = MLibStateExec( &( gState[ comNo ] ),
                             EVENT_VFSREAD,
                             &param,
                             &prevState,
                             &nextState,
                             &errMLib              );

    /* 実行結果判定 */
    if ( retMLib != MLIB_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "MLibStateExec(): ret=%X, err=%X", retMLib, errMLib );

        /* デバイスファイルread応答(失敗) */
        SendVfsReadResp( globalFD, LIBMVFS_RET_FAILURE, 0, NULL, 0 );
    }

    DEBUG_LOG_TRC(
        "%s(): state chg. %d -> %d.",
        __func__,
        prevState,
        nextState
    );

    return;
}


/******************************************************************************/
/**
 * @brief       デバイスファイルwrite要求
 * @details     グローバルFDから対象デバイスを識別し状態遷移を実行する。不正な
 *              (未openな)グローバルFDの場合はデバイスファイルwrite応答(失敗)を
 *              送信する。
 *
 * @param[in]   globalFD グローバルFD
 * @param[in]   writeIdx 書込みインデックス
 * @param[in]   *pBuffer 書込みバッファ
 * @param[in]   size     書込みサイズ
 */
/******************************************************************************/
static void DoVfsWrite( uint32_t globalFD,
                        uint64_t writeIdx,
                        void     *pBuffer,
                        size_t   size      )
{
    MLibErr_t       errMLib;    /* MLibエラー要因   */
    MLibRet_t       retMLib;    /* MLib関数戻り値   */
    MLibStateNo_t   prevState;  /* 遷移前状態       */
    MLibStateNo_t   nextState;  /* 遷移後状態       */
    NS16550ComNo_t  comNo;      /* デバイス識別番号 */
    paramVfsWrite_t param;      /* パラメータ       */

    /* 初期化 */
    errMLib   = MLIB_ERR_NONE;
    retMLib   = MLIB_RET_FAILURE;
    prevState = MLIB_STATE_NULL;
    nextState = MLIB_STATE_NULL;
    comNo     = NS16550_COM_NULL;
    memset( &param, 0, sizeof ( paramVfsWrite_t ) );

    /* グローバルFDデバイス識別番号変換 */
    comNo = ConvertGlobalFD( globalFD );

    DEBUG_LOG_TRC(
        "%s(): comNo=%d, globalFD=%d, writeIdx=%u, size=%u",
        __func__,
        comNo,
        globalFD,
        ( uint32_t ) writeIdx,
        size
    );

    /* 変換結果判定 */
    if ( comNo == NS16550_COM_NULL ) {
        /* 不正 */

        DEBUG_LOG_ERR( "Invalaid globalFD: %u", globalFD );

        /* デバイスファイルwrite応答(失敗) */
        SendVfsWriteResp( globalFD, LIBMVFS_RET_FAILURE, 0, 0 );

        return;
    }

    /* パラメータ設定 */
    param.comNo    = comNo;
    param.globalFD = globalFD;
    param.writeIdx = writeIdx;
    param.pBuffer  = pBuffer;
    param.size     = size;

    /* 状態遷移実行 */
    retMLib = MLibStateExec( &( gState[ comNo ] ),
                             EVENT_VFSWRITE,
                             &param,
                             &prevState,
                             &nextState,
                             &errMLib              );

    /* 実行結果判定 */
    if ( retMLib != MLIB_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR( "MLibStateExec(): ret=%X, err=%X", retMLib, errMLib );

        /* デバイスファイルwrite応答(失敗) */
        SendVfsWriteResp( globalFD, LIBMVFS_RET_FAILURE, 0, 0 );
    }

    DEBUG_LOG_TRC(
        "%s(): state chg. %d -> %d.",
        __func__,
        prevState,
        nextState
    );

    return;

}


/******************************************************************************/
/**
 * @brief       デバイスファイル毎初期化
 * @details     デバイスファイル毎にスピンロック、状態遷移、デバイスファイルを
 *              初期化する。
 *
 * @param[in]   comNo  デバイス識別番号
 *                  - NS16550_COM1 COM1
 *                  - NS16550_COM2 COM2
 */
/******************************************************************************/
static void Init( NS16550ComNo_t comNo )
{
    /* スピンロック初期化 */
    MLibSpinInit( &( gLock[ comNo ] ), NULL );

    /* 状態遷移初期化 */
    MLibStateInit( &( gState[ comNo ] ),
                   gStt,
                   sizeof ( gStt ),
                   STATE_INIT,
                   NULL                  );

    /* デバイスファイル作成 */
    CreateFile( gpPath[ comNo ] );

    /* グローバルFD初期化 */
    gGlobalFD[ comNo ] = 0;

    /* レディ通知状態初期化 */
    gReady[ comNo ] = 0;

    return;
}


/******************************************************************************/
/**
 * @brief       デバイスファイルclose応答送信
 * @details     mvfsにVfsClose応答を送信する。
 *
 * @param[in]   globalFD グローバルFD
 * @param[in]   result   処理結果
 *                  - LIBMVFS_RET_SUCCESS 成功
 *                  - LIBMVFS_RET_FAILURE 失敗
 */
/******************************************************************************/
static void SendVfsCloseResp( uint32_t globalFD,
                              uint32_t result    )
{
    LibMvfsErr_t errLibMvfs;    /* Libmvfsエラー要因 */
    LibMvfsRet_t retLibMvfs;    /* Libmvfs戻り値     */

    /* 初期化 */
    errLibMvfs = LIBMVFS_ERR_NONE;
    retLibMvfs = LIBMVFS_RET_FAILURE;

    DEBUG_LOG_TRC( "%s(): globalFD=%u, result=%X", __func__, globalFD, result );

    /* VfsClose応答送信 */
    retLibMvfs = LibMvfsSendVfsCloseResp( globalFD, result, &errLibMvfs );

    /* 送信結果判定 */
    if ( retLibMvfs != LIBMVFS_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR(
            "LibMvfsSendVfsCloseResp(): ret=%X, err=%X",
            retLibMvfs,
            errLibMvfs
        );
    }

    return;
}


/******************************************************************************/
/**
 * @brief       デバイスファイルopen応答送信
 * @details     mvfsにVfsOpen応答を送信する。
 *
 * @param[in]   globalFD グローバルFD
 * @param[in]   result   処理結果
 *                  - LIBMVFS_RET_SUCCESS 成功
 *                  - LIBMVFS_RET_FAILURE 失敗
 */
/******************************************************************************/
static void SendVfsOpenResp( uint32_t globalFD,
                             uint32_t result    )
{
    LibMvfsErr_t errLibMvfs;    /* Libmvfsエラー要因 */
    LibMvfsRet_t retLibMvfs;    /* Libmvfs戻り値     */

    /* 初期化 */
    errLibMvfs = LIBMVFS_ERR_NONE;
    retLibMvfs = LIBMVFS_RET_FAILURE;

    DEBUG_LOG_TRC( "%s(): globalFD=%u, result=%X", __func__, globalFD, result );

    /* VfsOpen応答送信 */
    retLibMvfs = LibMvfsSendVfsOpenResp( globalFD, result, &errLibMvfs );

    /* 送信結果判定 */
    if ( retLibMvfs != LIBMVFS_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR(
            "LibMvfsSendVfsOpenResp(): ret=%X, err=%X",
            retLibMvfs,
            errLibMvfs
        );
    }

    return;
}


/******************************************************************************/
/**
 * @brief       デバイスファイルread応答送信
 * @details     mvfsにVfsRead応答を送信する。
 *
 * @param[in]   globalFD グローバルFD
 * @param[in]   result   処理結果
 *                  - LIBMVFS_RET_SUCCESS 成功
 *                  - LIBMVFS_RET_FAILURE 失敗
 * @param[in]   ready    レディ状態
 *                  - 0               非レディ
 *                  - MVFS_READY_READ 読込レディ
 * @param[in]   *pBuffer バッファ
 * @param[in]   size     読込みサイズ
 */
/******************************************************************************/
static void SendVfsReadResp( uint32_t globalFD,
                             uint32_t result,
                             uint32_t ready,
                             void     *pBuffer,
                             size_t   size      )
{
    LibMvfsErr_t errLibMvfs;    /* LibMvfsエラー要因 */
    LibMvfsRet_t retLibMvfs;    /* LibMvfs戻り値     */

    /* 初期化 */
    errLibMvfs = LIBMVFS_ERR_NONE;
    retLibMvfs = LIBMVFS_RET_FAILURE;

    DEBUG_LOG_TRC(
        "%s(): globalFD=%u, result=%X, ready=%X, pBuffer=%p, size=%u",
        __func__,
        globalFD,
        result,
        ready,
        pBuffer,
        size
    );

    /* VfsRead応答送信 */
    retLibMvfs = LibMvfsSendVfsReadResp( globalFD,
                                         result,
                                         ready,
                                         pBuffer,
                                         size,
                                         &errLibMvfs );

    /* 送信結果判定 */
    if ( retLibMvfs != LIBMVFS_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR(
            "LibMvfsSendVfsReadResp(): ret=%X, err=%X",
            retLibMvfs,
            errLibMvfs
        );
    }

    return;
}


/******************************************************************************/
/**
 * @brief       読書レディ状態通知送信
 * @details     mvfsにVfsReadyを送信する。
 *
 * @param[in]   *pPath ファイルパス
 * @param[in]   ready  レディ状態
 *                  - MVFS_READY_READ  読込レディ
 *                  - MVFS_READY_WRITE 書込レディ
 */
/******************************************************************************/
static void SendVfsReadyNtc( const char *pPath,
                             uint32_t   ready   )
{
    LibMvfsErr_t errLibMvfs;    /* LibMvfsエラー要因 */
    LibMvfsRet_t retLibMvfs;    /* LibMvfs戻り値     */

    /* 初期化 */
    errLibMvfs = LIBMVFS_ERR_NONE;
    retLibMvfs = LIBMVFS_RET_FAILURE;

    DEBUG_LOG_TRC( "%s(): pPath=%s, ready=%X", __func__, pPath, ready );

    /* VfsReady応答送信 */
    retLibMvfs = LibMvfsSendVfsReadyNtc( pPath, ready, &errLibMvfs );

    /* 送信結果判定 */
    if ( retLibMvfs != LIBMVFS_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR(
            "LibMvfsSendVfsReadyNtc(): ret=%X, err=%X",
            retLibMvfs,
            errLibMvfs
        );
    }

    return;

}


/******************************************************************************/
/**
 * @brief       デバイスファイルwrite応答送信
 * @details     mvfsにVfsWrite応答を送信する。
 *
 * @param[in]   globalFD グローバルFD
 * @param[in]   result   処理結果
 *                  - LIBMVFS_RET_SUCCESS 成功
 *                  - LIBMVFS_RET_FAILURE 失敗
 * @param[in]   ready    レディ状態
 *                  - 0                非レディ
 *                  - MVFS_READY_WRITE 書込レディ
 * @param[in]   size     書込みサイズ
 */
/******************************************************************************/
static void SendVfsWriteResp( uint32_t globalFD,
                              uint32_t result,
                              uint32_t ready,
                              size_t   size      )
{
    LibMvfsErr_t errLibMvfs;    /* LibMvfsエラー要因 */
    LibMvfsRet_t retLibMvfs;    /* LibMvfs戻り値     */

    /* 初期化 */
    errLibMvfs = LIBMVFS_ERR_NONE;
    retLibMvfs = LIBMVFS_RET_FAILURE;

    DEBUG_LOG_TRC(
        "%s(): globalFD=%u, result=%X, ready=%X, size=%u",
        __func__,
        globalFD,
        result,
        ready,
        size
    );

    /* VfsRead応答送信 */
    retLibMvfs = LibMvfsSendVfsWriteResp( globalFD,
                                          result,
                                          ready,
                                          size,
                                          &errLibMvfs );

    /* 送信結果判定 */
    if ( retLibMvfs != LIBMVFS_RET_SUCCESS ) {
        /* 失敗 */

        DEBUG_LOG_ERR(
            "LibMvfsSendVfsWriteResp(): ret=%X, err=%X",
            retLibMvfs,
            errLibMvfs
        );
    }

    return;
}


/******************************************************************************/
