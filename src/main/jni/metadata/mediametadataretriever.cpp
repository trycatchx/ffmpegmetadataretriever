/*
 * FFmpegMediaMetadataRetriever: A unified interface for retrieving frame 
 * and meta data from an input media file.
 *
 * Copyright 2016 William Seemann
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "MediaMetadataRetriever"


extern "C" {
#include "mediametadataretriever.h"
#include "ffmpeg_mediametadataretriever.h"
#include "android_log.h"
}

MediaMetadataRetriever::MediaMetadataRetriever() {
    state = NULL;
    isInterrupted = 0;
    runner = {0, 10, 0};
}

MediaMetadataRetriever::~MediaMetadataRetriever() {
    Mutex::Autolock _l(mLock);
    runner = {0, 10, 0};
    ::release(state);
    state = NULL;

}

void MediaMetadataRetriever::initInline(const char *srcUrl, const char *headers) {
    Mutex::Autolock _l(mLock);
    ::initPre(&state, srcUrl, headers);
}

int MediaMetadataRetriever::setDataSource(const char *srcUrl, const char *headers) {
    MediaMetadataRetriever::initInline(srcUrl, headers);
    if (isInterrupted == 1) {
        return -2;
    }
    return ::set_data_source_uri(&state, srcUrl, headers, &runner);
}


/**
  * 不安全这个接口暂时不用
 */
int MediaMetadataRetriever::setDataSource(int fd, int64_t offset, int64_t length) {
    Mutex::Autolock _l(mLock);
    if (isInterrupted == 1) {
        return -2;
    }
    return ::set_data_source_fd(&state, fd, offset, length);
}

void MediaMetadataRetriever::cancelRequest() {
    Mutex::Autolock _l(mLock);
    //取消的标识
    isInterrupted = 1;
    runner.interrupt = 1;

}

int MediaMetadataRetriever::getFrameAtTime(int64_t timeUs, int option, AVPacket *pkt) {
//	Mutex::Autolock _l(mLock);
    if( runner.interrupt == 1) {
        return -1;
    }
    return ::get_frame_at_time(&state, timeUs, option, pkt, &runner);
}

int
MediaMetadataRetriever::getScaledFrameAtTime(int64_t timeUs, int option, AVPacket *pkt, int width,
                                             int height) {
//	Mutex::Autolock _l(mLock);
    if( runner.interrupt == 1) {
        return -1;
    }
    return ::get_scaled_frame_at_time(&state, timeUs, option, pkt, width, height, &runner);
}

const char *MediaMetadataRetriever::extractMetadata(const char *key) {
//	Mutex::Autolock _l(mLock);

    return ::extract_metadata(&state, key);
}

const char *MediaMetadataRetriever::extractMetadataFromChapter(const char *key, int chapter) {
//	Mutex::Autolock _l(mLock);

    return ::extract_metadata_from_chapter(&state, key, chapter);
}

int MediaMetadataRetriever::extractAlbumArt(AVPacket *pkt) {
//	Mutex::Autolock _l(mLock);
    if( runner.interrupt == 1) {
        return -1;
    }
    return ::get_embedded_picture(&state, pkt,&runner);
}

int
MediaMetadataRetriever::getMetadata(bool update_only, bool apply_filter, AVDictionary **metadata) {
//	Mutex::Autolock _l(mLock);

    return get_metadata(&state, metadata);
}

int MediaMetadataRetriever::setNativeWindow(ANativeWindow *native_window) {
//    Mutex::Autolock _l(mLock);
    return ::set_native_window(&state, native_window);
}


