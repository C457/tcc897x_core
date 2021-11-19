/*
**
** Copyright 2013,  Telechips, Inc.
**
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
**
**     http://www.apache.org/licenses/LICENSE-2.0 
**
** Unless required by applicable law or agreed to in writing, software 
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*
* Company: Telechips.co.Ltd
* Author: SW Hwang
*/

#ifndef SYSTEM_CORE_INCLUDE_ANDROID_HDMIRECEIVER_H
#define SYSTEM_CORE_INCLUDE_ANDROID_HDMIRECEIVER_H

#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <cutils/native_handle.h>
#include <hardware/hardware.h>
#include <hardware/gralloc.h>

__BEGIN_DECLS



enum {
    RECEIVER_FRAME_CALLBACK_FLAG_ENABLE_MASK = 0x01,
    RECEIVER_FRAME_CALLBACK_FLAG_ONE_SHOT_MASK = 0x02,
    RECEIVER_FRAME_CALLBACK_FLAG_COPY_OUT_MASK = 0x04,

    RECEIVER_FRAME_CALLBACK_FLAG_NOOP = 0x00,
    RECEIVER_FRAME_CALLBACK_FLAG_CAMCORDER = 0x01,
    RECEIVER_FRAME_CALLBACK_FLAG_CAMERA = 0x05,
    RECEIVER_FRAME_CALLBACK_FLAG_BARCODE_SCANNER = 0x07
};


enum {
    RECEIVER_MSG_ERROR = 0x0001,            // notifyCallback
    RECEIVER_MSG_SHUTTER = 0x0002,          // notifyCallback
    RECEIVER_MSG_FOCUS = 0x0004,            // notifyCallback
    RECEIVER_MSG_ZOOM = 0x0008,             // notifyCallback
    RECEIVER_MSG_PREVIEW_FRAME = 0x0010,    // dataCallback
    RECEIVER_MSG_VIDEO_FRAME = 0x0020,      // data_timestamp_callback
    RECEIVER_MSG_POSTVIEW_FRAME = 0x0040,   // dataCallback
    RECEIVER_MSG_RAW_IMAGE = 0x0080,        // dataCallback
    RECEIVER_MSG_COMPRESSED_IMAGE = 0x0100, // dataCallback
    RECEIVER_MSG_RAW_IMAGE_NOTIFY = 0x0200, // dataCallback

    RECEIVER_MSG_PREVIEW_METADATA = 0x0400, // dataCallback

    RECEIVER_MSG_FOCUS_MOVE = 0x0800,       // notifyCallback
    RECEIVER_MSG_ALL_MSGS = 0xFFFF
};

/** cmdType in sendCommand functions */
enum {
    RECEIVER_CMD_START_SMOOTH_ZOOM = 1,
    RECEIVER_CMD_STOP_SMOOTH_ZOOM = 2,


    RECEIVER_CMD_SET_DISPLAY_ORIENTATION = 3,

    RECEIVER_CMD_ENABLE_SHUTTER_SOUND = 4,

    RECEIVER_CMD_PLAY_RECORDING_SOUND = 5,

    RECEIVER_CMD_START_FACE_DETECTION = 6,

    RECEIVER_CMD_STOP_FACE_DETECTION = 7,

    RECEIVER_CMD_ENABLE_FOCUS_MOVE_MSG = 8,

    RECEIVER_CMD_PING = 9,

    RECEIVER_CMD_SET_VIDEO_BUFFER_COUNT = 10,
};

enum {
    RECEIVER_ERROR_UNKNOWN = 1,

    RECEIVER_ERROR_RELEASED = 2,
    RECEIVER_ERROR_SERVER_DIED = 100
};

enum {

    RECEIVER_FACING_BACK = 0,

    RECEIVER_FACING_FRONT = 1
};

enum {

    RECEIVER_FACE_DETECTION_HW = 0,

    RECEIVER_FACE_DETECTION_SW = 1
};


typedef struct receiver_face {

    int32_t rect[4];

    int32_t score;

    int32_t id;

    int32_t left_eye[2];

    int32_t right_eye[2];

    int32_t mouth[2];

} receiver_face_t;

typedef struct receiver_frame_metadata {

    int32_t number_of_faces;

    receiver_face_t *faces;
	
} receiver_frame_metadata_t;

__END_DECLS

#endif /* SYSTEM_CORE_INCLUDE_ANDROID_HDMIRECEIVER_H */
