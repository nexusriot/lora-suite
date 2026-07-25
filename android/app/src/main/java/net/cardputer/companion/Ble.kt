package net.cardputer.companion

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Handler
import android.os.Looper
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import org.json.JSONObject
import java.util.UUID

enum class ConnState { Idle, Scanning, Connecting, Connected }

data class ScanItem(val name: String, val address: String)
data class Msg(val from: String, val text: String, val rssi: Int, val mine: Boolean)
data class NodeInfo(val addr: String, val name: String, val batt: Int, val rssi: Int, val age: Long)
data class MeshInfo(val id: String, val name: String, val lat: Double, val lon: Double, val batt: Int, val hasPos: Boolean)
data class Identity(val name: String = "node", val addr: String = "0000")
data class Status(
    val fix: Boolean = false, val lat: Double = 0.0, val lon: Double = 0.0, val sats: Int = 0,
    val batt: Int = 0, val rx: Long = 0, val fwd: Long = 0, val ch: Int = 0, val q: Int = 0
)

// Talks to the cardputer's Nordic-UART-Service bridge: writes JSON commands, parses
// newline-delimited JSON events into Compose-observable state.
@SuppressLint("MissingPermission")
class BleManager(private val ctx: Context) {
    companion object {
        val NUS = UUID.fromString("6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
        val NUS_RX = UUID.fromString("6E400002-B5A3-F393-E0A9-E50E24DCCA9E")
        val NUS_TX = UUID.fromString("6E400003-B5A3-F393-E0A9-E50E24DCCA9E")
        val CCCD = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    }

    private val bt = ctx.getSystemService(BluetoothManager::class.java)
    private val main = Handler(Looper.getMainLooper())
    private var scanner: BluetoothLeScanner? = null
    private var gatt: BluetoothGatt? = null
    private var rxChar: BluetoothGattCharacteristic? = null
    private val rxBuf = StringBuilder()

    var conn by mutableStateOf(ConnState.Idle); private set
    var deviceName by mutableStateOf(""); private set
    val scanItems = mutableStateListOf<ScanItem>()
    val messages = mutableStateListOf<Msg>()
    val nodes = mutableStateListOf<NodeInfo>()
    val mesh = mutableStateListOf<MeshInfo>()
    var status by mutableStateOf(Status()); private set
    var identity by mutableStateOf(Identity()); private set
    var ntpResult by mutableStateOf<Boolean?>(null)   // null = idle/pending, true/false = last outcome

    fun startScan() {
        scanItems.clear()
        conn = ConnState.Scanning
        scanner = bt.adapter?.bluetoothLeScanner
        val settings = ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build()
        scanner?.startScan(null, settings, scanCb)
    }

    fun stopScan() {
        scanner?.stopScan(scanCb)
        if (conn == ConnState.Scanning) conn = ConnState.Idle
    }

    private val scanCb = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val dev = result.device
            val nm = dev.name ?: result.scanRecord?.deviceName ?: return
            val hasNus = result.scanRecord?.serviceUuids?.any { it.uuid == NUS } == true
            if (nm.startsWith("LoRa-") || hasNus) main.post {
                if (scanItems.none { it.address == dev.address }) scanItems.add(ScanItem(nm, dev.address))
            }
        }
    }

    fun connect(address: String) {
        stopScan()
        conn = ConnState.Connecting
        val dev = bt.adapter?.getRemoteDevice(address) ?: return
        gatt = dev.connectGatt(ctx, false, gattCb, BluetoothDevice.TRANSPORT_LE)
    }

    fun disconnect() {
        gatt?.disconnect()
        gatt?.close()
        gatt = null
        rxChar = null
        main.post { conn = ConnState.Idle }
    }

    fun send(json: String) {
        val c = rxChar ?: return
        val g = gatt ?: return
        c.writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
        @Suppress("DEPRECATION")
        run { c.value = json.toByteArray(Charsets.UTF_8); g.writeCharacteristic(c) }
    }

    fun sendText(to: String, text: String) {
        if (text.isBlank()) return
        send(JSONObject().put("c", "tx").put("to", to).put("t", text).toString())
        main.post { messages.add(Msg("me", text, 0, true)) }
    }

    fun requestNodes() { main.post { nodes.clear() }; send("{\"c\":\"get\",\"w\":\"nodes\"}") }
    fun requestMesh() { main.post { mesh.clear() }; send("{\"c\":\"get\",\"w\":\"mesh\"}") }
    fun requestStatus() = send("{\"c\":\"get\",\"w\":\"status\"}")

    fun sendConfig(name: String?, addr: String?, region: Int?, bright: Int?,
                   wssid: String? = null, wpass: String? = null) {
        val o = JSONObject().put("c", "cfg")
        if (!name.isNullOrBlank()) o.put("name", name)
        if (!addr.isNullOrBlank()) o.put("addr", addr)
        if (region != null) o.put("region", region)
        if (bright != null) o.put("bright", bright)
        if (!wssid.isNullOrBlank()) { o.put("wssid", wssid); o.put("wpass", wpass ?: "") }
        send(o.toString())
    }

    // Ask the device to associate with its stored WiFi and sync UTC over SNTP.
    fun syncNtp() { main.post { ntpResult = null }; send("{\"c\":\"ntp\"}") }

    private val gattCb = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, st: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) g.requestMtu(247)
            else if (newState == BluetoothProfile.STATE_DISCONNECTED) main.post { conn = ConnState.Idle }
        }

        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, st: Int) { g.discoverServices() }

        override fun onServicesDiscovered(g: BluetoothGatt, st: Int) {
            val svc = g.getService(NUS) ?: return
            rxChar = svc.getCharacteristic(NUS_RX)
            val tx = svc.getCharacteristic(NUS_TX) ?: return
            g.setCharacteristicNotification(tx, true)
            tx.getDescriptor(CCCD)?.let { d ->
                @Suppress("DEPRECATION")
                run { d.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE; g.writeDescriptor(d) }
            }
            main.post { conn = ConnState.Connected; deviceName = g.device.name ?: "" }
            requestStatus()
        }

        // Android <13
        @Deprecated("Deprecated in Java")
        override fun onCharacteristicChanged(g: BluetoothGatt, ch: BluetoothGattCharacteristic) {
            @Suppress("DEPRECATION")
            if (ch.uuid == NUS_TX) onBytes(ch.value)
        }

        // Android 13+
        override fun onCharacteristicChanged(g: BluetoothGatt, ch: BluetoothGattCharacteristic, value: ByteArray) {
            if (ch.uuid == NUS_TX) onBytes(value)
        }
    }

    private fun onBytes(b: ByteArray?) {
        if (b == null) return
        rxBuf.append(String(b, Charsets.UTF_8))
        var nl = rxBuf.indexOf("\n")
        while (nl >= 0) {
            val line = rxBuf.substring(0, nl)
            rxBuf.delete(0, nl + 1)
            if (line.isNotBlank()) handleLine(line)
            nl = rxBuf.indexOf("\n")
        }
    }

    private fun handleLine(line: String) {
        val o = try { JSONObject(line) } catch (e: Exception) { return }
        main.post {
            when (o.optString("e")) {
                "msg" -> messages.add(Msg(o.optString("from"), o.optString("t"), o.optInt("rssi"), false))
                "st" -> status = Status(
                    fix = o.optInt("fix") == 1, lat = o.optDouble("lat", 0.0), lon = o.optDouble("lon", 0.0),
                    sats = o.optInt("sats"), batt = o.optInt("batt"), rx = o.optLong("rx"),
                    fwd = o.optLong("fwd"), ch = o.optInt("ch"), q = o.optInt("q")
                )
                "nd" -> upsert(nodes, o.optString("addr")) {
                    NodeInfo(o.optString("addr"), o.optString("name"), o.optInt("batt"), o.optInt("rssi"), o.optLong("age"))
                }
                "mn" -> upsertMesh(o)
                "cfg" -> identity = Identity(o.optString("name"), o.optString("addr"))
                "ntp" -> ntpResult = o.optInt("ok") == 1
            }
        }
    }

    private fun upsert(list: MutableList<NodeInfo>, addr: String, make: () -> NodeInfo) {
        val i = list.indexOfFirst { it.addr == addr }
        val v = make()
        if (i >= 0) list[i] = v else list.add(v)
    }

    private fun upsertMesh(o: JSONObject) {
        val id = o.optString("id")
        val v = MeshInfo(id, o.optString("name"), o.optDouble("lat", 0.0), o.optDouble("lon", 0.0),
            o.optInt("batt"), o.has("lat"))
        val i = mesh.indexOfFirst { it.id == id }
        if (i >= 0) mesh[i] = v else mesh.add(v)
    }
}
