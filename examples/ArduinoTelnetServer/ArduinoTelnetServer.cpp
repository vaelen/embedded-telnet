#include <Arduino.h>
#include <WiFi.h>
#include <WiFiServer.h>
#include <WiFiClient.h>

#include <EmbeddedTelnet.h>

#define YOUR_WIFI_SSID "your-ssid"
#define YOUR_WIFI_PASSWORD "your-password"

telnet_session_t session;

WiFiServer server(23); // Telnet server on port 23

void setup() {
  Serial.begin(115200);

  telnet_init(&session);
  
  // Set supported options
  telnet_supported_options(&session, TELNET_OPTION_SUPPRESS_GO_AHEAD);

  // Initialize WiFi
  WiFi.begin(YOUR_WIFI_SSID, YOUR_WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Start the Telnet server
  server.begin();
  Serial.println("Telnet server started");
}

void writeToClient(telnet_session_t *session, const uint8_t *data, size_t length) {
  if (session == nullptr || data == nullptr || length == 0 || session->user_data == nullptr) {
    return;
  }

  WiFiClient *client = static_cast<WiFiClient *>(telnet_get_user_data(session));
  if (client && client->connected()) {
    client->write(data, length);
  }
}

bool handleTelnetPacket(telnet_session_t *session, const telnet_packet_t *packet) {
  if (packet->command == TELNET_IAC) {
    // Just an escaped 0xFF in the data stream, we can ignore it.
    return true;
  }
  
  // Print received command
  Serial.print("<IAC ");
  Serial.print(telnet_command_name(packet->command));
  
  // Respond to options if necessary
  if (packet->command == TELNET_DO || packet->command == TELNET_WILL || 
      packet->command == TELNET_DONT || packet->command == TELNET_WONT) {
    Serial.print(" ");
    Serial.print(telnet_option_name(packet->option));
  } else if (packet->command == TELNET_SB) {
    Serial.print(" ");
    Serial.print(telnet_option_name(packet->option));
    Serial.print(" ");
    Serial.print(telnet_subnegotiation_name(packet->subnegotiation_type));
    Serial.print(" ");
    Serial.print((const char *)packet->subnegotiation_data);
  }

  Serial.print(">");
  
  // Indicate that normal handling should continue
  return true;
}

void loop() {
  // Check for incoming connections
  if (server.available()) {
    WiFiClient client = server.accept();
    if (client) {
      Serial.println("New client connected");
      telnet_init(&session);
      telnet_set_user_data(&session, &client);
    }
  }
  // Read data from the client
  if (session.user_data) {
    WiFiClient *client = static_cast<WiFiClient *>(session.user_data);
    if (!client->connected()) {
      Serial.println("Client disconnected");
      telnet_set_user_data(&session, nullptr); // Clear user data when client disconnects
      return;
    }
    if (client->available()) {
      uint8_t buffer[256];
      size_t length = client->readBytes(buffer, sizeof(buffer));
      if (length > 0) {
        // Process the incoming data
        length = telnet_read(&session, buffer, length, handleTelnetPacket, writeToClient);
        if (length > 0) {
          Serial.write(buffer, length);
        }
      }
    }
    if (Serial.available()) {
      uint8_t buffer[256];
      int bytesToRead = Serial.available();
      if (bytesToRead > sizeof(buffer) - 1) {
        bytesToRead = sizeof(buffer) - 1; // Ensure we don't overflow the buffer
      }
      // Read input from Serial and write it to the telnet session
      size_t length = Serial.readBytesUntil('\n', buffer, bytesToRead);
      buffer[length] = '\0';  // Null-terminate the buffer for good measure
      if (length > 0) {
        // Write data to the telnet session
        telnet_write(&session, buffer, length, writeToClient);
      }
    }
  }
}