# EVA Robot Native Android App

A native Android application replica of the Cyberpunk Web Controller, built with Kotlin and Android Native Bluetooth RFCOMM SPP (`BluetoothAdapter` & `BluetoothSocket`).

## Features
- **100% Native Bluetooth SPP**: Connects directly to paired `"EVA"` Bluetooth device without USB OTG errors.
- **Horizontal Landscape RC Mode**: 360° touch joystick with zero-latency auto-stop `"S"` command on touch release.
- **Clock & Alarm UX**: One-tap phone time & date auto-sync (`TIME HH:MM` and `DATE DD/MM/YYYY`), alarm toggle.
- **Volume & Brightness Sliders**: Dynamic `V <val>` and `T <val>` sliders.
- **Wi-Fi Credentials Setup**: Save SSID/Password directly to EVA NVS (`WIFI <ssid> <pass>`).

## How to Build in Android Studio
1. Open **Android Studio**.
2. Select **Open an Existing Project**.
3. Choose the directory: `c:\Users\Admin\OneDrive\Desktop\Evav2claude\EVA\android_app`.
4. Click **Build -> Build Bundle(s) / APK(s) -> Build APK(s)**.
5. Install the generated `.apk` file onto your Android phone!

## Requirements
- Android 5.0 (Lollipop) or newer.
- Pair phone with **"EVA"** in Android Bluetooth Settings before connecting.
