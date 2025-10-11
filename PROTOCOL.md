# Soldato Messaging Protocol

## Overview

This document outlines the **Soldato Messaging Protocol**, a simple peer-to-peer communication protocol designed for local network chat applications.

The protocol operates over **UDP/IP Multicast**, enabling efficient group communication where clients do not need to know each other's individual IP addresses beforehand. All message payloads are serialized as **JSON objects**, providing a flexible and human-readable data format.

### Key Features

- **Peer Discovery**: Clients automatically discover each other by listening on a shared multicast address
- **Message Integrity**: A CRC32 checksum verifies that message content has not been corrupted in transit
- **Reliable Delivery**: An acknowledgment (ACK/NACK) system allows senders to track the delivery status of their messages

## Transport Layer

- **Protocol**: UDP (User Datagram Protocol)
- **Addressing**: IP Multicast
- **Default Multicast Group**: `224.0.0.122`
- **Default Port**: `1337`

Clients join the multicast group to send and receive messages. A single UDP datagram sent to this group address is delivered to all clients currently listening.

## Message Format

All data transmitted over the network is a UTF-8 encoded string representing a single JSON object.

### Message Structure

| Field               | Type   | Description                                                                                             |
| ------------------- | ------ | ------------------------------------------------------------------------------------------------------- |
| `type`              | String | The type of message. Must be one of: `"CHAT"`, `"ACK"`, `"NACK"`                                        |
| `messageId`         | String | A unique UUID (Universally Unique Identifier) identifying this specific message                        |
| `senderId`          | String | The unique UUID of the client sending the message                                                      |
| `body`              | String | The payload of the message. For a `CHAT` message, this is the user's text. For an `ACK`, it's a confirmation string |
| `checksum`          | Number | A 32-bit CRC32 checksum calculated on the `body` field to ensure integrity                             |
| `originalMessageId` | String | **(Optional)** Only present in `ACK` and `NACK` messages. Contains the `messageId` of the message being acknowledged |

### Message Examples

#### CHAT Message

A standard chat message sent from one user to the group:

```json
{
  "type": "CHAT",
  "messageId": "f220e902-fbd5-41eb-b2c1-01f5d887f293",
  "senderId": "b838b085-5dae-4aa7-be95-0164ebdbbde5",
  "body": "Hello everyone!",
  "checksum": 2341490237
}
```

#### ACK Message

An acknowledgment message sent in response to receiving a valid `CHAT` message. The `originalMessageId` links it back to the message it acknowledges:

```json
{
  "type": "ACK",
  "messageId": "1ecfe3c5-5854-4f8e-b598-d644d7219cc9",
  "senderId": "6938cfb0-3eb1-4ef4-91ff-b8b9c83a9f67",
  "body": "Message received",
  "checksum": 4168059567,
  "originalMessageId": "f220e902-fbd5-41eb-b2c1-01f5d887f293"
}
```

## Protocol Operations

### Peer Discovery

Peer discovery is **implicit**. When a client comes online, it starts listening on the multicast group. Any other client that sends a message is automatically discovered and added to the local "known peers" list. Peers that have not sent a message for a predefined timeout period are removed from this list.

### Message Integrity and Reliability

The protocol implements a three-step process for reliable message delivery:

1. **Sending**: A client constructs a `CHAT` message, calculates the CRC32 checksum of the `body`, and sends the full JSON object to the multicast group. The client then begins tracking this message's `messageId`, awaiting acknowledgment.

2. **Receiving**: Upon receiving a UDP datagram, a client parses the JSON:
   - It recalculates the checksum on the `body` field and compares it to the `checksum` value in the message
   - If the checksums match, the message is considered valid. The client displays the chat message to the user and sends an `ACK` message back to the group
   - If the checksums do not match, the message is corrupt. The client discards it and sends a `NACK` message back to the group

3. **Acknowledgment**: When a sending client receives an `ACK` for one of its tracked `messageId`s, it marks the message as successfully delivered. If it does not receive an `ACK` within a specific timeout period, it marks the message as "timed out."
