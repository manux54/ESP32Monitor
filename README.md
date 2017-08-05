# ESP32 Monitor Getting Started

This application has been tested using the following hardware:
- DOIT ESP32 Development Board but any ESP32 board should do
- MCP9808 Temperature sensor from [Adafruit](https://www.adafruit.com/product/1782)

## Step 1: ESP32 Toolchain Setup
Follow the instruction from [here](http://esp-idf.readthedocs.io/en/latest/#setup-toolchain) to setup your toolchain environment.

__Windows User Note:__ At the time of writing there is a bug in the toolchain under Windows 10. In order to build this project, or any of the samples, you need to edit *$IDF_PATH/make/project.mk* and change "gcc-ar" for "ar". [Info here](https://www.esp32.com/viewtopic.php?t=2500)

It would be wise to run one of the sample to validate your setup.

## Step 2: Install Azure IoT C SDK


Create an `azure-iot` directory in the ESP32 SDK's `components` directory:<br/>
`mkdir $IDF_PATH/components/azure-iot`

Clone the Azure IoT C SDK into the `azure-iot` directory as `sdk`:<br/>
`cd $IDF_PATH/components/azure-iot`<br/>
`git clone --recursive  https://github.com/Azure/azure-iot-sdk-c.git sdk`

__Note:__ At the time of writing, there is a bug in the SDK that prevent you from compiling the SDK and you might need to rollback to the previous release. You can try to skip this step first to see if it has been fixed.<br/>
`cd sdk`<br/>
`git reset --hard 34ba88699a76cbe1d88b2f75e67a3d91cb0f51e8`<br/>
`cd ..`<br/>

Copy the `component.mk` file for ESP32 into the `azure-iot` directory:<br/>
`cp sdk/c-utility/build_all/esp32/sdk/component.mk .`

## Step 3: Project setup

__Windows User Note:__ Paths and commands are relative to Msys2 environment built in step 1

Clone this project into ~/esp/projects

Setup build configuration:<br/>
`make menuconfig`

Set the appropriate values in __Serial Flasher Config > Default Serial Port__, __MCP 9808 Configuration__ and __Azure Configuration__

Build the application and flash the device<br/>
`make flash`

Monitor the serial port <br/>
`make monitor`
