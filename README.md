# Rejseplanen API Bus Depature E-Paper Display

**NOTE**: I have started a new project wherein I will do more in depth tests with the WeAct display. Star ([Still-in-Motion](https://github.com/QC20/still-in-motion)) to stay updated.

This project is a creative implementation of a **Rejseplanen** bus and train schedule display using an ESP32 and an E-Paper screen. The idea is to emulate the look and functionality of the actual bus stop displays found in Denmark, but in a compact, energy-efficient form. The E-Paper display updates at regular intervals (once every minute) to show the latest bus and train schedules.

<div align="center" title="This is a working weather API fetcher showing the weather in Copenhagen." style="max-width: 75%; margin: 20px auto; padding: 10px; border: 2px solid #ccc; background-color: #f9f9f9; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); border-radius: 8px; width: 75%;">
  <img src="/src/assets/images/Weather-API-Prototype.jpg" alt="Image of a working weather API fetcher showing the weather in Copenhagen" style="width: 75%; height: auto; display: block; border-radius: 8px;">
</div>

## Features

- **Energy Efficiency**: Utilizes an E-Paper display, which consumes very little power, especially when the content does not need to be refreshed frequently.
- **WiFi Connectivity**: The ESP32 connects to a WiFi network to fetch real-time data from the Rejseplanen API.
- **Minimalist Design**: The display is designed to resemble actual bus stop displays, providing a familiar and aesthetically pleasing interface.
- **Easy Customization**: Modify the code to adjust the city, fonts, or display parameters to suit your needs.

## Hardware Requirements

- **ESP32**: A powerful microcontroller with built-in WiFi.
- **E-Paper Display**: A monochrome E-Ink display that mimics paper, ideal for low-power projects.
- **WiFi Network**: Necessary for fetching data from the Rejseplanen API.

## Software Requirements

- **Arduino IDE**: For programming the ESP32.
- **GxEPD2 Library**: For controlling the E-Paper display.
- **ArduinoJson Library**: For parsing the JSON data received from the API.

## Setup

1. **Clone the Repository**:
   ```bash
   git clone https://github.com/yourusername/rejseplanen-e-paper-display.git
   cd rejseplanen-e-paper-display
