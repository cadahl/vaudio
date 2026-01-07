# vaudio

Car stereo head unit, implemented with an ESP32 Wroom.

Rock solid and with a 60 Hz UI. Inspired by the functional design of the late 80's and early 90's.

Has worked flawlessly in the field for over a year (so far).

Overview
- Double DIN front plate
    - SSD1309 OLED display (128x64 monochrome)
    - Volume knob on rotational encoder (in half-dB steps)
    - Chunky buttons (mute, connect, menu, up, down, select, cancel)
    - 2x USB-A ports (one charge 1.5A, one UART 500mA).
- Electronics
    - Powered by car stereo 12V rail.
    - ESP32 acts as Bluetooth audio receiver
    - Components mostly on the back of the front plate
    - External components housed in the double-DIN cassette:
        - 5V 3A stepdown converter
        - GY-PCM5102 32-bit/384kHz stereo DAC
        - GPIO expander
        - Relay for amplifier remote power control
- Firmware
    - Written in C11, ESP-IDF/FreeRTOS.
    - Modules
        - bt_* - BT audio streaming (from an Expressif sample).
        - va_app - Top level application code.
        - va_audio - BT app-side event handling, audio volume and channel matrix processing.
        - va_cqueue - Lockfree queue implementation (used to send waveform to the visualizers).
        - va_debug - Logging helpers mostly.
        - va_disp - OLED display control (init, dimming etc).
        - va_io - Button and rot. enc. inputs, DAC enable, amp enable.
        - va_math - Utility functions for the graphical effects.
        - va_menu - Menu system.
        - va_msg - Minimal message system between modules.
        - va_props - User settings persisted in NVRAM.
        - va_raster - Drawing functions for the graphical effects.
        - va_screen - Top level screen management.
        - va_screen_connect - Shown when the user pairs a BT device.
        - va_screen_idle - Shown when no user activity has occurred for a while.
        - va_screen_play - Shown when playing audio.
        - va_screen_splash - Shown on system startup.

To be expanded with schematics and PCB layout.
