# OpenXR CloudXR Client Demo
- **This software contains source code provided by NVIDIA Corporation**.
- The client decodes and renders content that is streamed from the CloudXR server and collects motion and controller data from the VR headset that is sent to the CloudXR server.The VR headset must be capable of decoding 4k HEVC video @ 60fps. The provided sample client has been tested with the Pico Neo3, Pico4, Pico4 Pro and Pico4 Enterprise.
- If you have any questions/comments, please visit [**Pico Developer community**](https://developer-cn.pico-interactive.com/community) and raise your question there.

## Feature
- The Project integrates with [CloudXR v3.2](https://developer.nvidia.com/nvidia-cloudxr-sdk-get-started) using [PICO OpenXR  SDK v2.2.0](https://developer-global.pico-interactive.com/sdk?deviceId=1&platformId=3&itemId=11).

## Pico OpenXR CloudXR Client Prerequisites
### Required
- A PICO VR headset that supports the PICO OpenXR SDK, for example - Pico3 series or Pico4 series.
- Android Debug Bridge (ADB) to install the client application without needing full developer tools.
  
### Optional
For developers who want to modify, build, and debug the client application, a complete Android development environment is required:
- Android Studio 4.0 or later.
- Android SDK 10 (API level 29) or higher.
- Android build tools 29.0.3
- Android NDK 21.4.7075529
- OpenJDK 1.8

> #### 💡 Note
>  - Android Studio can be downloaded from https://developer.android.com/studio.
>  - Installing Android Studio also installs Android Platform Tools, Android SDK, and OpenJDK.
>  - To install Android NDK from the Android Studio menus, click Tools -> SDK Manager, select SDK Tools and then NDK, and click OK.
>  - To use the bundled OpenJDK, click File -> Project Structure in the menu bar. In the SDK Location page under JDK Location, select the Use embedded JDK checkbox, and then click OK.
>  - The NDK version was selected because it was the LTS release at the time of development and is aligned with Android Studio 4.0.x as the default NDK version.
>  - Newer NDK versions, such as 23.x, might cause build issues.
>  - As of the CloudXR 3.2 release, only Android 64-bit builds are supported.
  

You also need:
- [Google Oboe SDK 1.6.0](https://github.com/google/oboe/releases/tag/1.6.0)
- [PICO OpenXR SDK](https://developer-global.pico-interactive.com/sdk?deviceId=1&platformId=3&itemId=16)

## Building the Pico CloudXR Client
1. Run Android Studio.
2. Complete one of the following tasks:
     - Select **Open existing Android Studio project** on the Welcome screen.
     - Click **File > Open**.
3. Navigate to `OpenXR_CloudXR_Client_Demo` folder and open the `OpenXR_CloudXR_Client_Demo` folder.
4. Select **Build > Make Project**.

This process should generate an `.apk` file in the `OpenXR_CloudXR_Client_Demo\app\build\outputs\apk\debug` directory that can be used to debug or be installed manually. You can also automatically generate an `.apk` file by running Android Studio.

> 💡 To build from the command line, run `gradlew build` from the `CloudXR_Client_Demo` folder.

## Installing the Pico OpenXR CloudXR Client

> 💡 You do not need these steps if you are running directly from Android Studio, it will install the `.apk` for you.
1. Place the Pico device in developer mode and allow a USB connection in debug mode on the device.
2. Use a USB cable to connect the Pico device to the development system.
3. If prompted on the device to allow connections, select **Allow**.
4. In a Command Prompt window, navigate to the folder that contains the `.apk` file that was created by building the sample application.
5. Use **ADB** to install the application from the release `.apk` file.
```
    adb.exe install -r <APK name>.apk
```
> 💡 By default, the ADB.exe program is installed by Android Studio in `C:\Users\{username}\AppData\Local\Android\Sdk\platform-tools`

When the installation is complete, ADB responds with a `Success` message.

## Running the Pico OpenXR CloudXR Client
1. (**Optional**) Pre-specify the command-line per device:
   1. Create a plain-text file named `CloudXRLaunchOptions.txt` that contains `-s <IP address NVIDIA CloudXR server>`.
  For example, for a server with `IP = 192.168.1.10`, the file should contain `-s 192.168.1.10`.
   2. Copy the `CloudXRLaunchOptions.txt` file to the base device folder as shown in Windows Explorer, or if using ADB, to the `/sdcard/` directory of the device using the following command:
      ```
      adb.exe push CloudXRLaunchOptions.txt /sdcard/CloudXRLaunchOptions.txt
      ```
      See [Command-Line Options](https://docs.nvidia.com/cloudxr-sdk/usr_guide/cmd_line_options.html#command-line-options) for more information about using launch options and a full list of all available options.

2. Start **SteamVR** on the server system.
3. Start the **OpenXR_CloudXR_Client_Demo** app on Pico device.
  This process can be completed in one of the following ways:
  - If installed from ADB with a launch options file, launch from VR on the device:
     - Open the main menu.
     - Select **Library**.
     - Select the **OpenXR_CloudXR_Client_Demo** app.
  - If building in Android Studio, deploy/run directly.
    - You can also set launch options inside the Configuration. See [Command-Line Options]((https://docs.nvidia.com/cloudxr-sdk/usr_guide/cmd_line_options.html#command-line-options)) for more information about how to set launch options from Android Studio.
    > 💡 If prompted, grant the requested permissions on the device.

4. Start the OpenVR application on the server that will be streamed to the client.
This process can be completed in one of the following ways:
  - Launch it directly on the server.
  > 💡 Launch the OpenVR application only after the client has connected to the server unless the client has been pre-configured on the server. Otherwise, the application will report that there is no connected headset. When a client first connects, it reports its specifications, such as resolution and refresh rate, to the server and then the server creates a virtual headset device
