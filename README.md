# Soldato

A Windows-native peer-to-peer chat application built with C++ and Win32 API. Soldato provides a decentralized chat platform with reliable message delivery and cross-platform compatibility.

## Features

- **Native Windows Application**: Built with C++ and Win32 API for optimal performance
- **Peer-to-Peer Communication**: Uses UDP multicast for decentralized chat without requiring a central server
- **Message Reliability**: Implements acknowledgment system (ACK/NACK) for message delivery confirmation
- **Cross-Platform Compatibility**: Fully compatible with [Teflon](https://github.com/maxdeliso/teflon) Java chat application
- **Real-time Chat**: Instant message delivery with visual feedback
- **Peer Discovery**: Automatic discovery of other users on the same network
- **Connection Management**: Easy connection setup with configurable multicast groups
- **Message Tracking**: Built-in message statistics and delivery tracking

## Technical Architecture

### Core Components

- **ChatForm**: Main UI window with chat history and message input
- **NetworkManager**: Handles UDP multicast communication and message routing
- **Message System**: JSON-based message protocol with checksums and acknowledgments
- **Peer Tracking**: Maintains list of active peers and their status
- **Message Tracker**: Manages message delivery confirmations and retransmissions

### Protocol Compatibility

Soldato uses the same message protocol as [Teflon](https://github.com/maxdeliso/teflon), ensuring seamless communication between:

- **Message Types**: CHAT, ACK, NACK, SYSTEM_EVENT
- **JSON Serialization**: Compatible message format with UUID message IDs
- **Checksum Validation**: CRC32 checksums for message integrity
- **Multicast Groups**: Support for IPv4 and IPv6 multicast addresses

## Requirements

- **Windows 10** or later
- **Visual Studio 2022** with C++ development tools
- **Windows SDK 10.0** or later
- **C++17** standard support

## Building

1. Clone the repository:
   ```bash
   git clone <repository-url>
   cd Soldato
   ```

2. Open `Soldato.sln` in Visual Studio 2022

3. Select your preferred configuration (Debug/Release) and platform (x64 recommended)

4. Build the solution (Ctrl+Shift+B)

The executable will be generated in `x64/Debug/` or `x64/Release/` directory.

## Usage

### Running the Application

1. Launch `Soldato.exe`
2. Click "Connect" to open the connection dialog
3. Configure your settings:
   - **Username**: Your display name in the chat
   - **Multicast IP**: Network multicast address (default: 239.255.255.250)
   - **Port**: UDP port for communication (default: 8888)
4. Click "Connect" to join the chat network

### Chat Features

- **Send Messages**: Type in the message input field and press Enter or click Send
- **View Peers**: See active participants in the peer panel
- **Message History**: Scroll through chat history with timestamps
- **Connection Status**: Monitor connection status and message statistics

### Network Configuration

#### Default Settings
- **Multicast IP**: `239.255.255.250`
- **Port**: `8888`
- **Protocol**: UDP Multicast

#### Custom Configuration
You can use any valid multicast address:
- **IPv4**: 224.0.0.0 - 239.255.255.255
- **IPv6**: FF02::/16 (Link-Local Scope)

## Cross-Platform Compatibility

Soldato is fully compatible with [Teflon](https://github.com/maxdeliso/teflon), a Java-based chat application. Users can communicate seamlessly between:

- **Soldato** (Windows C++ application)
- **Teflon** (Cross-platform Java application)

Both applications use the same:
- Message protocol and JSON format
- Multicast networking approach
- Acknowledgment system for reliable delivery
- Peer discovery mechanisms

## Development

### Project Structure

```
Soldato/
├── ChatForm.cpp/h          # Main UI and chat interface
├── NetworkManager.cpp/h    # Network communication and protocol
├── Message.cpp/h           # Message structure and serialization
├── MessageTracker.cpp/h    # Message delivery tracking
├── PeerTracker.cpp/h       # Peer discovery and management
├── ConnectDialog.cpp/h     # Connection configuration dialog
├── PeerPanel.cpp/h         # Peer list display
├── WinsockManager.h        # Winsock initialization wrapper
└── json.hpp               # JSON library for message serialization
```

### Key Features Implementation

- **Thread-Safe Networking**: Uses Windows events and worker threads for non-blocking I/O
- **RAII Resource Management**: Smart pointers and RAII wrappers for automatic cleanup
- **Message Queuing**: Asynchronous message sending with worker threads
- **UI Thread Safety**: Proper message passing between network and UI threads

## License

This project is open source. Please refer to the license file for details.

## Contributing

Contributions are welcome! Please feel free to submit issues, feature requests, or pull requests.

## Related Projects

- [Teflon](https://github.com/maxdeliso/teflon) - Java-based cross-platform chat application
- Compatible message protocol and networking approach
- Shared development of peer-to-peer chat solutions
