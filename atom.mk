LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libshdata
LOCAL_CATEGORY_PATH := libs
LOCAL_DESCRIPTION := Shared memory low level library
LOCAL_SRC_FILES := src/shd.c \
	src/shd_ctx.c \
	src/shd_section.c \
	src/shd_mdata_hdr.c \
	src/shd_hdr.c \
	src/shd_data.c \
	src/shd_sync.c \
	src/shd_sample.c \
	src/shd_window.c \
	src/shd_search.c \
	src/backend/shd_dev_mem.c \
	src/backend/shd_shm.c
LOCAL_CFLAGS += -DBUILD_TARGET_CPU=$(TARGET_CPU)

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/src

LOCAL_LIBRARIES := libshdata-section-lookup libfutils

LOCAL_CONDITIONAL_LIBRARIES := \
	OPTIONAL:libulog \
	OPTIONAL:libshdata-concurrency-hooks-implem

include $(BUILD_LIBRARY)

ifeq ($(TARGET_CPU),$(filter %$(TARGET_CPU),tegrax1 tegrak1))

# X1 implementation of shd_section_lookup()
include $(CLEAR_VARS)
LOCAL_MODULE := libshdata-section-lookup
LOCAL_CATEGORY_PATH := libs
LOCAL_DESCRIPTION := X1/K1 lookup table for /dev/mem
LOCAL_SRC_FILES := src/lookup/shd_lookup_x1.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include $(LOCAL_PATH)/src
LOCAL_DEPENDS_HEADERS := liblk-shdata
LOCAL_LIBRARIES := libfutils
include $(BUILD_STATIC_LIBRARY)

else

# Default implementation of shd_section_lookup()
include $(CLEAR_VARS)
LOCAL_MODULE := libshdata-section-lookup
LOCAL_CATEGORY_PATH := libs
LOCAL_DESCRIPTION := Dummy lookup table for /dev/mem
LOCAL_SRC_FILES := src/lookup/shd_lookup_default.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include $(LOCAL_PATH)/src
include $(BUILD_STATIC_LIBRARY)

endif

# Stress test
include $(CLEAR_VARS)
LOCAL_MODULE := libshdata-stress
LOCAL_CATEGORY_PATH := libs/libshdata/examples
LOCAL_DESCRIPTION := Configurable stress test for libshdata
LOCAL_SRC_FILES := examples/stress_test.c examples/common.c
LOCAL_LIBRARIES := libshdata libfutils
include $(BUILD_EXECUTABLE)

# Example code
include $(CLEAR_VARS)
LOCAL_MODULE := libshdata-1prod-1cons
LOCAL_CATEGORY_PATH := libs/libshdata/examples
LOCAL_DESCRIPTION := Example code for libshdata with 1 prod and 1 cons
LOCAL_SRC_FILES := examples/1prod_1cons.c examples/common.c
LOCAL_LIBRARIES := libshdata
include $(BUILD_EXECUTABLE)

# Unit testing
ifdef TARGET_TEST

include $(CLEAR_VARS)
LOCAL_MODULE := libshdata-concurrency-hooks-implem
LOCAL_CATEGORY_PATH := libs
LOCAL_DESCRIPTION := Implementation of libshdata concurrency hook points

LOCAL_SRC_FILES := tests/concurrency/hooks_implem.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
include $(BUILD_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := tst-libshdata
LOCAL_SRC_FILES := \
	tests/shd_test.c \
	tests/shd_test_helper.c \
	tests/shd_test_api.c \
	tests/shd_test_api_adv.c \
	tests/shd_test_func_write_basic.c \
	tests/shd_test_func_read_adv.c \
	tests/shd_test_func_read_basic.c \
	tests/shd_test_func_read_hdr.c \
	tests/shd_test_func_write_adv.c \
	tests/shd_test_error.c \
	tests/shd_test_concurrency.c \
	tests/lookup/section_lookup.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/src

LOCAL_LIBRARIES := libshdata libcunit libshdata-concurrency-hooks-implem

include $(BUILD_EXECUTABLE)

endif
