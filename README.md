# SMART-STORAGE
Smart Pneumatic Storage System 🏗️
Project Overview
An automated industrial-grade storage solution designed to optimize space and material handling. This system utilizes an Arduino Mega to control Hydraulic Cylinders based on real-time feedback from Ultrasonic Sensors, ensuring precise positioning and safety.

🚀 Key Features
Automated Leveling: Real-time distance tracking using ultrasonic sensors.

Precision Control: Smooth hydraulic actuation via relay modules/solenoid valves.

Fault Detection: Safety limits to prevent mechanical overextension.

Scalable Architecture: Built on Arduino Mega to allow for more sensors or manual overrides.

🛠️ Hardware Components
Microcontroller: Arduino Mega 2560.

Actuators: Pneumatic Cylinders & Pump System.

Sensors: HC-SR04 Ultrasonic Sensors.

Control: Relay Modules / Motor Driver (depending on your valve setup).

Power Supply: 12V/24V DC (External).

💻 Software & Tools
Programming: C++ (Arduino IDE).

Simulation:  Festo FluidSIM.

Documentation: Fritzing for circuit diagrams.

⚙️ How it Works
The Ultrasonic Sensor measures the current height/position of the storage platform.

The Arduino Mega processes the data and compares it against the target setpoint.

The controller triggers the Hydraulic Valves to extend or retract the cylinders.

Feedback loop ensures the platform stops exactly at the required level.
