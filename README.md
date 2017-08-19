# ESP32 Monitor Getting Started

This application has been tested using the following hardware:
- DOIT ESP32 Development Board but any ESP32 board should do
- MCP9808 Temperature sensor from [Adafruit](https://www.adafruit.com/product/1782)

## Step 1: ESP32 Toolchain Setup
Follow the instruction from [here](http://esp-idf.readthedocs.io/en/latest/#setup-toolchain) to setup your toolchain environment.

__Windows User Note:__ At the time of writing there is a bug in the toolchain under Windows. In order to build this project, or any of the samples, you need to copy __liblto_plugin.dll.a__ and __liblto_plugin-0.dll__ into the tool chain. Refer to the [link](https://github.com/espressif/esp-idf/issues/828) for more information.

It would be wise to run one of the sample to validate your setup.

## Step 2: Install Azure IoT C SDK


Create an `azure-iot` directory in the ESP32 SDK's `components` directory:<br/>
`mkdir $IDF_PATH/components/azure-iot`

Clone the Azure IoT C SDK into the `azure-iot` directory as `sdk`:<br/>
`cd $IDF_PATH/components/azure-iot`<br/>
`git clone --recursive  https://github.com/Azure/azure-iot-sdk-c.git sdk`

__Note:__ At the time of writing, there is also a bug in the SDK's component makefile that prevent you from compiling the SDK. Open the file `sdk/c-utility/build_all/esp32/sdk/component.mk` into your favorite editor and make sure that `sdk/iothub_client/src/iothub_client_retry_control.o` is in the __COMPONENT_OBJS__ section. Refer to the [link](https://github.com/Azure/azure-c-shared-utility/issues/94) for more information.<br/>

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
