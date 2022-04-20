LOCAL_PATH := $(call my-dir)

MC_PATH := ../../../../../src/client
CMN_PATH := ../../../../../src/common

include $(CLEAR_VARS)

LOCAL_MODULE := main

SDL_PATH := ../SDL2

LOCAL_CFLAGS := -I. -std=c99 -O0 -fsigned-char
LOCAL_CFLAGS += -g -O2 -W -Wall -Wextra -Wno-unused-parameter -pedantic -Wno-missing-field-initializers
LOCAL_CFLAGS += -DON_ANDROID -D_REENTRANT -D_GNU_SOURCE=1 -DUSE_SDL2 -DSOUND_SDL2

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include \
		$(MC_PATH) \
		$(CMN_PATH)

LOCAL_SRC_FILES := $(MC_PATH)/main.c \
		$(MC_PATH)/main-sdl2.c \
		$(MC_PATH)/snd-sdl.c \
		$(CMN_PATH)/buildid.c \
		$(MC_PATH)/conf.c \
		$(MC_PATH)/set_focus.c \
		$(CMN_PATH)/md5.c \
		$(CMN_PATH)/net-unix.c \
		$(CMN_PATH)/sockbuf.c \
		$(CMN_PATH)/util.c \
		$(CMN_PATH)/obj-tval.c \
		$(CMN_PATH)/obj-gear-common.c \
		$(MC_PATH)/c-player.c \
		$(MC_PATH)/c-cmd.c \
		$(MC_PATH)/c-cmd-obj.c \
		$(MC_PATH)/cmd-core.c \
		$(MC_PATH)/netclient.c \
		$(MC_PATH)/c-option.c \
		$(CMN_PATH)/datafile.c \
		$(MC_PATH)/game-event.c \
		$(MC_PATH)/game-input.c \
		$(MC_PATH)/grafmode.c \
		$(CMN_PATH)/guid.c \
		$(CMN_PATH)/option.c \
		$(CMN_PATH)/parser.c \
		$(CMN_PATH)/randname.c \
		$(MC_PATH)/sound-core.c \
		$(CMN_PATH)/display.c \
		$(CMN_PATH)/source.c \
		$(MC_PATH)/player-properties.c \
		$(MC_PATH)/ui-birth.c \
		$(MC_PATH)/ui-command.c \
		$(MC_PATH)/ui-context.c \
		$(MC_PATH)/ui-curse.c \
		$(MC_PATH)/ui-death.c \
		$(MC_PATH)/ui-display.c \
		$(MC_PATH)/ui-event.c \
		$(MC_PATH)/ui-game.c \
		$(MC_PATH)/ui-init.c \
		$(MC_PATH)/ui-input.c \
		$(MC_PATH)/ui-keymap.c \
		$(MC_PATH)/ui-knowledge.c \
		$(MC_PATH)/ui-message.c \
		$(MC_PATH)/ui-menu.c \
		$(MC_PATH)/ui-object.c \
		$(MC_PATH)/ui-options.c \
		$(MC_PATH)/ui-output.c \
		$(MC_PATH)/ui-prefs.c \
		$(MC_PATH)/ui-spell.c \
		$(MC_PATH)/ui-store.c \
		$(MC_PATH)/ui-term.c \
		$(CMN_PATH)/z-bitflag.c \
		$(CMN_PATH)/z-color.c \
		$(CMN_PATH)/z-dice.c \
		$(CMN_PATH)/z-expression.c \
		$(CMN_PATH)/z-file.c \
		$(CMN_PATH)/z-form.c \
		$(CMN_PATH)/z-rand.c \
		$(CMN_PATH)/z-set.c \
		$(CMN_PATH)/z-type.c \
		$(CMN_PATH)/z-util.c \
		$(CMN_PATH)/z-virt.c

LOCAL_SHARED_LIBRARIES := SDL2 SDL2_image SDL2_ttf SDL2_mixer

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -llog

include $(BUILD_SHARED_LIBRARY)
