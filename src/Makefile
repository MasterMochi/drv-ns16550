#******************************************************************************#
#*                                                                            *#
#* src/Makefile                                                               *#
#*                                                                 2020/07/27 *#
#* Copyright (C) 2019-2020 Mochi.                                             *#
#*                                                                            *#
#******************************************************************************#
#******************************************************************************#
#* 定義                                                                       *#
#******************************************************************************#
# プログラム名
PROG = drv-ns16550

# ソースコード
SRCS  = main.c
SRCS += Ioctrl.c
SRCS += Bufmng.c
SRCS += Intmng.c
SRCS += Filemng.c
SRCS += Rxctrl.c
SRCS += Txctrl.c

# ビルドディレクトリ
BUILD_DIR   = ../build
# リリースディレクトリ
RELEASE_DIR = $(BUILD_DIR)/release
# オブジェクトファイル生成先ディレクトリ
OBJ_DIR     = $(BUILD_DIR)/obj/$(PROG)

# Cフラグ
CFLAGS  = -ffreestanding
CFLAGS += -nostdlib
CFLAGS += -m32
CFLAGS += -fno-pic
CFLAGS += -static
CFLAGS += -masm=intel
CFLAGS += -O0
CFLAGS += -DDEBUG_LOG_LV=2
CFLAGS += -Iinclude
CFLAGS += -I$(BUILD_DIR)/include
ifdef BASE_CFLAGS
CFLAGS += $(BASE_CFLAGS)
endif

# LDフラグ
LDFLAGS  = -Tdrv-ns16550.lds
LDFLAGS += -melf_i386
LDFLAGS += -L$(BUILD_DIR)/lib
ifdef BASE_LDFLAGS
LDFLAGS += $(BASE_LDFLAGS)
endif

# ライブラリ
LIBS  = -lmlog
LIBS += -lmvfs
LIBS += -lc
LIBS += -lmk
LIBS += -lMLib


#******************************************************************************#
# マクロ                                                                      *#
#******************************************************************************#
# オブジェクトサブディレクトリ
OBJ_SUBDIRS = $(sort $(addprefix $(OBJ_DIR)/, $(dir $(SRCS))))

# オブジェクトファイル
OBJS = $(addprefix $(OBJ_DIR)/, $(SRCS:.c=.o))

# 依存関係ファイル
DEPS = $(addprefix $(OBJ_DIR)/, $(SRCS:.c=.d))


#******************************************************************************#
#* phonyターゲット                                                            *#
#******************************************************************************#
.PHONY: all
all: $(OBJ_SUBDIRS) $(OBJS) $(RELEASE_DIR)/$(PROG) Makefile

.PHONY: clean
clean:
	-rm -rf $(RELEASE_DIR)/$(PROG)
	-rm -rf $(OBJ_DIR)


#******************************************************************************#
#* 生成規則                                                                   *#
#******************************************************************************#
# 依存関係ファイル
-include $(DEPS)

# オブジェクトサブディレクトリ生成
ifdef OBJ_SUBDIRS
$(OBJ_SUBDIRS):
	mkdir -p $@
endif

# 実行ファイル生成
$(RELEASE_DIR)/$(PROG): $(OBJS) Makefile
	$(LD) $(LDFLAGS) -o $(OBJ_DIR)/$(PROG) $(OBJS) $(LIBS)
	ln -sfr $(OBJ_DIR)/$(PROG) $@

# コンパイル
$(OBJ_DIR)/%.o: %.c Makefile
	$(CC) $(CFLAGS) -o $@ -c $< -MD -MP


#******************************************************************************#
