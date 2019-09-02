/******************************************************************************/
/*                                                                            */
/* src/Ctrl.h                                                                 */
/*                                                                 2019/08/25 */
/* Copyright (C) 2019 Mochi.                                                  */
/*                                                                            */
/******************************************************************************/
#ifndef CTRL_H
#define CTRL_H
/******************************************************************************/
/* インクルード                                                               */
/******************************************************************************/
/* 標準ヘッダ */
#include <stdint.h>


/******************************************************************************/
/* グローバル関数宣言                                                         */
/******************************************************************************/
/* 割込み無効化 */
extern void CtrlDisableInterrupt( NS16550ComNo_t comNo,
                                  uint8_t        flag   );
/* 割込み有効化 */
extern void CtrlEnableInterrupt( NS16550ComNo_t comNo,
                                 uint8_t        flag   );
/* 割込み要因レジスタ読込み */
extern uint8_t CtrlInIIR( NS16550ComNo_t comNo );
/* ラインステータスレジスタ読込み */
extern uint8_t CtrlInLSR( NS16550ComNo_t comNo );
/* モデムステータスレジスタ読込み */
extern uint8_t CtrlInMSR( NS16550ComNo_t comNo );
/* 受信バッファレジスタ読込み */
extern uint8_t CtrlInRBR( NS16550ComNo_t comNo );
/* NS16550初期化 */
extern void CtrlInit( NS16550ComNo_t comNo );
/* THR書込み */
extern void CtrlOutTHR( NS16550ComNo_t comNo,
                        uint8_t        value  );
/* ディバイザラッチ設定 */
extern void CtrlSetDivisorLatch( NS16550ComNo_t comNo,
                                 uint16_t       value  );
/* FIFOコントロールレジスタ設定 */
extern void CtrlSetFCR( NS16550ComNo_t comNo,
                        uint8_t        mask,
                        uint8_t        value  );
/* ラインコントロールレジスタ設定 */
extern void CtrlSetLCR( NS16550ComNo_t comNo,
                        uint8_t        mask,
                        uint8_t        value  );
/* モデムコントロールレジスタ設定 */
extern void CtrlSetMCR( NS16550ComNo_t comNo,
                        uint8_t        mask,
                        uint8_t        value  );


/******************************************************************************/
#endif
