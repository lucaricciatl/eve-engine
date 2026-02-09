#include "engine/Network.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#endif

namespace vkengine {

// ============================================================================
// Packet Implementation
// ============================================================================

Packet::Packet() = default;

Packet::Packet(std::uint16_t packetType) {
    header.type = packetType;
}

Packet::Packet(const std::uint8_t* data, std::size_t size) {
    if (data && size > 0) {
        payload.assign(data, data + size);
    }
}

void Packet::ensureCapacity(std::size_t additionalBytes) {
    payload.reserve(payload.size() + additionalBytes);
}

void Packet::writeUInt8(std::uint8_t value) {
    payload.push_back(value);
}

void Packet::writeUInt16(std::uint16_t value) {
    payload.push_back(static_cast<std::uint8_t>(value >> 8));
    payload.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

void Packet::writeUInt32(std::uint32_t value) {
    payload.push_back(static_cast<std::uint8_t>(value >> 24));
    payload.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    payload.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    payload.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

void Packet::writeUInt64(std::uint64_t value) {
    writeUInt32(static_cast<std::uint32_t>(value >> 32));
    writeUInt32(static_cast<std::uint32_t>(value & 0xFFFFFFFF));
}

void Packet::writeInt8(std::int8_t value) {
    writeUInt8(static_cast<std::uint8_t>(value));
}

void Packet::writeInt16(std::int16_t value) {
    writeUInt16(static_cast<std::uint16_t>(value));
}

void Packet::writeInt32(std::int32_t value) {
    writeUInt32(static_cast<std::uint32_t>(value));
}

void Packet::writeInt64(std::int64_t value) {
    writeUInt64(static_cast<std::uint64_t>(value));
}

void Packet::writeFloat(float value) {
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(float));
    writeUInt32(bits);
}

void Packet::writeDouble(double value) {
    std::uint64_t bits;
    std::memcpy(&bits, &value, sizeof(double));
    writeUInt64(bits);
}

void Packet::writeString(const std::string& str) {
    writeUInt16(static_cast<std::uint16_t>(str.size()));
    for (char c : str) {
        writeUInt8(static_cast<std::uint8_t>(c));
    }
}

void Packet::writeVec2(const glm::vec2& value) {
    writeFloat(value.x);
    writeFloat(value.y);
}

void Packet::writeVec3(const glm::vec3& value) {
    writeFloat(value.x);
    writeFloat(value.y);
    writeFloat(value.z);
}

void Packet::writeVec4(const glm::vec4& value) {
    writeFloat(value.x);
    writeFloat(value.y);
    writeFloat(value.z);
    writeFloat(value.w);
}

void Packet::writeQuat(const glm::quat& value) {
    writeFloat(value.x);
    writeFloat(value.y);
    writeFloat(value.z);
    writeFloat(value.w);
}

void Packet::writeBytes(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < size; i++) {
        writeUInt8(bytes[i]);
    }
}

std::uint8_t Packet::readUInt8() {
    if (readPos >= payload.size()) return 0;
    return payload[readPos++];
}

std::uint16_t Packet::readUInt16() {
    std::uint16_t value = static_cast<std::uint16_t>(readUInt8()) << 8;
    value |= readUInt8();
    return value;
}

std::uint32_t Packet::readUInt32() {
    std::uint32_t value = static_cast<std::uint32_t>(readUInt8()) << 24;
    value |= static_cast<std::uint32_t>(readUInt8()) << 16;
    value |= static_cast<std::uint32_t>(readUInt8()) << 8;
    value |= readUInt8();
    return value;
}

std::uint64_t Packet::readUInt64() {
    std::uint64_t high = readUInt32();
    std::uint64_t low = readUInt32();
    return (high << 32) | low;
}

std::int8_t Packet::readInt8() {
    return static_cast<std::int8_t>(readUInt8());
}

std::int16_t Packet::readInt16() {
    return static_cast<std::int16_t>(readUInt16());
}

std::int32_t Packet::readInt32() {
    return static_cast<std::int32_t>(readUInt32());
}

std::int64_t Packet::readInt64() {
    return static_cast<std::int64_t>(readUInt64());
}

float Packet::readFloat() {
    std::uint32_t bits = readUInt32();
    float value;
    std::memcpy(&value, &bits, sizeof(float));
    return value;
}

double Packet::readDouble() {
    std::uint64_t bits = readUInt64();
    double value;
    std::memcpy(&value, &bits, sizeof(double));
    return value;
}

std::string Packet::readString() {
    std::uint16_t length = readUInt16();
    std::string str;
    str.reserve(length);
    for (std::uint16_t i = 0; i < length; ++i) {
        str.push_back(static_cast<char>(readUInt8()));
    }
    return str;
}

glm::vec2 Packet::readVec2() {
    float x = readFloat();
    float y = readFloat();
    return glm::vec2(x, y);
}

glm::vec3 Packet::readVec3() {
    float x = readFloat();
    float y = readFloat();
    float z = readFloat();
    return glm::vec3(x, y, z);
}

glm::vec4 Packet::readVec4() {
    float x = readFloat();
    float y = readFloat();
    float z = readFloat();
    float w = readFloat();
    return glm::vec4(x, y, z, w);
}

glm::quat Packet::readQuat() {
    float x = readFloat();
    float y = readFloat();
    float z = readFloat();
    float w = readFloat();
    return glm::quat(w, x, y, z);  // glm::quat constructor is (w, x, y, z)
}

void Packet::readBytes(void* data, std::size_t size) {
    auto* bytes = static_cast<std::uint8_t*>(data);
    for (std::size_t i = 0; i < size; i++) {
        bytes[i] = readUInt8();
    }
}

std::vector<std::uint8_t> Packet::serialize() const {
    std::vector<std::uint8_t> result;
    result.reserve(sizeof(PacketHeader) + payload.size());
    
    // Write header
    result.push_back(static_cast<std::uint8_t>(header.type >> 8));
    result.push_back(static_cast<std::uint8_t>(header.type & 0xFF));
    result.push_back(static_cast<std::uint8_t>(header.sequence >> 24));
    result.push_back(static_cast<std::uint8_t>((header.sequence >> 16) & 0xFF));
    result.push_back(static_cast<std::uint8_t>((header.sequence >> 8) & 0xFF));
    result.push_back(static_cast<std::uint8_t>(header.sequence & 0xFF));
    result.push_back(static_cast<std::uint8_t>(header.ack >> 24));
    result.push_back(static_cast<std::uint8_t>((header.ack >> 16) & 0xFF));
    result.push_back(static_cast<std::uint8_t>((header.ack >> 8) & 0xFF));
    result.push_back(static_cast<std::uint8_t>(header.ack & 0xFF));
    result.push_back(static_cast<std::uint8_t>(header.ackBitfield >> 24));
    result.push_back(static_cast<std::uint8_t>((header.ackBitfield >> 16) & 0xFF));
    result.push_back(static_cast<std::uint8_t>((header.ackBitfield >> 8) & 0xFF));
    result.push_back(static_cast<std::uint8_t>(header.ackBitfield & 0xFF));
    
    // Payload size
    std::uint16_t size = static_cast<std::uint16_t>(payload.size());
    result.push_back(static_cast<std::uint8_t>(size >> 8));
    result.push_back(static_cast<std::uint8_t>(size & 0xFF));
    
    // Checksum (placeholder)
    result.push_back(0);
    result.push_back(0);
    
    // Payload
    result.insert(result.end(), payload.begin(), payload.end());
    
    return result;
}

void Packet::reset() {
    header = PacketHeader{};
    payload.clear();
    readPos = 0;
}

// ============================================================================
// Connection Implementation
// ============================================================================

Connection::Connection() = default;

Connection::Connection(ClientId clientId, const NetworkAddress& addr)
    : id(clientId), remoteAddress(addr), connectionState(ConnectionState::Connecting) {
}

void Connection::trackSentPacket(SequenceNumber seq, const Packet& packet) {
    std::lock_guard<std::mutex> lock(ackMutex);
    SentPacket sent;
    sent.sequence = seq;
    sent.packet = packet;
    sent.sentTime = std::chrono::steady_clock::now();
    pendingAcks.push_back(sent);
}

void Connection::acknowledgePacket(SequenceNumber seq) {
    std::lock_guard<std::mutex> lock(ackMutex);
    pendingAcks.erase(
        std::remove_if(pendingAcks.begin(), pendingAcks.end(),
            [seq](const SentPacket& p) { return p.sequence == seq; }),
        pendingAcks.end()
    );
}

std::vector<Packet> Connection::getPacketsToResend() {
    std::lock_guard<std::mutex> lock(ackMutex);
    std::vector<Packet> toResend;
    auto now = std::chrono::steady_clock::now();
    
    for (auto& sent : pendingAcks) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - sent.sentTime).count();
        if (elapsed > 100 && sent.resendCount < 5) {  // Resend after 100ms, max 5 attempts
            toResend.push_back(sent.packet);
            sent.sentTime = now;
            sent.resendCount++;
        }
    }
    
    return toResend;
}

SequenceNumber Connection::nextOutgoingSequence() {
    return outgoingSeq++;
}

void Connection::setLastReceivedSequence(SequenceNumber seq) {
    lastRecvSeq = seq;
    statistics.lastReceived = std::chrono::steady_clock::now();
}

// ============================================================================
// Socket Implementation
// ============================================================================

Socket::Socket() = default;

Socket::~Socket() {
    close();
}

bool Socket::bind(std::uint16_t port) {
    socketHandle = static_cast<int>(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (socketHandle < 0) return false;
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (::bind(socketHandle, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close();
        return false;
    }
    
    isTcp = false;
    return true;
}

bool Socket::sendTo(const NetworkAddress& address, const void* data, std::size_t size) {
    if (socketHandle < 0) return false;
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(address.port);
    inet_pton(AF_INET, address.host.c_str(), &addr.sin_addr);
    
    auto sent = ::sendto(socketHandle, static_cast<const char*>(data), static_cast<int>(size), 0,
                         reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    return sent > 0;
}

int Socket::receiveFrom(NetworkAddress& address, void* buffer, std::size_t bufferSize) {
    if (socketHandle < 0) return -1;
    
    sockaddr_in addr{};
    socklen_t addrLen = sizeof(addr);
    
    int received = recvfrom(socketHandle, static_cast<char*>(buffer), static_cast<int>(bufferSize), 0,
                            reinterpret_cast<sockaddr*>(&addr), &addrLen);
    
    if (received > 0) {
        char hostBuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, hostBuf, sizeof(hostBuf));
        address.host = hostBuf;
        address.port = ntohs(addr.sin_port);
    }
    
    return received;
}

bool Socket::connect(const NetworkAddress& address) {
    socketHandle = static_cast<int>(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (socketHandle < 0) return false;
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(address.port);
    inet_pton(AF_INET, address.host.c_str(), &addr.sin_addr);
    
    if (::connect(socketHandle, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close();
        return false;
    }
    
    isTcp = true;
    return true;
}

bool Socket::listen(std::uint16_t port, int backlog) {
    socketHandle = static_cast<int>(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (socketHandle < 0) return false;
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (::bind(socketHandle, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close();
        return false;
    }
    
    if (::listen(socketHandle, backlog) < 0) {
        close();
        return false;
    }
    
    isTcp = true;
    return true;
}

std::unique_ptr<Socket> Socket::accept() {
    if (socketHandle < 0) return nullptr;
    
    sockaddr_in clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);
    
    int clientSocket = static_cast<int>(::accept(socketHandle, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen));
    if (clientSocket < 0) return nullptr;
    
    auto sock = std::make_unique<Socket>();
    sock->socketHandle = clientSocket;
    sock->isTcp = true;
    return sock;
}

bool Socket::send(const void* data, std::size_t size) {
    if (socketHandle < 0) return false;
    auto sent = ::send(socketHandle, static_cast<const char*>(data), static_cast<int>(size), 0);
    return sent > 0;
}

int Socket::receive(void* buffer, std::size_t bufferSize) {
    if (socketHandle < 0) return -1;
    return recv(socketHandle, static_cast<char*>(buffer), static_cast<int>(bufferSize), 0);
}

void Socket::close() {
    if (socketHandle >= 0) {
#ifdef _WIN32
        closesocket(socketHandle);
#else
        ::close(socketHandle);
#endif
        socketHandle = -1;
    }
}

void Socket::setNonBlocking(bool nonBlocking) {
#ifdef _WIN32
    u_long mode = nonBlocking ? 1 : 0;
    ioctlsocket(socketHandle, FIONBIO, &mode);
#else
    int flags = fcntl(socketHandle, F_GETFL, 0);
    if (nonBlocking) {
        fcntl(socketHandle, F_SETFL, flags | O_NONBLOCK);
    } else {
        fcntl(socketHandle, F_SETFL, flags & ~O_NONBLOCK);
    }
#endif
}

void Socket::setReceiveTimeout(int milliseconds) {
#ifdef _WIN32
    DWORD timeout = milliseconds;
    setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = milliseconds / 1000;
    tv.tv_usec = (milliseconds % 1000) * 1000;
    setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

// ============================================================================
// NetworkServer Implementation
// ============================================================================

NetworkServer::NetworkServer() = default;

NetworkServer::~NetworkServer() {
    stop();
}

bool NetworkServer::start(std::uint16_t port, std::uint32_t max) {
    if (running) return false;
    
    socket = std::make_unique<Socket>();
    if (!socket->bind(port)) {
        socket.reset();
        return false;
    }
    
    socket->setNonBlocking(true);
    serverPort = port;
    maxClients = max;
    running = true;
    
    networkWorker = std::thread(&NetworkServer::networkThread, this);
    return true;
}

void NetworkServer::stop() {
    running = false;
    if (networkWorker.joinable()) {
        networkWorker.join();
    }
    socket.reset();
    connections.clear();
}

void NetworkServer::disconnectClient(ClientId clientId, const std::string& reason) {
    std::lock_guard<std::mutex> lock(connectionMutex);
    auto it = connections.find(clientId);
    if (it != connections.end()) {
        // Send disconnect packet
        Packet packet(PacketTypes::Disconnect);
        packet.writeString(reason);
        send(clientId, packet);
        
        // Remove connection
        connections.erase(it);
        
        // Queue event
        std::lock_guard<std::mutex> eventLock(eventMutex);
        NetworkEvent event;
        event.type = NetworkEvent::Type::Disconnected;
        event.clientId = clientId;
        event.message = reason;
        eventQueue.push_back(std::move(event));
    }
}

void NetworkServer::disconnectAll(const std::string& reason) {
    std::vector<ClientId> clients = connectedClients();
    for (ClientId id : clients) {
        disconnectClient(id, reason);
    }
}

std::vector<ClientId> NetworkServer::connectedClients() const {
    std::lock_guard<std::mutex> lock(connectionMutex);
    std::vector<ClientId> result;
    for (const auto& [id, _] : connections) {
        result.push_back(id);
    }
    return result;
}

Connection* NetworkServer::getConnection(ClientId clientId) {
    std::lock_guard<std::mutex> lock(connectionMutex);
    auto it = connections.find(clientId);
    return it != connections.end() ? it->second.get() : nullptr;
}

std::size_t NetworkServer::clientCount() const {
    std::lock_guard<std::mutex> lock(connectionMutex);
    return connections.size();
}

void NetworkServer::send(ClientId clientId, const Packet& packet, PacketReliability reliability) {
    (void)reliability;  // TODO: implement reliability
    
    std::lock_guard<std::mutex> lock(connectionMutex);
    auto it = connections.find(clientId);
    if (it == connections.end()) return;
    
    auto data = packet.serialize();
    socket->sendTo(it->second->address(), data.data(), data.size());
}

void NetworkServer::broadcast(const Packet& packet, PacketReliability reliability) {
    for (ClientId id : connectedClients()) {
        send(id, packet, reliability);
    }
}

void NetworkServer::broadcastExcept(ClientId excludeId, const Packet& packet, PacketReliability reliability) {
    for (ClientId id : connectedClients()) {
        if (id != excludeId) {
            send(id, packet, reliability);
        }
    }
}

bool NetworkServer::pollEvent(NetworkEvent& event) {
    std::lock_guard<std::mutex> lock(eventMutex);
    if (eventQueue.empty()) return false;
    
    event = std::move(eventQueue.front());
    eventQueue.pop_front();
    return true;
}

void NetworkServer::setOnClientConnected(ConnectionCallback callback) {
    onConnected = std::move(callback);
}

void NetworkServer::setOnClientDisconnected(DisconnectionCallback callback) {
    onDisconnected = std::move(callback);
}

void NetworkServer::setOnPacketReceived(PacketCallback callback) {
    onPacketReceived = std::move(callback);
}

void NetworkServer::update() {
    // Process events
    NetworkEvent event;
    while (pollEvent(event)) {
        switch (event.type) {
            case NetworkEvent::Type::Connected:
                if (onConnected) onConnected(event.clientId);
                break;
            case NetworkEvent::Type::Disconnected:
                if (onDisconnected) onDisconnected(event.clientId, event.message);
                break;
            case NetworkEvent::Type::DataReceived:
                if (onPacketReceived && event.packet) {
                    onPacketReceived(event.clientId, *event.packet);
                }
                break;
            default:
                break;
        }
    }
}

void NetworkServer::networkThread() {
    std::vector<std::uint8_t> buffer(65536);
    
    while (running) {
        NetworkAddress fromAddr;
        int received = socket->receiveFrom(fromAddr, buffer.data(), buffer.size());
        
        if (received > 0) {
            processIncomingPacket(fromAddr, buffer.data(), received);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void NetworkServer::processIncomingPacket(const NetworkAddress& from, const std::uint8_t* data, std::size_t size) {
    if (size < sizeof(PacketHeader)) return;
    
    // Find or create connection
    std::string addrKey = from.host + ":" + std::to_string(from.port);
    ClientId clientId = InvalidClientId;
    
    {
        std::lock_guard<std::mutex> lock(connectionMutex);
        auto it = addressToClient.find(addrKey);
        if (it != addressToClient.end()) {
            clientId = it->second;
        } else if (connections.size() < maxClients) {
            clientId = generateClientId();
            connections[clientId] = std::make_unique<Connection>(clientId, from);
            addressToClient[addrKey] = clientId;
            
            // Queue connected event
            std::lock_guard<std::mutex> eventLock(eventMutex);
            NetworkEvent event;
            event.type = NetworkEvent::Type::Connected;
            event.clientId = clientId;
            eventQueue.push_back(std::move(event));
        }
    }
    
    if (clientId == InvalidClientId) return;
    
    // Create packet from data
    auto packet = std::make_unique<Packet>(data, size);
    
    // Queue event
    std::lock_guard<std::mutex> lock(eventMutex);
    NetworkEvent event;
    event.type = NetworkEvent::Type::DataReceived;
    event.clientId = clientId;
    event.packet = std::move(packet);
    eventQueue.push_back(std::move(event));
}

ClientId NetworkServer::generateClientId() {
    return nextClientId++;
}

// ============================================================================
// NetworkClient Implementation
// ============================================================================

NetworkClient::NetworkClient() = default;

NetworkClient::~NetworkClient() {
    disconnect();
}

bool NetworkClient::connect(const std::string& host, std::uint16_t port) {
    if (running) return false;
    
    socket = std::make_unique<Socket>();
    if (!socket->bind(0)) {  // Bind to any port
        socket.reset();
        return false;
    }
    
    socket->setNonBlocking(true);
    
    serverAddress.host = host;
    serverAddress.port = port;
    
    // Send connection request
    Packet packet(PacketTypes::ConnectionRequest);
    auto data = packet.serialize();
    socket->sendTo(serverAddress, data.data(), data.size());
    
    connectionState = ConnectionState::Connecting;
    running = true;
    networkWorker = std::thread(&NetworkClient::networkThread, this);
    
    return true;
}

void NetworkClient::disconnect() {
    if (!running) return;
    
    // Send disconnect packet
    Packet packet(PacketTypes::Disconnect);
    auto data = packet.serialize();
    socket->sendTo(serverAddress, data.data(), data.size());
    
    running = false;
    connectionState = ConnectionState::Disconnected;
    if (networkWorker.joinable()) {
        networkWorker.join();
    }
    socket.reset();
}

void NetworkClient::send(const Packet& packet, PacketReliability reliability) {
    (void)reliability;
    if (!running) return;
    
    auto data = packet.serialize();
    socket->sendTo(serverAddress, data.data(), data.size());
}

bool NetworkClient::pollEvent(NetworkEvent& event) {
    std::lock_guard<std::mutex> lock(eventMutex);
    if (eventQueue.empty()) return false;
    
    event = std::move(eventQueue.front());
    eventQueue.pop_front();
    return true;
}

void NetworkClient::setOnConnected(std::function<void()> callback) {
    onConnected = std::move(callback);
}

void NetworkClient::setOnDisconnected(std::function<void(const std::string&)> callback) {
    onDisconnected = std::move(callback);
}

void NetworkClient::setOnPacketReceived(std::function<void(const Packet&)> callback) {
    onPacketReceived = std::move(callback);
}

void NetworkClient::update() {
    NetworkEvent event;
    while (pollEvent(event)) {
        switch (event.type) {
            case NetworkEvent::Type::Connected:
                if (onConnected) onConnected();
                break;
            case NetworkEvent::Type::Disconnected:
                if (onDisconnected) onDisconnected(event.message);
                break;
            case NetworkEvent::Type::DataReceived:
                if (onPacketReceived && event.packet) {
                    onPacketReceived(*event.packet);
                }
                break;
            default:
                break;
        }
    }
}

void NetworkClient::sendPing() {
    Packet packet(PacketTypes::Ping);
    auto data = packet.serialize();
    socket->sendTo(serverAddress, data.data(), data.size());
    lastPingTime = std::chrono::steady_clock::now();
}

void NetworkClient::processIncomingPacket(const std::uint8_t* data, std::size_t size) {
    Packet packet(data, size);
    
    switch (packet.type()) {
        case PacketTypes::ConnectionAccept:
            connectionState = ConnectionState::Connected;
            if (onConnected) onConnected();
            break;
        case PacketTypes::ConnectionReject:
            connectionState = ConnectionState::Disconnected;
            break;
        case PacketTypes::Disconnect:
            connectionState = ConnectionState::Disconnected;
            if (onDisconnected) onDisconnected("Server disconnected");
            break;
        case PacketTypes::Pong:
            lastPongTime = std::chrono::steady_clock::now();
            statistics.roundTripTime = std::chrono::duration<float>(lastPongTime - lastPingTime).count();
            statistics.smoothedRTT = statistics.smoothedRTT * 0.9f + statistics.roundTripTime * 0.1f;
            break;
        default:
            if (onPacketReceived) onPacketReceived(packet);
            break;
    }
}

void NetworkClient::networkThread() {
    std::vector<std::uint8_t> buffer(65536);
    
    while (running) {
        NetworkAddress fromAddr;
        int received = socket->receiveFrom(fromAddr, buffer.data(), buffer.size());
        
        if (received > 0) {
            processIncomingPacket(buffer.data(), static_cast<std::size_t>(received));
            
            std::lock_guard<std::mutex> lock(eventMutex);
            NetworkEvent event;
            event.type = NetworkEvent::Type::DataReceived;
            event.packet = std::make_unique<Packet>(buffer.data(), static_cast<std::size_t>(received));
            eventQueue.push_back(std::move(event));
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// ============================================================================
// ClientPrediction Implementation
// ============================================================================

void ClientPrediction::recordInput(const InputCommand& input) {
    pendingInputs.push_back(input);
    
    // Limit pending inputs
    while (pendingInputs.size() > 60) {
        pendingInputs.pop_front();
    }
}

void ClientPrediction::applyServerState(const Transform& serverState, std::uint32_t lastProcessedInput) {
    lastAcknowledgedInput = lastProcessedInput;
    
    // Remove acknowledged inputs
    while (!pendingInputs.empty() && pendingInputs.front().sequence <= lastProcessedInput) {
        pendingInputs.pop_front();
    }
    
    // Re-predict from server state (any remaining inputs need reapplication)
    (void)serverState;  // Would re-apply remaining inputs
}

Transform ClientPrediction::predict(const Transform& baseState, float deltaSeconds) const {
    Transform predicted = baseState;
    
    // Apply all pending inputs
    for (const auto& input : pendingInputs) {
        predicted.position += input.movement * moveSpeed * deltaSeconds;
    }
    
    return predicted;
}

// ============================================================================
// InterpolationBuffer Implementation
// ============================================================================

InterpolationBuffer::InterpolationBuffer(float delay)
    : interpolationDelay(delay) {
}

void InterpolationBuffer::addSnapshot(float timestamp, const Transform& transform) {
    InterpolationSnapshot snapshot;
    snapshot.timestamp = timestamp;
    snapshot.transform = transform;
    
    snapshots.push_back(snapshot);
    
    // Keep only recent snapshots
    while (snapshots.size() > MaxSnapshots) {
        snapshots.pop_front();
    }
}

Transform InterpolationBuffer::interpolate(float currentTime) const {
    if (snapshots.empty()) {
        return Transform{};
    }
    
    float renderTime = currentTime - interpolationDelay;
    
    // Find surrounding snapshots
    const InterpolationSnapshot* before = nullptr;
    const InterpolationSnapshot* after = nullptr;
    
    for (const auto& snap : snapshots) {
        if (snap.timestamp <= renderTime) {
            before = &snap;
        } else if (!after) {
            after = &snap;
            break;
        }
    }
    
    if (!before) return snapshots.front().transform;
    if (!after) return snapshots.back().transform;
    
    // Interpolate
    float t = (renderTime - before->timestamp) / (after->timestamp - before->timestamp);
    t = std::clamp(t, 0.0f, 1.0f);
    
    Transform result;
    result.position = glm::mix(before->transform.position, after->transform.position, t);
    result.rotation = glm::mix(before->transform.rotation, after->transform.rotation, t);
    result.scale = glm::mix(before->transform.scale, after->transform.scale, t);
    
    return result;
}

} // namespace vkengine
