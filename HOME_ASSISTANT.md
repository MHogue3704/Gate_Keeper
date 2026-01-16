# Home Assistant Integration Guide

This guide explains how to integrate your ESP32 Gate Keeper device into Home Assistant to view live status, battery levels, and logs.

## Prerequisites

1.  Your ESP32 Gate Keeper must be connected to the same network as your Home Assistant instance.
2.  Note the **IP Address** displayed at the bottom of the Gate Keeper screen (e.g., `192.168.1.50`).

## 1. Add Sensors to `configuration.yaml`

Add the following configuration to your `configuration.yaml` file in Home Assistant. Replace `192.168.1.50` with your device's actual IP address.

```yaml
sensor:
  - platform: rest
    resource: http://192.168.1.50/status
    name: "Gate Keeper"
    scan_interval: 10
    json_attributes:
      - state
      - state_code
      - battery_percent
      - charging
      - weather
      - ip
      - uptime_ms
    value_template: "{{ value_json.state }}"  # Main state: OPEN, CLOSED, or LOST SIGNAL

  # Template sensors to break out attributes into individual entities
  - platform: template
    sensors:
      gate_battery:
        friendly_name: "Gate Keeper Battery"
        unit_of_measurement: "%"
        device_class: battery
        value_template: "{{ state_attr('sensor.gate_keeper', 'battery_percent') }}"
      
      gate_charging:
        friendly_name: "Gate Keeper Charging"
        value_template: "{{ state_attr('sensor.gate_keeper', 'charging') }}"

      gate_weather:
        friendly_name: "Gate Weather Info"
        value_template: "{{ state_attr('sensor.gate_keeper', 'weather') }}"
```

**Restart Home Assistant** after adding these lines.

## 2. Accessing the Log

The device saves a raw text log of events to the SD card. You can access this directly in Home Assistant using a `downloader` command or simply view it in a dashboard using an iframe (Webpage card).

### Option A: Webpage Card (Dashboard)

1.  Edit your Dashboard.
2.  Add a **Webpage** card.
3.  Set the URL to: `http://192.168.1.50/` (This shows the full dashboard) or `http://192.168.1.50/log.txt` (for just the raw log).

### Option B: Command Line Sensor (Advanced)

If you want the last log line as a sensor:

```yaml
sensor:
  - platform: command_line
    name: Gate Last Log
    command: "curl -s http://192.168.1.50/log.txt | tail -n 1"
    scan_interval: 60
```

## 3. Example Dashboard Card

Here is a simple YAML configuration for an Entities card to show your new sensors:

```yaml
type: entities
title: Gate Keeper
entities:
  - entity: sensor.gate_keeper
    name: Status
  - entity: sensor.gate_battery
    name: Battery
  - entity: sensor.gate_charging
    name: Charging
  - entity: sensor.gate_weather
    name: Weather At Gate
  - type: weblink
    name: Open Dashboard
    url: http://192.168.1.50/
    icon: mdi:web
```

## Available API Endpoints

The Gate Keeper exposes the following HTTP endpoints:

*   **`GET /`**: Full graphical dashboard with status and scrollable log.
*   **`GET /status`**: JSON formatted data for Home Assistant.
*   **`GET /log.txt`**: Raw text file content of the event log.
*   **`POST /clear`**: Clears the event log (protected by a confirmation on the web UI).
