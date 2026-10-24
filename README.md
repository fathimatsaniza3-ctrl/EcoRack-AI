# 🌱 EcoRack AI — Smart Server Rack Monitoring

EcoRack AI is an IoT-based system that monitors server rack temperature and humidity in real time using ESP32 and DHT22 sensors.

## 🚀 Features
- 🌡️ Real-time temperature and humidity monitoring
- 📊 Streamlit dashboard
- 🖥️ 3D server rack visualization
- 🌬️ Automatic fan control
- 🔔 Buzzer alerts for high temperatures
- 📡 ESP32 Wi-Fi connectivity

## 🛠️ Technologies
- ESP32 and DHT22
- Python and Streamlit
- HTML, CSS, and JavaScript
- PlatformIO
- NumPy, Pandas, Plotly, and Scikit-learn

## 🔌 Hardware
- ESP32 development board
- DHT22 sensor
- Cooling fan and suitable driver circuit
- Buzzer
- OLED display (if configured)

## ⚙️ Run the Dashboard

Install the dependencies:

```bash
pip install streamlit requests numpy pandas plotly scikit-learn streamlit-autorefresh
```

Start the application from the project folder:

```bash
python -m streamlit run app.py
```

Open the local URL shown in your terminal.

## 📡 How It Works
1. The DHT22 measures temperature and humidity.
2. The ESP32 reads the sensor data.
3. The ESP32 controls the fan and buzzer according to configured thresholds.
4. The dashboard retrieves sensor readings from the ESP32's `/data` endpoint.

## 🔒 Security
Never publish Wi-Fi passwords or other private credentials in your source code.

## 🎯 Goal
To improve server rack safety, environmental monitoring, and cooling efficiency using IoT technology.

**Project:** EcoRack AI  
**GitHub:** [fathimatsaniza3-ctrl](https://github.com/fathimatsaniza3-ctrl)

