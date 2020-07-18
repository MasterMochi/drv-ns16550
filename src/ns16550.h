/******************************************************************************/
/*                                                                            */
/* src/ns16550.h                                                              */
/*                                                                 2020/07/16 */
/* Copyright (C) 2019-2020 Mochi.                                             */
/*                                                                            */
/******************************************************************************/
#ifndef NS16550_H
#define NS16550_H
/******************************************************************************/
/* 定義                                                                       */
/******************************************************************************/
/** COM番号 */
typedef int NS16550ComNo_t;

/* COM番号 */
#define NS16550_COM1     ( 0 )                      /**< COM1          */
#define NS16550_COM2     ( 1 )                      /**< COM2          */
#define NS16550_COM_MIN  ( NS16550_COM1 )           /**< COM番号最小値 */
#define NS16550_COM_MAX  ( NS16550_COM2 )           /**< COM番号最大値 */
#define NS16550_COM_NUM  ( NS16550_COM_MAX + 1 )    /**< COMポート数   */
#define NS16550_COM_NULL ( NS16550_COM_NUM )        /**< COM番号無効   */

/* I/Oポートベースアドレス */
#define NS16550_COM1_IOBASE ( 0x03F8 )  /**< I/Oポートベース(COM1) */
#define NS16550_COM2_IOBASE ( 0x02F8 )  /**< I/Oポートベース(COM2) */
#define NS16550_COM3_IOBASE ( 0x03E8 )  /**< I/Oポートベース(COM3) */
#define NS16550_COM4_IOBASE ( 0x02E8 )  /**< I/Oポートベース(COM4) */

/* I/Oポートアドレス */
#define NS16550_RBR ( 0x0000 )  /**< Receiver Buffer Register      */
#define NS16550_THR ( 0x0000 )  /**< Transmit Holding Register     */
#define NS16550_DLL ( 0x0000 )  /**< Divisor Latch (LS)            */
#define NS16550_IER ( 0x0001 )  /**< Interrupt Enable Register     */
#define NS16550_DLM ( 0x0001 )  /**< Divisor Latch (MS)            */
#define NS16550_IIR ( 0x0002 )  /**< Interrupt Identifing Register */
#define NS16550_FCR ( 0x0002 )  /**< Fifo Control Register         */
#define NS16550_LCR ( 0x0003 )  /**< Line Control Register         */
#define NS16550_MCR ( 0x0004 )  /**< Modem Control Register        */
#define NS16550_LSR ( 0x0005 )  /**< Line Status Register          */
#define NS16550_MSR ( 0x0006 )  /**< Modem Status Register         */
#define NS16550_SCR ( 0x0007 )  /**< SCratch pad Register          */

/** 転送バッファサイズ */
#define NS16550_TRANSMIT_BUFFER_SIZE ( 16 )

/** デフォルトディバイザラッチ値 */
#define NS16550_DIVISOR_LATCH_DEFAULT ( 0x000C )


/*---------------------------*/
/* Interrupt Enable Register */
/*---------------------------*/
/* mask */
#define NS16550_IER_RBR ( 0x01 )    /**< IER[0]: データ受信割込み可否フラグ */
#define NS16550_IER_THR ( 0x02 )    /**< IER[1]: THR空割込み可否フラグ      */
#define NS16550_IER_LSR ( 0x04 )    /**< IER[2]: LSR要因割込み可否フラグ    */
#define NS16550_IER_MSR ( 0x08 )    /**< IER[3]: MSR要因割込み可否フラグ    */
#define NS16550_IER_ALL ( 0x0F )    /**< IER有効ビット                      */

/* データ受信割込み可否フラグ */
#define NS16550_IER_RBR_ENABLE  ( 0x01 )    /**< データ受信割込み許可 */
#define NS16550_IER_RBR_DISABLE ( 0x00 )    /**< データ受信割込み禁止 */

/* THR空割込み可否フラグ */
#define NS16550_IER_THR_ENABLE  ( 0x02 )    /**< THR空割込み許可 */
#define NS16550_IER_THR_DISABLE ( 0x00 )    /**< THR空割込み禁止 */

/* LSR要因割込み可否フラグ */
#define NS16550_IER_LSR_ENABLE  ( 0x04 )    /**< LSR要因割込み許可 */
#define NS16550_IER_LSR_DISABLE ( 0x00 )    /**< LSR要因割込み禁止 */

/* MSR要因割込み可否フラグ */
#define NS16550_IER_MSR_ENABLE  ( 0x08 )    /**< MSR要因割込み許可 */
#define NS16550_IER_MSR_DISABLE ( 0x00 )    /**< MSR要因割込み禁止 */


/*-------------------------------*/
/* Interrupt Identifing Register */
/*-------------------------------*/
/* mask */
#define NS16550_IIR_PENDING      ( 0x01 )   /**< IIR[0]: 割込み保留フラグ */
#define NS16550_IIR_ID           ( 0x0E )   /**< IIR[1-3]: 割込み要因     */
#define NS16550_IIR_FIFO         ( 0xC0 )   /**< IIR[6-7]: FIFO有効フラグ */
#define NS16550_IIR_ALL          ( 0xCF )   /**< IIR有効ビット            */

/* 割込み保留フラグ */
#define NS16550_IIR_PENDING_NO   ( 0x01 )   /**< 割込み保留無し */
#define NS16550_IIR_PENDING_YES  ( 0x00 )   /**< 割込み保留有り */

/* 割込み要因 */
#define NS16550_IIR_ID_MSR       ( 0x00 )   /**< MSR要因割込み                */
#define NS16550_IIR_ID_THR       ( 0x02 )   /**< THR空割込み                  */
#define NS16550_IIR_ID_RBR       ( 0x04 )   /**< データ受信割込み             */
#define NS16550_IIR_ID_LSR       ( 0x06 )   /**< LSR要因割込み                */
#define NS16550_IIR_ID_RBR_TO    ( 0x0C )   /**< データ受信タイムアウト割込み */

/* FIFO有効フラグ */
#define NS16550_IIR_FIFO_ENABLE  ( 0xC0 )   /**< FIFO有効 */
#define NS16550_IIR_FIFO_DISABLE ( 0x00 )   /**< FIFO無効 */


/*-----------------------*/
/* Fifo Control Register */
/*-----------------------*/
/* mask */
#define NS16550_FCR_FIFO          ( 0x01 )  /**< FCR[0]: FIFO有効フラグ     */
#define NS16550_FCR_RXFIFO        ( 0x02 )  /**< FCR[1]: 受信FIFOリセット   */
#define NS16550_FCR_TXFIFO        ( 0x04 )  /**< FCR[2]: 転送FIFOリセット   */
#define NS16550_FCR_TRG           ( 0xC0 )  /**< FCR[6-7]: 受信トリガレベル */
#define NS16550_FCR_ALL           ( 0xC7 )  /**< FCR有効ビット              */

/* FIFO有効フラグ */
#define NS16550_FCR_FIFO_ENABLE   ( 0x01 )  /**< FIFO有効 */
#define NS16550_FCR_FIFO_DISABLE  ( 0x00 )  /**< FIFO無効 */

/* 受信FIFOリセット */
#define NS16550_FCR_RXFIFO_RST    ( 0x02 )  /**< 受信FIFOリセット     */
#define NS16550_FCR_RXFIFO_NORST  ( 0x00 )  /**< 受信FIFOリセット無し */

/* 転送FIFOリセット */
#define NS16550_FCR_TXFIFO_RST    ( 0x04 )  /**< 転送FIFOリセット     */
#define NS16550_FCR_TXFIFO_NORST  ( 0x00 )  /**< 転送FIFOリセット無し */

/* 受信トリガレベル */
#define NS16550_FCR_TRG_1         ( 0x00 )  /**< 受信トリガレベル(1バイト)  */
#define NS16550_FCR_TRG_4         ( 0x40 )  /**< 受信トリガレベル(4バイト)  */
#define NS16550_FCR_TRG_8         ( 0x80 )  /**< 受信トリガレベル(8バイト)  */
#define NS16550_FCR_TRG_14        ( 0xC0 )  /**< 受信トリガレベル(14バイト) */


/*-----------------------*/
/* Line Control Register */
/*-----------------------*/
/* mask */
#define NS16550_LCR_WLS   ( 0x03 )  /**< LCR[0-1]: ワード長               */
#define NS16550_LCR_STB   ( 0x04 )  /**< LCR[2]: ストップビット数         */
#define NS16550_LCR_PEN   ( 0x08 )  /**< LCR[3]: パリティ許可フラグ       */
#define NS16550_LCR_EPS   ( 0x10 )  /**< LCR[4]: パリティ選択フラグ       */
#define NS16550_LCR_STICK ( 0x20 )  /**< LCR[5]: スティックパリティフラグ */
#define NS16550_LCR_BREAK ( 0x40 )  /**< LCR[6]: ブレイク制御フラグ       */
#define NS16550_LCR_DLAB  ( 0x80 )  /**< LCR[7]: ディバイザラッチアクセス */
#define NS16550_LCR_ALL   ( 0xFF )  /**< LCR全有効ビット                  */

/* ワード長 */
#define NS16550_LCR_WLS_5         ( 0x00 )  /**< ワード長(5バイト) */
#define NS16550_LCR_WLS_6         ( 0x01 )  /**< ワード長(6バイト) */
#define NS16550_LCR_WLS_7         ( 0x02 )  /**< ワード長(7バイト) */
#define NS16550_LCR_WLS_8         ( 0x03 )  /**< ワード長(8バイト) */

/* ストップビット数 */
#define NS16550_LCR_STB_2         ( 0x04 )  /**< ストップビット数(1.5/2bit) */
#define NS16550_LCR_STB_1         ( 0x00 )  /**< ストップビット数(1bit)     */

/* パリティ許可フラグ */
#define NS16550_LCR_PEN_ENABLE    ( 0x08 )  /**< パリティ有効 */
#define NS16550_LCR_PEN_DISABLE   ( 0x00 )  /**< パリティ無効 */

/* パリティ選択フラグ */
#define NS16550_LCR_EPS_EVEN      ( 0x10 )  /**< 偶数パリティ */
#define NS16550_LCR_EPS_ODD       ( 0x00 )  /**< 奇数パリティ */

/* スティックパリティフラグ */
#define NS16550_LCR_STICK_ENABLE  ( 0x20 )  /**< スティックパリティ有効 */
#define NS16550_LCR_STICK_DISABLE ( 0x00 )  /**< スティックパリティ無効 */

/* ブレイク制御フラグ */
#define NS16550_LCR_BREAK_ENABLE  ( 0x40 )  /**< ブレイク強制 */
#define NS16550_LCR_BREAK_DISABLE ( 0x00 )  /**< ブレイク無効 */

/* ディバイザラッチアクセス */
#define NS16550_LCR_DLAB_ON       ( 0x80 )  /**< ディバイザラッチアクセス   */
#define NS16550_LCR_DLAB_OFF      ( 0x00 )  /**< 非ディバイザラッチアクセス */


/*------------------------*/
/* Modem Control Register */
/*------------------------*/
/* mask */
#define NS16550_MCR_DTR      ( 0x01 )   /**< LCR[0]: DTRピン出力        */
#define NS16550_MCR_RTS      ( 0x02 )   /**< LCR[1]: RTSピン出力        */
#define NS16550_MCR_OUT1     ( 0x04 )   /**< LCR[2]: OUT1ピン出力       */
#define NS16550_MCR_OUT2     ( 0x08 )   /**< LCR[3]: OUT2ピン出力       */
#define NS16550_MCR_LOOP     ( 0x10 )   /**< LCR[4]: ループバックモード */
#define NS16550_MCR_ALL      ( 0x1F )   /**< MCR全有効ビット            */

/* DTRピン出力 */
#define NS16550_MCR_DTR_L    ( 0x01 )   /**< DTRピン出力(L) */
#define NS16550_MCR_DTR_H    ( 0x00 )   /**< DTRピン出力(H) */

/* RTSピン出力 */
#define NS16550_MCR_RTS_L    ( 0x02 )   /**< RTSピン出力(L) */
#define NS16550_MCR_RTS_H    ( 0x00 )   /**< RTSピン出力(H) */

/* OUT1ピン出力 */
#define NS16550_MCR_OUT1_L   ( 0x04 )   /**< OUT1ピン出力(L) */
#define NS16550_MCR_OUT1_H   ( 0x00 )   /**< OUT1ピン出力(H) */

/* OUT2ピン出力 */
#define NS16550_MCR_OUT2_L   ( 0x08 )   /**< OUT2ピン出力(L) */
#define NS16550_MCR_OUT2_H   ( 0x00 )   /**< OUT2ピン出力(H) */

/* ループバックモード */
#define NS16550_MCR_LOOP_ON  ( 0x10 )   /**< ループバックモードオン */
#define NS16550_MCR_LOOP_OFF ( 0x00 )   /**< ループバックモードオフ */


/*----------------------*/
/* Line Status Register */
/*----------------------*/
/* mask */
#define NS16550_LSR_DR        ( 0x01 ) /**< LSR[0]: 受信データ有無           */
#define NS16550_LSR_OE        ( 0x02 ) /**< LSR[1]: オーバランエラー         */
#define NS16550_LSR_PE        ( 0x04 ) /**< LSR[2]: パリティエラー           */
#define NS16550_LSR_FE        ( 0x08 ) /**< LSR[3]: フレーミングエラー       */
#define NS16550_LSR_BI        ( 0x10 ) /**< LSR[4]: ブレイク割込み           */
#define NS16550_LSR_THRE      ( 0x20 ) /**< LSR[5]: THRステータス            */
#define NS16550_LSR_TEMT      ( 0x40 ) /**< LSR[6]: トランスミッタステータス */
#define NS16550_LSR_RFIFOE    ( 0x80 ) /**< LSR[7]: 受信FIFOエラー           */
#define NS16550_LSR_ALL       ( 0xFF ) /**< LSR全有効ビット                  */

/* 受信データ有無 */
#define NS16550_LSR_DR_YES    ( 0x01 ) /**< 受信データ有り */
#define NS16550_LSR_DR_NO     ( 0x00 ) /**< 受信データ無し */

/* オーバランエラー */
#define NS16550_LSR_OE_YES    ( 0x02 ) /**< オーバランエラー有り */
#define NS16550_LSR_OE_NO     ( 0x00 ) /**< オーバランエラー無し */

/* パリティエラー */
#define NS16550_LSR_PE_YES    ( 0x04 ) /**< パリティエラー有り */
#define NS16550_LSR_PE_NO     ( 0x00 ) /**< パリティエラー無し */

/* フレーミングエラー */
#define NS16550_LSR_FE_YES    ( 0x08 ) /**< フレーミングエラー有り */
#define NS16550_LSR_FE_NO     ( 0x00 ) /**< フレーミングエラー無し */

/* ブレイク割込み */
#define NS16550_LSR_BI_YES    ( 0x10 ) /**< ブレイク信号受信有り */
#define NS16550_LSR_BI_NO     ( 0x00 ) /**< ブレイク信号受信無し */

/* THRステータス */
#define NS16550_LSR_THRE_YES  ( 0x20 )  /**< 転送FIFOデータ無し */
#define NS16550_LSR_THRE_NO   ( 0x00 )  /**< 転送FIFOデータ有り */

/* トランスミッタステータス */
#define NS16550_LSR_TEMT_YES  ( 0x40 )  /**< 転送FIFO/ｼﾌﾄﾚｼﾞｽﾀデータ無し */
#define NS16550_LSR_TEMT_NO   ( 0x00 )  /**< 転送FIFO/ｼﾌﾄﾚｼﾞｽﾀデータ有り */

/* 受信FIFOエラー */
#define NS16550_LSR_RFIFO_YES ( 0x80 )  /**< 受信FIFOエラーデータ有り */
#define NS16550_LSR_RFIFO_NO  ( 0x80 )  /**< 受信FIFOエラーデータ有り */


/*-----------------------*/
/* Modem Status Register */
/*-----------------------*/
/* mask */
#define NS16550_MSR_DCTS     ( 0x01 )   /**< MSR[0]: CTSピン入力変化      */
#define NS16550_MSR_DDSR     ( 0x02 )   /**< MSR[1]: DSRピン入力変化      */
#define NS16550_MSR_TERI     ( 0x04 )   /**< MSR[2]: RIピン入力変化(L->H) */
#define NS16550_MSR_DDCD     ( 0x08 )   /**< MSR[3]: DCDピン入力変化      */
#define NS16550_MSR_CTS      ( 0x10 )   /**< MSR[4]: CTSピン入力          */
#define NS16550_MSR_DSR      ( 0x20 )   /**< MSR[5]: DSRピン入力          */
#define NS16550_MSR_RI       ( 0x40 )   /**< MSR[6]: RIピン入力           */
#define NS16550_MSR_DCD      ( 0x80 )   /**< MSR[7]: DCDピン入力          */

/* CTSピン入力変化 */
#define NS16550_MSR_DCTS_YES ( 0x01 )   /**< CTSピン入力変化有り */
#define NS16550_MSR_DCTS_NO  ( 0x00 )   /**< CTSピン入力変化無し */

/* DSRピン入力変化 */
#define NS16550_MSR_DDSR_YES ( 0x02 )   /**< DSRピン入力変化有り */
#define NS16550_MSR_DDSR_NO  ( 0x00 )   /**< DSRピン入力変化無し */

/* RIピン入力変化(L->H) */
#define NS16550_MSR_TERI_YES ( 0x04 )   /**< RIピン入力変化(L->H)有り */
#define NS16550_MSR_TERI_NO  ( 0x00 )   /**< RIピン入力変化(L->H)無し */

/* DCDピン入力変化 */
#define NS16550_MSR_DDCD_YES ( 0x02 )   /**< DCDピン入力変化有り */
#define NS16550_MSR_DDCD_NO  ( 0x00 )   /**< DCDピン入力変化無し */

/* CTSピン入力 */
#define NS16550_MSR_CTS_L    ( 0x10 )   /**< CTSピン入力(L) */
#define NS16550_MSR_CTS_H    ( 0x00 )   /**< CTSピン入力(H) */

/* DSRピン入力 */
#define NS16550_MSR_DSR_L    ( 0x20 )   /**< DSRピン入力(L) */
#define NS16550_MSR_DSR_H    ( 0x00 )   /**< DSRピン入力(H) */

/* RIピン入力 */
#define NS16550_MSR_RI_L     ( 0x40 )   /**< RIピン入力(L) */
#define NS16550_MSR_RI_H     ( 0x00 )   /**< RIピン入力(H) */

/* DCDピン入力 */
#define NS16550_MSR_DCD_L    ( 0x80 )   /**< DCDピン入力(L) */
#define NS16550_MSR_DCD_H    ( 0x00 )   /**< DCDピン入力(H) */


/******************************************************************************/
#endif
