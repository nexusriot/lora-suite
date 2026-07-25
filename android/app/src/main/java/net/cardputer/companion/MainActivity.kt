package net.cardputer.companion

import android.Manifest
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Bluetooth
import androidx.compose.material.icons.filled.Email
import androidx.compose.material.icons.filled.Groups
import androidx.compose.material.icons.filled.Hub
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
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

private enum class Dest(val label: String, val icon: ImageVector) {
    Link("Link", Icons.Filled.Bluetooth),
    Messages("Msgs", Icons.Filled.Email),
    Fleet("Fleet", Icons.Filled.Groups),
    Mesh("Mesh", Icons.Filled.Hub),
    Ops("Ops", Icons.Filled.Warning),
    Config("Config", Icons.Filled.Settings),
}

private val PRESENCE = listOf("Available", "Busy", "En-route", "Resting")
private val POWER = listOf("Performance", "Balanced", "Endurance", "Survival")

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun App(ble: BleManager) {
    MaterialTheme(colorScheme = darkColorScheme()) {
        var dest by remember { mutableStateOf(Dest.Link) }
        val bottom = listOf(Dest.Link, Dest.Messages, Dest.Fleet, Dest.Mesh, Dest.Ops)
        Scaffold(
            topBar = {
                TopAppBar(
                    title = { Text("LoRa  •  ${connLabel(ble.conn)}") },
                    actions = {
                        IconButton(onClick = { dest = Dest.Config }) {
                            Icon(Icons.Filled.Settings, contentDescription = "Config")
                        }
                    }
                )
            },
            bottomBar = {
                NavigationBar {
                    bottom.forEach { d ->
                        NavigationBarItem(
                            selected = dest == d,
                            onClick = { dest = d },
                            icon = { Icon(d.icon, contentDescription = d.label) },
                            label = { Text(d.label) }
                        )
                    }
                }
            }
        ) { pad ->
            Box(Modifier.padding(pad).fillMaxSize()) {
                when (dest) {
                    Dest.Link -> LinkScreen(ble)
                    Dest.Messages -> MessagesScreen(ble)
                    Dest.Fleet -> FleetScreen(ble)
                    Dest.Mesh -> MeshScreen(ble)
                    Dest.Ops -> OpsScreen(ble)
                    Dest.Config -> ConfigScreen(ble)
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

private fun connected(ble: BleManager) = ble.conn == ConnState.Connected

@Composable
private fun LinkScreen(ble: BleManager) {
    Column(Modifier.fillMaxSize().padding(16.dp)) {
        if (connected(ble)) {
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
private fun MessagesScreen(ble: BleManager) {
    var text by remember { mutableStateOf("") }
    var dst by remember { mutableStateOf("FFFF") }
    var showContacts by remember { mutableStateOf(false) }
    val canned = listOf("ROGER", "OK", "ON MY WAY", "STANDBY", "NEGATIVE")
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
        Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()), verticalAlignment = Alignment.CenterVertically) {
            canned.forEach { c ->
                AssistChip(onClick = { ble.sendText(dst.ifBlank { "FFFF" }, c) }, label = { Text(c) },
                    modifier = Modifier.padding(end = 6.dp))
            }
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            OutlinedTextField(dst, { dst = it.take(4) }, label = { Text("to") }, singleLine = true, modifier = Modifier.width(90.dp))
            Spacer(Modifier.width(6.dp))
            OutlinedTextField(text, { text = it }, label = { Text("message") }, modifier = Modifier.weight(1f))
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            Button(onClick = { ble.sendText(dst.ifBlank { "FFFF" }, text); text = "" }, enabled = connected(ble)) {
                Text("Send")
            }
            Spacer(Modifier.width(8.dp))
            TextButton(onClick = { showContacts = !showContacts; if (showContacts) ble.requestRoster() }) {
                Text(if (showContacts) "Hide contacts" else "Contacts")
            }
            Spacer(Modifier.weight(1f))
            Text("FFFF = broadcast", style = MaterialTheme.typography.bodySmall)
        }
        if (showContacts) {
            LazyColumn(Modifier.fillMaxWidth().heightIn(max = 180.dp)) {
                items(ble.contacts) { c ->
                    Row(Modifier.fillMaxWidth().clickable { dst = c.addr }.padding(vertical = 6.dp)) {
                        Text(c.addr, modifier = Modifier.width(56.dp), style = MaterialTheme.typography.bodyMedium)
                        Text(c.name + (if (c.favorite) "  ★" else "") + (if (c.blocked) "  ⛔" else ""),
                            style = MaterialTheme.typography.bodyMedium)
                    }
                }
                if (ble.contacts.isEmpty()) item { Text("no contacts", style = MaterialTheme.typography.bodySmall) }
            }
        }
    }
}

@Composable
private fun FleetScreen(ble: BleManager) {
    val s = ble.status
    Column(Modifier.fillMaxSize().padding(16.dp).verticalScroll(rememberScrollState())) {
        Button(onClick = { ble.requestStatus(); ble.requestNodes() }, enabled = connected(ble)) { Text("Refresh") }
        Spacer(Modifier.height(10.dp))
        Text("Node: ${s.name}", style = MaterialTheme.typography.titleSmall)
        Text("GPS: " + if (s.fix) "fix, %d sats — %.5f, %.5f".format(s.sats, s.lat, s.lon) else "no fix")
        Text("Battery: ${s.batt}%    Duty: ${s.duty}%")
        Text("RX: ${s.rx}    relayed: ${s.fwd}    queue: ${s.q}")
        Text("Channel: ${s.ch}    Power: ${POWER.getOrElse(s.pwr) { "?" }}")
        Spacer(Modifier.height(10.dp))
        Text("My presence", style = MaterialTheme.typography.titleSmall)
        Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState())) {
            PRESENCE.forEachIndexed { i, p ->
                FilterChip(selected = s.pres == i, onClick = { ble.setPresence(i) }, label = { Text(p) },
                    enabled = connected(ble), modifier = Modifier.padding(end = 6.dp))
            }
        }
        Spacer(Modifier.height(12.dp))
        Text("Nodes (${ble.nodes.size})", style = MaterialTheme.typography.titleSmall)
        ble.nodes.forEach { n ->
            Row(Modifier.fillMaxWidth().padding(vertical = 3.dp), verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    Text("${n.addr}  ${n.name}", style = MaterialTheme.typography.bodyMedium)
                    Text("${if (n.batt > 0) "${n.batt}%" else "?"}   ${n.rssi}dBm   ${n.age}s ago",
                        style = MaterialTheme.typography.bodySmall)
                }
                TextButton(onClick = { ble.sendPing(n.addr) }, enabled = connected(ble)) { Text("Ping") }
            }
        }
    }
}

@Composable
private fun MeshScreen(ble: BleManager) {
    var text by remember { mutableStateOf("") }
    Column(Modifier.fillMaxSize().padding(16.dp)) {
        Button(onClick = { ble.requestMesh() }, enabled = connected(ble)) { Text("Refresh Meshtastic") }
        Spacer(Modifier.height(8.dp))
        Text("Foreign Meshtastic nodes (${ble.mesh.size})", style = MaterialTheme.typography.titleSmall)
        LazyColumn(Modifier.weight(1f).fillMaxWidth()) {
            items(ble.mesh) { m ->
                Column(Modifier.padding(vertical = 3.dp)) {
                    Text("!${m.id}  ${m.name}", style = MaterialTheme.typography.bodyMedium)
                    Text((if (m.hasPos) "%.5f, %.5f".format(m.lat, m.lon) else "no position") +
                        (if (m.batt > 0) "   ${m.batt}%" else ""), style = MaterialTheme.typography.bodySmall)
                }
            }
        }
        HorizontalDivider(Modifier.padding(vertical = 8.dp))
        Text("Send into the Meshtastic public channel", style = MaterialTheme.typography.titleSmall)
        OutlinedTextField(text, { text = it }, label = { Text("message") }, modifier = Modifier.fillMaxWidth())
        Row(verticalAlignment = Alignment.CenterVertically) {
            Button(onClick = { ble.meshSendText(text); text = "" }, enabled = connected(ble)) { Text("Send text") }
            Spacer(Modifier.width(8.dp))
            OutlinedButton(onClick = { ble.meshSendPosition() }, enabled = connected(ble)) { Text("Send GPS position") }
            Spacer(Modifier.width(8.dp))
            ble.meshTxResult?.let { Text(if (it) "sent ✓" else "failed ✗") }
        }
    }
}

@Composable
private fun OpsScreen(ble: BleManager) {
    var pingTo by remember { mutableStateOf("FFFF") }
    var alertLabel by remember { mutableStateOf("HELP") }
    var cd by remember { mutableStateOf("60") }
    var armDistress by remember { mutableStateOf(false) }
    val on = connected(ble)
    Column(Modifier.fillMaxSize().padding(16.dp).verticalScroll(rememberScrollState())) {
        if (ble.opsNote.isNotBlank()) {
            Text(ble.opsNote, color = MaterialTheme.colorScheme.primary)
            Spacer(Modifier.height(8.dp))
        }

        Text("Beacon", style = MaterialTheme.typography.titleSmall)
        Button(onClick = { ble.sendBeacon() }, enabled = on) { Text("Broadcast my GPS position") }
        Spacer(Modifier.height(14.dp))

        Text("Ping a node", style = MaterialTheme.typography.titleSmall)
        Row(verticalAlignment = Alignment.CenterVertically) {
            OutlinedTextField(pingTo, { pingTo = it.take(4) }, label = { Text("addr") }, singleLine = true, modifier = Modifier.width(110.dp))
            Spacer(Modifier.width(8.dp))
            Button(onClick = { ble.sendPing(pingTo.ifBlank { "FFFF" }) }, enabled = on) { Text("Ping") }
        }
        Spacer(Modifier.height(14.dp))

        Text("Alert (pager)", style = MaterialTheme.typography.titleSmall)
        Row(verticalAlignment = Alignment.CenterVertically) {
            OutlinedTextField(alertLabel, { alertLabel = it.take(11) }, label = { Text("label") }, singleLine = true, modifier = Modifier.weight(1f))
            Spacer(Modifier.width(8.dp))
            Button(onClick = { ble.sendAlert(alertLabel.ifBlank { "ALERT" }) }, enabled = on) { Text("Send") }
        }
        Spacer(Modifier.height(14.dp))

        Text("Countdown timer", style = MaterialTheme.typography.titleSmall)
        Row(verticalAlignment = Alignment.CenterVertically) {
            OutlinedTextField(cd, { cd = it.filter { c -> c.isDigit() }.take(5) }, label = { Text("seconds") },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number), singleLine = true, modifier = Modifier.width(140.dp))
            Spacer(Modifier.width(8.dp))
            Button(onClick = { ble.sendCountdown(cd.toIntOrNull() ?: 0) }, enabled = on) { Text("Start (mesh)") }
        }
        Text("Needs the device to hold UTC (GPS or NTP).", style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(14.dp))

        Text("Gateway (USB uplink)", style = MaterialTheme.typography.titleSmall)
        Row(verticalAlignment = Alignment.CenterVertically) {
            Switch(checked = ble.status.gw, onCheckedChange = { ble.setGateway(it) }, enabled = on)
            Spacer(Modifier.width(8.dp))
            Text(if (ble.status.gw) "on" else "off")
        }
        Spacer(Modifier.height(20.dp))

        HorizontalDivider()
        Spacer(Modifier.height(12.dp))
        Text("Distress / Mayday", style = MaterialTheme.typography.titleSmall, color = MaterialTheme.colorScheme.error)
        if (!armDistress) {
            OutlinedButton(onClick = { armDistress = true }, enabled = on) { Text("Send distress…") }
        } else {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Button(onClick = { ble.sendDistress(); armDistress = false },
                    colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error)) {
                    Text("CONFIRM DISTRESS")
                }
                Spacer(Modifier.width(8.dp))
                TextButton(onClick = { armDistress = false }) { Text("Cancel") }
            }
        }
    }
}

@Composable
private fun ConfigScreen(ble: BleManager) {
    var name by remember { mutableStateOf("") }
    var addr by remember { mutableStateOf("") }
    var bright by remember { mutableStateOf("") }
    var vol by remember { mutableStateOf("") }
    var psk by remember { mutableStateOf("") }
    var region by remember { mutableIntStateOf(-1) }
    var ssid by remember { mutableStateOf("") }
    var pass by remember { mutableStateOf("") }
    val regions = listOf("EU868", "US915", "AS923")
    Column(Modifier.fillMaxSize().padding(16.dp).verticalScroll(rememberScrollState())) {
        Text("Current: ${ble.identity.name} / ${ble.identity.addr}", style = MaterialTheme.typography.bodySmall)
        OutlinedTextField(name, { name = it }, label = { Text("callsign / name") }, singleLine = true, modifier = Modifier.fillMaxWidth())
        OutlinedTextField(addr, { addr = it.take(4) }, label = { Text("address (hex, e.g. 1A2B)") }, singleLine = true, modifier = Modifier.fillMaxWidth())
        OutlinedTextField(bright, { bright = it.filter { c -> c.isDigit() }.take(3) }, label = { Text("brightness 0-255") },
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number), singleLine = true, modifier = Modifier.fillMaxWidth())
        OutlinedTextField(vol, { vol = it.filter { c -> c.isDigit() }.take(3) }, label = { Text("volume 0-255") },
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number), singleLine = true, modifier = Modifier.fillMaxWidth())
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
                    if (region >= 0) region else null, bright.toIntOrNull(), vol.toIntOrNull(),
                    null, ssid.ifBlank { null }, pass
                )
            },
            enabled = connected(ble)
        ) { Text("Apply to device") }

        Spacer(Modifier.height(20.dp))
        HorizontalDivider()
        Spacer(Modifier.height(12.dp))
        Text("Channel encryption", style = MaterialTheme.typography.titleSmall)
        OutlinedTextField(psk, { psk = it }, label = { Text("channel PSK (blank = public)") }, singleLine = true, modifier = Modifier.fillMaxWidth())
        Button(onClick = { ble.sendConfig(null, null, null, null, null, psk) }, enabled = connected(ble)) { Text("Set channel key") }

        Spacer(Modifier.height(20.dp))
        HorizontalDivider()
        Spacer(Modifier.height(12.dp))
        Text("WiFi (for NTP time sync)", style = MaterialTheme.typography.titleSmall)
        OutlinedTextField(ssid, { ssid = it }, label = { Text("WiFi SSID") }, singleLine = true, modifier = Modifier.fillMaxWidth())
        OutlinedTextField(pass, { pass = it }, label = { Text("WiFi password") },
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password), singleLine = true, modifier = Modifier.fillMaxWidth())
        Text("Saved with Apply above. Sync fetches UTC over SNTP (~10 s).", style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(8.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            Button(onClick = { ble.syncNtp() }, enabled = connected(ble)) { Text("Sync time (NTP)") }
            Spacer(Modifier.width(10.dp))
            ble.ntpResult?.let { Text(if (it) "synced ✓" else "failed ✗") }
        }
    }
}
