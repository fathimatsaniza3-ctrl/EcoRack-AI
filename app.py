import requests
from pathlib import Path
import streamlit.components.v1 as components
import numpy as np
import pandas as pd
import streamlit as st
import plotly.express as px

from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import accuracy_score
from streamlit_autorefresh import st_autorefresh

# Refresh the dashboard every 3 seconds
st_autorefresh(interval=3000, key="esp32_live_refresh")

st.set_page_config(page_title="EcoRack AI", page_icon="🌿", layout="wide")
st.title("🌿 EcoRack AI")
st.subheader("AI-Driven Data Centre Thermal Intelligence and Sustainable Resource Optimization")
st.caption("Student proof-of-concept. Sensor measurements and scenario estimates are shown separately.")

with st.sidebar:
    st.header("Connection")
    mode = st.radio("Data source", ["Simulation demo", "ESP32 live sensor"])
    esp_url = st.text_input("ESP32 base URL", "http://10.13.75.166")
    st.divider()
    st.header("Scenario assumptions")
    it_load_kw = st.slider("Illustrative IT load (kW)", 1.0, 100.0, 20.0, 1.0)
    water_l_day = st.slider("Illustrative cooling water (L/day)", 0, 10000, 1200, 50)
    grid_factor = st.slider("Grid emissions factor (kg CO2/kWh)", 0.1, 1.2, 0.7, 0.05)

tab3d, tab_analytics = st.tabs(["3D Digital Twin", "Analytics and Scenarios"])

with tab3d:
    # Standalone Three.js digital twin (needs internet once to load the 3D library).
    html_path = Path(__file__).parent / "ecorack_3d.html"
    components.html(html_path.read_text(encoding="utf-8"), height=980, scrolling=False)

with tab_analytics:
    # Synthetic training data for a demonstrator only, not real data-centre data.
    rng = np.random.default_rng(7)
    n = 1200
    temp_train = rng.uniform(18, 42, n)
    humidity_train = rng.uniform(20, 80, n)
    load_train = rng.uniform(0, 100, n)
    risk_train = ((temp_train >= 35) | ((temp_train >= 30) & (load_train > 75))).astype(int)

    X = pd.DataFrame({
        "temperature_c": temp_train,
        "humidity_pct": humidity_train,
        "load_pct": load_train
    })
    X_train, X_test, y_train, y_test = train_test_split(
        X, risk_train, test_size=0.2, random_state=7, stratify=risk_train
    )
    model = RandomForestClassifier(n_estimators=100, random_state=7,
                                   class_weight="balanced")
    model.fit(X_train, y_train)
    demo_accuracy = accuracy_score(y_test, model.predict(X_test))

    live_ok = False
    if mode == "ESP32 live sensor":
        try:
            response = requests.get(esp_url.rstrip("/") + "/data", timeout=2)
            response.raise_for_status()
            data = response.json()
            temperature = float(data["temperature_c"])
            humidity = float(data["humidity_pct"])
            hardware_status = str(data["status"])
            fan_on = bool(data["fan_on"])
            if temperature < -100:
                raise ValueError("Sensor reading is invalid")
            live_ok = True
        except Exception as exc:
            st.error(f"ESP32 connection failed: {exc}. Check the IP and Wi-Fi. Use Simulation demo if needed.")
            temperature, humidity, hardware_status, fan_on = 31.0, 48.0, "warning", False
    else:
        temperature = st.slider("Simulated temperature (°C)", 18.0, 42.0, 31.0, 0.5)
        humidity = st.slider("Simulated humidity (%)", 20.0, 80.0, 48.0, 1.0)
        hardware_status = ("critical" if temperature >= 35 else
                           "warning" if temperature >= 30 else "normal")
        fan_on = temperature >= 30

    load_pct = st.slider("Illustrative server workload (%)", 0, 100, 68, 1)
    features = pd.DataFrame([{
        "temperature_c": temperature,
        "humidity_pct": humidity,
        "load_pct": load_pct
    }])
    risk_probability = float(model.predict_proba(features)[0][1])
    risk_label = "Elevated" if risk_probability >= 0.5 else "Lower"

    m1, m2, m3, m4 = st.columns(4)
    m1.metric("Temperature", f"{temperature:.1f} °C")
    m2.metric("Humidity", f"{humidity:.1f} %")
    m3.metric("Illustrative ML risk", risk_label, f"{risk_probability:.0%} model score")
    m4.metric("Cooling fan", "ON" if fan_on else "OFF")

    if mode == "ESP32 live sensor" and live_ok:
        st.success(f"Live ESP32 sensor connected. Hardware status: {hardware_status}.")
    else:
        st.warning("Simulation mode: values are not live data-centre measurements.")

    st.subheader("Thermal trend")
    history = pd.DataFrame({
        "Minute": range(20),
        "Temperature (°C)": [
            max(18, min(42, temperature + np.sin(i / 3) * 2 + (i - 10) * 0.12))
            for i in range(20)
        ]
    })
    st.plotly_chart(px.line(history, x="Minute", y="Temperature (°C)", markers=True),
                    use_container_width=True)

    st.subheader("Water and energy scenario estimates")
    water_saving = st.slider("Hypothetical water reduction (%)", 0, 40, 10, 1)
    energy_saving = st.slider("Hypothetical energy reduction (%)", 0, 25, 5, 1)
    daily_energy = it_load_kw * 24
    daily_co2 = daily_energy * grid_factor
    c1, c2, c3 = st.columns(3)
    c1.metric("Illustrative energy/day", f"{daily_energy:,.1f} kWh")
    c2.metric("Illustrative CO2/day", f"{daily_co2:,.1f} kg")
    c3.metric("Scenario water saved/day", f"{water_l_day * water_saving / 100:,.0f} L")
    st.caption("These are calculations from user-entered assumptions, not measured savings. The energy reduction slider is a scenario, not a verified result.")
    st.metric("Hypothetical energy reduction", f"{daily_energy * energy_saving / 100:,.1f} kWh/day")

    st.subheader("Suggested actions")
    if temperature >= 35:
        st.error("CRITICAL demo threshold: check airflow and fan operation; the hardware buzzer should sound.")
    elif temperature >= 30:
        st.warning("WARNING demo threshold: check airflow and confirm the fan is operating.")
    else:
        st.success("Temperature is below the demo warning threshold. Continue monitoring.")

    if risk_probability >= 0.5:
        st.write("- The synthetic demo model estimates elevated risk; inspect sensor trends and workload.")
    else:
        st.write("- The synthetic demo model estimates lower risk; continue monitoring.")
    st.write("- Consider cooling set-point optimization and reclaimed water where technically appropriate.")
    st.write("- Verify any energy or water savings with real measurements before making claims.")

    with st.expander("Model details and limitations"):
        st.write(f"Synthetic holdout accuracy: {demo_accuracy:.1%}. This is only a code/demo check, not evidence of real-world prediction accuracy.")
        st.write("The model is trained on synthetic examples generated from simple rules. Replace it with labelled, representative data and validate it before real use.")
        st.write("The 30°C and 35°C thresholds are for a tabletop demonstration only, not data-centre standards.")
        st.write("Never connect this prototype to a production data-centre control system.")
