#ifndef TELNET_H
#define TELNET_H

/*
Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>

Permission is hereby granted, free of charge, to any person obtaining a copy
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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

/**
* This library implements the telnet protocol as defined in RFC 854 and related documents.
* It provides functions to initialize a telnet session, read and write telnet packets,
* and manage telnet options and subnegotiations.
* 
* To make it easier to use in an embedded environment, it does not use dynamic memory allocation.
* However, a telnet session may use up to 300 bytes of memory (depending on the architecture).
* 
* To use the library, first create a `telnet_session_t` structure and initialize it with `telnet_init`.
* ```c
* telnet_session_t session;
* telnet_init(&session);
* ```
* 
* If you need to tie the session to something specific in your application, you can use `telnet_set_user_data`.
* ```c
* telnet_set_user_data(&session, my_data);
* ```
* 
* To process incoming data, call `telnet_read` with the session, the data to read, and a callback function.
* The `telnet_read` function will modify the provided buffer in place when telnet commands are received.
* ```c
* bool my_callback(telnet_session_t *session, const telnet_packet_t *packet) {
*   // Handle incoming packet
*   printf("Received telnet command: %s", telnet_command_name(packet->command));
*   if (packet->command == TELNET_DO || packet->command == TELNET_WILL || 
        packet->command == TELNET_DONT || packet->command == TELNET_WONT) {
*     printf(" %s", telnet_option_name(packet->option));
*   }
*   if (packet->command == TELNET_SB) {
*     printf(" %s %s %s", telnet_option_name(packet->option),
*            telnet_subnegotiation_name(packet->subnegotiation_type), 
*            packet->subnegotiation_data);
*   }
*   println();
*   // Return true to indicate that we handled the packet
*   return true;
* }
*
* void my_source_writer(telnet_session_t *session, const uint8_t *data, size_t length) {
*   write_to_source(data, length);
* }
*
* void read_data() {
*   uint8_t buffer[256];
*   size_t length = read_from_source(buffer, sizeof(buffer));
*   length = telnet_read(&session, buffer, length, my_callback, my_source_writer);
* }
* ```
* 
* The library will automatically respond to telnet option requests. By default all options are set to false.
* You can change this by calling the `telnet_supported_options` or `telnet_set_option` functions.
* ```c
* telnet_supported_options(&session, TELNET_OPTION_BINARY, TELNET_OPTION_SUPPRESS_GO_AHEAD);
* ```
*  
* By default, the library will not respond to subnegotiation requests but will instead call the callback function.
* If you want to automatically respond to a subnegotiation request, you can call the
* `telnet_set_subnegotiation_option` function. Setting a subnegotiation option to NULL will disable
* automatic responses for that subnegotiation.
* 
* If you do not provide a callback function to `telnet_read`, the library will automatically respond to
* telnet options and subnegotiation requests when it can, but other packets will be ignored.
*
* To write data to a telnet session, use the `telnet_write` function. This function will escape data as necessary
* to conform to the telnet protocol.
* ```c
* void my_writer(telnet_session_t *session, const uint8_t *data, size_t length) {
*   write_to_destination(data, length);
* }
*
* void write_data(const uint8_t* data) {
*   telnet_write(&session, data, strlen(data), my_writer);
* }
* ```
*/

/*
* Telnet commands and options as per RFC 854, RFC 855, and related documents.
* For more information on Telnet options, see:
* https://www.iana.org/assignments/telnet-options/telnet-options.xhtml
*/

// Telnet commands
typedef enum {
  TELNET_SE   = 240, /* 0xF0 Subnegotiation End */
  TELNET_NOP  = 241, /* 0xF1 No Operation */
  TELNET_DM   = 242, /* 0xF2 Data Mark */
  TELNET_BRK  = 243, /* 0xF3 Break */
  TELNET_IP   = 244, /* 0xF4 Interrupt Process */
  TELNET_AO   = 245, /* 0xF5 Abort Output */
  TELNET_AYT  = 246, /* 0xF6 Are You There? */
  TELNET_EC   = 247, /* 0xF7 Erase Character */
  TELNET_EL   = 248, /* 0xF8 Erase Line */
  TELNET_GA   = 249, /* 0xF9 Go Ahead */
  TELNET_SB   = 250, /* 0xFA Subnegotiation */
  TELNET_WILL = 251, /* 0xFB Sender says it can handle option */
  TELNET_WONT = 252, /* 0xFC Receiver cannot handle option */
  TELNET_DO   = 253, /* 0xFD Sender asks for option support */
  TELNET_DONT = 254, /* 0xFE Receiver can't support option */
  TELNET_IAC  = 255  /* 0xFF Interpret As Command */
} telnet_command_t;

typedef enum {
  TELNET_SE_IS = 0,
  TELNET_SE_SEND = 1
} telnet_subnegotiation_t;

typedef enum {
  TELNET_OPTION_BINARY                = 0,   /* 0x00 - RFC 856 */
  TELNET_OPTION_ECHO                  = 1,   /* 0x01 - RFC 857 */
  TELNET_OPTION_RECONNECTION          = 2,   /* 0x02 - NIC 15391 of 1973 */
  TELNET_OPTION_SUPPRESS_GO_AHEAD     = 3,   /* 0x03 - RFC 858 */
  TELNET_OPTION_MSG_SIZE              = 4,   /* 0x04 - NIC 15393 of 1973 */
  TELNET_OPTION_STATUS                = 5,   /* 0x05 - RFC 859 */
  TELNET_OPTION_TIMING_MARK           = 6,   /* 0x06 - RFC 860 */
  TELNET_OPTION_RCTE                  = 7,   /* 0x07 - RFC 726 */
  TELNET_OPTION_OUTPUT_LINE_WIDTH     = 8,   /* 0x08 - NIC 20196 of 1978 */
  TELNET_OPTION_OUTPUT_PAGE_SIZE      = 9,   /* 0x09 - NIC 20197 of 1978 */
  TELNET_OPTION_OUTPUT_CR_DISPOSITION = 10,  /* 0x0A - RFC 652 */
  TELNET_OPTION_OUTPUT_HORIZONTAL_TAB = 11,  /* 0x0B - RFC 653 */
  TELNET_OPTION_OUTPUT_HORIZ_TAB_DISP = 12,  /* 0x0C - RFC 654 */
  TELNET_OPTION_OUTPUT_FORM_FEED      = 13,  /* 0x0D - RFC 655 */
  TELNET_OPTION_OUTPUT_VERTICAL_TAB   = 14,  /* 0x0E - RFC 656 */
  TELNET_OPTION_OUTPUT_VERT_TAB_DISP  = 15,  /* 0x0F - RFC 657 */
  TELNET_OPTION_OUTPUT_LINE_FEED      = 16,  /* 0x10 - RFC 658 */
  TELNET_OPTION_EXTENDED_ASCII        = 17,  /* 0x11 - RFC 698 */
  TELNET_OPTION_LOGOUT                = 18,  /* 0x12 - RFC 727 */
  TELNET_OPTION_BYTE_MACRO            = 19,  /* 0x13 - RFC 735 */
  TELNET_OPTION_DATA_ENTRY            = 20,  /* 0x14 - RFC 1043 / RFC 732 */
  TELNET_OPTION_SUPDUP                = 21,  /* 0x15 - RFC 736 / RFC 734 */
  TELNET_OPTION_SUPDUP_OUTPUT         = 22,  /* 0x16 - RFC 749 */
  TELNET_OPTION_SEND_LOCATION         = 23,  /* 0x17 - RFC 779 */
  TELNET_OPTION_TERMINAL_TYPE         = 24,  /* 0x18 - RFC 1091 */
  TELNET_OPTION_END_OF_RECORD         = 25,  /* 0x19 - RFC 885 */
  TELNET_OPTION_TACACAS               = 26,  /* 0x1A - RFC 927 */
  TELNET_OPTION_OUTPUT_MARKING        = 27,  /* 0x1B - RFC 933 */
  TELNET_OPTION_TERMINAL_LOCATION     = 28,  /* 0x1C - RFC 946 */
  TELNET_OPTION_TN3270                = 29,  /* 0x1D - RFC 1041 */
  TELNET_OPTION_X3_PAD                = 30,  /* 0x1E - RFC 1053 */
  TELNET_OPTION_WINDOW_SIZE           = 31,  /* 0x1F - RFC 1073 */
  TELNET_OPTION_TERMINAL_SPEED        = 32,  /* 0x20 - RFC 1079 */
  TELNET_OPTION_FLOW_CONTROL          = 33,  /* 0x21 - RFC 1372 */
  TELNET_OPTION_LINE_MODE             = 34,  /* 0x22 - RFC 1184 */
  TELNET_OPTION_X_DISPLAY_LOCATION    = 35,  /* 0x23 - RFC 1096 */
  TELNET_OPTION_ENV                   = 36,  /* 0x24 - RFC 1408 */
  TELNET_OPTION_AUTHENTICATION        = 37,  /* 0x25 - RFC 2941 */
  TELNET_OPTION_ENCRYPTION            = 38,  /* 0x26 - RFC 2946 */
  TELNET_OPTION_NEW_ENVIRON           = 39,  /* 0x27 - RFC 1572 */
  TELNET_OPTION_TN3270E               = 40,  /* 0x28 - RFC 2355 */
  TELNET_OPTION_XAUTH                 = 41,  /* 0x29 - No RFC  */
  TELNET_OPTION_CHARSET               = 42,  /* 0x2A - RFC 2066 */
  TELNET_OPTION_REMOTE_SERIAL_PORT    = 43,  /* 0x2B - No RFC */
  TELNET_OPTION_COM_PORT_CONTROL      = 44,  /* 0x2C - RFC 2217 */
  TELNET_OPTION_SUPPRESS_LOCAL_ECHO   = 45,  /* 0x2D - No RFC*/
  TELNET_OPTION_START_TLS             = 46,  /* 0x2E - draft-altman-telnet-starttls-02 */
  TELNET_OPTION_KERMIT                = 47,  /* 0x2F - RFC 2840*/
  TELNET_OPTION_SEND_URL              = 48,  /* 0x30 - No RFC */
  TELNET_OPTION_FORWARD_X             = 49   /* 0x31 - No RFC */
} telnet_option_t;

#define TELNET_OPTION_MAX 50

/**
 * This enum defines the possible states of a telnet command parser.
 * It is used to track the current state of the parser while processing incoming data.
 */
typedef enum {
  TELNET_STATE_READY = 0,
  TELNET_STATE_IN_COMMAND,
  TELNET_STATE_IN_OPTION,
  TELNET_STATE_IN_SUBNEGOTIATION_TYPE,
  TELNET_STATE_IN_SUBNEGOTIATION_VALUE,
  TELNET_STATE_IN_SB_IAC
} telnet_parse_state_t;

/** 
* Structure representing a telnet command packet.
* This structure is used to encapsulate the command type and any associated data.
* We limit the subnegotiation data to 64 bytes to prevent the need for memory allocation.
*/
typedef struct {
  telnet_command_t command;
  telnet_option_t option;
  telnet_subnegotiation_t subnegotiation_type;
  size_t subnegotiation_length;
  uint8_t subnegotiation_data[64];
} telnet_packet_t;

/**
* This structure holds the state of a telnet session.
* You can use the `telnet_set_user_data` to store any user-specific data you need to associate with the session.
* If you call `telnet_set_automatic_response`, the session will automatically respond to option and subnegotiation requests with the options provided.
* Please do not modify the value of this struct directly; use the provided functions to manage its state.
*/
typedef struct {
  telnet_parse_state_t state;
  telnet_packet_t packet;
  uint64_t options;
  const uint8_t *subnegotiation_options[TELNET_OPTION_MAX];
  void *user_data; 
} telnet_session_t;

/**
* Callback function type for handling telnet packets.
* This function will be called when a complete telnet packet is received.
* To prevent automatic responses, return false from this function.
* 
* @param session The current telnet session.
* @param packet The received telnet packet.
* @return True if the packet was handled successfully, false otherwise.
*/
typedef bool (*telnet_packet_callback_t)(telnet_session_t *session, const telnet_packet_t *packet);

/**
* Callback function type for writing data to a telnet session.
* When reading from a telnet session, this function will be called to send data back to the source.
* When writing to a telnet session, this function will be called to send data to the destination.
*
* @param session The current telnet session.
* @param data The data to send.
* @param length The length of the data to send.
*/
typedef void (*telnet_writer_t)(telnet_session_t *session, const uint8_t *data, size_t length);

/** 
* Initialize a telnet session. 
* 
* @param session Pointer to the telnet session structure to initialize.
*/
void telnet_init(telnet_session_t *session);

/** 
* Get user-specific data associated with a telnet session. 
*
* @param session Pointer to the telnet session structure.
* @return Pointer to user-specific data associated with the session.
*/
void *telnet_get_user_data(telnet_session_t *session);

/** 
* Set user-specific data associated with a telnet session. 
* 
* @param session Pointer to the telnet session structure.
* @param user_data Pointer to user-specific data to associate with the session.
*/
void telnet_set_user_data(telnet_session_t *session, void *user_data);

/** 
* Get the the value of an option for a telnet session.
* 
* @param session Pointer to the telnet session structure.
* @return True if the option is supported, false otherwise.
*/
bool telnet_get_option(telnet_session_t *session, telnet_option_t option);

/** 
* Set the value of an option for a telnet session.
*
* @param session Pointer to the telnet session structure.
* @param options An array of booleans representing the options to return.
*/
void telnet_set_option(telnet_session_t *session, telnet_option_t option, bool value);

/**
* This is a helper method to set all supported options for a telnet session at once.
*/
void telnet_supported_options(telnet_session_t *session, telnet_option_t option, ...);

/** 
* Get the subnegotiation options for a telnet session.
* 
* @param session Pointer to the telnet session structure.
* @return A pointer to an array of strings representing the subnegotiation options.
*/
const uint8_t **telnet_get_subnegotiation_options(telnet_session_t *session);

/** 
* Set the subnegotiation options for a telnet session.
* These options are used when the response mode is set to automatic.
* **Note:** The options should be null-terminated strings.
* **Note:** The array must be of length 50.
* 
* @param session Pointer to the telnet session structure.
* @param options An array of strings representing the subnegotiation options to return.
*/
void telnet_set_subnegotiation_options(telnet_session_t *session, const uint8_t **options);

/** 
* Read data from a telnet session.
* This function processes the incoming data and calls the provided callback when a complete packet is received.
* When packet data is received, it will remove that data from the buffer by modifying the buffer in place.
* 
* @param session Pointer to the telnet session structure.
* @param data Pointer to the data to read.
* @param length Length of the data to read.
* @param callback Function to call when a complete packet is received.
* @param writer Function for sending automatic replies back to the source.
* @return The new size of the data buffer after processing.
*/
size_t telnet_read(telnet_session_t *session, uint8_t *data, size_t length, telnet_packet_callback_t callback,  telnet_writer_t writer);

/**
 * Write data to a telnet session.
 * This function sends the provided data to the destination using the specified writer function.
 * It escapes data as necessary to conform to the telnet protocol.
 * 
 * @param session Pointer to the telnet session structure.
 * @param data Pointer to the data to write.
 * @param length Length of the data to write.
 * @param writer Function for sending data to the destination.
 */
void telnet_write(telnet_session_t *session, const uint8_t *data, size_t length, telnet_writer_t writer);

/**
 * Initialize a telnet packet.
 * This function sets the default values for a telnet packet.
 * 
 * @param packet Pointer to the telnet packet structure to initialize.
 */
void telnet_init_packet(telnet_packet_t *packet);

/**
 * Write a telnet packet to a telnet session.
 * This function sends the provided packet to the destination using the specified writer function.
 * It escapes data as necessary to conform to the telnet protocol.
 * 
 * @param session Pointer to the telnet session structure.
 * @param packet Pointer to the telnet packet to write.
 * @param writer Function for sending data to the destination.
 */
void telnet_write_packet(telnet_session_t *session, const telnet_packet_t *packet, telnet_writer_t writer);

/** Returns a human readable name for a given telnet option */
const char *telnet_option_name(uint8_t option);

/** Returns a human readable name for a given telnet subnegotiation flag */
const char *telnet_subnegotiation_name(telnet_subnegotiation_t subnegotiation);

/** Returns a human readable name for a given telnet command */
const char *telnet_command_name(uint8_t command);

// End of telnet commands
#endif // TELNET_H