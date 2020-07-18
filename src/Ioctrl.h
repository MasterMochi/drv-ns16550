/******************************************************************************/
/*                                                                            */
/* src/Ioctrl.h                                                               */
/*                                                                 2020/07/18 */
/* Copyright (C) 2019-2020 Mochi.                                             */
/*                                                                            */
/******************************************************************************/
#ifndef IOCTRL_H
#define IOCTRL_H
/******************************************************************************/
/* インクルード                                                               */
/******************************************************************************/
/* 標準ヘッダ */
#include <stddef.h>
#include <stdint.h>

/* モジュール内ヘッダ */
#include "ns16550.h"


/******************************************************************************/
/* グローバル関数宣言                                                         */
/******************************************************************************/
/* 入出力制御初期化 */
extern void IoctrlInit( void );
/* IIR読込み */
extern uint8_t IoctrlInIIR( NS16550ComNo_t comNo );
/* LSR読込み */
extern uint8_t IoctrlInLSR( NS16550ComNo_t comNo );
/* MSR読込み */
extern uint8_t IoctrlInMSR( NS16550ComNo_t comNo );
/* RBR読込み */
extern uint8_t IoctrlInRBR( NS16550ComNo_t comNo );
/* THR書込み */
extern void IoctrlOutTHR( NS16550ComNo_t comNo,
                          uint8_t        *pValue,
                          size_t         size     );
/* DivisorLatch設定 */
extern void IoctrlSetDivisorLatch( NS16550ComNo_t comNo,
                                   uint16_t       value  );
/* FCR設定 */
extern void IoctrlSetFCR( NS16550ComNo_t comNo,
                          uint8_t        mask,
                          uint8_t        value  );
/* IER設定 */
extern void IoctrlSetIER( NS16550ComNo_t comNo,
                          uint8_t        mask,
                          uint8_t        value  );
/* LCR設定 */
extern void IoctrlSetLCR( NS16550ComNo_t comNo,
                          uint8_t        mask,
                          uint8_t        value  );
/* MCR設定 */
extern void IoctrlSetMCR( NS16550ComNo_t comNo,
                          uint8_t        mask,
                          uint8_t        value  );


/******************************************************************************/
#endif
