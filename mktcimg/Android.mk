
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := mktcimg.c \
		   gpt.c  \
		   crc32.c\
		   sparse.c\
		   fparser.c

LOCAL_STATIC_LIBRARIES := libz
LOCAL_MODULE := mktcimg

include $(BUILD_HOST_EXECUTABLE)

$(call dist-for-goals,dist_files,$(LOCAL_BUILT_MODULE))
