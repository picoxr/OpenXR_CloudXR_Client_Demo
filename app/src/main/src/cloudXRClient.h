/* Created by kevin at 2022/11/7
  this file wrapped the cloudxr client
*/

#pragma once
#include "pch.h"
#include "common.h"
#include <oboe/Oboe.h>
#include <CloudXRClient.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <map>
#include <memory>
#include <mutex>

class CloudXRClient : public oboe::AudioStreamDataCallback {
public:

    CloudXRClient();

    ~CloudXRClient();

    void Initialize(XrInstance instance, XrSystemId systemId, XrSession session);

    void SetPaused(bool pause);

    bool LatchFrame(cxrFramesLatched *framesLatched);

    void BlitFrame(cxrFramesLatched *framesLatched, bool frameValid, uint32_t eye);

    void ReleaseFrame(cxrFramesLatched *framesLatched);

    void SetSenserPoseState(XrPosef& pose, XrVector3f& linearVelocity, XrVector3f& angularVelocity, std::vector<XrPosef> &handPose);

    XrQuaternionf cxrToQuaternion(const cxrMatrix34 &m);

    XrVector3f cxrGetTranslation(const cxrMatrix34 &m);

    void SetTrackingState(cxrVRTrackingState &trackingState);

private:

    bool Start();

    void Stop();

    // AudioStreamDataCallback interface
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) override;

    bool CreateReceiver();

    void TeardownReceiver();

    void GetDeviceDesc(cxrDeviceDesc *params) const;

    void GetTrackingState(cxrVRTrackingState *trackingState);

    void ProcessControllers();

    void TriggerHaptic(const cxrHapticFeedback *);

    cxrTrackedDevicePose ConvertPose(const XrPosef& inPose, float rotationX = 0);

    cxrBool RenderAudio(const cxrAudioFrame *audioFrame);

    bool SetupFramebuffer(GLuint colorTexture, uint32_t eye, uint32_t width, uint32_t height);

    void FillBackground();

private:
    cxrReceiverHandle mReceiver;
    cxrClientState mClientState;
    cxrDeviceDesc mDeviceDesc;
    cxrConnectionDesc mConnectionDesc;
    cxrGraphicsContext mContext;
    cxrVRTrackingState mTrackingState;

    XrInstance mInstance;
    XrSystemId mSystemId;
    XrSession  mSession;

    std::mutex mPoseMutex;
    XrPosef    mHeadPose;
    XrVector3f mLinearVelocity;
    XrVector3f mAngularVelocity;
    std::map<uint64_t, std::vector<XrView>> mPoseViewsMap;
    std::vector<XrPosef> mHandPose;

    std::shared_ptr<oboe::AudioStream> mPlaybackStream;

    bool mIsPaused;
    bool mWasPaused;

    float mIPD;

    GLuint mFramebuffers[2];

    uint32_t mDefaultBGColor = 0xFF000000; // black to start until we set around OnResume.
    uint32_t mBGColor = mDefaultBGColor;
};

typedef enum {
    NONE = -1,
    PXR_Home = 0,
    PXR_BtnX = 1,
    PXR_BtnY = 2,
    PXR_BtnA = 3,
    PXR_BtnB = 4,
    PXR_Trigger = 5,
    PXR_Grip = 6,
    PXR_Joystick = 7,
    PXR_Touch_Trigger = 8,
    PXR_Touchpad = 9,
} PxrInputId;

// Row-major 4x4 matrix.
typedef struct pxrMatrix4f_ {
    float M[4][4];
} pxrMatrix4f;

struct PxrCxrButtonMapping {
    unsigned int pxrId;
    cxrButtonId cxrId;
    char nameStr[32];
};
