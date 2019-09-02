/******************************************************************************/
/*                                                                            */
/* src/Debug.h                                                                */
/*                                                                 2019/09/02 */
/* Copyright (C) 2019 Mochi.                                                  */
/*                                                                            */
/******************************************************************************/
#ifndef DEBUG_H
#define DEBUG_H
/******************************************************************************/
/* インクルード                                                               */
/******************************************************************************/
/* 標準ヘッダ */
#include <stdarg.h>
#include <stdbool.h>

/* ライブラリヘッダ */
#include <libmlog.h>


/******************************************************************************/
/* 定義                                                                       */
/******************************************************************************/
/** デバッグログレベル */
#ifndef DEBUG_LOG_LV
    #define DEBUG_LOG_LV ( 0 )
#endif

/* アボート */
#if ( DEBUG_LOG_LV < 1 )
    /** アボート */
    #define DEBUG_ABORT() { while ( true ); }
#else
    /** アボート */
    #define DEBUG_ABORT()                            \
        {                                            \
            LibMlogPut(                              \
                "[ABRT][drv-ns16550][%s:%u] ABORT!", \
                __FILE__,                            \
                __LINE__                             \
            );                                       \
            while ( true );                          \
        }
#endif

/* デバッグログ出力(エラー) */
#if ( DEBUG_LOG_LV < 2 )
    #define DEBUG_LOG_ERR( _FORMAT, ... )
#else
    /** デバッグログ出力(エラー) */
    #define DEBUG_LOG_ERR( _FORMAT, ... )                   \
        LibMlogPut( "[ERR][drv-ns16550][%s:%u] " _FORMAT,   \
                    __FILE__,                               \
                    __LINE__,                               \
                    ##__VA_ARGS__                         )
#endif

/* デバッグログ出力(トレース) */
#if ( DEBUG_LOG_LV < 3 )
    #define DEBUG_LOG_TRC( _FORMAT, ... )
#else
    /** デバッグログ出力(トレース) */
    #define DEBUG_LOG_TRC( _FORMAT, ... )                   \
        LibMlogPut( "[TRC][drv-ns16550][%s:%u] " _FORMAT,   \
                    __FILE__,                               \
                    __LINE__,                               \
                    ##__VA_ARGS__                         )
#endif

/* デバッグログ出力(関数トレース) */
#if ( DEBUG_LOG_LV < 4 )
    #define DEBUG_LOG_FNC( _FORMAT, ... )
#else
    /** デバッグログ出力(関数トレース) */
    #define DEBUG_LOG_FNC( _FORMAT, ... )                   \
        LibMlogPut( "[FNC][drv-ns16550][%s:%u] " _FORMAT,   \
                    __FILE__,                               \
                    __LINE__,                               \
                    ##__VA_ARGS__                         )
#endif


/******************************************************************************/
#endif
