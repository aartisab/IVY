# Nintendo Switch Arduino IDE Setup for flashing
You need additional libraries in the arduino IDE for this to work:  
 
1. **Arduino IDE → Settings → Additional boards manager URLs**, add both on separate lines:
```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   https://raw.githubusercontent.com/ricardoquesada/esp32-arduino-lib-builder/master/bluepad32_files/package_esp32_bluepad32_index.json
```
2. **Tools → Board → Boards Manager**, install **esp32** (Espressif Systems) and **esp32_bluepad32** (Ricardo Quesada).
4. **Tools → Board → esp32_bluepad32 → ESP32 Dev Module**, select the port, and then you can upload connector code.

# To make a venv to run the IMU + Controller GUI (do this only once):
Inside the IVY root folder
```bash
/opt/homebrew/bin/python3.14 -m venv ivy-env 
source ivy-env/bin/activate
pip install pyserial
```

# To run the test GUI: 
```bash
source ivy-env/bin/activate
python3 IMU_controller/dev_controller_gui.py      
```
 
## Usage
 
1. Hold the teeny **sync button** on top of the controller until it connects, should pop up on the GUI
2. Stand the rod upright, hold still, press **HOME** to calibrate.
3. Left stick = forward/back, right stick = turn. Tilt past 10° shows **TIPPING**.