// Copyright (c) 2017-2022, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "pch.h"
#include "common.h"
#include "options.h"
#include "platformdata.h"
#include "platformplugin.h"
#include "graphicsplugin.h"
#include "openxr_program.h"
#include <common/xr_linear.h>
#include <array>
#include <cmath>
#include <math.h>
#include "cloudXRClient.h"

namespace {

    ////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////
#ifndef CLOUDXR3_5
    //-----------------------------------------------------------------------------
    static constexpr int inputCountQuest = 21;

    static const char* inputPathsQuest[inputCountQuest] =
            {
                    "/input/system/click",
                    "/input/application_menu/click", // this is carried over from old system and might be remove, it's not a button binding, more action.
                    "/input/trigger/click",
                    "/input/trigger/touch",
                    "/input/trigger/value",
                    "/input/grip/click",
                    "/input/grip/touch",
                    "/input/grip/value",
                    "/input/joystick/click",
                    "/input/joystick/touch",
                    "/input/joystick/x",
                    "/input/joystick/y",
                    "/input/a/click",
                    "/input/b/click",
                    "/input/x/click", // Touch has X/Y on L controller, so we'll map the raw strings.
                    "/input/y/click",
                    "/input/a/touch",
                    "/input/b/touch",
                    "/input/x/touch",
                    "/input/y/touch",
                    "/input/thumb_rest/touch",
            };

    cxrInputValueType inputValueTypesQuest[inputCountQuest] =
            {
                    cxrInputValueType_boolean, //input/system/click
                    cxrInputValueType_boolean, //input/application_menu/click
                    cxrInputValueType_boolean, //input/trigger/click
                    cxrInputValueType_boolean, //input/trigger/touch
                    cxrInputValueType_float32, //input/trigger/value
                    cxrInputValueType_boolean, //input/grip/click
                    cxrInputValueType_boolean, //input/grip/touch
                    cxrInputValueType_float32, //input/grip/value
                    cxrInputValueType_boolean, //input/joystick/click
                    cxrInputValueType_boolean, //input/joystick/touch
                    cxrInputValueType_float32, //input/joystick/x
                    cxrInputValueType_float32, //input/joystick/y
                    cxrInputValueType_boolean, //input/a/click
                    cxrInputValueType_boolean, //input/b/click
                    cxrInputValueType_boolean, //input/x/click
                    cxrInputValueType_boolean, //input/y/click
                    cxrInputValueType_boolean, //input/a/touch
                    cxrInputValueType_boolean, //input/b/touch
                    cxrInputValueType_boolean, //input/x/touch
                    cxrInputValueType_boolean, //input/y/touch
                    cxrInputValueType_boolean, //input/thumb_rest/touch
            };
#endif
    ////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////

    typedef enum {
    DeviceTypeNone = 0,
    DeviceTypeNeo3,
    DeviceTypeNeo3Pro,
    DeviceTypeNeo3ProEye,
    DeviceTypePico4,
    DeviceTypePico4Pro,
}DeviceType;

#if !defined(XR_USE_PLATFORM_WIN32)
#define strcpy_s(dest, source) strncpy((dest), (source), sizeof(dest))
#endif

namespace Side {
const int LEFT = 0;
const int RIGHT = 1;
const int COUNT = 2;
}  // namespace Side

inline std::string GetXrVersionString(XrVersion ver) {
    return Fmt("%d.%d.%d", XR_VERSION_MAJOR(ver), XR_VERSION_MINOR(ver), XR_VERSION_PATCH(ver));
}

namespace Math {
namespace Pose {
XrPosef Identity() {
    XrPosef t{};
    t.orientation.w = 1;
    return t;
}

XrPosef Translation(const XrVector3f& translation) {
    XrPosef t = Identity();
    t.position = translation;
    return t;
}

XrPosef RotateCCWAboutYAxis(float radians, XrVector3f translation) {
    XrPosef t = Identity();
    t.orientation.x = 0.f;
    t.orientation.y = std::sin(radians * 0.5f);
    t.orientation.z = 0.f;
    t.orientation.w = std::cos(radians * 0.5f);
    t.position = translation;
    return t;
}
}  // namespace Pose
}  // namespace Math

inline XrReferenceSpaceCreateInfo GetXrReferenceSpaceCreateInfo(const std::string& referenceSpaceTypeStr) {
    XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::Identity();
    if (EqualsIgnoreCase(referenceSpaceTypeStr, "View")) {
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "ViewFront")) {
        // Render head-locked 2m in front of device.
        referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::Translation({0.f, 0.f, -2.f}),
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "Local")) {
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "Stage")) {
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "StageLeft")) {
        referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::RotateCCWAboutYAxis(0.f, {-2.f, 0.f, -2.f});
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "StageRight")) {
        referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::RotateCCWAboutYAxis(0.f, {2.f, 0.f, -2.f});
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "StageLeftRotated")) {
        referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::RotateCCWAboutYAxis(3.14f / 3.f, {-2.f, 0.5f, -2.f});
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "StageRightRotated")) {
        referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::RotateCCWAboutYAxis(-3.14f / 3.f, {2.f, 0.5f, -2.f});
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    } else {
        throw std::invalid_argument(Fmt("Unknown reference space type '%s'", referenceSpaceTypeStr.c_str()));
    }
    return referenceSpaceCreateInfo;
}

#ifndef CLOUDXR3_5
    const int MAX_CONTROLLERS = 2;
    cxrControllerHandle     m_newControllers[MAX_CONTROLLERS] = {};
#endif

struct OpenXrProgram : IOpenXrProgram {
    OpenXrProgram(const std::shared_ptr<Options>& options, const std::shared_ptr<IPlatformPlugin>& platformPlugin,
                  const std::shared_ptr<IGraphicsPlugin>& graphicsPlugin)
        : m_options(*options), m_platformPlugin(platformPlugin), m_graphicsPlugin(graphicsPlugin), m_isSupport_epic_view_configuration_fov_extention(false) {}

    ~OpenXrProgram() override {
        if (m_input.actionSet != XR_NULL_HANDLE) {
            for (auto hand : {Side::LEFT, Side::RIGHT}) {
                xrDestroySpace(m_input.handSpace[hand]);
                xrDestroySpace(m_input.aimSpace[hand]);
            }
            xrDestroyActionSet(m_input.actionSet);
        }

        for (Swapchain swapchain : m_swapchains) {
            xrDestroySwapchain(swapchain.handle);
        }

        if (m_appSpace != XR_NULL_HANDLE) {
            xrDestroySpace(m_appSpace);
        }

        if (m_session != XR_NULL_HANDLE) {
            xrDestroySession(m_session);
        }

        if (m_instance != XR_NULL_HANDLE) {
            xrDestroyInstance(m_instance);
        }
    }

    void LogLayersAndExtensions() {
        // Write out extension properties for a given layer.
        const auto logExtensions = [&](const char* layerName, int indent = 0) {
            uint32_t instanceExtensionCount;
            CHECK_XRCMD(xrEnumerateInstanceExtensionProperties(layerName, 0, &instanceExtensionCount, nullptr));

            std::vector<XrExtensionProperties> extensions(instanceExtensionCount);
            for (XrExtensionProperties& extension : extensions) {
                extension.type = XR_TYPE_EXTENSION_PROPERTIES;
            }

            CHECK_XRCMD(xrEnumerateInstanceExtensionProperties(layerName, (uint32_t)extensions.size(), &instanceExtensionCount, extensions.data()));

            const std::string indentStr(indent, ' ');
            Log::Write(Log::Level::Info, Fmt("%sAvailable Extensions: (%d)", indentStr.c_str(), instanceExtensionCount));
            for (const XrExtensionProperties& extension : extensions) {
                Log::Write(Log::Level::Info, Fmt("%sAvailable Extensions:  Name=%s version=%d", indentStr.c_str(), extension.extensionName, extension.extensionVersion));
                if (strstr(extension.extensionName, XR_EPIC_VIEW_CONFIGURATION_FOV_EXTENSION_NAME)) {
                    m_isSupport_epic_view_configuration_fov_extention = true;
                }
            }
        };

        // Log non-layer extensions (layerName==nullptr).
        logExtensions(nullptr);

        // Log layers and any of their extensions.
        {
            uint32_t layerCount;
            CHECK_XRCMD(xrEnumerateApiLayerProperties(0, &layerCount, nullptr));

            std::vector<XrApiLayerProperties> layers(layerCount);
            for (XrApiLayerProperties& layer : layers) {
                layer.type = XR_TYPE_API_LAYER_PROPERTIES;
            }

            CHECK_XRCMD(xrEnumerateApiLayerProperties((uint32_t)layers.size(), &layerCount, layers.data()));

            Log::Write(Log::Level::Info, Fmt("Available Layers: (%d)", layerCount));
            for (const XrApiLayerProperties& layer : layers) {
                Log::Write(Log::Level::Verbose,
                           Fmt("  Name=%s SpecVersion=%s LayerVersion=%d Description=%s", layer.layerName,
                               GetXrVersionString(layer.specVersion).c_str(), layer.layerVersion, layer.description));
                logExtensions(layer.layerName, 4);
            }
        }
    }

    void LogInstanceInfo() {
        CHECK(m_instance != XR_NULL_HANDLE);

        XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
        CHECK_XRCMD(xrGetInstanceProperties(m_instance, &instanceProperties));

        Log::Write(Log::Level::Info, Fmt("Instance RuntimeName=%s RuntimeVersion=%s", instanceProperties.runtimeName,
                                         GetXrVersionString(instanceProperties.runtimeVersion).c_str()));
    }

    void CreateInstanceInternal() {
        CHECK(m_instance == XR_NULL_HANDLE);

        // Create union of extensions required by platform and graphics plugins.
        std::vector<const char*> extensions;
        // Transform platform and graphics extension std::strings to C strings.
        const std::vector<std::string> platformExtensions = m_platformPlugin->GetInstanceExtensions();
        std::transform(platformExtensions.begin(), platformExtensions.end(), std::back_inserter(extensions),
                       [](const std::string& ext) { return ext.c_str(); });
        const std::vector<std::string> graphicsExtensions = m_graphicsPlugin->GetInstanceExtensions();
        std::transform(graphicsExtensions.begin(), graphicsExtensions.end(), std::back_inserter(extensions),
                       [](const std::string& ext) { return ext.c_str(); });

        extensions.push_back(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME);

        if (m_isSupport_epic_view_configuration_fov_extention) {
            extensions.push_back(XR_EPIC_VIEW_CONFIGURATION_FOV_EXTENSION_NAME);
        }

        XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
        createInfo.next = m_platformPlugin->GetInstanceCreateExtension();
        createInfo.enabledExtensionCount = (uint32_t)extensions.size();
        createInfo.enabledExtensionNames = extensions.data();
        
        strcpy(createInfo.applicationInfo.applicationName, "CloudXR");
        createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

        CHECK_XRCMD(xrCreateInstance(&createInfo, &m_instance));
    }

    void CreateInstance() override {
        LogLayersAndExtensions();
        CreateInstanceInternal();
        LogInstanceInfo();
    }

    void LogViewConfigurations() {
        CHECK(m_instance != XR_NULL_HANDLE);
        CHECK(m_systemId != XR_NULL_SYSTEM_ID);

        uint32_t viewConfigTypeCount;
        CHECK_XRCMD(xrEnumerateViewConfigurations(m_instance, m_systemId, 0, &viewConfigTypeCount, nullptr));
        std::vector<XrViewConfigurationType> viewConfigTypes(viewConfigTypeCount);
        CHECK_XRCMD(xrEnumerateViewConfigurations(m_instance, m_systemId, viewConfigTypeCount, &viewConfigTypeCount,
                                                  viewConfigTypes.data()));
        CHECK((uint32_t)viewConfigTypes.size() == viewConfigTypeCount);

        Log::Write(Log::Level::Info, Fmt("Available View Configuration Types: (%d)", viewConfigTypeCount));
        for (XrViewConfigurationType viewConfigType : viewConfigTypes) {
            Log::Write(Log::Level::Verbose, Fmt("  View Configuration Type: %s %s", to_string(viewConfigType),
                                                viewConfigType == m_options.Parsed.ViewConfigType ? "(Selected)" : ""));

            XrViewConfigurationProperties viewConfigProperties{XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};
            CHECK_XRCMD(xrGetViewConfigurationProperties(m_instance, m_systemId, viewConfigType, &viewConfigProperties));

            Log::Write(Log::Level::Verbose,
                       Fmt("  View configuration FovMutable=%s", viewConfigProperties.fovMutable == XR_TRUE ? "True" : "False"));

            uint32_t viewCount;
            CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, viewConfigType, 0, &viewCount, nullptr));
            if (viewCount > 0) {
                std::vector<XrViewConfigurationView> views(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
                CHECK_XRCMD(
                    xrEnumerateViewConfigurationViews(m_instance, m_systemId, viewConfigType, viewCount, &viewCount, views.data()));

                for (uint32_t i = 0; i < views.size(); i++) {
                    const XrViewConfigurationView& view = views[i];

                    Log::Write(Log::Level::Verbose, Fmt("    View [%d]: Recommended Width=%d Height=%d SampleCount=%d", i,
                                                        view.recommendedImageRectWidth, view.recommendedImageRectHeight,
                                                        view.recommendedSwapchainSampleCount));
                    Log::Write(Log::Level::Verbose,
                               Fmt("    View [%d]:     Maximum Width=%d Height=%d SampleCount=%d", i, view.maxImageRectWidth,
                                   view.maxImageRectHeight, view.maxSwapchainSampleCount));
                }
            } else {
                Log::Write(Log::Level::Error, Fmt("Empty view configuration type"));
            }

            LogEnvironmentBlendMode(viewConfigType);
        }
    }

    void LogEnvironmentBlendMode(XrViewConfigurationType type) {
        CHECK(m_instance != XR_NULL_HANDLE);
        CHECK(m_systemId != 0);

        uint32_t count;
        CHECK_XRCMD(xrEnumerateEnvironmentBlendModes(m_instance, m_systemId, type, 0, &count, nullptr));
        CHECK(count > 0);

        Log::Write(Log::Level::Info, Fmt("Available Environment Blend Mode count : (%d)", count));

        std::vector<XrEnvironmentBlendMode> blendModes(count);
        CHECK_XRCMD(xrEnumerateEnvironmentBlendModes(m_instance, m_systemId, type, count, &count, blendModes.data()));

        bool blendModeFound = false;
        for (XrEnvironmentBlendMode mode : blendModes) {
            const bool blendModeMatch = (mode == m_options.Parsed.EnvironmentBlendMode);
            Log::Write(Log::Level::Info, Fmt("Environment Blend Mode (%s) : %s", to_string(mode), blendModeMatch ? "(Selected)" : ""));
            blendModeFound |= blendModeMatch;
        }
        CHECK(blendModeFound);
    }

    void InitializeSystem() override {
        CHECK(m_instance != XR_NULL_HANDLE);
        CHECK(m_systemId == XR_NULL_SYSTEM_ID);

        XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
        systemInfo.formFactor = m_options.Parsed.FormFactor;
        CHECK_XRCMD(xrGetSystem(m_instance, &systemInfo, &m_systemId));

        Log::Write(Log::Level::Verbose, Fmt("Using system %d for form factor %s", m_systemId, to_string(m_options.Parsed.FormFactor)));
        CHECK(m_instance != XR_NULL_HANDLE);
        CHECK(m_systemId != XR_NULL_SYSTEM_ID);

        LogViewConfigurations();

        // The graphics API can initialize the graphics device now that the systemId and instance
        // handle are available.
        m_graphicsPlugin->InitializeDevice(m_instance, m_systemId);
    }

    void LogReferenceSpaces() {
        CHECK(m_session != XR_NULL_HANDLE);

        uint32_t spaceCount;
        CHECK_XRCMD(xrEnumerateReferenceSpaces(m_session, 0, &spaceCount, nullptr));
        std::vector<XrReferenceSpaceType> spaces(spaceCount);
        CHECK_XRCMD(xrEnumerateReferenceSpaces(m_session, spaceCount, &spaceCount, spaces.data()));

        Log::Write(Log::Level::Info, Fmt("Available reference spaces: %d", spaceCount));
        for (XrReferenceSpaceType space : spaces) {
            Log::Write(Log::Level::Verbose, Fmt("  Name: %s", to_string(space)));
        }
    }

    struct InputState {
        std::array<XrPath, Side::COUNT> handSubactionPath;
        std::array<XrSpace, Side::COUNT> handSpace;
        std::array<XrSpace, Side::COUNT> aimSpace;

        XrActionSet actionSet{XR_NULL_HANDLE};
        XrAction gripPoseAction{XR_NULL_HANDLE};
        XrAction aimPoseAction{XR_NULL_HANDLE};
        XrAction hapticAction{XR_NULL_HANDLE};

        XrAction thumbstickValueAction{XR_NULL_HANDLE};
        XrAction thumbstickClickAction{XR_NULL_HANDLE};
        XrAction thumbstickTouchAction{XR_NULL_HANDLE};
        XrAction triggerValueAction{XR_NULL_HANDLE};
        XrAction triggerClickAction{XR_NULL_HANDLE};
        XrAction triggerTouchAction{XR_NULL_HANDLE};
        XrAction squeezeValueAction{XR_NULL_HANDLE};
        XrAction squeezeClickAction{XR_NULL_HANDLE};

        XrAction AAction{XR_NULL_HANDLE};
        XrAction BAction{XR_NULL_HANDLE};
        XrAction XAction{XR_NULL_HANDLE};
        XrAction YAction{XR_NULL_HANDLE};
        XrAction ATouchAction{XR_NULL_HANDLE};
        XrAction BTouchAction{XR_NULL_HANDLE};
        XrAction XTouchAction{XR_NULL_HANDLE};
        XrAction YTouchAction{XR_NULL_HANDLE};
        XrAction menuAction{XR_NULL_HANDLE};
    };

    void InitializeActions() {
        // Create an action set.
        {
            XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy_s(actionSetInfo.actionSetName, "gameplay");
            strcpy_s(actionSetInfo.localizedActionSetName, "Gameplay");
            actionSetInfo.priority = 0;
            CHECK_XRCMD(xrCreateActionSet(m_instance, &actionSetInfo, &m_input.actionSet));
        }

        // Get the XrPath for the left and right hands - we will use them as subaction paths.
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left", &m_input.handSubactionPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right", &m_input.handSubactionPath[Side::RIGHT]));

        // Create actions.
        {
            // Create an input action getting the left and right hand poses.
            XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
            actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
            strcpy_s(actionInfo.actionName, "grip_pose");
            strcpy_s(actionInfo.localizedActionName, "Grip_pose");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.gripPoseAction));

            actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
            strcpy_s(actionInfo.actionName, "aim_pose");
            strcpy_s(actionInfo.localizedActionName, "Aim_pose");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.aimPoseAction));

            // Create output actions for vibrating the left and right controller.
            actionInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
            strcpy_s(actionInfo.actionName, "haptic");
            strcpy_s(actionInfo.localizedActionName, "Haptic");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.hapticAction));

            actionInfo.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
            strcpy_s(actionInfo.actionName, "thumbstick_value");
            strcpy_s(actionInfo.localizedActionName, "Thumbstick_value");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.thumbstickValueAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "thumbstick_click");
            strcpy_s(actionInfo.localizedActionName, "Thumbstick_click");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.thumbstickClickAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "thumbstick_touch");
            strcpy_s(actionInfo.localizedActionName, "Thumbstick_touch");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.thumbstickTouchAction));

            actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
            strcpy_s(actionInfo.actionName, "trigger_value");
            strcpy_s(actionInfo.localizedActionName, "Trigger_value");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.triggerValueAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "trigger_click");
            strcpy_s(actionInfo.localizedActionName, "Trigger_click");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.triggerClickAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "trigger_touch");
            strcpy_s(actionInfo.localizedActionName, "Trigger_touch");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.triggerTouchAction));

            actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
            strcpy_s(actionInfo.actionName, "squeeze_value");
            strcpy_s(actionInfo.localizedActionName, "Squeeze_value");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.squeezeValueAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "squeeze_click");
            strcpy_s(actionInfo.localizedActionName, "Squeeze_click");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.squeezeClickAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "akey");
            strcpy_s(actionInfo.localizedActionName, "Akey");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.AAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "bkey");
            strcpy_s(actionInfo.localizedActionName, "Bkey");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.BAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "xkey");
            strcpy_s(actionInfo.localizedActionName, "Xkey");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.XAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "ykey");
            strcpy_s(actionInfo.localizedActionName, "Ykey");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.YAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "akey_touch");
            strcpy_s(actionInfo.localizedActionName, "Akey_touch");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.ATouchAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "bkey_touch");
            strcpy_s(actionInfo.localizedActionName, "Bkey_touch");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.BTouchAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "xkey_touch");
            strcpy_s(actionInfo.localizedActionName, "Xkey_touch");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.XTouchAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "ykey_touch");
            strcpy_s(actionInfo.localizedActionName, "Ykey_touch");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.YTouchAction)); 

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "menukey");
            strcpy_s(actionInfo.localizedActionName, "Menukey");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.menuAction));    
        }

        std::array<XrPath, Side::COUNT> gripPosePath;
        std::array<XrPath, Side::COUNT> aimPosePath;
        std::array<XrPath, Side::COUNT> hapticPath;

        std::array<XrPath, Side::COUNT> thumbstickValuePath;
        std::array<XrPath, Side::COUNT> thumbstickClickPath;
        std::array<XrPath, Side::COUNT> thumbstickTouchPath;
        std::array<XrPath, Side::COUNT> squeezeValuePath;
        std::array<XrPath, Side::COUNT> squeezeClickPath;
        std::array<XrPath, Side::COUNT> triggerClickPath;
        std::array<XrPath, Side::COUNT> triggerValuePath;
        std::array<XrPath, Side::COUNT> triggerTouchPath;
        std::array<XrPath, Side::COUNT> AClickPath;
        std::array<XrPath, Side::COUNT> BClickPath;
        std::array<XrPath, Side::COUNT> XClickPath;
        std::array<XrPath, Side::COUNT> YClickPath;
        std::array<XrPath, Side::COUNT> ATouchPath;
        std::array<XrPath, Side::COUNT> BTouchPath;
        std::array<XrPath, Side::COUNT> XTouchPath;
        std::array<XrPath, Side::COUNT> YTouchPath;
        std::array<XrPath, Side::COUNT> menuPath;

        // see https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_BD_controller_interaction
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/grip/pose",  &gripPosePath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/grip/pose", &gripPosePath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/aim/pose",   &aimPosePath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/aim/pose",  &aimPosePath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/output/haptic",    &hapticPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/output/haptic",   &hapticPath[Side::RIGHT]));

        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/thumbstick",        &thumbstickValuePath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/thumbstick",       &thumbstickValuePath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/thumbstick/click",  &thumbstickClickPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/thumbstick/click", &thumbstickClickPath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/thumbstick/touch",  &thumbstickTouchPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/thumbstick/touch", &thumbstickTouchPath[Side::RIGHT]));

        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/trigger/value",  &triggerValuePath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/trigger/value", &triggerValuePath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/trigger/click",  &triggerClickPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/trigger/click", &triggerClickPath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/trigger/touch",  &triggerTouchPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/trigger/touch", &triggerTouchPath[Side::RIGHT]));

        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/squeeze/value",  &squeezeValuePath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/squeeze/value", &squeezeValuePath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/squeeze/click",  &squeezeClickPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/squeeze/click", &squeezeClickPath[Side::RIGHT]));

        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/a/click", &AClickPath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/b/click", &BClickPath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/x/click",  &XClickPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/y/click",  &YClickPath[Side::LEFT]));

        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/a/touch", &ATouchPath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/b/touch", &BTouchPath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/x/touch",  &XTouchPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/y/touch",  &YTouchPath[Side::LEFT]));

        if (m_deviceROM < 0x540) {
            CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/back/click", &menuPath[Side::LEFT]));
            CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/back/click", &menuPath[Side::RIGHT]));
        } else {
            CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/menu/click", &menuPath[Side::LEFT]));
            CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/menu/click", &menuPath[Side::RIGHT]));
        }

        // Suggest bindings for the PICO Controller.
        {
            //see https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_BD_controller_interaction
            const char* interactionProfilePath = nullptr;
            if (m_deviceType == DeviceTypeNeo3 || m_deviceType == DeviceTypeNeo3Pro || m_deviceType == DeviceTypeNeo3ProEye) {
                interactionProfilePath = "/interaction_profiles/bytedance/pico_neo3_controller";
            } else {
                interactionProfilePath = "/interaction_profiles/bytedance/pico4_controller";
            }
            if (m_deviceROM < 0x540) {
                interactionProfilePath = "/interaction_profiles/pico/neo3_controller";
            }

            XrPath picoMixedRealityInteractionProfilePath;
            CHECK_XRCMD(xrStringToPath(m_instance, interactionProfilePath, &picoMixedRealityInteractionProfilePath));
            std::vector<XrActionSuggestedBinding> bindings{{{m_input.gripPoseAction, gripPosePath[Side::LEFT]},
                                                            {m_input.gripPoseAction, gripPosePath[Side::RIGHT]},
                                                            {m_input.aimPoseAction, aimPosePath[Side::LEFT]},
                                                            {m_input.aimPoseAction, aimPosePath[Side::RIGHT]},
                                                            {m_input.hapticAction, hapticPath[Side::LEFT]},
                                                            {m_input.hapticAction, hapticPath[Side::RIGHT]},

                                                            {m_input.thumbstickValueAction, thumbstickValuePath[Side::LEFT]},
                                                            {m_input.thumbstickValueAction, thumbstickValuePath[Side::RIGHT]},
                                                            {m_input.thumbstickClickAction, thumbstickClickPath[Side::LEFT]},
                                                            {m_input.thumbstickClickAction, thumbstickClickPath[Side::RIGHT]},
                                                            {m_input.thumbstickTouchAction, thumbstickTouchPath[Side::LEFT]},
                                                            {m_input.thumbstickTouchAction, thumbstickTouchPath[Side::RIGHT]}, 

                                                            {m_input.triggerValueAction, triggerValuePath[Side::LEFT]},
                                                            {m_input.triggerValueAction, triggerValuePath[Side::RIGHT]},
                                                            {m_input.triggerClickAction, triggerClickPath[Side::LEFT]},
                                                            {m_input.triggerClickAction, triggerClickPath[Side::RIGHT]},
                                                            {m_input.triggerTouchAction, triggerTouchPath[Side::LEFT]},
                                                            {m_input.triggerTouchAction, triggerTouchPath[Side::RIGHT]},
                                                            
                                                            {m_input.squeezeClickAction, squeezeClickPath[Side::LEFT]},
                                                            {m_input.squeezeClickAction, squeezeClickPath[Side::RIGHT]},
                                                            {m_input.squeezeValueAction, squeezeValuePath[Side::LEFT]},
                                                            {m_input.squeezeValueAction, squeezeValuePath[Side::RIGHT]},

                                                            {m_input.AAction, AClickPath[Side::RIGHT]},
                                                            {m_input.BAction, BClickPath[Side::RIGHT]},
                                                            {m_input.XAction, XClickPath[Side::LEFT]},
                                                            {m_input.YAction, YClickPath[Side::LEFT]},

                                                            {m_input.ATouchAction, ATouchPath[Side::RIGHT]},
                                                            {m_input.BTouchAction, BTouchPath[Side::RIGHT]},
                                                            {m_input.XTouchAction, XTouchPath[Side::LEFT]},
                                                            {m_input.YTouchAction, YTouchPath[Side::LEFT]},

                                                            {m_input.menuAction, menuPath[Side::LEFT]}}};

            if (m_deviceType == DeviceTypeNeo3 || m_deviceType == DeviceTypeNeo3Pro || m_deviceType == DeviceTypeNeo3ProEye) {
                XrActionSuggestedBinding menuRightBinding{m_input.menuAction, menuPath[Side::RIGHT]};
                bindings.push_back(menuRightBinding);
            }

            XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            suggestedBindings.interactionProfile = picoMixedRealityInteractionProfilePath;
            suggestedBindings.suggestedBindings = bindings.data();
            suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
            CHECK_XRCMD(xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
        }

        XrActionSpaceCreateInfo actionSpaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        actionSpaceInfo.action = m_input.gripPoseAction;
        actionSpaceInfo.poseInActionSpace.orientation.w = 1.0f;
        actionSpaceInfo.subactionPath = m_input.handSubactionPath[Side::LEFT];
        CHECK_XRCMD(xrCreateActionSpace(m_session, &actionSpaceInfo, &m_input.handSpace[Side::LEFT]));
        actionSpaceInfo.subactionPath = m_input.handSubactionPath[Side::RIGHT];
        CHECK_XRCMD(xrCreateActionSpace(m_session, &actionSpaceInfo, &m_input.handSpace[Side::RIGHT]));

        actionSpaceInfo.action = m_input.aimPoseAction;
        actionSpaceInfo.poseInActionSpace.orientation.w = 1.0f;
        actionSpaceInfo.subactionPath = m_input.handSubactionPath[Side::LEFT];
        CHECK_XRCMD(xrCreateActionSpace(m_session, &actionSpaceInfo, &m_input.aimSpace[Side::LEFT]));
        actionSpaceInfo.subactionPath = m_input.handSubactionPath[Side::RIGHT];
        CHECK_XRCMD(xrCreateActionSpace(m_session, &actionSpaceInfo, &m_input.aimSpace[Side::RIGHT]));

        XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        attachInfo.countActionSets = 1;
        attachInfo.actionSets = &m_input.actionSet;
        CHECK_XRCMD(xrAttachSessionActionSets(m_session, &attachInfo));
    }

    void CreateVisualizedSpaces() {
        CHECK(m_session != XR_NULL_HANDLE);
        XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::Identity();
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
        CHECK_XRCMD(xrCreateReferenceSpace(m_session, &referenceSpaceCreateInfo, &m_ViewSpace));
    }

    void GetDeviceInfo() {
        char buffer[64] = {0};
        __system_property_get("sys.pxr.product.name", buffer);
        Log::Write(Log::Level::Info, Fmt("device is: %s", buffer));
        if (std::string(buffer) == "Pico Neo 3 Pro Eye") {
            m_deviceType = DeviceTypeNeo3ProEye;
        } else if (std::string(buffer) == "Pico 4") {
            m_deviceType = DeviceTypePico4;
        }else if (std::string(buffer) == "PICO 4 Pro") {
            m_deviceType = DeviceTypePico4Pro;
        }

        __system_property_get("ro.build.id", buffer);
        int a, b, c;
        sscanf(buffer, "%d.%d.%d",&a, &b, &c);
        m_deviceROM = (a << 8) + (b << 4) + c;
        Log::Write(Log::Level::Info, Fmt("device ROM: %x", m_deviceROM));
        if (m_deviceROM < 0x540) {
            //CHECK_XRRESULT(XR_ERROR_VALIDATION_FAILURE, "This demo can only run on devices with ROM version greater than 540");
        }
    }


    void InitializeSession() override {
        CHECK(m_instance != XR_NULL_HANDLE);
        CHECK(m_session == XR_NULL_HANDLE);

        {
            Log::Write(Log::Level::Verbose, Fmt("Creating session..."));

            XrSessionCreateInfo createInfo{XR_TYPE_SESSION_CREATE_INFO};
            createInfo.next = m_graphicsPlugin->GetGraphicsBinding();
            createInfo.systemId = m_systemId;
            CHECK_XRCMD(xrCreateSession(m_instance, &createInfo, &m_session));
        }

        GetDeviceInfo();
        LogReferenceSpaces();
        InitializeActions();
        CreateVisualizedSpaces();

        {
            XrReferenceSpaceCreateInfo referenceSpaceCreateInfo = GetXrReferenceSpaceCreateInfo(m_options.AppSpace);
            CHECK_XRCMD(xrCreateReferenceSpace(m_session, &referenceSpaceCreateInfo, &m_appSpace));
        }

        CHECK_XRCMD(xrGetInstanceProcAddr(m_instance, "xrGetDisplayRefreshRateFB", (PFN_xrVoidFunction*)&m_pfnXrGetDisplayRefreshRateFB));
        m_pfnXrGetDisplayRefreshRateFB(m_session, &m_displayRefreshRate);
        Log::Write(Log::Level::Info, Fmt("device fps:%0.3f", m_displayRefreshRate));
    }

    void CreateSwapchains() override {
        CHECK(m_session != XR_NULL_HANDLE);
        CHECK(m_swapchains.empty());
        CHECK(m_configViews.empty());

        Log::Write(Log::Level::Info, Fmt("CreateSwapchains......"));

        // Read graphics properties for preferred swapchain length and logging.
        XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES};
        CHECK_XRCMD(xrGetSystemProperties(m_instance, m_systemId, &systemProperties));

        // Log system properties.
        Log::Write(Log::Level::Info,
                   Fmt("System Properties: Name=%s VendorId=%d", systemProperties.systemName, systemProperties.vendorId));
        Log::Write(Log::Level::Info, Fmt("System Graphics Properties: MaxWidth=%d MaxHeight=%d MaxLayers=%d",
                                         systemProperties.graphicsProperties.maxSwapchainImageWidth,
                                         systemProperties.graphicsProperties.maxSwapchainImageHeight,
                                         systemProperties.graphicsProperties.maxLayerCount));
        Log::Write(Log::Level::Info, Fmt("System Tracking Properties: OrientationTracking=%s PositionTracking=%s",
                                         systemProperties.trackingProperties.orientationTracking == XR_TRUE ? "True" : "False",
                                         systemProperties.trackingProperties.positionTracking == XR_TRUE ? "True" : "False"));

        // Note: No other view configurations exist at the time this code was written. If this
        // condition is not met, the project will need to be audited to see how support should be
        // added.
        CHECK_MSG(m_options.Parsed.ViewConfigType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, "Unsupported view configuration type");

        // Query and cache view configuration views.
        uint32_t viewCount;
        CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_options.Parsed.ViewConfigType, 0, &viewCount, nullptr));
        m_configViews.resize(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
        CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_options.Parsed.ViewConfigType, viewCount, &viewCount, m_configViews.data()));

        // Create and cache view buffer for xrLocateViews later.
        m_views.resize(viewCount, {XR_TYPE_VIEW});

        // Create the swapchain and get the images.
        if (viewCount > 0) {
            // Select a swapchain format.
            uint32_t swapchainFormatCount;
            CHECK_XRCMD(xrEnumerateSwapchainFormats(m_session, 0, &swapchainFormatCount, nullptr));
            std::vector<int64_t> swapchainFormats(swapchainFormatCount);
            CHECK_XRCMD(xrEnumerateSwapchainFormats(m_session, (uint32_t)swapchainFormats.size(), &swapchainFormatCount, swapchainFormats.data()));
            CHECK(swapchainFormatCount == swapchainFormats.size());
            m_colorSwapchainFormat = m_graphicsPlugin->SelectColorSwapchainFormat(swapchainFormats);

            // Print swapchain formats and the selected one.
            {
                std::string swapchainFormatsString;
                for (int64_t format : swapchainFormats) {
                    const bool selected = format == m_colorSwapchainFormat;
                    swapchainFormatsString += " ";
                    if (selected) {
                        swapchainFormatsString += "[";
                    }
                    swapchainFormatsString += std::to_string(format);
                    if (selected) {
                        swapchainFormatsString += "]";
                    }
                }
                Log::Write(Log::Level::Verbose, Fmt("Swapchain Formats: %s", swapchainFormatsString.c_str()));
            }

            // Create a swapchain for each view.
            for (uint32_t i = 0; i < viewCount; i++) {
                const XrViewConfigurationView& vp = m_configViews[i];
                Log::Write(Log::Level::Info,
                           Fmt("Creating swapchain for view %d with dimensions Width=%d Height=%d SampleCount=%d", i,
                               vp.recommendedImageRectWidth, vp.recommendedImageRectHeight, vp.recommendedSwapchainSampleCount));

                // Create the swapchain.
                XrSwapchainCreateInfo swapchainCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
                swapchainCreateInfo.arraySize = 1;
                swapchainCreateInfo.format = m_colorSwapchainFormat;
                swapchainCreateInfo.width = vp.recommendedImageRectWidth;
                swapchainCreateInfo.height = vp.recommendedImageRectHeight;
                swapchainCreateInfo.mipCount = 1;
                swapchainCreateInfo.faceCount = 1;
                swapchainCreateInfo.sampleCount = m_graphicsPlugin->GetSupportedSwapchainSampleCount(vp);
                swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
                Swapchain swapchain;
                swapchain.width = swapchainCreateInfo.width;
                swapchain.height = swapchainCreateInfo.height;
                CHECK_XRCMD(xrCreateSwapchain(m_session, &swapchainCreateInfo, &swapchain.handle));

                m_swapchains.push_back(swapchain);

                uint32_t imageCount;
                CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain.handle, 0, &imageCount, nullptr));
                // XXX This should really just return XrSwapchainImageBaseHeader*
                std::vector<XrSwapchainImageBaseHeader*> swapchainImages = m_graphicsPlugin->AllocateSwapchainImageStructs(imageCount, swapchainCreateInfo);
                CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain.handle, imageCount, &imageCount, swapchainImages[0]));

                m_swapchainImages.insert(std::make_pair(swapchain.handle, std::move(swapchainImages)));
            }
        }
    }

    // Return event if one is available, otherwise return null.
    const XrEventDataBaseHeader* TryReadNextEvent() {
        // It is sufficient to clear the just the XrEventDataBuffer header to
        // XR_TYPE_EVENT_DATA_BUFFER
        XrEventDataBaseHeader* baseHeader = reinterpret_cast<XrEventDataBaseHeader*>(&m_eventDataBuffer);
        *baseHeader = {XR_TYPE_EVENT_DATA_BUFFER};
        const XrResult xr = xrPollEvent(m_instance, &m_eventDataBuffer);
        if (xr == XR_SUCCESS) {
            if (baseHeader->type == XR_TYPE_EVENT_DATA_EVENTS_LOST) {
                const XrEventDataEventsLost* const eventsLost = reinterpret_cast<const XrEventDataEventsLost*>(baseHeader);
                Log::Write(Log::Level::Warning, Fmt("%d events lost", eventsLost));
            }

            return baseHeader;
        }
        if (xr == XR_EVENT_UNAVAILABLE) {
            return nullptr;
        }
        THROW_XR(xr, "xrPollEvent");
    }

    void PollEvents(bool* exitRenderLoop, bool* requestRestart) override {
        *exitRenderLoop = *requestRestart = false;

        // Process all pending messages.
        while (const XrEventDataBaseHeader* event = TryReadNextEvent()) {
            switch (event->type) {
                case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
                    const auto& instanceLossPending = *reinterpret_cast<const XrEventDataInstanceLossPending*>(event);
                    Log::Write(Log::Level::Warning, Fmt("XrEventDataInstanceLossPending by %lld", instanceLossPending.lossTime));
                    *exitRenderLoop = true;
                    *requestRestart = true;
                    return;
                }
                case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                    auto sessionStateChangedEvent = *reinterpret_cast<const XrEventDataSessionStateChanged*>(event);
                    HandleSessionStateChangedEvent(sessionStateChangedEvent, exitRenderLoop, requestRestart);
                    break;
                }
                case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
                    LogActionSourceName(m_input.gripPoseAction, "Pose");
                    LogActionSourceName(m_input.hapticAction, "Haptic");
                    break;
                case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
                default: {
                    Log::Write(Log::Level::Verbose, Fmt("Ignoring event type %d", event->type));
                    break;
                }
            }
        }
    }

    void HandleSessionStateChangedEvent(const XrEventDataSessionStateChanged& stateChangedEvent, bool* exitRenderLoop, bool* requestRestart) {
        const XrSessionState oldState = m_sessionState;
        m_sessionState = stateChangedEvent.state;

        Log::Write(Log::Level::Info, Fmt("XrEventDataSessionStateChanged: state %s->%s session=%lld time=%lld", to_string(oldState),
                                         to_string(m_sessionState), stateChangedEvent.session, stateChangedEvent.time));

        if ((stateChangedEvent.session != XR_NULL_HANDLE) && (stateChangedEvent.session != m_session)) {
            Log::Write(Log::Level::Error, "XrEventDataSessionStateChanged for unknown session");
            return;
        }

        switch (m_sessionState) {
            case XR_SESSION_STATE_READY: {
                CHECK(m_session != XR_NULL_HANDLE);
                XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                sessionBeginInfo.primaryViewConfigurationType = m_options.Parsed.ViewConfigType;
                CHECK_XRCMD(xrBeginSession(m_session, &sessionBeginInfo));
                m_sessionRunning = true;
                break;
            }
            case XR_SESSION_STATE_STOPPING: {
                CHECK(m_session != XR_NULL_HANDLE);
                m_sessionRunning = false;
                CHECK_XRCMD(xrEndSession(m_session))
                break;
            }
            case XR_SESSION_STATE_EXITING: {
                *exitRenderLoop = true;
                // Do not attempt to restart because user closed this session.
                *requestRestart = false;
                break;
            }
            case XR_SESSION_STATE_LOSS_PENDING: {
                *exitRenderLoop = true;
                // Poll for a new instance.
                *requestRestart = true;
                break;
            }
            default:
                break;
        }
    }

    void LogActionSourceName(XrAction action, const std::string& actionName) const {
        XrBoundSourcesForActionEnumerateInfo getInfo = {XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO};
        getInfo.action = action;
        uint32_t pathCount = 0;
        CHECK_XRCMD(xrEnumerateBoundSourcesForAction(m_session, &getInfo, 0, &pathCount, nullptr));
        std::vector<XrPath> paths(pathCount);
        CHECK_XRCMD(xrEnumerateBoundSourcesForAction(m_session, &getInfo, uint32_t(paths.size()), &pathCount, paths.data()));

        std::string sourceName;
        for (uint32_t i = 0; i < pathCount; ++i) {
            constexpr XrInputSourceLocalizedNameFlags all = XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT |
                                                            XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT |
                                                            XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT;

            XrInputSourceLocalizedNameGetInfo nameInfo = {XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO};
            nameInfo.sourcePath = paths[i];
            nameInfo.whichComponents = all;

            uint32_t size = 0;
            CHECK_XRCMD(xrGetInputSourceLocalizedName(m_session, &nameInfo, 0, &size, nullptr));
            if (size < 1) {
                continue;
            }
            std::vector<char> grabSource(size);
            CHECK_XRCMD(xrGetInputSourceLocalizedName(m_session, &nameInfo, uint32_t(grabSource.size()), &size, grabSource.data()));
            if (!sourceName.empty()) {
                sourceName += " and ";
            }
            sourceName += "'";
            sourceName += std::string(grabSource.data(), size - 1);
            sourceName += "'";
        }

        Log::Write(Log::Level::Info,
                   Fmt("%s action is bound to %s", actionName.c_str(), ((!sourceName.empty()) ? sourceName.c_str() : "nothing")));
    }

    bool IsSessionRunning() const override { return m_sessionRunning; }

    bool IsSessionFocused() const override { return m_sessionState == XR_SESSION_STATE_FOCUSED; }

    void PollActions() override {
        // Sync actions
        const XrActiveActionSet activeActionSet{m_input.actionSet, XR_NULL_PATH};
        XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
        syncInfo.countActiveActionSets = 1;
        syncInfo.activeActionSets = &activeActionSet;
        CHECK_XRCMD(xrSyncActions(m_session, &syncInfo));

#ifndef CLOUDXR3_5
        const int MAX_CONTROLLERS = 2;
        // 64 should be more than large enough. 2x32b masks that are < half used, plus scalars.
        cxrControllerEvent events[MAX_CONTROLLERS][64] = {};
        uint32_t eventCount[MAX_CONTROLLERS] = {};
#endif
        cxrVRTrackingState trackingState{};

        // Get pose and grab action state and start haptic vibrate when hand is 90% squeezed.
        for (auto hand : {Side::LEFT, Side::RIGHT}) {

#ifndef CLOUDXR3_5
            const int handIndex = hand == Side::LEFT ? 0 : (Side::RIGHT ? 1:-1);
            if (!m_newControllers[handIndex]) // null, so open to create+add
            {
                cxrControllerDesc desc = {};
                //desc.id = capsHeader.DeviceID; // turns out this is NOT UNIQUE.  it's a fixed starting number, incremented, and thus devices can 'swap' IDs.
                desc.id = handIndex; // so for now, we're going to just use handIndex, as we're guaranteed left+right will remain 0+1 always.
                desc.role = handIndex?"cxr://input/hand/right":"cxr://input/hand/left";
                desc.controllerName = "Oculus Touch";
                desc.inputCount = inputCountQuest;
                desc.inputPaths = inputPathsQuest;
                desc.inputValueTypes = inputValueTypesQuest;
#if false
                CXR_LOGI("Adding controller index %u, ID %llu, role %s", handIndex, desc.id, desc.role);
                CXR_LOGI("Controller caps bits = 0x%08x", capsHeader.DeviceID, remoteCaps.ControllerCapabilities);
#endif
                cxrError e = cxrAddController(Receiver, &desc, &m_newControllers[handIndex]);
                if (e!=cxrError_Success)
                {
#if false
                    CXR_LOGE("Error adding controller: %s", cxrErrorString(e));
#endif
                    // TODO!!! proper example for client to handle client-call errors, fatal vs 'notice'.
                    continue;
                }
            }
#endif

            XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
            getInfo.subactionPath = m_input.handSubactionPath[hand];

            //menu click
            getInfo.action = m_input.menuAction;
            XrActionStateBoolean menuValue{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &menuValue));
            if (menuValue.isActive == XR_TRUE) {
                if (menuValue.changedSinceLastSync == XR_TRUE) {
                    if (menuValue.currentState == XR_TRUE) {
#ifdef CLOUDXR3_5
                        trackingState.controller[hand].booleanComps |= 1UL << cxrButton_System;
#endif
                    }
                }
            }

            //thumbstick value x/y
            getInfo.action = m_input.thumbstickValueAction;
            XrActionStateVector2f thumbstickValue{XR_TYPE_ACTION_STATE_VECTOR2F};
            CHECK_XRCMD(xrGetActionStateVector2f(m_session, &getInfo, &thumbstickValue));
            if (thumbstickValue.isActive == XR_TRUE) {
#ifdef CLOUDXR3_5
                trackingState.controller[hand].scalarComps[cxrAnalog_JoystickX] = thumbstickValue.currentState.x;
                trackingState.controller[hand].scalarComps[cxrAnalog_JoystickY] = thumbstickValue.currentState.y;
#endif
            }
            // thumbstick click
            getInfo.action = m_input.thumbstickClickAction;
            XrActionStateBoolean thumbstickClick{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &thumbstickClick));
            if ((thumbstickClick.isActive == XR_TRUE) && (thumbstickClick.changedSinceLastSync == XR_TRUE)) {
                if (thumbstickClick.currentState == XR_TRUE) {
                    Log::Write(Log::Level::Info, Fmt("pico keyevent thumbstick pressed %d",hand));
                } else {
                    Log::Write(Log::Level::Info, Fmt("pico keyevent thumbstick released %d",hand));
                }
            }
            // thumbstick touch
            getInfo.action = m_input.thumbstickTouchAction;
            XrActionStateBoolean thumbstickTouch{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &thumbstickTouch));
            if (thumbstickTouch.isActive == XR_TRUE) {
                if (thumbstickTouch.changedSinceLastSync == XR_TRUE && thumbstickTouch.currentState == XR_TRUE) {
                    Log::Write(Log::Level::Info, Fmt("pico keyevent thumbstick click %d", hand));
                }
            }

            //trigger value
            getInfo.action = m_input.triggerValueAction;
            XrActionStateFloat triggerValue{XR_TYPE_ACTION_STATE_FLOAT};
            CHECK_XRCMD(xrGetActionStateFloat(m_session, &getInfo, &triggerValue));
            if (triggerValue.isActive == XR_TRUE) {
#ifdef CLOUDXR3_5
                trackingState.controller[hand].scalarComps[cxrAnalog_Trigger] = triggerValue.currentState;
#endif
            }
            // trigger touch
            getInfo.action = m_input.triggerTouchAction;
            XrActionStateBoolean triggerTouch{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &triggerTouch));
            if (triggerTouch.isActive == XR_TRUE) {
                if (triggerTouch.changedSinceLastSync == XR_TRUE && triggerTouch.currentState == XR_TRUE) {
#ifdef CLOUDXR3_5
                    trackingState.controller[hand].booleanComps |= 1UL << cxrButton_Trigger_Touch;
#endif
                }
            }
            // trigger click
            getInfo.action = m_input.triggerClickAction;
            XrActionStateBoolean triggerClick{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &triggerClick));
            if (triggerClick.isActive == XR_TRUE) {
                if (triggerClick.changedSinceLastSync == XR_TRUE && triggerClick.currentState == XR_TRUE) {
#ifdef CLOUDXR3_5
                    trackingState.controller[hand].booleanComps |= 1UL << cxrButton_Trigger_Click;
#endif
                }
            }

            // squeeze value
            getInfo.action = m_input.squeezeValueAction;
            XrActionStateFloat squeezeValue{XR_TYPE_ACTION_STATE_FLOAT};
            CHECK_XRCMD(xrGetActionStateFloat(m_session, &getInfo, &squeezeValue));
            if (squeezeValue.isActive == XR_TRUE) {
#ifdef CLOUDXR3_5
                trackingState.controller[hand].scalarComps[cxrAnalog_Grip] = squeezeValue.currentState;
#endif
            }
            // squeeze click
            getInfo.action = m_input.squeezeClickAction;
            XrActionStateBoolean squeezeClick{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &squeezeClick));
            if ((squeezeClick.isActive == XR_TRUE) && (squeezeClick.changedSinceLastSync == XR_TRUE)) {
                if(squeezeClick.currentState == XR_TRUE) {
                    Log::Write(Log::Level::Info, Fmt("pico keyevent squeeze click pressed %d", hand));
                } else{
                    Log::Write(Log::Level::Info, Fmt("pico keyevent squeeze click released %d", hand));
                }
            }

            // A button
            getInfo.action = m_input.AAction;
            XrActionStateBoolean AValue{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &AValue));
            if ((AValue.isActive == XR_TRUE) && (AValue.changedSinceLastSync == XR_TRUE)) {
                if(AValue.currentState == XR_TRUE) {
                    Log::Write(Log::Level::Info, Fmt("pico keyevent A button pressed %d", hand));
#ifdef CLOUDXR3_5
                    trackingState.controller[hand].booleanComps |= 1UL << cxrButton_A;
#endif
                }
            }
            // B button
            getInfo.action = m_input.BAction;
            XrActionStateBoolean BValue{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &BValue));
            if ((BValue.isActive == XR_TRUE) && (BValue.changedSinceLastSync == XR_TRUE)) {
                if(BValue.currentState == XR_TRUE) {
                    Log::Write(Log::Level::Info, Fmt("pico keyevent B button pressed %d", hand));
#ifdef CLOUDXR3_5
                    trackingState.controller[hand].booleanComps |= 1UL << cxrButton_B;
#endif
                }
            }
            // X button
            getInfo.action = m_input.XAction;
            XrActionStateBoolean XValue{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &XValue));
            if ((XValue.isActive == XR_TRUE) && (XValue.changedSinceLastSync == XR_TRUE)) {
                if(XValue.currentState == XR_TRUE) {
                    Log::Write(Log::Level::Info, Fmt("pico keyevent X button pressed %d", hand));
#ifdef CLOUDXR3_5
                    trackingState.controller[hand].booleanComps |= 1UL << cxrButton_X;
#endif
                }
            }
            // Y button
            getInfo.action = m_input.YAction;
            XrActionStateBoolean YValue{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &YValue));
            if ((YValue.isActive == XR_TRUE) && (YValue.changedSinceLastSync == XR_TRUE)) {
                if(YValue.currentState == XR_TRUE) {
                    Log::Write(Log::Level::Info, Fmt("pico keyevent Y button pressed %d", hand));
#ifdef CLOUDXR3_5
                    trackingState.controller[hand].booleanComps |= 1UL << cxrButton_Y;
#endif
                }
            }

#ifndef CLOUDXR3_5
            if (eventCount[handIndex])
            {
                cxrError err = cxrFireControllerEvents(Receiver, m_newControllers[handIndex], events[handIndex], eventCount[handIndex]);
                if (err != cxrError_Success)
                {
                    CXR_LOGE("cxrFireControllerEvents failed: %s", cxrErrorString(err));
                    // TODO: how to handle UNUSUAL API errors? might just return up.
                    throw("Error firing events"); // just to do something fatal until we can propagate and 'handle' it.
                }
                // save input state for easy comparison next time, ONLY if we sent the events...
                mLastInputState[handIndex] = input;
            }

            // clear event count.
            eventCount[handIndex] = 0;
#endif

        }

        if (m_cloudxr.get()) {
            m_cloudxr->SetTrackingState(trackingState);
        }
    }

    void RenderFrame() override {
        CHECK(m_session != XR_NULL_HANDLE);

        XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState frameState{XR_TYPE_FRAME_STATE};
        CHECK_XRCMD(xrWaitFrame(m_session, &frameWaitInfo, &frameState));

        XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        CHECK_XRCMD(xrBeginFrame(m_session, &frameBeginInfo));

        std::vector<XrCompositionLayerBaseHeader*> layers;
        XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        std::vector<XrCompositionLayerProjectionView> projectionLayerViews;
        if (frameState.shouldRender == XR_TRUE) {
            if (RenderLayer(frameState.predictedDisplayTime, projectionLayerViews, layer)) {
                layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer));
            }
        }

        XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
        frameEndInfo.displayTime = frameState.predictedDisplayTime;
        frameEndInfo.environmentBlendMode = m_options.Parsed.EnvironmentBlendMode;
        frameEndInfo.layerCount = (uint32_t)layers.size();
        frameEndInfo.layers = layers.data();
        CHECK_XRCMD(xrEndFrame(m_session, &frameEndInfo));
    }

    bool RenderLayer(XrTime predictedDisplayTime, std::vector<XrCompositionLayerProjectionView>& projectionLayerViews,
                     XrCompositionLayerProjection& layer) {
        XrResult res;
        XrViewState viewState{XR_TYPE_VIEW_STATE};
        uint32_t viewCapacityInput = (uint32_t)m_views.size();
        uint32_t viewCountOutput;
        XrViewLocateInfo viewLocateInfo{XR_TYPE_VIEW_LOCATE_INFO};
        viewLocateInfo.viewConfigurationType = m_options.Parsed.ViewConfigType;
        viewLocateInfo.displayTime = predictedDisplayTime;
        viewLocateInfo.space = m_appSpace;

        res = xrLocateViews(m_session, &viewLocateInfo, &viewState, viewCapacityInput, &viewCountOutput, m_views.data());
        CHECK_XRRESULT(res, "xrLocateViews");
        if ((viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) == 0 ||
            (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0) {
            return false;  // There is no valid tracking poses for the views.
        }

        // get ipd
        float ipd = sqrt(pow(abs(m_views[1].pose.position.x-m_views[0].pose.position.x),2)+pow(abs(m_views[1].pose.position.y-m_views[0].pose.position.y),2)+pow(abs(m_views[1].pose.position.z-m_views[0].pose.position.z),2));

        CHECK(viewCountOutput == viewCapacityInput);
        CHECK(viewCountOutput == m_configViews.size());
        CHECK(viewCountOutput == m_swapchains.size());

        projectionLayerViews.resize(viewCountOutput);

        std::vector<XrPosef> handPose;
        for (auto hand : {Side::LEFT, Side::RIGHT}) {
            XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION};
            res = xrLocateSpace(m_input.handSpace[hand], m_appSpace, predictedDisplayTime, &spaceLocation);
            CHECK_XRRESULT(res, "xrLocateSpace");
            if (XR_UNQUALIFIED_SUCCESS(res)) {
                if ((spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
                    (spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
                    handPose.push_back(spaceLocation.pose);
                }
            }
        }

        XrSpaceVelocity velocity{XR_TYPE_SPACE_VELOCITY};
        XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION, &velocity};
        res = xrLocateSpace(m_ViewSpace, m_appSpace, predictedDisplayTime, &spaceLocation);
        CHECK_XRRESULT(res, "xrLocateSpace");

        m_cloudxr->SetSenserPoseState(spaceLocation.pose, velocity.linearVelocity, velocity.angularVelocity, handPose, ipd);

        cxrFramesLatched framesLatched{};
        bool framevaild = m_cloudxr->LatchFrame(&framesLatched);

        XrPosef pose[Side::COUNT];
        for (uint32_t i = 0; i < viewCountOutput; i++) {
            pose[i] = m_views[i].pose;
        }
        if (framevaild) {
            XrQuaternionf orientation = m_cloudxr->cxrToQuaternion(framesLatched.poseMatrix);
            XrVector3f position =  m_cloudxr->cxrGetTranslation(framesLatched.poseMatrix);
            //Log::Write(Log::Level::Error, Fmt("-get poseId %llu, position: %f,%f,%f orientation: %f,%f,%f,%f", framesLatched.poseID,
            //    position.x, position.y, position.z, orientation.x, orientation.y, orientation.z, orientation.w));
           
            // todo get views pose
            for (uint32_t i = 0; i < viewCountOutput; i++) {
                pose[i].position = position;
                pose[i].orientation = orientation;
            }
        } else {
            Log::Write(Log::Level::Info, Fmt("not get framesLatched"));
        }

        // Render view to the appropriate part of the swapchain image.
        for (uint32_t i = 0; i < viewCountOutput; i++) {
            // Each view has a separate swapchain which is acquired, rendered to, and released.
            const Swapchain viewSwapchain = m_swapchains[i];
            XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            uint32_t swapchainImageIndex;
            CHECK_XRCMD(xrAcquireSwapchainImage(viewSwapchain.handle, &acquireInfo, &swapchainImageIndex));

            XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            waitInfo.timeout = XR_INFINITE_DURATION;
            CHECK_XRCMD(xrWaitSwapchainImage(viewSwapchain.handle, &waitInfo));

            projectionLayerViews[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
            projectionLayerViews[i].pose = pose[i];
            projectionLayerViews[i].fov = m_views[i].fov;
            projectionLayerViews[i].subImage.swapchain = viewSwapchain.handle;
            projectionLayerViews[i].subImage.imageRect.offset = {0, 0};
            projectionLayerViews[i].subImage.imageRect.extent = {viewSwapchain.width, viewSwapchain.height};

            const XrSwapchainImageBaseHeader* const swapchainImage = m_swapchainImages[viewSwapchain.handle][swapchainImageIndex];
            const uint32_t colorTexture = reinterpret_cast<const XrSwapchainImageOpenGLESKHR*>(swapchainImage)->image;
            if (m_cloudxr->SetupFramebuffer(colorTexture, i, projectionLayerViews[i].subImage.imageRect.extent.width, projectionLayerViews[i].subImage.imageRect.extent.height)) {
                cxrVideoFrame &videoFrame = framesLatched.frames[i];
                m_cloudxr->BlitFrame(&framesLatched, framevaild, i);
            }

            XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            CHECK_XRCMD(xrReleaseSwapchainImage(viewSwapchain.handle, &releaseInfo));
        }

        if (framevaild) {
            m_cloudxr->ReleaseFrame(&framesLatched);
        }

        layer.space = m_appSpace;
        layer.layerFlags = m_options.Parsed.EnvironmentBlendMode == XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND
                         ? XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT
                         : 0;
        layer.viewCount = (uint32_t)projectionLayerViews.size();
        layer.views = projectionLayerViews.data();
        return true;
    }


    bool CreateCloudxrClient() override {
        m_cloudxr = std::make_shared<CloudXRClient>();
        return true;
    }

    void SetCloudxrClientPaused(bool pause) override {
        if (m_cloudxr.get()) {
            m_cloudxr->SetPaused(pause);
        }
    }

    void StartCloudxrClient() override {
        if (m_cloudxr.get()) {
            m_cloudxr->Initialize(m_instance, m_systemId, m_session, m_displayRefreshRate, m_isSupport_epic_view_configuration_fov_extention, (void*)this, [](void *arg, int controllerIdx, float amplitude, float seconds, float frequency) {
                Log::Write(Log::Level::Error, Fmt("this:%p, index:%d, amplitude:%f, seconds:%f, frequency:%f", arg, controllerIdx, amplitude, seconds, frequency));
                OpenXrProgram* thiz = (OpenXrProgram*)arg;
                XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
                vibration.amplitude = amplitude;
                vibration.duration = seconds * 1000 * 10000000;  //nanoseconds
                vibration.frequency = frequency;
                XrHapticActionInfo hapticActionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
                hapticActionInfo.action = thiz->m_input.hapticAction;
                hapticActionInfo.subactionPath = thiz->m_input.handSubactionPath[controllerIdx];
                CHECK_XRCMD(xrApplyHapticFeedback(thiz->m_session, &hapticActionInfo, (XrHapticBaseHeader*)&vibration));
            });
        }
    }

   private:
    const Options m_options;
    std::shared_ptr<IPlatformPlugin> m_platformPlugin;
    std::shared_ptr<IGraphicsPlugin> m_graphicsPlugin;
    XrInstance m_instance{XR_NULL_HANDLE};
    XrSession m_session{XR_NULL_HANDLE};
    XrSpace m_appSpace{XR_NULL_HANDLE};
    XrSystemId m_systemId{XR_NULL_SYSTEM_ID};

    std::vector<XrViewConfigurationView> m_configViews;
    std::vector<Swapchain> m_swapchains;
    std::map<XrSwapchain, std::vector<XrSwapchainImageBaseHeader*>> m_swapchainImages;
    std::vector<XrView> m_views;
    int64_t m_colorSwapchainFormat{-1};

    // Application's current lifecycle state according to the runtime
    XrSessionState m_sessionState{XR_SESSION_STATE_UNKNOWN};
    bool m_sessionRunning{false};

    XrEventDataBuffer m_eventDataBuffer;
    InputState m_input;

    std::shared_ptr<CloudXRClient> m_cloudxr;
    XrSpace m_ViewSpace{XR_NULL_HANDLE};
    PFN_xrGetDisplayRefreshRateFB m_pfnXrGetDisplayRefreshRateFB;
    float m_displayRefreshRate;
    bool m_isSupport_epic_view_configuration_fov_extention;
    DeviceType m_deviceType;
    uint32_t m_deviceROM;
};
}  // namespace

std::shared_ptr<IOpenXrProgram> CreateOpenXrProgram(const std::shared_ptr<Options>& options,
                                                    const std::shared_ptr<IPlatformPlugin>& platformPlugin,
                                                    const std::shared_ptr<IGraphicsPlugin>& graphicsPlugin) {
    return std::make_shared<OpenXrProgram>(options, platformPlugin, graphicsPlugin);
}
