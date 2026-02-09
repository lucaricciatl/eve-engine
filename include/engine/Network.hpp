#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "core/ecs/Components.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

namespace vkengine {

// Forward declarations
class Scene;
class GameObject;

// ============================================================================
// Network Types
// ============================================================================

using ClientId = std::uint32_t;
using SequenceNumber = std::uint32_t;
constexpr ClientId InvalidClientId = 0;

enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting
};

enum class PacketReliability {
    Unreliable,           // Fire and forget (UDP-like)
    UnreliableSequenced,  // Unreliable but ordered, drops old packets
    Reliable,             // Guaranteed delivery (resends until acked)
    ReliableOrdered       // Guaranteed delivery and ordering
};

enum class PacketPriority {
    Low,
    Medium,
    High,
    Immediate
};

// ============================================================================
// Network Address
// ============================================================================

struct NetworkAddress {
    std::string host;
    std::uint16_t port{0};

    bool operator==(const NetworkAddress& other) const {
        return host == other.host && port == other.port;
    }
};

// ============================================================================
// Packet
// ============================================================================

struct PacketHeader {
    std::uint32_t magic{0x45564500};  // "EVE\0"
    std::uint16_t type{0};
    std::uint16_t flags{0};
    SequenceNumber sequence{0};
    SequenceNumber ack{0};
    std::uint32_t ackBitfield{0};
    std::uint16_t payloadSize{0};
    std::uint16_t checksum{0};
};

class Packet {
public:
    Packet();
    explicit Packet(std::uint16_t type);
    Packet(const std::uint8_t* data, std::size_t size);

    // Writing
    void writeUInt8(std::uint8_t value);
    void writeUInt16(std::uint16_t value);
    void writeUInt32(std::uint32_t value);
    void writeUInt64(std::uint64_t value);
    void writeInt8(std::int8_t value);
    void writeInt16(std::int16_t value);
    void writeInt32(std::int32_t value);
    void writeInt64(std::int64_t value);
    void writeFloat(float value);
    void writeDouble(double value);
    void writeString(const std::string& value);
    void writeVec2(const glm::vec2& value);
    void writeVec3(const glm::vec3& value);
    void writeVec4(const glm::vec4& value);
    void writeQuat(const glm::quat& value);
    void writeBytes(const void* data, std::size_t size);

    // Reading
    std::uint8_t readUInt8();
    std::uint16_t readUInt16();
    std::uint32_t readUInt32();
    std::uint64_t readUInt64();
    std::int8_t readInt8();
    std::int16_t readInt16();
    std::int32_t readInt32();
    std::int64_t readInt64();
    float readFloat();
    double readDouble();
    std::string readString();
    glm::vec2 readVec2();
    glm::vec3 readVec3();
    glm::vec4 readVec4();
    glm::quat readQuat();
    void readBytes(void* data, std::size_t size);

    // Packet info
    [[nodiscard]] std::uint16_t type() const { return header.type; }
    [[nodiscard]] SequenceNumber sequence() const { return header.sequence; }
    [[nodiscard]] std::size_t payloadSize() const { return payload.size(); }
    [[nodiscard]] const std::vector<std::uint8_t>& data() const { return payload; }
    [[nodiscard]] std::vector<std::uint8_t> serialize() const;

    void setType(std::uint16_t type) { header.type = type; }
    void setSequence(SequenceNumber seq) { header.sequence = seq; }
    void reset();
    void resetRead() { readPos = 0; }

private:
    void ensureCapacity(std::size_t additionalBytes);

    PacketHeader header;
    std::vector<std::uint8_t> payload;
    std::size_t readPos{0};
};

// ============================================================================
// Built-in Packet Types
// ============================================================================

namespace PacketTypes {
    constexpr std::uint16_t ConnectionRequest = 1;
    constexpr std::uint16_t ConnectionAccept = 2;
    constexpr std::uint16_t ConnectionReject = 3;
    constexpr std::uint16_t Disconnect = 4;
    constexpr std::uint16_t Ping = 5;
    constexpr std::uint16_t Pong = 6;
    constexpr std::uint16_t EntityCreate = 10;
    constexpr std::uint16_t EntityDestroy = 11;
    constexpr std::uint16_t EntityUpdate = 12;
    constexpr std::uint16_t ComponentUpdate = 13;
    constexpr std::uint16_t RPC = 20;
    constexpr std::uint16_t StateSnapshot = 30;
    constexpr std::uint16_t StateDelta = 31;
    constexpr std::uint16_t Input = 40;
    constexpr std::uint16_t UserBase = 100;  // User-defined packets start here
}

// ============================================================================
// Connection
// ============================================================================

struct ConnectionStats {
    std::uint64_t bytesSent{0};
    std::uint64_t bytesReceived{0};
    std::uint64_t packetsSent{0};
    std::uint64_t packetsReceived{0};
    std::uint64_t packetsLost{0};
    float packetLoss{0.0f};
    float roundTripTime{0.0f};
    float smoothedRTT{0.0f};
    float jitter{0.0f};
    std::chrono::steady_clock::time_point lastReceived;
};

class Connection {
public:
    Connection();
    Connection(ClientId id, const NetworkAddress& address);

    [[nodiscard]] ClientId clientId() const { return id; }
    [[nodiscard]] const NetworkAddress& address() const { return remoteAddress; }
    [[nodiscard]] ConnectionState state() const { return connectionState; }
    [[nodiscard]] const ConnectionStats& stats() const { return statistics; }

    void setState(ConnectionState state) { connectionState = state; }
    void updateStats(const ConnectionStats& newStats) { statistics = newStats; }

    // Reliable packet tracking
    void trackSentPacket(SequenceNumber seq, const Packet& packet);
    void acknowledgePacket(SequenceNumber seq);
    [[nodiscard]] std::vector<Packet> getPacketsToResend();

    // Sequence numbers
    SequenceNumber nextOutgoingSequence();
    void setLastReceivedSequence(SequenceNumber seq);
    [[nodiscard]] SequenceNumber lastReceivedSequence() const { return lastRecvSeq; }

private:
    ClientId id{InvalidClientId};
    NetworkAddress remoteAddress;
    ConnectionState connectionState{ConnectionState::Disconnected};
    ConnectionStats statistics;

    SequenceNumber outgoingSeq{0};
    SequenceNumber lastRecvSeq{0};

    struct SentPacket {
        SequenceNumber sequence;
        Packet packet;
        std::chrono::steady_clock::time_point sentTime;
        int resendCount{0};
    };
    std::deque<SentPacket> pendingAcks;
    std::mutex ackMutex;
};

// ============================================================================
// Socket Abstraction
// ============================================================================

class Socket {
public:
    Socket();
    ~Socket();

    // Non-copyable
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // UDP operations
    bool bind(std::uint16_t port);
    bool sendTo(const NetworkAddress& address, const void* data, std::size_t size);
    int receiveFrom(NetworkAddress& address, void* buffer, std::size_t bufferSize);

    // TCP operations (for initial connection or fallback)
    bool connect(const NetworkAddress& address);
    bool listen(std::uint16_t port, int backlog = 10);
    std::unique_ptr<Socket> accept();
    bool send(const void* data, std::size_t size);
    int receive(void* buffer, std::size_t bufferSize);

    void close();
    [[nodiscard]] bool isOpen() const { return socketHandle >= 0; }
    void setNonBlocking(bool nonBlocking);
    void setReceiveTimeout(int milliseconds);

private:
    int socketHandle{-1};
    bool isTcp{false};
};

// ============================================================================
// Network Events
// ============================================================================

struct NetworkEvent {
    enum class Type {
        Connected,
        Disconnected,
        DataReceived,
        ConnectionFailed,
        Timeout
    };

    Type type;
    ClientId clientId{InvalidClientId};
    std::unique_ptr<Packet> packet;
    std::string message;
};

// ============================================================================
// Network Server
// ============================================================================

class NetworkServer {
public:
    NetworkServer();
    ~NetworkServer();

    // Server lifecycle
    bool start(std::uint16_t port, std::uint32_t maxClients = 32);
    void stop();
    [[nodiscard]] bool isRunning() const { return running; }

    // Client management
    void disconnectClient(ClientId clientId, const std::string& reason = "");
    void disconnectAll(const std::string& reason = "");
    [[nodiscard]] std::vector<ClientId> connectedClients() const;
    [[nodiscard]] Connection* getConnection(ClientId clientId);
    [[nodiscard]] std::size_t clientCount() const;

    // Sending
    void send(ClientId clientId, const Packet& packet, PacketReliability reliability = PacketReliability::Reliable);
    void broadcast(const Packet& packet, PacketReliability reliability = PacketReliability::Reliable);
    void broadcastExcept(ClientId excludeId, const Packet& packet, PacketReliability reliability = PacketReliability::Reliable);

    // Receiving (poll-based)
    [[nodiscard]] bool pollEvent(NetworkEvent& event);

    // Callbacks (alternative to polling)
    using ConnectionCallback = std::function<void(ClientId)>;
    using DisconnectionCallback = std::function<void(ClientId, const std::string&)>;
    using PacketCallback = std::function<void(ClientId, const Packet&)>;

    void setOnClientConnected(ConnectionCallback callback);
    void setOnClientDisconnected(DisconnectionCallback callback);
    void setOnPacketReceived(PacketCallback callback);

    // Update (call each frame)
    void update();

    // Server info
    [[nodiscard]] std::uint16_t port() const { return serverPort; }

private:
    void networkThread();
    void processIncomingPacket(const NetworkAddress& from, const std::uint8_t* data, std::size_t size);
    ClientId generateClientId();

    std::unique_ptr<Socket> socket;
    std::uint16_t serverPort{0};
    std::uint32_t maxClients{32};
    std::atomic<bool> running{false};
    std::thread networkWorker;

    std::unordered_map<ClientId, std::unique_ptr<Connection>> connections;
    std::unordered_map<std::string, ClientId> addressToClient;  // "host:port" -> ClientId
    ClientId nextClientId{1};
    mutable std::mutex connectionMutex;

    std::deque<NetworkEvent> eventQueue;
    std::mutex eventMutex;

    ConnectionCallback onConnected;
    DisconnectionCallback onDisconnected;
    PacketCallback onPacketReceived;
};

// ============================================================================
// Network Client
// ============================================================================

class NetworkClient {
public:
    NetworkClient();
    ~NetworkClient();

    // Connection
    bool connect(const std::string& host, std::uint16_t port);
    void disconnect();
    [[nodiscard]] bool isConnected() const { return connectionState == ConnectionState::Connected; }
    [[nodiscard]] ConnectionState state() const { return connectionState; }

    // Sending
    void send(const Packet& packet, PacketReliability reliability = PacketReliability::Reliable);

    // Receiving (poll-based)
    [[nodiscard]] bool pollEvent(NetworkEvent& event);

    // Callbacks
    using ConnectedCallback = std::function<void()>;
    using DisconnectedCallback = std::function<void(const std::string&)>;
    using PacketCallback = std::function<void(const Packet&)>;

    void setOnConnected(ConnectedCallback callback);
    void setOnDisconnected(DisconnectedCallback callback);
    void setOnPacketReceived(PacketCallback callback);

    // Update
    void update();

    // Stats
    [[nodiscard]] const ConnectionStats& stats() const { return statistics; }
    [[nodiscard]] float ping() const { return statistics.smoothedRTT * 1000.0f; }

private:
    void networkThread();
    void processIncomingPacket(const std::uint8_t* data, std::size_t size);
    void sendPing();

    std::unique_ptr<Socket> socket;
    NetworkAddress serverAddress;
    std::atomic<ConnectionState> connectionState{ConnectionState::Disconnected};
    std::thread networkWorker;
    std::atomic<bool> running{false};

    SequenceNumber outgoingSeq{0};
    SequenceNumber lastRecvSeq{0};
    ConnectionStats statistics;

    std::deque<NetworkEvent> eventQueue;
    std::mutex eventMutex;

    ConnectedCallback onConnected;
    DisconnectedCallback onDisconnected;
    PacketCallback onPacketReceived;

    std::chrono::steady_clock::time_point lastPingTime;
    std::chrono::steady_clock::time_point lastPongTime;
};

// ============================================================================
// Network Entity Replication
// ============================================================================

enum class NetworkAuthority {
    Server,          // Server controls this entity
    Client,          // Owning client controls this entity
    ClientPredicted  // Client predicts, server validates
};

struct NetworkIdentity {
    std::uint32_t networkId{0};
    ClientId owner{InvalidClientId};
    NetworkAuthority authority{NetworkAuthority::Server};
    bool isLocalPlayer{false};
};

struct ReplicatedComponent {
    std::string componentType;
    std::uint32_t dirtyBits{0};
    std::function<void(Packet&)> serialize;
    std::function<void(Packet&)> deserialize;
    float updateInterval{0.0f};  // 0 = every frame
    float lastUpdateTime{0.0f};
};

// ============================================================================
// Network Synchronization System
// ============================================================================

class NetworkSyncSystem {
public:
    NetworkSyncSystem();

    void setServer(NetworkServer* server);
    void setClient(NetworkClient* client);

    // Entity registration
    std::uint32_t registerEntity(GameObject& object, NetworkAuthority authority = NetworkAuthority::Server);
    void unregisterEntity(std::uint32_t networkId);
    void unregisterEntity(GameObject& object);

    // Component registration for replication
    template<typename T>
    void registerReplicatedComponent(const std::string& name,
                                     std::function<void(const T&, Packet&)> serialize,
                                     std::function<void(T&, Packet&)> deserialize);

    // Sync updates
    void update(Scene& scene, float deltaSeconds);

    // State snapshots
    Packet createSnapshot(const Scene& scene) const;
    void applySnapshot(Scene& scene, const Packet& snapshot);

    // Delta compression
    Packet createDelta(const Scene& scene) const;
    void applyDelta(Scene& scene, const Packet& delta);

    // Ownership
    void setOwner(std::uint32_t networkId, ClientId owner);
    void requestOwnership(std::uint32_t networkId);

    // Network ID lookup
    [[nodiscard]] GameObject* getObject(std::uint32_t networkId);
    [[nodiscard]] std::uint32_t getNetworkId(const GameObject& object) const;

private:
    void sendEntityCreate(std::uint32_t networkId, const GameObject& object);
    void sendEntityDestroy(std::uint32_t networkId);
    void sendEntityUpdate(std::uint32_t networkId, const GameObject& object);
    void processEntityCreate(const Packet& packet);
    void processEntityDestroy(const Packet& packet);
    void processEntityUpdate(const Packet& packet);

    NetworkServer* server{nullptr};
    NetworkClient* client{nullptr};

    std::unordered_map<std::uint32_t, GameObject*> networkIdToObject;
    std::unordered_map<const GameObject*, std::uint32_t> objectToNetworkId;
    std::uint32_t nextNetworkId{1};

    std::unordered_map<std::string, ReplicatedComponent> componentRegistry;

    // For delta compression
    std::unordered_map<std::uint32_t, std::vector<std::uint8_t>> lastSentState;
};

// ============================================================================
// Remote Procedure Calls (RPC)
// ============================================================================

class RPCManager {
public:
    using RPCHandler = std::function<void(ClientId sender, Packet& params)>;

    void registerRPC(const std::string& name, RPCHandler handler);
    void unregisterRPC(const std::string& name);

    // Server-side: call RPC on specific client
    void callOnClient(NetworkServer& server, ClientId clientId, const std::string& name, const Packet& params);

    // Server-side: call RPC on all clients
    void callOnAllClients(NetworkServer& server, const std::string& name, const Packet& params);

    // Client-side: call RPC on server
    void callOnServer(NetworkClient& client, const std::string& name, const Packet& params);

    // Process incoming RPC
    void processRPC(ClientId sender, const Packet& packet);

private:
    std::unordered_map<std::string, RPCHandler> rpcHandlers;
    std::unordered_map<std::string, std::uint16_t> rpcNameToId;
    std::unordered_map<std::uint16_t, std::string> rpcIdToName;
    std::uint16_t nextRpcId{0};
};

// ============================================================================
// Lag Compensation
// ============================================================================

struct HistoricalState {
    float timestamp;
    std::unordered_map<std::uint32_t, Transform> transforms;
};

class LagCompensation {
public:
    LagCompensation(float historyDuration = 1.0f);

    void recordState(const Scene& scene, float timestamp);
    void rewindTo(Scene& scene, float timestamp) const;
    void restoreState(Scene& scene) const;

    void setHistoryDuration(float seconds) { maxHistoryDuration = seconds; }

private:
    float maxHistoryDuration;
    std::deque<HistoricalState> stateHistory;
    HistoricalState currentState;
};

// ============================================================================
// Client-Side Prediction
// ============================================================================

struct InputCommand {
    std::uint32_t sequence{0};
    float timestamp{0.0f};
    glm::vec3 movement{0.0f};
    glm::vec2 look{0.0f};
    std::uint32_t buttons{0};  // Bitfield for button states
};

class ClientPrediction {
public:
    void recordInput(const InputCommand& input);
    void applyServerState(const Transform& serverState, std::uint32_t lastProcessedInput);
    [[nodiscard]] Transform predict(const Transform& baseState, float deltaSeconds) const;

    void setMovementSpeed(float speed) { moveSpeed = speed; }

private:
    std::deque<InputCommand> pendingInputs;
    std::uint32_t lastAcknowledgedInput{0};
    float moveSpeed{5.0f};
};

// ============================================================================
// Interpolation Buffer
// ============================================================================

struct InterpolationSnapshot {
    float timestamp;
    Transform transform;
};

class InterpolationBuffer {
public:
    InterpolationBuffer(float interpolationDelay = 0.1f);

    void addSnapshot(float timestamp, const Transform& transform);
    [[nodiscard]] Transform interpolate(float currentTime) const;

    void setDelay(float delay) { interpolationDelay = delay; }
    void clear() { snapshots.clear(); }

private:
    float interpolationDelay;
    std::deque<InterpolationSnapshot> snapshots;
    static constexpr std::size_t MaxSnapshots = 30;
};

} // namespace vkengine
