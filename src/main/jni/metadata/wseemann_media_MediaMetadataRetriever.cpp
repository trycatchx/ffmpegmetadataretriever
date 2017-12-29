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
#define LOG_TAG "MediaMetadataRetrieverJNI"

#include <assert.h>
#include <android/log.h>

#include "jni.h"

// for native window JNI
#include <android/bitmap.h>
#include <android/native_window_jni.h>

extern "C" {
#include "mediametadataretriever.h"
#include "ffmpeg_mediametadataretriever.h"
}

using namespace std;

struct fields_t {
    jfieldID context;
};

static fields_t fields;
static const char *const kClassPathName = "wseemann/media/FFmpegMediaMetadataRetriever";

static JavaVM *m_vm;

// video sink for the player
static ANativeWindow *theNativeWindow;

static jstring NewStringUTF(JNIEnv *env, const char *data) {
    jstring str = NULL;

    int size = strlen(data);

    jbyteArray array = NULL;
    array = env->NewByteArray(size);
    if (!array) {  // OutOfMemoryError exception has already been thrown.
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "convertString: OutOfMemoryError is thrown.");
    } else {
        jbyte *bytes = env->GetByteArrayElements(array, NULL);
        if (bytes != NULL) {
            memcpy(bytes, data, size);
            env->ReleaseByteArrayElements(array, bytes, 0);

            jclass string_Clazz = env->FindClass("java/lang/String");
            jmethodID string_initMethodID = env->GetMethodID(string_Clazz, "<init>",
                                                             "([BLjava/lang/String;)V");
            jstring utf = env->NewStringUTF("UTF-8");
            str = (jstring) env->NewObject(string_Clazz, string_initMethodID, array, utf);

            env->DeleteLocalRef(utf);
            //env->DeleteLocalRef(str);
        }
    }
    env->DeleteLocalRef(array);

    return str;
}

void jniThrowException(JNIEnv *env, const char *className,
                       const char *msg) {
    jclass exception = env->FindClass(className);
    env->ThrowNew(exception, msg);
}

static void process_media_retriever_call(JNIEnv *env, int opStatus, const char *exception,
                                         const char *message) {
    if (opStatus == -2) {

        jniThrowException(env, "java/lang/IllegalStateException", NULL);
    } else if (opStatus == -1) {
        if (strlen(message) > 230) {
            // If the message is too long, don't bother displaying the status code.
            jniThrowException(env, exception, message);
        } else {
            char msg[256];
            // Append the status code to the message.
            sprintf(msg, "%s: status = 0x%X", message, opStatus);
            jniThrowException(env, exception, msg);
        }
    }
}

static MediaMetadataRetriever *getRetriever(JNIEnv *env, jobject thiz) {
    // No lock is needed, since it is called internally by other methods that are protected
    MediaMetadataRetriever *retriever = (MediaMetadataRetriever *) env->GetLongField(thiz,
                                                                                     fields.context);
    return retriever;
}

static void setRetriever(JNIEnv *env, jobject thiz, long retriever) {
    // No lock is needed, since it is called internally by other methods that are protected
    //MediaMetadataRetriever *old = (MediaMetadataRetriever*) env->GetLongField(thiz, fields.context);
    env->SetLongField(thiz, fields.context, retriever);
}

static void
Java_wseemann_media_FFmpegMediaMetadataRetriever__setDataSource(
        JNIEnv *env, jobject thiz, jstring path,
        jobjectArray keys, jobjectArray values) {

    MediaMetadataRetriever *retriever = getRetriever(env, thiz);
    if (retriever == 0) {
        jniThrowException(env, "java/lang/IllegalStateException", "No retriever available");
        return;
    }

    if (!path) {
        jniThrowException(env, "java/lang/IllegalArgumentException", "Null pointer");
        return;
    }

    const char *tmp = env->GetStringUTFChars(path, NULL);
    if (!tmp) {  // OutOfMemoryError exception already thrown
        return;
    }

    // Don't let somebody trick us in to reading some random block of memory
    if (strncmp("mem://", tmp, 6) == 0) {
        jniThrowException(env, "java/lang/IllegalArgumentException", "Invalid pathname");
        return;
    }

    // Workaround for FFmpeg ticket #998
    // "must convert mms://... streams to mmsh://... for FFmpeg to work"
    char *restrict_to = strstr(tmp, "mms://");
    if (restrict_to) {
        strncpy(restrict_to, "mmsh://", 6);
        puts(tmp);
    }

    char *headers = NULL;

    if (keys && values != NULL) {
        int keysCount = env->GetArrayLength(keys);
        int valuesCount = env->GetArrayLength(values);

        if (keysCount != valuesCount) {
            jniThrowException(env, "java/lang/IllegalArgumentException", NULL);
            return;
        }

        int i = 0;
        const char *rawString = NULL;
        char hdrs[2048];

        for (i = 0; i < keysCount; i++) {
            jstring key = (jstring) env->GetObjectArrayElement(keys, i);
            rawString = env->GetStringUTFChars(key, NULL);
            strcat(hdrs, rawString);
            strcat(hdrs, ": ");
            env->ReleaseStringUTFChars(key, rawString);

            jstring value = (jstring) env->GetObjectArrayElement(values, i);
            rawString = env->GetStringUTFChars(value, NULL);
            strcat(hdrs, rawString);
            strcat(hdrs, "\r\n");
            env->ReleaseStringUTFChars(value, rawString);
        }
        headers = &hdrs[0];
    }
    process_media_retriever_call(
            env,
            retriever->setDataSource(tmp, headers),
            "java/lang/IllegalArgumentException",
            "setDataSource failed");

    env->ReleaseStringUTFChars(path, tmp);
    tmp = NULL;
}

static void Java_wseemann_media_FFmpegMediaMetadataRetriever_setDataSource(
        JNIEnv *env, jobject thiz, jstring path) {
    Java_wseemann_media_FFmpegMediaMetadataRetriever__setDataSource(
            env, thiz, path, NULL, NULL);
}

static int jniGetFDFromFileDescriptor(JNIEnv *env, jobject fileDescriptor) {
    jint fd = -1;
    jclass fdClass = env->FindClass("java/io/FileDescriptor");

    if (fdClass != NULL) {
        jfieldID fdClassDescriptorFieldID = env->GetFieldID(fdClass, "descriptor", "I");
        if (fdClassDescriptorFieldID != NULL && fileDescriptor != NULL) {
            fd = env->GetIntField(fileDescriptor, fdClassDescriptorFieldID);
        }
    }

    return fd;
}

static void Java_wseemann_media_FFmpegMediaMetadataRetriever___setDataSource(JNIEnv *env, jobject thiz,
                                                                        jobject fileDescriptor,
                                                                        jlong offset,
                                                                        jlong length) {
    MediaMetadataRetriever *retriever = getRetriever(env, thiz);
    if (retriever == 0) {
        jniThrowException(env, "java/lang/IllegalStateException", "No retriever available");
        return;
    }
    if (!fileDescriptor) {
        jniThrowException(env, "java/lang/IllegalArgumentException", NULL);
        return;
    }
    int fd = jniGetFDFromFileDescriptor(env, fileDescriptor);
    if (offset < 0 || length < 0 || fd < 0) {

        jniThrowException(env, "java/lang/IllegalArgumentException", NULL);
        return;
    }
    process_media_retriever_call(env, retriever->setDataSource(fd, offset, length),
                                 "java/lang/RuntimeException", "setDataSource failed");
}

static jbyteArray
Java_wseemann_media_FFmpegMediaMetadataRetriever_getFrameAtTime(JNIEnv *env, jobject thiz, jlong timeUs,
                                                           jint option) {
    MediaMetadataRetriever *retriever = getRetriever(env, thiz);
    if (retriever == 0) {
        jniThrowException(env, "java/lang/IllegalStateException", "No retriever available");
        return NULL;
    }

    AVPacket packet;
    av_init_packet(&packet);
    jbyteArray array = NULL;

    if (retriever->getFrameAtTime(timeUs, option, &packet) == 0) {
        int size = packet.size;
        uint8_t *data = packet.data;
        array = env->NewByteArray(size);
        if (!array) {  // OutOfMemoryError exception has already been thrown.
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                                "getFrameAtTime: OutOfMemoryError is thrown.");
        } else {
            jbyte *bytes = env->GetByteArrayElements(array, NULL);
            if (bytes != NULL) {
                memcpy(bytes, data, size);
                env->ReleaseByteArrayElements(array, bytes, 0);
            }
        }
    }

    av_packet_unref(&packet);

    return array;
}


static jobject
Java_wseemann_media_FFmpegMediaMetadataRetriever_getFrameAtTimeAndCopyToStream(JNIEnv *env, jobject thiz,
                                                                          jlong timeUs, jint option) {
    MediaMetadataRetriever *retriever = getRetriever(env, thiz);
    if (retriever == 0) {
        jniThrowException(env, "java/lang/IllegalStateException", "No retriever available");
        return NULL;
    }
    jobject ret = NULL;



    AVPacket packet;
    av_init_packet(&packet);
    jbyteArray array = NULL;

    //找到返回java 层的类
    jclass jImageInputStreamClass = env->FindClass("wseemann/media/ImageInputStream");
    jmethodID init_ImageInputStream = NULL;
    int result = 0;
    if (jImageInputStreamClass == 0) {
        goto exit;
    }

    //获取类构造函数
    init_ImageInputStream = env->GetMethodID(jImageInputStreamClass, "<init>",
                                                       "(ZLjava/io/InputStream;)V");
    if (init_ImageInputStream == NULL) {
        goto exit;
    }

     result = retriever->getFrameAtTime(timeUs, option, &packet);

    if (result == 0) {
        int size = packet.size;
        uint8_t *data = packet.data;
        array = env->NewByteArray(size);
        if (array && size > 0) {
            jclass jInPutStreamClass = env->FindClass("java/io/ByteArrayInputStream");

            if (jInPutStreamClass == 0) {
                env->DeleteLocalRef(array);
                goto exit;
            }

            //获取类构造函数
            jmethodID init_inputStream = env->GetMethodID(jInPutStreamClass, "<init>",
                                                          "([B)V");
            if (init_inputStream == NULL) {
                env->DeleteLocalRef(array);
                env->DeleteLocalRef(jInPutStreamClass);
                goto exit;
            }
            //吧byte 指针设置到 java 的byte数组对象中
            env->SetByteArrayRegion(array, 0, size, (jbyte *) data);

            ret = env->NewObject(jImageInputStreamClass, init_ImageInputStream,
                                 JNI_TRUE,
                                 env->NewObject(jInPutStreamClass, init_inputStream, array));
            env->DeleteLocalRef(array);
            env->DeleteLocalRef(jInPutStreamClass);
        }
    }

    exit:
    av_packet_unref(&packet);
    env->DeleteLocalRef(jImageInputStreamClass);

    return ret;
}

static jbyteArray
Java_wseemann_media_FFmpegMediaMetadataRetriever_getScaledFrameAtTime(JNIEnv *env, jobject thiz,
                                                                 jlong timeUs, jint option,
                                                                 jint width, jint height) {
    MediaMetadataRetriever *retriever = getRetriever(env, thiz);
    if (retriever == 0) {
        jniThrowException(env, "java/lang/IllegalStateException", "No retriever available");
        return NULL;
    }

    AVPacket packet;
    av_init_packet(&packet);
    jbyteArray array = NULL;

    if (retriever->getScaledFrameAtTime(timeUs, option, &packet, width, height) == 0) {
        int size = packet.size;
        uint8_t *data = packet.data;
        array = env->NewByteArray(size);
        if (!array) {  // OutOfMemoryError exception has already been thrown.
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                                "getFrameAtTime: OutOfMemoryError is thrown.");
        } else {
            jbyte *bytes = env->GetByteArrayElements(array, NULL);
            if (bytes != NULL) {
                memcpy(bytes, data, size);
                env->ReleaseByteArrayElements(array, bytes, 0);
            }
        }
    }

    av_packet_unref(&packet);

    return array;
}

static jbyteArray
Java_wseemann_media_FFmpegMediaMetadataRetriever_getEmbeddedPicture(JNIEnv *env, jobject thiz) {
    MediaMetadataRetriever *retriever = getRetriever(env, thiz);
    if (retriever == 0) {
        jniThrowException(env, "java/lang/IllegalStateException", "No retriever available");
        return NULL;
    }

    AVPacket packet;
    av_init_packet(&packet);
    jbyteArray array = NULL;

    if (retriever->extractAlbumArt(&packet) == 0) {
        int size = packet.size;
        uint8_t *data = packet.data;
        array = env->NewByteArray(size);
        if (!array) {  // OutOfMemoryError exception has already been thrown.
            //__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "getEmbeddedPicture: OutOfMemoryError is thrown.");
        } else {
            jbyte *bytes = env->GetByteArrayElements(array, NULL);
            if (bytes != NULL) {
                memcpy(bytes, data, size);
                env->ReleaseByteArrayElements(array, bytes, 0);
            }
        }
    }

    av_packet_unref(&packet);

    return array;
}

static jobject
Java_wseemann_media_FFmpegMediaMetadataRetriever_extractMetadata(JNIEnv *env, jobject thiz,
                                                            jstring jkey) {
    //__android_log_write(ANDROID_LOG_INFO, LOG_TAG, "extractMetadata");
    MediaMetadataRetriever *retriever = getRetriever(env, thiz);
    if (retriever == 0) {
        jniThrowException(env, "java/lang/IllegalStateException", "No retriever available");
        return NULL;
    }

    if (!jkey) {
        jniThrowException(env, "java/lang/IllegalArgumentException", "Null pointer");
        return NULL;
    }

    const char *key = env->GetStringUTFChars(jkey, NULL);
    if (!key) {  // OutOfMemoryError exception already thrown
        return NULL;
    }

    const char *value = retriever->extractMetadata(key);
    if (!value) {
        //__android_log_write(ANDROID_LOG_INFO, LOG_TAG, "extractMetadata: Metadata is not found");
        return NULL;
    }
    //__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "extractMetadata: value (%s) for keyCode(%s)", value, key);
    env->ReleaseStringUTFChars(jkey, key);
    return NewStringUTF(env, value);
}

static jobject
Java_wseemann_media_FFmpegMediaMetadataRetriever_extractMetadataFromChapter(JNIEnv *env, jobject thiz,
                                                                       jstring jkey, jint chapter) {
    //__android_log_write(ANDROID_LOG_INFO, LOG_TAG, "extractMetadataFromChapter");
    MediaMetadataRetriever *retriever = getRetriever(env, thiz);
    if (retriever == 0) {
        jniThrowException(env, "java/lang/IllegalStateException", "No retriever available");
        return NULL;
    }

    if (!jkey) {
        jniThrowException(env, "java/lang/IllegalArgumentException", "Null pointer");
        return NULL;
    }

    const char *key = env->GetStringUTFChars(jkey, NULL);
    if (!key) {  // OutOfMemoryError exception already thrown
        return NULL;
    }

    if (chapter < 0) {
        return NULL;
    }

    const char *value = retriever->extractMetadataFromChapter(key, chapter);
    if (!value) {
        //__android_log_write(ANDROID_LOG_INFO, LOG_TAG, "extractMetadata: Metadata is not found");
        return NULL;
    }
    //__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "extractMetadata: value (%s) for keyCode(%s)", value, key);
    env->ReleaseStringUTFChars(jkey, key);
    return env->NewStringUTF(value);
}

static jobject
Java_wseemann_media_FFmpegMediaMetadataRetriever_nativeGetMetadata(JNIEnv *env, jobject thiz,
                                                        jboolean update_only,
                                                        jboolean apply_filter, jobject reply) {
    //__android_log_write(ANDROID_LOG_INFO, LOG_TAG, "getMetadata");
    MediaMetadataRetriever *retriever = getRetriever(env, thiz);
    if (retriever == NULL) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return JNI_FALSE;
    }

    // On return metadata is positioned at the beginning of the
    // metadata. Note however that the parcel actually starts with the
    // return code so you should not rewind the parcel using
    // setDataPosition(0).
    AVDictionary *metadata = NULL;

    if (retriever->getMetadata(update_only, apply_filter, &metadata) == 0) {
        jclass hashMap_Clazz = env->FindClass("java/util/HashMap");
        jmethodID gHashMap_initMethodID = env->GetMethodID(hashMap_Clazz, "<init>", "()V");
        jobject map = env->NewObject(hashMap_Clazz, gHashMap_initMethodID);
        jmethodID gHashMap_putMethodID = env->GetMethodID(hashMap_Clazz, "put",
                                                          "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");

        int i = 0;

        for (i = 0; i < metadata->count; i++) {
            jstring jKey = NewStringUTF(env, metadata->elems[i].key);
            jstring jValue = NewStringUTF(env, metadata->elems[i].value);
            (jobject) env->CallObjectMethod(map, gHashMap_putMethodID, jKey, jValue);
            env->DeleteLocalRef(jKey);
            env->DeleteLocalRef(jValue);
        }

        if (metadata) {
            av_dict_free(&metadata);

        }

        return map;
    } else {
        return reply;
    }
}

static void Java_wseemann_media_FFmpegMediaMetadataRetriever_release(JNIEnv *env, jobject thiz) {

    //Mutex::Autolock lock(sLock);
    MediaMetadataRetriever *retriever = getRetriever(env, thiz);
    delete retriever;
    setRetriever(env, thiz, 0);
}

// set the surface
static void
Java_wseemann_media_FFmpegMediaMetadataRetriever_setSurface(JNIEnv *env, jclass thiz, jobject surface) {
    //__android_log_write(ANDROID_LOG_INFO, LOG_TAG, "setSurface");
    MediaMetadataRetriever *retriever = getRetriever(env, thiz);
    if (retriever == 0) {
        jniThrowException(env, "java/lang/IllegalStateException", "No retriever available");
        return;
    }

    // obtain a native window from a Java surface
    theNativeWindow = ANativeWindow_fromSurface(env, surface);

    if (theNativeWindow != NULL) {
        retriever->setNativeWindow(theNativeWindow);
    }
}

static void Java_wseemann_media_FFmpegMediaMetadataRetriever_nativeFinalize(JNIEnv *env, jobject thiz) {
    //__android_log_write(ANDROID_LOG_INFO, LOG_TAG, "native_finalize");
    // No lock is needed, since Java_wseemann_media_FFmpegMediaMetadataRetriever_release() is protected
    Java_wseemann_media_FFmpegMediaMetadataRetriever_release(env, thiz);
}

static void Java_wseemann_media_FFmpegMediaMetadataRetriever_nativeInit(JNIEnv *env, jobject thiz) {
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "native_init");

    jclass clazz = env->FindClass(kClassPathName);
    if (clazz == NULL) {
        return;
    }

    fields.context = env->GetFieldID(clazz, "mNativeContext", "J");
    if (fields.context == NULL) {
        return;
    }

    // Initialize libavformat and register all the muxers, demuxers and protocols.
    av_register_all();
    avformat_network_init();
}

static void Java_wseemann_media_FFmpegMediaMetadataRetriever_nativeSetup(JNIEnv *env, jobject thiz) {
    MediaMetadataRetriever *retriever = new MediaMetadataRetriever();
    if (retriever == 0) {
        jniThrowException(env, "java/lang/RuntimeException", "Out of memory");
        return;
    }
    setRetriever(env, thiz, (long) retriever);
}

static void Java_wseemann_media_FFmpegMediaMetadataRetriever_nativeCancelRequest(JNIEnv *env, jobject thiz) {
    MediaMetadataRetriever *retriever = getRetriever(env, thiz);
    if (retriever != 0) {
        retriever->cancelRequest();
    }
}



static jobject
Java_wseemann_media_FFmpegMediaMetadataRetriever_getEmbeddedPictureAndCopyToStream(JNIEnv *env,
                                                                              jobject thiz) {
    MediaMetadataRetriever *retriever = getRetriever(env, thiz);
    if (retriever == 0) {
        jniThrowException(env, "java/lang/IllegalStateException", "No retriever available");
        return JNI_FALSE;
    }

    jobject ret = NULL;
    AVPacket packet;
    av_init_packet(&packet);
    jbyteArray array = NULL;

    jclass jImageInputStreamClass = env->FindClass("wseemann/media/ImageInputStream");
    jmethodID init_ImageInputStream = NULL;
    if (jImageInputStreamClass == 0) {
        goto exit;
    }

    //获取类构造函数
    init_ImageInputStream = env->GetMethodID(jImageInputStreamClass, "<init>",
                                                  "(ZLjava/io/InputStream;)V");
    if (init_ImageInputStream == NULL) {
        goto exit;
    }




    if (retriever->extractAlbumArt(&packet) == 0) {
        int size = packet.size;
        uint8_t *data = packet.data;
        array = env->NewByteArray(size);

        if (array && size > 0) {


            jclass jInPutStreamClass = env->FindClass("java/io/ByteArrayInputStream");

            if (jInPutStreamClass == 0) {
                env->DeleteLocalRef(array);
                goto exit;
            }

            //获取类构造函数
            jmethodID init_inputStream = env->GetMethodID(jInPutStreamClass, "<init>",
                                                          "([B)V");
            if (init_inputStream == NULL) {
                env->DeleteLocalRef(array);
                env->DeleteLocalRef(jInPutStreamClass);
                goto exit;
            }
            //吧byte 指针设置到 java 的byte数组对象中
            env->SetByteArrayRegion(array, 0, size, (jbyte *) data);

            ret = env->NewObject(jImageInputStreamClass, init_ImageInputStream,
                                 JNI_TRUE,
                                 env->NewObject(jInPutStreamClass, init_inputStream, array));

            env->DeleteLocalRef(array);
            env->DeleteLocalRef(jInPutStreamClass);
        }
    }
    exit:
    av_packet_unref(&packet);
    env->DeleteLocalRef(jImageInputStreamClass);
    return ret;

}

// JNI mapping between Java methods and native methods
static JNINativeMethod nativeMethods[] = {
        {"setDataSource",                     "(Ljava/lang/String;)V",                      (void *) Java_wseemann_media_FFmpegMediaMetadataRetriever_setDataSource},

        {
         "_setDataSource",
                                              "(Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;)V",
                                                                                            (void *) Java_wseemann_media_FFmpegMediaMetadataRetriever__setDataSource
        },

        {"__setDataSource",                     "(Ljava/io/FileDescriptor;JJ)V",              (void *) Java_wseemann_media_FFmpegMediaMetadataRetriever___setDataSource},
        {"_getFrameAtTime",                   "(JI)[B",                                     (void *) Java_wseemann_media_FFmpegMediaMetadataRetriever_getFrameAtTime},
        {"_getFrameAtTimeAndCopyToStream",    "(JI)Lwseemann/media/ImageInputStream;",      (void *) Java_wseemann_media_FFmpegMediaMetadataRetriever_getFrameAtTimeAndCopyToStream},
        {"_getScaledFrameAtTime",             "(JIII)[B",                                   (void *) Java_wseemann_media_FFmpegMediaMetadataRetriever_getScaledFrameAtTime},
        {"extractMetadata",                   "(Ljava/lang/String;)Ljava/lang/String;",     (void *) Java_wseemann_media_FFmpegMediaMetadataRetriever_extractMetadata},
        {"extractMetadataFromChapter",        "(Ljava/lang/String;I)Ljava/lang/String;",    (void *) Java_wseemann_media_FFmpegMediaMetadataRetriever_extractMetadataFromChapter},
        {"nativeGetMetadata",                "(ZZLjava/util/HashMap;)Ljava/util/HashMap;", (void *) Java_wseemann_media_FFmpegMediaMetadataRetriever_nativeGetMetadata},
        {"getEmbeddedPicture",                "()[B",                                       (void *) Java_wseemann_media_FFmpegMediaMetadataRetriever_getEmbeddedPicture},
        {"release",                           "()V",                                        (void *) Java_wseemann_media_FFmpegMediaMetadataRetriever_release},
        {"setSurface",                        "(Ljava/lang/Object;)V",                      (void *) Java_wseemann_media_FFmpegMediaMetadataRetriever_setSurface},
        {"nativeFinalize",                   "()V",                                        (void *) Java_wseemann_media_FFmpegMediaMetadataRetriever_nativeFinalize},
        {"nativeSetup",                      "()V",                                        (void *) Java_wseemann_media_FFmpegMediaMetadataRetriever_nativeSetup},
        {"nativeInit",                       "()V",                                        (void *) Java_wseemann_media_FFmpegMediaMetadataRetriever_nativeInit},
        {"getEmbeddedPictureAndCopyToStream", "()Lwseemann/media/ImageInputStream;",        (void *) Java_wseemann_media_FFmpegMediaMetadataRetriever_getEmbeddedPictureAndCopyToStream},
        {"nativeCancelRequest",             "()V",                                       (void *) Java_wseemann_media_FFmpegMediaMetadataRetriever_nativeCancelRequest}



};

// This function only registers the native methods, and is called from
// JNI_OnLoad in wseemann_media_FFmpegMediaMetadataRetriever.cpp
int register_wseemann_media_FFmpegMediaMetadataRetriever(JNIEnv *env) {
    int numMethods = (sizeof(nativeMethods) / sizeof((nativeMethods)[0]));
    jclass clazz = env->FindClass("wseemann/media/FFmpegMediaMetadataRetriever");
    jint ret = env->RegisterNatives(clazz, nativeMethods, numMethods);
    env->DeleteLocalRef(clazz);
    return ret;
}

jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    m_vm = vm;
    JNIEnv *env = NULL;
    jint result = -1;

    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        goto bail;
    }
    assert(env != NULL);

    if (register_wseemann_media_FFmpegMediaMetadataRetriever(env) < 0) {
        goto bail;
    }

    /* success -- return valid version number */
    result = JNI_VERSION_1_6;

    bail:
    return result;
}
