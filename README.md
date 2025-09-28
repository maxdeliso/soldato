# Soldato

A Windows-native peer-to-peer chat application built with C++ and Win32 API. Soldato provides a decentralized chat platform with reliable message delivery, automatic peer discovery, and message integrity verification.

## Requirements

- **Windows 10** or later
- **Visual Studio 2022** with C++ development tools
- **Windows SDK 10.0** or later
- **C++17** standard support

## Building

### Prerequisites

Ensure you have Visual Studio 2022 installed with the following workloads:
- Desktop development with C++
- Windows 10/11 SDK

### Build Steps

1. Clone the repository:
   ```bash
   git clone <repository-url>
   cd Soldato
   ```

2. Open `Soldato.sln` in Visual Studio 2022

3. Select your preferred configuration:
   - **Debug**: For development and debugging
   - **Release**: For production use
   - **Platform**: x64 (recommended)

4. Build the solution:
   - Press `Ctrl+Shift+B` or
   - Go to `Build` → `Build Solution`

5. The executable will be generated in:
   - `x64/Debug/Soldato.exe` (Debug build)
   - `x64/Release/Soldato.exe` (Release build)

## Usage

1. **First Run**: Launch `Soldato.exe` - the application will automatically start listening for other peers
2. **Network Discovery**: Other clients on the same network will automatically appear in your peer list
3. **Chat**: Type messages in the chat input and press Enter to send
4. **Peer Management**: View connected peers in the side panel - they will appear/disappear automatically as they join/leave the network

### Network Requirements

- All clients must be on the same local network
- Default multicast address: `224.0.0.122:1337`
- No firewall blocking required for local network communication

## Architecture

Soldato uses a custom messaging protocol built on UDP multicast for efficient peer-to-peer communication. For detailed protocol specifications, see [PROTOCOL.md](PROTOCOL.md).

## Contributing

Contributions are welcome! Here's how you can help:

### Reporting Issues
- Use the GitHub issue tracker to report bugs or request features
- Include detailed steps to reproduce issues
- Provide system information (Windows version, Visual Studio version)

### Development Guidelines
- Follow existing code style and conventions
- Add comments for complex logic
- Test changes thoroughly before submitting
- Update documentation as needed

### Pull Requests
- Fork the repository and create a feature branch
- Make your changes with clear commit messages
- Test your changes on Windows 10/11
- Submit a pull request with a clear description of changes

## Related Projects

- [Teflon](https://github.com/maxdeliso/teflon) - Java-based cross-platform chat application
- Compatible message protocol and networking approach
- Shared development of peer-to-peer chat solutions
