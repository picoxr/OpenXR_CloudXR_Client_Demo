/*
    this file wrapped the cloudxr client
*/
#include <thread>
#include <chrono>
#include "CloudXRClientOptions.h"
#include <CloudXRMatrixHelpers.h>
#include "cloudXRClient.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include "logger.h"
#include "common/gfxwrapper_opengl.h"
#include "pController.h"

PFN_xrGetIPDPICO pfnXrGetIPDPICO = nullptr;

static CloudXR::ClientOptions s_options;

CloudXRClient::CloudXRClient(): mReceiver(nullptr), mClientState(cxrClientState_ReadyToConnect), mInstance(nullptr), mSystemId(0), mSession(nullptr) {
    mIsPaused = true;
    mWasPaused = true;
    mIPD = 0.060f;
    memset(mFramebuffers, 0, sizeof(mFramebuffers));
}

CloudXRClient::~CloudXRClient() {
}

void CloudXRClient::Initialize(XrInstance instance, XrSystemId systemId, XrSession session) {
    mInstance = instance;
    mSystemId = systemId;
    mSession = session;

    Log::Write(Log::Level::Info, Fmt("CloudXRClient::Initialize......"));

    XrResult ret = xrGetInstanceProcAddr(mInstance, "xrGetIPDPICO", reinterpret_cast<PFN_xrVoidFunction *>(&pfnXrGetIPDPICO));
    if (pfnXrGetIPDPICO == nullptr) {
        Log::Write(Log::Level::Error, Fmt("pfnXrGetIPDPICO:%p, ret:%d", pfnXrGetIPDPICO, ret));
        return;
    }

    pfnXrGetIPDPICO(mSession, &mIPD);
    Log::Write(Log::Level::Info, Fmt("pfnXrGetIPDPICO:%p, pid:%f", pfnXrGetIPDPICO, mIPD));

    s_options.ParseFile("/sdcard/CloudXRLaunchOptions.txt");

    mContext.type = cxrGraphicsContext_GLES;
    mContext.egl.display = eglGetCurrentDisplay();
    mContext.egl.context = eglGetCurrentContext();
    if (mContext.egl.context == nullptr) {
        Log::Write(Log::Level::Error, Fmt("Error, null context"));
        return;
    }
    if (mContext.egl.display == nullptr) {
        Log::Write(Log::Level::Error, Fmt("Error, null display"));
        return;
    }

    std::thread([=](){
        static uint64_t lastTimeMs = 0;
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            if (mWasPaused != mIsPaused) {
                mWasPaused = mIsPaused;
                if (mIsPaused == false && mClientState == cxrClientState_ReadyToConnect) {
                    Start();
                } else if (mIsPaused == true) {
                    Stop();
                }
            }

            if (mReceiver && mClientState == cxrClientState_StreamingSessionInProgress) {
                uint64_t nowTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();  //milliseconds
                // display network quality information pre second
                if (nowTimeMs - lastTimeMs >= 100) {
                    lastTimeMs = nowTimeMs;
                    cxrConnectionStats stats = {0};
                    cxrError ret = cxrGetConnectionStats(mReceiver, &stats);
                    if (ret == cxrError_Success) {
                        Log::Write(Log::Level::Info, Fmt("clientstats framesPerSecond:%f, frameDeliveryTime:%f, frameQueueTime:%f, frameLatchTime:%f", 
                            stats.framesPerSecond, stats.frameDeliveryTime, stats.frameQueueTime, stats.frameLatchTime));
                        Log::Write(Log::Level::Error, Fmt("bandKbps:%6d, bandwidthUtilizationKbps:%5d, bandUtilizationPercent:%d%%, roundTripDelayMs:%d, "
                            "jitterUs:%d, totalPacketsReceived:%d, totalPacketsLost:%d, totalPacketsDropped:%d, quality:%d, qualityReasons:%d",
                            stats.bandwidthAvailableKbps, stats.bandwidthUtilizationKbps, stats.bandwidthUtilizationPercent, stats.roundTripDelayMs,
                            stats.jitterUs, stats.totalPacketsReceived, stats.totalPacketsLost, stats.totalPacketsDropped, stats.quality, stats.qualityReasons));    
                    } else {
                        Log::Write(Log::Level::Error, Fmt("cxrGetConnectionStats error %d", ret));
                    }
                }
            }
        }

        Log::Write(Log::Level::Warning, Fmt("exit cloudxr thread ......"));
    }).detach(); 
}

bool CloudXRClient::Start() {
    Log::Write(Log::Level::Info, Fmt("CloudXRClient::Start ......"));
    return CreateReceiver();
}

void CloudXRClient::Stop() {
    Log::Write(Log::Level::Info, Fmt("CloudXRClient::Stop ......"));
    TeardownReceiver();
}

void CloudXRClient::SetPaused(bool pause) {
    Log::Write(Log::Level::Info, Fmt("SetPaused %d", pause));
    mIsPaused = pause;
    if (mIsPaused) {
        Stop();
    }
}

void CloudXRClient::SetSenserPoseState(XrPosef& pose, XrVector3f& linearVelocity, XrVector3f& angularVelocity, std::vector<XrPosef> &handPose) {
    std::lock_guard<std::mutex> guard(mPoseMutex);
    const float offsetHeight = 1.7f;
    mHeadPose = pose;
    mHeadPose.position.y += offsetHeight;
    mLinearVelocity = linearVelocity;
    mAngularVelocity = angularVelocity;

    mHandPose = handPose;
    for (auto &hand : mHandPose) {
        hand.position.y += offsetHeight;
    }

    /*Log::Write(Log::Level::Info, Fmt("set headpose x:%f, y:%f, x:%f,  x:%f, y:%f, x:%f, w:%f, linearVelocity x:%f, y:%f, x:%f, angularVelocity x:%f, y:%f, x:%f",
        mHeadPose.position.x, mHeadPose.position.y, mHeadPose.position.z,
        mHeadPose.orientation.x, mHeadPose.orientation.y, mHeadPose.orientation.z, mHeadPose.orientation.w,
        linearVelocity.x, linearVelocity.y, linearVelocity.z,
        angularVelocity.x, angularVelocity.y, angularVelocity.z));
        */      
}

void CloudXRClient::SetTrackingState(cxrVRTrackingState &trackingState) {
    for (uint32_t i = 0; i < CXR_NUM_CONTROLLERS; i++) {
        uint32_t booleanComps = mTrackingState.controller[i].booleanComps;
        mTrackingState.controller[i] = trackingState.controller[i];
        mTrackingState.controller[i].booleanCompsChanged = trackingState.controller[i].booleanComps ^ booleanComps;
    }
}

void CloudXRClient::GetTrackingState(cxrVRTrackingState *trackingState) {
    std::lock_guard<std::mutex> guard(mPoseMutex);
    ProcessControllers();

    mTrackingState.hmd.ipd = mIPD;
    // so we truncate the value to 5 decimal places (sub-millimeter precision)
    mTrackingState.hmd.ipd = truncf(mTrackingState.hmd.ipd * 10000.0f) / 10000.0f;
    mTrackingState.hmd.flags = 0; // reset dynamic flags every frame
    mTrackingState.hmd.flags |= cxrHmdTrackingFlags_HasIPD;

    mTrackingState.hmd.pose = ConvertPose(mHeadPose);
    mTrackingState.hmd.pose.poseIsValid = cxrTrue;
    mTrackingState.hmd.pose.deviceIsConnected = cxrTrue ;
    mTrackingState.hmd.pose.trackingResult = cxrTrackingResult_Running_OK;

    *trackingState = mTrackingState;
}

bool CloudXRClient::SetupFramebuffer(GLuint colorTexture, uint32_t eye, uint32_t width, uint32_t height) {
    if (mFramebuffers[eye] == 0) {
        GLuint framebuffer;
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
        GLenum status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            Log::Write(Log::Level::Error, Fmt("Incomplete frame buffer object, status:0x%x", status));
            return false;
        }
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        mFramebuffers[eye] = framebuffer;
        Log::Write(Log::Level::Info, Fmt("Created FBO %d for eye%d texture %d.", framebuffer, eye, colorTexture));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mFramebuffers[eye]);
    } else {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mFramebuffers[eye]);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
    }
    glViewport(0, 0, width, height);
    return true;
}

bool CloudXRClient::LatchFrame(cxrFramesLatched *framesLatched) {
    const uint32_t timeoutMs = 500;
    bool frameValid = false;
    if (mReceiver) {
        if (mClientState == cxrClientState_StreamingSessionInProgress) {
            cxrError frameErr = cxrLatchFrame(mReceiver, framesLatched, cxrFrameMask_All, timeoutMs);
            frameValid = (frameErr == cxrError_Success);
            if (!frameValid) {
                if (frameErr == cxrError_Frame_Not_Ready) {
                    Log::Write(Log::Level::Info, Fmt("Error in LatchFrame, frame not ready for %d ms", timeoutMs));
                } else {
                    Log::Write(Log::Level::Error, Fmt("Error in LatchFrame [%0d] = %s", frameErr, cxrErrorString(frameErr)));
                }
            }
        }
    }
    return frameValid;
}

void CloudXRClient::BlitFrame(cxrFramesLatched *framesLatched, bool frameValid, uint32_t eye) {
    if (frameValid) {
        cxrBlitFrame(mReceiver, framesLatched, 1 << eye);
    } else {
        FillBackground();
    }
}

void CloudXRClient::ReleaseFrame(cxrFramesLatched *framesLatched) {
    cxrReleaseFrame(mReceiver, framesLatched);
}

void CloudXRClient::FillBackground() {
    float cr = ((mBGColor & 0x00FF0000) >> 16) / 255.0f;
    float cg = ((mBGColor & 0x0000FF00) >> 8) / 255.0f;
    float cb = ((mBGColor & 0x000000FF)) / 255.0f;
    float ca = ((mBGColor & 0xFF000000) >> 24) / 255.0f;
    glClearColor(cr, cg, cb, ca);
    glClear(GL_COLOR_BUFFER_BIT);
}

bool CloudXRClient::CreateReceiver() {
    if (mReceiver) {
        return true;
    }
//    s_options.mServerIP = "192.168.1.110";
    if (s_options.mServerIP.empty()) {
        Log::Write(Log::Level::Error, Fmt("no server ip specifid!!!!!!"));
        return false;
    }    

    GetDeviceDesc(&mDeviceDesc);

    if (mDeviceDesc.receiveAudio) {
        // Initialize audio playback
        oboe::AudioStreamBuilder playbackStreamBuilder;
        playbackStreamBuilder.setDirection(oboe::Direction::Output);
        playbackStreamBuilder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
        playbackStreamBuilder.setSharingMode(oboe::SharingMode::Exclusive);
        playbackStreamBuilder.setFormat(oboe::AudioFormat::I16);
        playbackStreamBuilder.setChannelCount(oboe::ChannelCount::Stereo);
        playbackStreamBuilder.setSampleRate(CXR_AUDIO_SAMPLING_RATE);

        oboe::Result ret = playbackStreamBuilder.openStream(mPlaybackStream);
        if (ret != oboe::Result::OK) {
            Log::Write(Log::Level::Error, Fmt("Failed to open playback stream. Error: %s", oboe::convertToText(ret)));
            return cxrError_Failed;
        }

        int bufferSizeFrames = mPlaybackStream->getFramesPerBurst() * 2;
        ret = mPlaybackStream->setBufferSizeInFrames(bufferSizeFrames);
        if (ret != oboe::Result::OK) {
            Log::Write(Log::Level::Error, Fmt("Failed to set playback stream buffer size to: %d. Error: %s", bufferSizeFrames, oboe::convertToText(ret)));
            return cxrError_Failed;
        }

        ret = mPlaybackStream->start();
        if (ret != oboe::Result::OK) {
            Log::Write(Log::Level::Error, Fmt("Failed to start playback stream. Error: %s", oboe::convertToText(ret)));
            return cxrError_Failed;
        }
    }

    Log::Write(Log::Level::Info, Fmt("Trying to create Receiver at %s.", s_options.mServerIP.c_str()));

    cxrClientCallbacks clientProxy = {nullptr};
    clientProxy.GetTrackingState = [](void *context, cxrVRTrackingState *trackingState) {
        return reinterpret_cast<CloudXRClient*>(context)->GetTrackingState(trackingState);
    };

    clientProxy.TriggerHaptic = [](void *context, const cxrHapticFeedback *haptic) {
        auto &haptic1 = const_cast<cxrHapticFeedback &>(*haptic);
        return reinterpret_cast<CloudXRClient*>(context)->TriggerHaptic(haptic);
    };

    clientProxy.RenderAudio = [](void *context, const cxrAudioFrame *audioFrame) {
        return reinterpret_cast<CloudXRClient*>(context)->RenderAudio(audioFrame);
    };

    //the client_lib calls into here when the async connection status changes
    clientProxy.UpdateClientState = [](void *context, cxrClientState state, cxrStateReason reason) {
        switch (state) {
            case cxrClientState_ReadyToConnect:
                Log::Write(Log::Level::Info, Fmt("ready to connect......"));
                break;
            case cxrClientState_ConnectionAttemptInProgress:
                Log::Write(Log::Level::Info, Fmt("Connection attempt in progress......"));
                break;
            case cxrClientState_ConnectionAttemptFailed:
                Log::Write(Log::Level::Error, Fmt("Connection attempt failed. [%i]", reason));
                break;
            case cxrClientState_StreamingSessionInProgress:
                Log::Write(Log::Level::Info, Fmt("Async connection succeeded."));
                break;
            case cxrClientState_Disconnected:
                Log::Write(Log::Level::Error, Fmt("Server disconnected with reason: %d", reason));
                break;
            default:
                Log::Write(Log::Level::Error, Fmt("Client state updated: %d, reason: %d", state, reason));
                break;
        }
        reinterpret_cast<CloudXRClient *>(context)->mClientState = state;
    };

    cxrReceiverDesc desc = {0};
    desc.requestedVersion = CLOUDXR_VERSION_DWORD;
    desc.deviceDesc = mDeviceDesc;
    desc.clientCallbacks = clientProxy;
    desc.clientContext = this;
    desc.shareContext = &mContext;
    desc.numStreams = 2;
    desc.receiverMode = cxrStreamingMode_XR;
    desc.debugFlags = s_options.mDebugFlags;
    desc.debugFlags |= cxrDebugFlags_EnableAImageReaderDecoder + cxrDebugFlags_LogVerbose;
    desc.logMaxSizeKB = CLOUDXR_LOG_MAX_DEFAULT;
    desc.logMaxAgeDays = CLOUDXR_LOG_MAX_DEFAULT;

    cxrError err = cxrCreateReceiver(&desc, &mReceiver);
    if (err != cxrError_Success) {
        Log::Write(Log::Level::Error, Fmt("Failed to create CloudXR receiver. Error %d, %s.", err, cxrErrorString(err)));
        return false;
    }
    Log::Write(Log::Level::Info, Fmt("cxrCreateReceiver mReceiver:%p", mReceiver));

    mConnectionDesc.async = cxrTrue;
    mConnectionDesc.maxVideoBitrateKbps = s_options.mMaxVideoBitrate;
    mConnectionDesc.clientNetwork = s_options.mClientNetwork;
    mConnectionDesc.topology = s_options.mTopology;
    err = cxrConnect(mReceiver, s_options.mServerIP.c_str(), &mConnectionDesc);
    if (!mConnectionDesc.async) {
        if (err != cxrError_Success) {
            Log::Write(Log::Level::Error, Fmt("Failed to connect to CloudXR server at %s. Error %d, %s.", s_options.mServerIP.c_str(), (int) err, cxrErrorString(err)));
            TeardownReceiver();
            return false;
        } else {
            mClientState = cxrClientState_StreamingSessionInProgress;
            Log::Write(Log::Level::Info, Fmt("Receiver created for server: %s", s_options.mServerIP.c_str()));
        }
    }
    return true;
}

void CloudXRClient::TeardownReceiver() {
    Log::Write(Log::Level::Info, Fmt("TeardownReceiver..."));
    if (mClientState == cxrClientState_ReadyToConnect) {
        return;
    }
    mClientState = cxrClientState_ReadyToConnect;
    if (mPlaybackStream) {
        mPlaybackStream->stop();
    }
    if (mReceiver != nullptr) {
        cxrDestroyReceiver(mReceiver);
        mReceiver = nullptr;
    }
}

void CloudXRClient::GetDeviceDesc(cxrDeviceDesc *desc) const {
    int32_t defaultFoveation = 0;
    char buffer[128] = {0};
    __system_property_get("ro.product.model", buffer);
    if (std::string(buffer) == "Pico Neo 3") {
        // the fov value from tested
        defaultFoveation = 88;
    }
    Log::Write(Log::Level::Info, Fmt("ro.product.model:%s, default foveation:%d", buffer, defaultFoveation));

    uint32_t viewCount = 0;
    std::vector<XrViewConfigurationView> configViews;
    xrEnumerateViewConfigurationViews(mInstance, mSystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr);
    configViews.resize(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    xrEnumerateViewConfigurationViews(mInstance, mSystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, viewCount, &viewCount, configViews.data());

    for (int i =0 ;i < viewCount; i++) {
        Log::Write(Log::Level::Info, Fmt("viewCount:%d, maxImageRectWidth:%d, maxImageRectHeight:%d, recommendedImageRectWidth:%d, recommendedImageRectHeight:%d", i,
                                         configViews[i].maxImageRectWidth,configViews[i].maxImageRectHeight, configViews[i].recommendedImageRectWidth, configViews[i].recommendedImageRectHeight));
    }

    desc->deliveryType = cxrDeliveryType_Stereo_RGB;
    desc->width = configViews[0].recommendedImageRectWidth;
    desc->height = configViews[0].recommendedImageRectHeight;
    desc->fps = 90;
    desc->ipd = mIPD;
    desc->predOffset = -0.02f;
    desc->receiveAudio = true;
    desc->sendAudio = false;
    desc->posePollFreq = 0;
    desc->ctrlType = cxrControllerType_OculusTouch;
    desc->disablePosePrediction = false;
    desc->angularVelocityInDeviceSpace = false;
    desc->disableVVSync = false;
    desc->foveatedScaleFactor = (s_options.mFoveation > 0 && s_options.mFoveation < 100) ? s_options.mFoveation : defaultFoveation;
    desc->maxResFactor = 1.0f;

    desc->proj[0][0] = -1.25;
    desc->proj[0][1] = 1.25;
    desc->proj[0][2] = -1.25;
    desc->proj[0][3] = 1.25;

    desc->proj[1][0] = desc->proj[0][0];
    desc->proj[1][1] = desc->proj[0][1];

    desc->proj[1][2] = desc->proj[0][2];
    desc->proj[1][3] = desc->proj[0][3];

    // cxrChaperone chap;
    desc->chaperone.universe = cxrUniverseOrigin_Standing;
    desc->chaperone.origin.m[0][0] = desc->chaperone.origin.m[1][1] = desc->chaperone.origin.m[2][2] = 1;
    desc->chaperone.origin.m[0][1] = desc->chaperone.origin.m[0][2] = desc->chaperone.origin.m[0][3] = 0;
    desc->chaperone.origin.m[1][0] = desc->chaperone.origin.m[1][2] = desc->chaperone.origin.m[1][3] = 0;
    desc->chaperone.origin.m[2][0] = desc->chaperone.origin.m[2][1] = desc->chaperone.origin.m[2][3] = 0;
    desc->chaperone.playArea.v[0] = 2.f * 1.5f * 0.5f;
    desc->chaperone.playArea.v[1] = 2.f * 1.5f * 0.5f;
    Log::Write(Log::Level::Info, Fmt("Setting play area to %0.2f x %0.2f", desc->chaperone.playArea.v[0], desc->chaperone.playArea.v[1]));
}

XrQuaternionf CloudXRClient::cxrToQuaternion(const cxrMatrix34 &m) {
    XrQuaternionf q;
    const float trace = m.m[0][0] + m.m[1][1] + m.m[2][2];
    if (trace > 0.f) {
        float s = 0.5f / sqrtf(trace + 1.0f);
        q.w = 0.25f / s;
        q.x = (m.m[2][1] - m.m[1][2]) * s;
        q.y = (m.m[0][2] - m.m[2][0]) * s;
        q.z = (m.m[1][0] - m.m[0][1]) * s;
    } else {
        if (m.m[0][0] > m.m[1][1] && m.m[0][0] > m.m[2][2]) {
            float s = 2.0f * sqrtf(1.0f + m.m[0][0] - m.m[1][1] - m.m[2][2]);
            q.w = (m.m[2][1] - m.m[1][2]) / s;
            q.x = 0.25f * s;
            q.y = (m.m[0][1] + m.m[1][0]) / s;
            q.z = (m.m[0][2] + m.m[2][0]) / s;
        } else if (m.m[1][1] > m.m[2][2]) {
            float s = 2.0f * sqrtf(1.0f + m.m[1][1] - m.m[0][0] - m.m[2][2]);
            q.w = (m.m[0][2] - m.m[2][0]) / s;
            q.x = (m.m[0][1] + m.m[1][0]) / s;
            q.y = 0.25f * s;
            q.z = (m.m[1][2] + m.m[2][1]) / s;
        } else {
            float s = 2.0f * sqrtf(1.0f + m.m[2][2] - m.m[0][0] - m.m[1][1]);
            q.w = (m.m[1][0] - m.m[0][1]) / s;
            q.x = (m.m[0][2] + m.m[2][0]) / s;
            q.y = (m.m[1][2] + m.m[2][1]) / s;
            q.z = 0.25f * s;
        }
    }
    return q;
}

XrVector3f CloudXRClient::cxrGetTranslation(const cxrMatrix34 &m) {
    return {m.m[0][3], m.m[1][3], m.m[2][3]};
}

/// Returns the 4x4 rotation matrix for the given quaternion.
static inline pxrMatrix4f CreateFromQuaternion(const XrQuaternionf* q) {
    const float ww = q->w * q->w;
    const float xx = q->x * q->x;
    const float yy = q->y * q->y;
    const float zz = q->z * q->z;

    pxrMatrix4f out;
    out.M[0][0] = ww + xx - yy - zz;
    out.M[0][1] = 2 * (q->x * q->y - q->w * q->z);
    out.M[0][2] = 2 * (q->x * q->z + q->w * q->y);
    out.M[0][3] = 0;

    out.M[1][0] = 2 * (q->x * q->y + q->w * q->z);
    out.M[1][1] = ww - xx + yy - zz;
    out.M[1][2] = 2 * (q->y * q->z - q->w * q->x);
    out.M[1][3] = 0;

    out.M[2][0] = 2 * (q->x * q->z - q->w * q->y);
    out.M[2][1] = 2 * (q->y * q->z + q->w * q->x);
    out.M[2][2] = ww - xx - yy + zz;
    out.M[2][3] = 0;

    out.M[3][0] = 0;
    out.M[3][1] = 0;
    out.M[3][2] = 0;
    out.M[3][3] = 1;
    return out;
}

XrVector3f cxrGetTranslation(const cxrMatrix34 &m) {
    return {m.m[0][3], m.m[1][3], m.m[2][3]};
}

cxrMatrix34 cxrConvert(const pxrMatrix4f &m) {
    cxrMatrix34 out{};
    // The matrices are compatible so doing a memcpy() here
    //  noting that we are a [3][4] and ovr uses [4][4]
    memcpy(&out, &m, sizeof(out));
    return out;
}

cxrVector3 cxrConvert(const XrVector3f &v) {
    return {{v.x / 1000, v.y / 1000, v.z / 1000}};
}

/// Use left-multiplication to accumulate transformations.
static inline pxrMatrix4f Matrix4f_Multiply(const pxrMatrix4f* a, const pxrMatrix4f* b) {
    pxrMatrix4f out;
    out.M[0][0] = a->M[0][0] * b->M[0][0] + a->M[0][1] * b->M[1][0] + a->M[0][2] * b->M[2][0] +
                  a->M[0][3] * b->M[3][0];
    out.M[1][0] = a->M[1][0] * b->M[0][0] + a->M[1][1] * b->M[1][0] + a->M[1][2] * b->M[2][0] +
                  a->M[1][3] * b->M[3][0];
    out.M[2][0] = a->M[2][0] * b->M[0][0] + a->M[2][1] * b->M[1][0] + a->M[2][2] * b->M[2][0] +
                  a->M[2][3] * b->M[3][0];
    out.M[3][0] = a->M[3][0] * b->M[0][0] + a->M[3][1] * b->M[1][0] + a->M[3][2] * b->M[2][0] +
                  a->M[3][3] * b->M[3][0];

    out.M[0][1] = a->M[0][0] * b->M[0][1] + a->M[0][1] * b->M[1][1] + a->M[0][2] * b->M[2][1] +
                  a->M[0][3] * b->M[3][1];
    out.M[1][1] = a->M[1][0] * b->M[0][1] + a->M[1][1] * b->M[1][1] + a->M[1][2] * b->M[2][1] +
                  a->M[1][3] * b->M[3][1];
    out.M[2][1] = a->M[2][0] * b->M[0][1] + a->M[2][1] * b->M[1][1] + a->M[2][2] * b->M[2][1] +
                  a->M[2][3] * b->M[3][1];
    out.M[3][1] = a->M[3][0] * b->M[0][1] + a->M[3][1] * b->M[1][1] + a->M[3][2] * b->M[2][1] +
                  a->M[3][3] * b->M[3][1];

    out.M[0][2] = a->M[0][0] * b->M[0][2] + a->M[0][1] * b->M[1][2] + a->M[0][2] * b->M[2][2] +
                  a->M[0][3] * b->M[3][2];
    out.M[1][2] = a->M[1][0] * b->M[0][2] + a->M[1][1] * b->M[1][2] + a->M[1][2] * b->M[2][2] +
                  a->M[1][3] * b->M[3][2];
    out.M[2][2] = a->M[2][0] * b->M[0][2] + a->M[2][1] * b->M[1][2] + a->M[2][2] * b->M[2][2] +
                  a->M[2][3] * b->M[3][2];
    out.M[3][2] = a->M[3][0] * b->M[0][2] + a->M[3][1] * b->M[1][2] + a->M[3][2] * b->M[2][2] +
                  a->M[3][3] * b->M[3][2];

    out.M[0][3] = a->M[0][0] * b->M[0][3] + a->M[0][1] * b->M[1][3] + a->M[0][2] * b->M[2][3] +
                  a->M[0][3] * b->M[3][3];
    out.M[1][3] = a->M[1][0] * b->M[0][3] + a->M[1][1] * b->M[1][3] + a->M[1][2] * b->M[2][3] +
                  a->M[1][3] * b->M[3][3];
    out.M[2][3] = a->M[2][0] * b->M[0][3] + a->M[2][1] * b->M[1][3] + a->M[2][2] * b->M[2][3] +
                  a->M[2][3] * b->M[3][3];
    out.M[3][3] = a->M[3][0] * b->M[0][3] + a->M[3][1] * b->M[1][3] + a->M[3][2] * b->M[2][3] +
                  a->M[3][3] * b->M[3][3];
    return out;
}

/// Returns a 4x4 homogeneous translation matrix.
static inline pxrMatrix4f CreateTranslation(const float x, const float y, const float z) {
    pxrMatrix4f out;
    out.M[0][0] = 1.0f;
    out.M[0][1] = 0.0f;
    out.M[0][2] = 0.0f;
    out.M[0][3] = x;
    out.M[1][0] = 0.0f;
    out.M[1][1] = 1.0f;
    out.M[1][2] = 0.0f;
    out.M[1][3] = y;
    out.M[2][0] = 0.0f;
    out.M[2][1] = 0.0f;
    out.M[2][2] = 1.0f;
    out.M[2][3] = z;
    out.M[3][0] = 0.0f;
    out.M[3][1] = 0.0f;
    out.M[3][2] = 0.0f;
    out.M[3][3] = 1.0f;
    return out;
}

/// Returns a 4x4 homogeneous rotation matrix.
static inline pxrMatrix4f CreateRotation(const float radiansX, const float radiansY, const float radiansZ) {
    const float sinX = sinf(radiansX);
    const float cosX = cosf(radiansX);
    const pxrMatrix4f rotationX = {{{1, 0, 0, 0}, {0, cosX, -sinX, 0}, {0, sinX, cosX, 0}, {0, 0, 0, 1}}};
    const float sinY = sinf(radiansY);
    const float cosY = cosf(radiansY);
    const pxrMatrix4f rotationY = {{{cosY, 0, sinY, 0}, {0, 1, 0, 0}, {-sinY, 0, cosY, 0}, {0, 0, 0, 1}}};
    const float sinZ = sinf(radiansZ);
    const float cosZ = cosf(radiansZ);
    const pxrMatrix4f rotationZ = {{{cosZ, -sinZ, 0, 0}, {sinZ, cosZ, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
    const pxrMatrix4f rotationXY = Matrix4f_Multiply(&rotationY, &rotationX);
    return Matrix4f_Multiply(&rotationZ, &rotationXY);
}

static inline pxrMatrix4f GetTransformFromPose(const XrPosef* pose) {
    const pxrMatrix4f rotation = CreateFromQuaternion(&pose->orientation);
    const pxrMatrix4f translation = CreateTranslation(pose->position.x, pose->position.y, pose->position.z);
    return Matrix4f_Multiply(&translation, &rotation);
}

cxrTrackedDevicePose CloudXRClient::ConvertPose(const XrPosef& inPose, float rotationX) {
    pxrMatrix4f transform = GetTransformFromPose(&inPose);
    if (rotationX) {
        const pxrMatrix4f rotation = CreateRotation(rotationX, 0, 0);
        transform = Matrix4f_Multiply(&transform, &rotation);
    }

    cxrTrackedDevicePose TrackedPose{};
    cxrMatrix34 m = cxrConvert(transform);
    cxrMatrixToVecQuat(&m, &TrackedPose.position, &TrackedPose.rotation);
    TrackedPose.velocity = cxrConvert(mLinearVelocity);
    TrackedPose.angularVelocity = cxrConvert(mAngularVelocity);

    TrackedPose.poseIsValid = cxrTrue;

    return TrackedPose;
}

void CloudXRClient::ProcessControllers() {
    for (int32_t hand = 0; hand < mHandPose.size(); hand++) {
        XrPosef &pose = mHandPose[hand];
        mTrackingState.controller[hand].pose = ConvertPose(pose, 0.45f);
        mTrackingState.controller[hand].pose.deviceIsConnected = cxrTrue;
        mTrackingState.controller[hand].pose.trackingResult = cxrTrackingResult_Running_OK;
    }
}

void CloudXRClient::TriggerHaptic(const cxrHapticFeedback *hapticFeedback) {
    const cxrHapticFeedback &haptic = *hapticFeedback;
    if (haptic.seconds <= 0) {
        return;
    }
    pxr::Pxr_VibrateController(haptic.amplitude, haptic.seconds * 1000, haptic.controllerIdx);
}

cxrBool CloudXRClient::RenderAudio(const cxrAudioFrame *audioFrame) {
    if (!mPlaybackStream.get()) {
        return cxrFalse;
    }
    const uint32_t timeout = audioFrame->streamSizeBytes / CXR_AUDIO_BYTES_PER_MS;
    const uint32_t numFrames = timeout * CXR_AUDIO_SAMPLING_RATE / 1000;
    mPlaybackStream->write(audioFrame->streamBuffer, numFrames, timeout * oboe::kNanosPerMillisecond);
    return cxrTrue;
}

oboe::DataCallbackResult CloudXRClient::onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) {
    return oboe::DataCallbackResult::Continue;
}
