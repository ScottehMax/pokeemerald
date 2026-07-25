package org.pokeemerald.pc

import android.content.Context
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp

private const val PREFERENCES_NAME = "link"
private const val SERVER_KEY = "server"
private const val DEFAULT_SERVER = "127.0.0.1:8765"
private const val MAX_SERVER_LENGTH = 255

object LinkServerSettings {
    @JvmStatic
    fun get(context: Context): String =
        context.getSharedPreferences(PREFERENCES_NAME, Context.MODE_PRIVATE)
            .getString(SERVER_KEY, DEFAULT_SERVER)
            ?.takeIf { it.isNotBlank() }
            ?: DEFAULT_SERVER

    @JvmStatic
    fun save(context: Context, server: String) {
        context.getSharedPreferences(PREFERENCES_NAME, Context.MODE_PRIVATE)
            .edit()
            .putString(SERVER_KEY, server)
            .apply()
    }
}

class LinkSettingsActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme(
                colorScheme = darkColorScheme(
                    primary = Color(0xFF65C978),
                    secondary = Color(0xFFE6B85C),
                    background = Color(0xFF121614),
                    surface = Color(0xFF121614),
                ),
            ) {
                LinkSettingsScreen(
                    initialServer = LinkServerSettings.get(this),
                    onCancel = ::finish,
                    onSave = { server ->
                        LinkServerSettings.save(this, server)
                        Toast.makeText(this, "Link server saved", Toast.LENGTH_SHORT).show()
                        finish()
                    },
                )
            }
        }
    }
}

@Composable
private fun LinkSettingsScreen(
    initialServer: String,
    onCancel: () -> Unit,
    onSave: (String) -> Unit,
) {
    var server by remember { mutableStateOf(initialServer) }
    val normalized = server.trim()
    val error = validateLinkServer(normalized)

    Surface(modifier = Modifier.fillMaxSize()) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(horizontal = 32.dp, vertical = 24.dp),
            verticalArrangement = Arrangement.Center,
        ) {
            Text("Link server", style = MaterialTheme.typography.headlineMedium)
            Text(
                "Both players must use the same rendezvous server. " +
                    "Enter a host name or IP address, with an optional UDP port.",
                modifier = Modifier.padding(top = 8.dp, bottom = 20.dp),
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                style = MaterialTheme.typography.bodyMedium,
            )
            OutlinedTextField(
                value = server,
                onValueChange = {
                    if (it.length <= MAX_SERVER_LENGTH)
                        server = it
                },
                modifier = Modifier.fillMaxWidth(),
                label = { Text("Server address") },
                placeholder = { Text("example.com:8765") },
                supportingText = {
                    Text(error ?: "UDP port 8765 is used when no port is specified.")
                },
                isError = error != null,
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Uri),
                singleLine = true,
            )
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 20.dp),
                horizontalArrangement = Arrangement.End,
            ) {
                TextButton(onClick = { server = DEFAULT_SERVER }) {
                    Text("Reset")
                }
                Spacer(modifier = Modifier.width(8.dp))
                OutlinedButton(onClick = onCancel) {
                    Text("Cancel")
                }
                Spacer(modifier = Modifier.width(8.dp))
                Button(
                    onClick = { onSave(normalized) },
                    enabled = error == null,
                ) {
                    Text("Save")
                }
            }
        }
    }
}

internal fun validateLinkServer(server: String): String? {
    if (server.isEmpty())
        return "Enter a server address."
    if (server.length > MAX_SERVER_LENGTH)
        return "The server address is too long."
    if (server.any { it.isWhitespace() } || server.contains('/') || server.contains("://"))
        return "Use a host name or IP address, not a URL."

    val colon = server.lastIndexOf(':')
    if (colon < 0)
        return null
    if (colon == 0 || server.indexOf(':') != colon)
        return "IPv6 addresses are not currently supported."

    val portText = server.substring(colon + 1)
    val port = portText.toIntOrNull()
    if (port == null || port !in 1..65535)
        return "Enter a UDP port from 1 to 65535."
    return null
}
