/*
Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>

Permission is hereby granted, free of uint8_tge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <EmbeddedTelnet.h>

#define _bit_set(bits, index) ((bits) |= (1ULL << (index)))
#define _bit_clear(bits, index) ((bits) &= ~(1ULL << (index)))
#define _bit_get(bits, index) (((bits) & (1ULL << (index))) != 0)

void telnet_init_packet(telnet_packet_t *packet) {
  if (packet == NULL) {
    return;
  }
  
  packet->command = TELNET_NOP; // Default command
  packet->option = TELNET_OPTION_BINARY; // Default option
  packet->subnegotiation_type = TELNET_SE_IS; // Default subnegotiation type
  packet->subnegotiation_length = 0; // No data initially
  memset(packet->subnegotiation_data, 0, sizeof(packet->subnegotiation_data)); // Clear data
}

void telnet_write_packet(telnet_session_t *session, const telnet_packet_t *packet, telnet_writer_t writer) {
  if (session == NULL || packet == NULL || writer == NULL) {
    return;
  }
  
  uint8_t buffer[80];
  size_t length = 0;
  
  // Start with IAC (Interpret As Command)
  buffer[length++] = TELNET_IAC;
  buffer[length++] = packet->command;
  switch(packet->command) {
    case TELNET_DO:
    case TELNET_DONT:
    case TELNET_WILL:
    case TELNET_WONT:
    buffer[length++] = packet->option;
    break;
    case TELNET_SB:
    buffer[length++] = packet->option; // Option for subnegotiation
    buffer[length++] = packet->subnegotiation_type; // Subnegotiation type
    memcpy(&buffer[length], packet->subnegotiation_data, packet->subnegotiation_length);
    length += packet->subnegotiation_length;
    buffer[length++] = TELNET_IAC; // End of subnegotiation
    buffer[length++] = TELNET_SE; // Subnegotiation end
    break;
    default:
    // For other commands, we just send the command byte
    break;
  }
  
  // Call the writer to send the packet
  writer(session, buffer, length);
}

void telnet_init(telnet_session_t *session) {
  if (session == NULL) {
    return;
  }
  
  session->state = TELNET_STATE_READY;
  telnet_init_packet(&session->packet);
  session->options = 0;
  memset(session->subnegotiation_options, 0, sizeof(session->subnegotiation_options));
  session->user_data = NULL;
}

void *telnet_get_user_data(telnet_session_t *session) {
  if (session == NULL) {
    return NULL;
  }
  return session->user_data;
}

void telnet_set_user_data(telnet_session_t *session, void *user_data) {
  if (session == NULL) {
    return;
  }
  session->user_data = user_data;
}

bool telnet_get_option(telnet_session_t *session, telnet_option_t option) {
  if (session == NULL) {
    return false;
  }
  return _bit_get(session->options, option);
}

void telnet_set_option(telnet_session_t *session, telnet_option_t option, bool value) {
  if (session == NULL) {
    return;
  }
  if (value) {
    _bit_set(session->options, option);
  } else {
    _bit_clear(session->options, option);
  }
}

void telnet_supported_options(telnet_session_t *session, telnet_option_t option, ...) {
  if (session == NULL) {
    return;
  }
  
  va_list args;
  va_start(args, option);
  
  for (telnet_option_t opt = option; opt < TELNET_OPTION_MAX; opt++) {
    bool supported = _bit_get(session->options, opt);
    // Do something with the supported option...
  }
  
  va_end(args);
}

const uint8_t *telnet_get_subnegotiation_option(telnet_session_t *session, telnet_option_t option) {
  if (session == NULL) {
    return NULL;
  }
  return session->subnegotiation_options[option];
}

void telnet_set_subnegotiation_option(telnet_session_t *session, telnet_option_t option, uint8_t *value) {
  if (session == NULL) {
    return;
  }
  session->subnegotiation_options[option] = value;
}

static void _handle_incomming_packet(telnet_session_t *session, telnet_writer_t writer, telnet_packet_callback_t callback) {
  if (session == NULL) {
    return;
  }
  
  telnet_packet_t *packet = &session->packet;
  telnet_packet_t response_packet;

  bool automatic_response = true;
  if (callback != NULL) {
    automatic_response = callback(session, packet);
  }
  
  if (!automatic_response) {
    // If the callback returns false, we do not send an automatic response
    return;
  }

  telnet_init_packet(&response_packet);

  switch (packet->command) {
    case TELNET_WILL:
      // Respond with DO if we support the option
      if (telnet_get_option(session, packet->option)) {
        response_packet.command = TELNET_DO;
      } else {
        response_packet.command = TELNET_DONT;
      }
      response_packet.option = packet->option;
      telnet_write_packet(session, &response_packet, writer);
      break;
    case TELNET_WONT:
      // Always respond with DONT
      response_packet.command = TELNET_DONT;
      response_packet.option = packet->option;
      telnet_write_packet(session, &response_packet, writer);
      break;
    case TELNET_DO:
      // Respond with WILL if we support the option
      if (telnet_get_option(session, packet->option)) {
        response_packet.command = TELNET_WILL;
      } else {
        response_packet.command = TELNET_WONT;
      }
      response_packet.option = packet->option;
      telnet_write_packet(session, &response_packet, writer);
      break;
    case TELNET_DONT:
      // Always respond with WONT
      response_packet.command = TELNET_WONT;
      response_packet.option = packet->option;
      telnet_write_packet(session, &response_packet, writer);
      break;
    case TELNET_SB:
      // Handle subnegotiation if type is SEND.
      if (packet->subnegotiation_type == TELNET_SE_SEND) {
        // If we have a subnegotiation option set, we can respond automatically
        const uint8_t *subnegotiation_option = telnet_get_subnegotiation_option(session, packet->option);
        if (subnegotiation_option != NULL) {
          response_packet.command = TELNET_SB;
          response_packet.option = packet->option;
          response_packet.subnegotiation_type = TELNET_SE_IS;
          response_packet.subnegotiation_length = strlen((const char*)subnegotiation_option);
          memcpy(response_packet.subnegotiation_data, subnegotiation_option, response_packet.subnegotiation_length);
          telnet_write_packet(session, &response_packet, writer);
        }
      }
      break;
    default:
      // For all other commands, we do not send a response
      break;
  }
}

static size_t _delete_uint8_tacter(uint8_t *data, size_t length, size_t index) {
  if (index >= length) {
    return length;
  }
  
  // Shift data left to remove uint8_tacter at index
  memmove(&data[index], &data[index + 1], length - index - 1);
  return length - 1; // Return new length
}

// Macro to handle deleting a uint8_tacter in the data buffer
#define _TELNET_DELETE_CHARACTER \
  do { \
    new_length = _delete_uint8_tacter(data, new_length, i); \
    i--; \
  } while (0)

size_t telnet_read(telnet_session_t *session, uint8_t *data, size_t length, telnet_packet_callback_t callback, telnet_writer_t writer) {
  if (data == NULL || length == 0) {
    return 0;
  }
  if (session == NULL) {
    return length;
  }
  
  // Process incoming data
  size_t new_length = length;
  for (size_t i = 0; i < new_length; i++) {
    uint8_t c = data[i];
    switch(session->state) {
      case TELNET_STATE_READY:
        if (c == TELNET_IAC) {
          _TELNET_DELETE_CHARACTER;
          session->state = TELNET_STATE_IN_COMMAND;
        }
        break;
      case TELNET_STATE_IN_COMMAND:
        session->packet.command = c;
        if (c == TELNET_IAC) {
          // Escape sequence, don't delete uint8_tacter
          session->state = TELNET_STATE_READY;
          break;
        }
        new_length = _delete_uint8_tacter(data, new_length, i);
        i--;
        if (c == TELNET_DO || c == TELNET_DONT || c == TELNET_WILL || c == TELNET_WONT) {
          // Handle option negotiation
          session->state = TELNET_STATE_IN_OPTION;
        } else if (c == TELNET_SB) {
          // Start of subnegotiation
          session->state = TELNET_STATE_IN_SUBNEGOTIATION_TYPE;
        } else {
          // Other command
          _handle_incomming_packet(session, writer, callback);
          session->state = TELNET_STATE_READY;
        }
        break;
      case TELNET_STATE_IN_OPTION:
        _TELNET_DELETE_CHARACTER;
        session->packet.option = c;
        _handle_incomming_packet(session, writer, callback);
        session->state = TELNET_STATE_READY;
        break;
      case TELNET_STATE_IN_SUBNEGOTIATION_TYPE:
        _TELNET_DELETE_CHARACTER;
        session->packet.subnegotiation_type = c;
        session->state = TELNET_STATE_IN_SUBNEGOTIATION_VALUE;
        break;
      case TELNET_STATE_IN_SUBNEGOTIATION_VALUE:
        _TELNET_DELETE_CHARACTER;
        if (c == TELNET_IAC) {
          session->state = TELNET_STATE_IN_SB_IAC;
        } else {
          session->packet.subnegotiation_data[session->packet.subnegotiation_length++] = c;
        }
        break;
      case TELNET_STATE_IN_SB_IAC:
        if (c == TELNET_IAC) {
          // Double IAC means we escape it
          session->packet.subnegotiation_data[session->packet.subnegotiation_length++] = TELNET_IAC;
          session->state = TELNET_STATE_IN_SUBNEGOTIATION_VALUE;
          _TELNET_DELETE_CHARACTER;
        } else if (c == TELNET_SE) {
          // End of subnegotiation
          _handle_incomming_packet(session, writer, callback);
          session->state = TELNET_STATE_READY;
          _TELNET_DELETE_CHARACTER;
        } else {
          // Handle the previous packet
          _handle_incomming_packet(session, writer, callback);
          // Begin handling of a new packet
          telnet_init_packet(&session->packet);
          session->state = TELNET_STATE_IN_COMMAND;
        }
    }
  }
  return new_length;
}

void telnet_write(telnet_session_t *session, const uint8_t *data, size_t length, telnet_writer_t writer) {
  if (session == NULL || data == NULL || length == 0 || writer == NULL) {
    return;
  }

  uint8_t escape[2] = { TELNET_IAC, TELNET_IAC };

  size_t start = 0;
  size_t new_length = 0;

  for (size_t i = 0; i < length; i++) {
    uint8_t c = data[i];
    new_length++;
    
    // Check for IAC (Interpret As Command) and escape it
    if (c == TELNET_IAC) {
      // Write the data before IAC
      writer(session, data + start, new_length); 
      // Write the escape sequence
      writer(session, escape, 2); 
      // Reset start and new_length
      start = i + 1;
      new_length = 0;
    }
  }

  // Write the remaining data
  writer(session, data + start, new_length);
}

const char *telnet_command_name(uint8_t command) {
  switch (command) {
    case TELNET_IAC: return "IAC";
    case TELNET_DONT: return "DONT";
    case TELNET_DO: return "DO";
    case TELNET_WONT: return "WONT";
    case TELNET_WILL: return "WILL";
    case TELNET_SB: return "SB";
    case TELNET_GA: return "GA";
    case TELNET_EL: return "EL";
    case TELNET_EC: return "EC";
    case TELNET_AYT: return "AYT";
    case TELNET_AO: return "AO";
    case TELNET_IP: return "IP";
    case TELNET_BRK: return "BRK";
    case TELNET_DM: return "DM";
    case TELNET_NOP: return "NOP";
    case TELNET_SE: return "SE";
    default: return "UNKNOWN";
  }
}

const char *telnet_subnegotiation_name(telnet_subnegotiation_t subnegotiation) {
  switch (subnegotiation) {
    case TELNET_SE_IS: return "SE IS";
    case TELNET_SE_SEND: return "SE SEND";
    default: return "UNKNOWN";
  }
}

const char *telnet_option_name(uint8_t option) {
  switch (option) {
    case TELNET_OPTION_BINARY: return "BINARY";
    case TELNET_OPTION_ECHO: return "ECHO";
    case TELNET_OPTION_RECONNECTION: return "RECONNECTION";
    case TELNET_OPTION_SUPPRESS_GO_AHEAD: return "SUPPRESS GO AHEAD";
    case TELNET_OPTION_MSG_SIZE: return "MSG SIZE";
    case TELNET_OPTION_STATUS: return "STATUS";
    case TELNET_OPTION_TIMING_MARK: return "TIMING MARK";
    case TELNET_OPTION_RCTE: return "REMOTE CONTROLLED TRANSMISSION AND ECHO";
    case TELNET_OPTION_OUTPUT_LINE_WIDTH: return "OUTPUT LINE WIDTH";
    case TELNET_OPTION_OUTPUT_PAGE_SIZE: return "OUTPUT PAGE SIZE";
    case TELNET_OPTION_OUTPUT_CR_DISPOSITION: return "OUTPUT CR DISPOSITION";
    case TELNET_OPTION_OUTPUT_HORIZONTAL_TAB: return "OUTPUT HORIZONTAL TAB";
    case TELNET_OPTION_OUTPUT_HORIZ_TAB_DISP: return "OUTPUT HORIZONTAL TAB DISP";
    case TELNET_OPTION_OUTPUT_FORM_FEED: return "OUTPUT FORM FEED";
    case TELNET_OPTION_OUTPUT_VERTICAL_TAB: return "OUTPUT VERTICAL TAB";
    case TELNET_OPTION_OUTPUT_VERT_TAB_DISP: return "OUTPUT VERTICAL TAB DISP";
    case TELNET_OPTION_OUTPUT_LINE_FEED: return "OUTPUT LINE FEED";
    case TELNET_OPTION_EXTENDED_ASCII: return "EXTENDED ASCII";
    case TELNET_OPTION_LOGOUT: return "LOGOUT";
    case TELNET_OPTION_BYTE_MACRO: return "BYTE MACRO";
    case TELNET_OPTION_DATA_ENTRY: return "DATA ENTRY";
    case TELNET_OPTION_SUPDUP: return "SUPDUP";
    case TELNET_OPTION_SUPDUP_OUTPUT: return "SUPDUP OUTPUT";
    case TELNET_OPTION_SEND_LOCATION: return "SEND LOCATION";
    case TELNET_OPTION_TERMINAL_TYPE: return "TERMINAL TYPE";
    case TELNET_OPTION_END_OF_RECORD: return "END OF RECORD";
    case TELNET_OPTION_TACACAS: return "TACACS";
    case TELNET_OPTION_OUTPUT_MARKING: return "OUTPUT MARKING";
    case TELNET_OPTION_TERMINAL_LOCATION: return "TERMINAL LOCATION";
    case TELNET_OPTION_TN3270: return "TN3270";
    case TELNET_OPTION_X3_PAD: return "X3 PAD";
    case TELNET_OPTION_WINDOW_SIZE: return "WINDOW SIZE";
    case TELNET_OPTION_TERMINAL_SPEED: return "TERMINAL SPEED";
    case TELNET_OPTION_FLOW_CONTROL: return "FLOW CONTROL";
    case TELNET_OPTION_LINE_MODE: return "LINE MODE";
    case TELNET_OPTION_X_DISPLAY_LOCATION: return "X DISPLAY LOCATION";
    case TELNET_OPTION_ENV: return "ENVIRONMENT";
    case TELNET_OPTION_AUTHENTICATION: return "AUTHENTICATION";
    case TELNET_OPTION_ENCRYPTION: return "ENCRYPTION";
    case TELNET_OPTION_NEW_ENVIRON: return "NEW ENVIRONMENT";
    case TELNET_OPTION_TN3270E: return "TN3270E";
    case TELNET_OPTION_XAUTH: return "XAUTH";
    case TELNET_OPTION_CHARSET: return "CHARSET";
    case TELNET_OPTION_REMOTE_SERIAL_PORT: return "REMOTE SERIAL PORT";
    case TELNET_OPTION_COM_PORT_CONTROL: return "COM PORT CONTROL";
    case TELNET_OPTION_SUPPRESS_LOCAL_ECHO: return "SUPPRESS LOCAL ECHO";    
    case TELNET_OPTION_START_TLS: return "START TLS";
    case TELNET_OPTION_KERMIT: return "KERMIT";
    case TELNET_OPTION_SEND_URL: return "SEND URL";
    case TELNET_OPTION_FORWARD_X: return "FORWARD X";
    default: return "UNKNOWN";
  }
}
