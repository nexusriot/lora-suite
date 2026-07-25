package net.cardputer.companion

import android.Manifest
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp

class MainActivity : ComponentActivity() {
    private lateinit var ble: BleManager

    private val perms = registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {}

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        ble = BleManager(applicationContext)
        perms.launch(
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
                arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
            else
                arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        )
        setContent { App(ble) }
    }

    override fun onDestroy() {
        super.onDestroy()
        ble.disconnect()
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun App(ble: BleManager) {
    MaterialTheme(colorScheme = darkColorScheme()) {
        var tab by remember { mutableIntStateOf(0) }
        val titles = listOf("Link", "Chat", "Status", "Mesh", "Config")
        Scaffold(topBar = {
            TopAppBar(title = { Text("Cardputer  •  ${connLabel(ble.conn)}") })
        }) { pad ->
            Column(Modifier.padding(pad).fillMaxSize()) {
                ScrollableTabRow(selectedTabIndex = tab, edgePadding = 0.dp) {
                    titles.forEachIndexed { i, t -> Tab(selected = tab == i, onClick = { tab = i }, text = { Text(t) }) }
                }
                Box(Modifier.weight(1f).fillMaxWidth()) {
                    when (tab) {
                        0 -> LinkScreen(ble)
                        1 -> ChatScreen(ble)
                        2 -> StatusScreen(ble)
                        3 -> MeshScreen(ble)
                        else -> ConfigScreen(ble)
                    }
                }
            }
        }
    }
}

private fun connLabel(c: ConnState) = when (c) {
    ConnState.Idle -> "disconnected"
    ConnState.Scanning -> "scanning"
    ConnState.Connecting -> "connecting"
    ConnState.Connected -> "connected"
}

@Composable
private fun LinkScreen(ble: BleManager) {
    Column(Modifier.fillMaxSize().padding(16.dp)) {
        if (ble.conn == ConnState.Connected) {
            Text("Connected to ${ble.deviceName}", style = MaterialTheme.typography.titleMedium)
            Spacer(Modifier.height(12.dp))
            Button(onClick = { ble.disconnect() }) { Text("Disconnect") }
        } else {
            Button(onClick = { ble.startScan() }, enabled = ble.conn != ConnState.Scanning) {
                Text(if (ble.conn == ConnState.Scanning) "Scanning…" else "Scan for device")
            }
            Spacer(Modifier.height(8.dp))
            Text("Tap a device to connect (enable Bluetooth on the cardputer first):",
                style = MaterialTheme.typography.bodySmall)
            LazyColumn(Modifier.fillMaxWidth()) {
                items(ble.scanItems) { d ->
                    Card(Modifier.fillMaxWidth().padding(vertical = 4.dp).clickable { ble.connect(d.address) }) {
                        Column(Modifier.padding(12.dp)) {
                            Text(d.name, style = MaterialTheme.typography.titleSmall)
                            Text(d.address, style = MaterialTheme.typography.bodySmall)
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun ChatScreen(ble: BleManager) {
    var text by remember { mutableStateOf("") }
    var dst by remember { mutableStateOf("FFFF") }
    Column(Modifier.fillMaxSize().padding(12.dp)) {
        LazyColumn(Modifier.weight(1f).fillMaxWidth()) {
            items(ble.messages) { m ->
                Text(
                    (if (m.mine) "me → " else "${m.from} → ") + m.text +
                        (if (!m.mine && m.rssi != 0) "  (${m.rssi})" else ""),
                    color = if (m.mine) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurface,
                    modifier = Modifier.padding(vertical = 2.dp)
                )
            }
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            OutlinedTextField(dst, { dst = it.take(4) }, label = { Text("to") }, modifier = Modifier.width(90.dp))
            Spacer(Modifier.width(6.dp))
            OutlinedTextField(text, { text = it }, label = { Text("message") }, modifier = Modifier.weight(1f))
        }
        Button(onClick = { ble.sendText(dst.ifBlank { "FFFF" }, text); text = "" },
            enabled = ble.conn == ConnState.Connected, modifier = Modifier.padding(top = 6.dp)) {
            Text("Send (FFFF = broadcast)")
        }
    }
}

@Composable
private fun StatusScreen(ble: BleManager) {
    val s = ble.status
    Column(Modifier.fillMaxSize().padding(16.dp).verticalScroll(rememberScrollState())) {
        Button(onClick = { ble.requestStatus(); ble.requestNodes() },
            enabled = ble.conn == ConnState.Connected) { Text("Refresh") }
        Spacer(Modifier.height(10.dp))
        Text("GPS: " + if (s.fix) "fix, %d sats — %.5f, %.5f".format(s.sats, s.lat, s.lon) else "no fix")
        Text("Battery: ${s.batt}%")
        Text("RX frames: ${s.rx}    relayed: ${s.fwd}")
        Text("Channel: ${s.ch}    TX queue: ${s.q}")
        Spacer(Modifier.height(12.dp))
        Text("Nodes (${ble.nodes.size})", style = MaterialTheme.typography.titleSmall)
        ble.nodes.forEach { n ->
            Text("${n.addr}  ${n.name}  ${if (n.batt > 0) "${n.batt}%" else "?"}  ${n.rssi}dBm  ${n.age}s")
        }
    }
}

@Composable
private fun MeshScreen(ble: BleManager) {
    Column(Modifier.fillMaxSize().padding(16.dp)) {
        Button(onClick = { ble.requestMesh() }, enabled = ble.conn == ConnState.Connected) { Text("Refresh Meshtastic") }
        Spacer(Modifier.height(8.dp))
        Text("Foreign Meshtastic nodes (${ble.mesh.size})", style = MaterialTheme.typography.titleSmall)
        LazyColumn(Modifier.fillMaxWidth()) {
            items(ble.mesh) { m ->
                Column(Modifier.padding(vertical = 3.dp)) {
                    Text("!${m.id}  ${m.name}", style = MaterialTheme.typography.bodyMedium)
                    Text((if (m.hasPos) "%.5f, %.5f".format(m.lat, m.lon) else "no position") +
                        (if (m.batt > 0) "   ${m.batt}%" else ""), style = MaterialTheme.typography.bodySmall)
                }
            }
        }
    }
}

@Composable
private fun ConfigScreen(ble: BleManager) {
    var name by remember { mutableStateOf("") }
    var addr by remember { mutableStateOf("") }
    var bright by remember { mutableStateOf("") }
    var region by remember { mutableIntStateOf(-1) }
    var ssid by remember { mutableStateOf("") }
    var pass by remember { mutableStateOf("") }
    val regions = listOf("EU868", "US915", "AS923")
    Column(Modifier.fillMaxSize().padding(16.dp).verticalScroll(rememberScrollState())) {
        Text("Current: ${ble.identity.name} / ${ble.identity.addr}", style = MaterialTheme.typography.bodySmall)
        OutlinedTextField(name, { name = it }, label = { Text("callsign / name") }, modifier = Modifier.fillMaxWidth())
        OutlinedTextField(addr, { addr = it.take(4) }, label = { Text("address (hex, e.g. 1A2B)") }, modifier = Modifier.fillMaxWidth())
        OutlinedTextField(bright, { bright = it.filter { c -> c.isDigit() }.take(3) }, label = { Text("brightness 0-255") },
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number), modifier = Modifier.fillMaxWidth())
        Spacer(Modifier.height(8.dp))
        Text("Region:")
        Row {
            regions.forEachIndexed { i, r ->
                FilterChip(selected = region == i, onClick = { region = if (region == i) -1 else i },
                    label = { Text(r) }, modifier = Modifier.padding(end = 6.dp))
            }
        }
        Spacer(Modifier.height(12.dp))
        Button(
            onClick = {
                ble.sendConfig(
                    name.ifBlank { null }, addr.ifBlank { null },
                    if (region >= 0) region else null, bright.toIntOrNull(),
                    ssid.ifBlank { null }, pass
                )
            },
            enabled = ble.conn == ConnState.Connected
        ) { Text("Apply to device") }

        Spacer(Modifier.height(20.dp))
        HorizontalDivider()
        Spacer(Modifier.height(12.dp))
        Text("WiFi (for NTP time sync)", style = MaterialTheme.typography.titleSmall)
        OutlinedTextField(ssid, { ssid = it }, label = { Text("WiFi SSID") }, modifier = Modifier.fillMaxWidth())
        OutlinedTextField(pass, { pass = it }, label = { Text("WiFi password") },
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password), modifier = Modifier.fillMaxWidth())
        Text("Saved with Apply above. Sync fetches UTC over SNTP (~10 s).",
            style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(8.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            Button(onClick = { ble.syncNtp() }, enabled = ble.conn == ConnState.Connected) { Text("Sync time (NTP)") }
            Spacer(Modifier.width(10.dp))
            ble.ntpResult?.let { Text(if (it) "synced ✓" else "failed ✗") }
        }
    }
}
