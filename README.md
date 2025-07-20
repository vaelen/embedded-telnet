# embedded-telnet
**NOTE: This library has not yet been throughly tested.**

This library implements the telnet protocol as defined in RFC 854 and related documents.
It provides functions to initialize a telnet session, read and write telnet packets,
and manage telnet options and subnegotiations.

To make it easier to use in an embedded environment, it does not use dynamic memory allocation.
However, a telnet session may use up to 300 bytes of memory (depending on the architecture).

To use the library, first create a `telnet_session_t` structure and initialize it with `telnet_init`.
```c
telnet_session_t session;
telnet_init(&session);
```

If you need to tie the session to something specific in your application, you can use `telnet_set_user_data`.
```c
telnet_set_user_data(&session, my_data);
```

To process incoming data, call `telnet_read` with the session, the data to read, and a callback function.
The `telnet_read` function will modify the provided buffer in place when telnet commands are received.
```c
bool my_callback(telnet_session_t *session, const telnet_packet_t *packet) {
  // Handle incoming packet
  printf("Received telnet command: %s", telnet_command_name(packet->command));
  if (packet->command == TELNET_DO || packet->command == TELNET_WILL || 
      packet->command == TELNET_DONT || packet->command == TELNET_WONT) {
    printf(" %s", telnet_option_name(packet->option));
  }
  if (packet->command == TELNET_SB) {
    printf(" %s %s %s", telnet_option_name(packet->option),
           telnet_subnegotiation_name(packet->subnegotiation_type)
           packet->subnegotiation_data);
  }
  println();
  // Return true to indicate that we handled the packet
  return true;
}

void my_source_writer(telnet_session_t *session, const uint8_t *data, size_t length) {
  write_to_source(data, length);
}

void read_data() {
  uint8_t buffer[256];
  size_t length = read_from_source(buffer, sizeof(buffer));
  length = telnet_read(&session, buffer, length, my_callback, my_source_writer);
}
```

The library will automatically respond to telnet option requests. By default all options are set to false.
You can change this by calling the `telnet_supported_options` or `telnet_set_option` functions.
```c
telnet_supported_options(&session, TELNET_OPTION_BINARY, TELNET_OPTION_SUPPRESS_GO_AHEAD);
```
 
By default, the library will not respond to subnegotiation requests but will instead call the callback function.
If you want to automatically respond to a subnegotiation request, you can call the
`telnet_set_subnegotiation_option` function. Setting a subnegotiation option to NULL will disable
automatic responses for that subnegotiation.

If you do not provide a callback function to `telnet_read`, the library will automatically respond to
telnet options and subnegotiation requests when it can, but other packets will be ignored.

To write data to a telnet session, use the `telnet_write` function. This function will escape data as necessary
to conform to the telnet protocol.
```c
void my_writer(telnet_session_t *session, const uint8_t *data, size_t length) {
  write_to_destination(data, length);
}

void write_data(const uint8_t* data) {
  telnet_write(&session, data, strlen(data), my_writer);
}
```

