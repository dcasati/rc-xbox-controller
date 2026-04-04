# rc xbox controller project

The rc xbox controller a solution consisting of an ESP32 wroom that replaces the standard controller from a RC car. The XBOX Controller communicates with the ESP32 over BLE using Bluepad2. Together allow for someone to operate the car using a XBOX controller.

## Main Function

I want to use an XBOX Controller to control my RC car over BLE on ESP32-WROOM.

The ESP32 should control the car two motors. There is a step motor that controls the left and right direction of the front wheels. There is another motor on the back that controls the speed of the RC car. There is also a LED in front of the car that functions as headlights.

To control the motors, we will use a TB6612FNG H-Bridge 

Because we use a USB for the project, the ESP32 should use OTA flashing and logging. 

## Implementation Phases

1. Implement all of the basic ESP32 functions, including driving forward, backward and turning left and right using the xbox controller over BLE.
2. Implement a function that allows for the LEDs to be turned on and off using the xbox controller over BLE.


## Tools

Use ESP-IDF and Apple native tools for the project.
Use the ESP32-workbench (https://github.com/SensorsIot/Universal-ESP32-Workbench) with its skills for flashing, testing.

Create a new GitHub Repository on https://github.com/dcasati/rc-xbox-controller