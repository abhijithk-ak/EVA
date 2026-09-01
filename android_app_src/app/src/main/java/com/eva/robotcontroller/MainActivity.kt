package com.eva.robotcontroller

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothSocket
import android.content.Context
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.webkit.JavascriptInterface
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import java.io.OutputStream
import java.util.UUID

class MainActivity : AppCompatActivity() {

    private var webView: WebView? = null
    private var bluetoothSocket: BluetoothSocket? = null
    private var outputStream: OutputStream? = null
    private val SPP_UUID: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        try {
            setContentView(R.layout.activity_main)

            webView = findViewById(R.id.webView)
            webView?.settings?.apply {
                javaScriptEnabled = true
                domStorageEnabled = true
                allowFileAccess = true
                allowContentAccess = true
                databaseEnabled = true
            }
            webView?.webViewClient = WebViewClient()

            // Bind Native Android Bluetooth helper to JavaScript engine
            webView?.addJavascriptInterface(WebAppInterface(), "AndroidBluetooth")

            // Load local HTML app asset
            webView?.loadUrl("file:///android_asset/index.html")
        } catch (e: Throwable) {
            Log.e("EVA_APP", "Error initializing activity", e)
        }

        requestPermissions()
    }

    private fun getAdapter(): BluetoothAdapter? {
        return try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager
                bluetoothManager?.adapter
            } else {
                @Suppress("DEPRECATION")
                BluetoothAdapter.getDefaultAdapter()
            }
        } catch (e: Throwable) {
            Log.e("EVA_APP", "Error getting BluetoothAdapter", e)
            null
        }
    }

    private fun requestPermissions() {
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                ActivityCompat.requestPermissions(
                    this,
                    arrayOf(
                        android.Manifest.permission.BLUETOOTH_CONNECT,
                        android.Manifest.permission.BLUETOOTH_SCAN,
                        android.Manifest.permission.ACCESS_FINE_LOCATION
                    ),
                    101
                )
            } else {
                ActivityCompat.requestPermissions(
                    this,
                    arrayOf(
                        android.Manifest.permission.BLUETOOTH,
                        android.Manifest.permission.BLUETOOTH_ADMIN,
                        android.Manifest.permission.ACCESS_FINE_LOCATION
                    ),
                    101
                )
            }
        } catch (e: Throwable) {
            Log.e("EVA_APP", "Error requesting permissions", e)
        }
    }

    private fun handleLinkDrop() {
        try {
            bluetoothSocket?.close()
        } catch (e: Throwable) {}
        bluetoothSocket = null
        outputStream = null
        runOnUiThread {
            webView?.evaluateJavascript("if (window.setConnected) window.setConnected(false); if (window.log) window.log('Bluetooth link dropped!', 'err');", null)
        }
    }

    inner class WebAppInterface {
        @JavascriptInterface
        fun connectDevice(targetName: String): Boolean {
            return connectToBluetoothDevice(targetName)
        }

        @JavascriptInterface
        fun disconnectDevice() {
            handleLinkDrop()
        }

        @JavascriptInterface
        fun sendCommand(command: String) {
            try {
                val os = outputStream ?: throw java.io.IOException("Socket null")
                os.write((command + "\n").toByteArray())
                os.flush()
            } catch (e: Throwable) {
                Log.e("EVA_APP", "Error sending command", e)
                handleLinkDrop()
            }
        }

        @JavascriptInterface
        fun sendDrive(charCmd: String) {
            try {
                val os = outputStream ?: throw java.io.IOException("Socket null")
                os.write(charCmd.toByteArray())
                os.flush()
            } catch (e: Throwable) {
                Log.e("EVA_APP", "Error sending drive char", e)
                handleLinkDrop()
            }
        }
    }

    @SuppressLint("MissingPermission")
    private fun connectToBluetoothDevice(targetName: String): Boolean {
        return try {
            val adapter = getAdapter() ?: return false
            if (!adapter.isEnabled) {
                runOnUiThread {
                    Toast.makeText(this, "Please turn ON Bluetooth on your phone", Toast.LENGTH_SHORT).show()
                }
                return false
            }

            val pairedDevices: Set<BluetoothDevice>? = adapter.bondedDevices
            val device = pairedDevices?.firstOrNull {
                try {
                    it.name != null && it.name.contains(targetName, ignoreCase = true)
                } catch (e: Throwable) {
                    false
                }
            } ?: return false

            bluetoothSocket = device.createRfcommSocketToServiceRecord(SPP_UUID)
            bluetoothSocket?.connect()
            outputStream = bluetoothSocket?.outputStream
            true
        } catch (e: Throwable) {
            Log.e("EVA_APP", "Error connecting to BT device", e)
            false
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        handleLinkDrop()
    }
}
