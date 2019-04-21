#******************************************************************************#
#*                                                                            *#
#* src/Makefile                                                               *#
#*                                                                 2019/04/21 *#
#* Copyright (C) 2019 Mochi.                                                  *#
#*                                                                            *#
#******************************************************************************#
#******************************************************************************#
#* 定義                                                                       *#
#******************************************************************************#
# プログラム名
PROG = drv-ns16550

# ソースコード
SRCS = main.c

# リリースディレクトリ
RELEASE_DIR = ../release
# ビルドディレクトリ
BUILD_DIR   = ../build
# オブジェクトファイル生成先ディレクトリ
OBJ_DIR     = $(BUILD_DIR)/obj

# Cフラグ
CFLAGS  = -ffreestanding
CFLAGS += -nostdlib
CFLAGS += -m32
CFLAGS += -fno-pic
CFLAGS += -static
CFLAGS += -masm=intel
CFLAGS += -Iinclude
CFLAGS += -I$(BUILD_DIR)/include

# LDフラグ
LDFLAGS  = -Tdrv-ns16550.lds
LDFLAGS += -melf_i386
LDFLAGS += -L$(BUILD_DIR)/lib

# ライブラリ
LIBS  = -lmlog
LIBS += -lmvfs
LIBS += -lc
LIBS += -lMk
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
