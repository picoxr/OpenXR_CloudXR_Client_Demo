LOCAL_PATH := $(call my-dir)

# Add prebuilt Oboe library
include $(CLEAR_VARS)
LOCAL_MODULE := Oboe
LOCAL_SRC_FILES := $(OBOE_SDK_ROOT)/prefab/modules/oboe/libs/android.$(TARGET_ARCH_ABI)/liboboe.so
include $(PREBUILT_SHARED_LIBRARY)

# Add prebuilt CloudXR client library
include $(CLEAR_VARS)
LOCAL_MODULE := CloudXRClient
LOCAL_SRC_FILES := $(CLOUDXR_SDK_ROOT)/jni/$(TARGET_ARCH_ABI)/libCloudXRClient.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := Grid
LOCAL_SRC_FILES := $(CLOUDXR_SDK_ROOT)/jni/$(TARGET_ARCH_ABI)/libgrid.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := Poco
LOCAL_SRC_FILES := $(CLOUDXR_SDK_ROOT)/jni/$(TARGET_ARCH_ABI)/libPoco.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := GsAudioWebRTC
LOCAL_SRC_FILES := $(CLOUDXR_SDK_ROOT)/jni/$(TARGET_ARCH_ABI)/libGsAudioWebRTC.so
include $(PREBUILT_SHARED_LIBRARY)

# Add prebuilt pxr_api library
include $(CLEAR_VARS)
LOCAL_MODULE := openxr_loader
LOCAL_SRC_FILES := $(LOCAL_PATH)/openxr_loader/$(TARGET_ARCH_ABI)/libopenxr_loader.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := CloudXRClientPXR

LOCAL_CFLAGS += -DXR_USE_PLATFORM_ANDROID=1 -DXR_USE_GRAPHICS_API_OPENGL_ES=1

LOCAL_C_INCLUDES := $(PXR_SDK_ROOT)/include \
                    $(OBOE_SDK_ROOT)/prefab/modules/oboe/include \
                    $(C_SHARED_INCLUDE) \
                    $(CLOUDXR_SDK_ROOT)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/openxr_loader/include

LOCAL_SRC_FILES := main.cpp \
                   logger.cpp \
                   platformplugin_factory.cpp \
                   platformplugin_android.cpp \
                   graphicsplugin_factory.cpp \
                   graphicsplugin_opengles.cpp \
                   openxr_loader/include/common/gfxwrapper_opengl.c \
                   cloudXRClient.cpp \
                   pController.cpp \
                   openxr_program.cpp

LOCAL_LDLIBS := -llog -landroid -lGLESv3 -lEGL
LOCAL_STATIC_LIBRARIES	:= android_native_app_glue
LOCAL_SHARED_LIBRARIES := Oboe CloudXRClient Grid Poco GsAudioWebRTC openxr_loader

include $(BUILD_SHARED_LIBRARY)

$(call import-module, android/native_app_glue)
$(call import-add-path, $(PXR_SDK_ROOT))
