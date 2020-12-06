#include "Client.h"
#include "MulticastReceiver.h"

#include "../output/PluginManager.h"
#include "../output/IOutputChannel.h"

#include <Format.h>
#include <Logging.h>
#include <ConfigManager.h>

#include <proto/ProtoMessages.h>

#include <sys/types.h>
#include <netdb.h>

#include <stdexcept>
#include <algorithm>

#include <base64.h>

#include <bitsery/bitsery.h>
#include <bitsery/adapter/buffer.h>

using namespace Lichtenstein::Proto;
using namespace Lichtenstein::Client::Proto;

using Buffer = std::vector<uint8_t>;
using OutputAdapter = bitsery::OutputBufferAdapter<Buffer>;
using InputAdapter = bitsery::InputBufferAdapter<Buffer>;

/// singleton client instance
std::shared_ptr<Client> Client::shared = nullptr;

/**
 * Allocates the shared client instance, which will automagically attempt to
 * connect to the server.
 */
void Client::start() {
    XASSERT(shared == nullptr, "Repeated calls to Client::start() not allowed");

    shared = std::make_shared<Client>();
}

/**
 * Tears down the client handler and cleans up associated resources.
 */
void Client::stop() {
    XASSERT(shared != nullptr, "Shared client must be set up");

    shared->terminate();
    shared = nullptr;
}



/**
 * Creates a protocol client. This will set up a work thread that performs all
 * connection and message handling in the background.
 */
Client::Client() {
    // read node uuid
    auto uuidStr = ConfigManager::get("id.uuid", "");
    if(uuidStr.empty()) {
        throw std::runtime_error("Node UUID (id.uuid) is required");
    }

    auto uuid = uuids::uuid::from_string(uuidStr);
    if(!uuid.has_value()) {
        auto what = f("Couldn't parse uuid string '{}'", uuidStr);
        throw std::runtime_error(what);
    }

    this->uuid = uuid.value();
    Logging::trace("Node uuid: {}", uuids::to_string(this->uuid));

    // read node secret
    auto secretStr = ConfigManager::get("id.secret", "");
    if(secretStr.empty()) {
        throw std::runtime_error("Node secret (id.secret) is required");
    }

    auto secretDecoded = base64_decode(secretStr);
    if(secretDecoded.empty()) {
        auto what = f("Couldn't decode base64 string '{}'", secretStr);
        throw std::runtime_error(what);
    }
    else if(secretDecoded.size() < kSecretMinLength) {
        auto what = f("Got {} bytes of node secret; expected at least {}",
                secretDecoded.size(), (const size_t)kSecretMinLength);
        throw std::runtime_error(what);
    }

    auto secretBytes = reinterpret_cast<std::byte *>(secretDecoded.data());
    this->secret.assign(secretBytes, secretBytes+secretDecoded.size());

    // get the read timeout
    this->readTimeout = ConfigManager::getTimeval("remote.recv_timeout", 2);
    Logging::trace("Read timeout is {} seconds", this->readTimeout);

    // get server address/port and attempt to resolve
    this->serverV4Only = ConfigManager::getBool("remote.server.ipv4_only", false);

    this->serverHost = ConfigManager::get("remote.server.address", "");
    if(this->serverHost.empty()) {
        throw std::runtime_error("Remote address (remote.server.address) is required");
    }

    this->serverPort = ConfigManager::getUnsigned("remote.server.port", 7420);
    if(this->serverPort > 65535) {
        auto what = f("Invalid remote port {}", this->serverPort);
        throw std::runtime_error(what);
    }

    this->resolve();

    // create the multicast receiver
    this->mcastReceiver = std::make_shared<MulticastReceiver>(this);

    // create the worker thread
    this->run = true;
    this->worker = std::make_unique<std::thread>(&Client::workerMain, this);
}

/**
 * Cleans up client resources. This will waits for the work thread to join
 * before we can exit.
 */
Client::~Client() {
    // we should've had terminate called before
    if(this->run) {
        Logging::error("No call to Client::terminate() before destruction!");
        this->terminate();
    }

    // wait for worker to finish execution
    this->worker->join();

    // we should have terminated it already by this point
    this->mcastReceiver = nullptr;
}

/**
 * Prepared the client for termination by signalling the worker thread.
 */
void Client::terminate() {
    if(!this->run) {
        Logging::error("Ignoring repeated call to Client::terminate()");
        return;
    }

    Logging::debug("Requesting client worker termination");
    this->run = false;
}

/**
 * Tries to resolve the server hostname/address into an IP address that we can
 * connect to.
 */
void Client::resolve() {
    int err;
    struct addrinfo hints, *res = nullptr;

    // provide hints to resolver (if we want IPv4 only or all)
    memset(&hints, 0, sizeof(hints));

    if(this->serverV4Only) {
        hints.ai_family = AF_INET;
    } else {
        hints.ai_family = AF_UNSPEC;
    }
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags |= (AI_ADDRCONFIG | AI_V4MAPPED);

    // attempt to resolve it; then get the first address
    std::string port = f("{}", this->serverPort);

    err = getaddrinfo(this->serverHost.c_str(), port.c_str(), &hints, &res);
    if(err != 0) {
        throw std::system_error(errno, std::generic_category(),
                "getaddrinfo() failed");
    }

    if(!res) {
        auto what = f("Failed to resolve '{}'", this->serverHost);
        throw std::runtime_error(what);
    }

    // just pick the first result
    XASSERT(res->ai_addrlen <= sizeof(this->serverAddr),
            "Invalid address length {}; have space for {}", res->ai_addrlen,
            sizeof(this->serverAddr));

    memcpy(&this->serverAddr, res->ai_addr, res->ai_addrlen);
    this->serverAddrLen = res->ai_addrlen;

    Logging::debug("Resolved '{}' -> '{}'", this->serverHost, this->serverAddr);

    // clean up
done:;
    freeaddrinfo(res);
}



/**
 * Entry point for the client worker thread.
 */
void Client::workerMain() {
    int attempts, err;
    bool success;

    // initialize an SSL context
    this->ctx = SSL_CTX_new(DTLS_client_method());
    SSL_CTX_set_read_ahead(this->ctx, 1);

    // attempt to connect to the server
connect:;
    this->establishConnection();
    Logging::info("Server connection established");

    // register subscriptions for all channels and set up multicast receiver
    this->subscribeChannels();
    this->getMulticastInfo();

    // process messages as long as we're supposed to run
    while(this->run) {
        struct MessageHeader header;
        std::vector<unsigned char> payload;

        try {
            // try to read a message and its payload
            if(!this->readMessage(header, payload)) {
                goto beach;
            }

            // pixel data
            if(header.endpoint == PixelData && 
                    header.messageType == MessageTypes::PixelMessageType::PIX_DATA) {
                this->handlePixelData(header, payload);
            }
            // multicast group keying
            else if(header.endpoint == MulticastControl) {
                this->mcastReceiver->handleMessage(header, payload);
            }
            // unhandled
            else {
                Logging::warn("Unhandled message: type={:x}:{:x} len={}", header.endpoint,
                    header.messageType, header.length);

            }

            // if we need to re-establish connection, do that
            if(this->needsReconnect) {
                this->needsReconnect = false;
                goto connect;
            }
        } catch(std::exception &e) {
            Logging::error("Exception while processing message type {:x}:{:x}: {}",
                    header.endpoint, header.messageType, e.what());

            if(!this->ssl) {
                goto closed;
            }
        }

        // prepare for the next round of processing
beach: ;
    }

closed:;
    // remove subscriptions and multicast
    this->removeSubscriptions();
    this->mcastReceiver->stop();

    // perform clean-up
cleanup: ;
    Logging::debug("Client worker thread is shutting down");

    // attempt to close down the connection
    this->close();

    // clean up the SSL context
    if(this->ctx) {
        SSL_CTX_free(this->ctx);
        this->ctx = nullptr;
    }

    // TODO: this should terminate the program
}

/**
 * Uses our node UUID and shared secret to authenticate the connection.
 *
 * Authentication is implemented by means of a simple state machine. When the function is invoked
 * first, we initialize it and progress through the various states until the node is either
 * successfully authenticated, or an error occurs.
 *
 * @return true if server accepted credentials, false otherwise.
 */
bool Client::authenticate() {
    using namespace MessageTypes;

    uint8_t tag = 0;
    std::string method;

    Header head;
    PayloadType payload;

    // states for the state machine
    enum {
        // Send authentication request
        SEND_REQ,
        // Wait to receive auth request
        READ_REQ_ACK,
        // Perform auth according to selected algorithm and send to server
        SEND_RESPONSE,
        // Await response from server whether auth was a success or not
        READ_AUTH_STATE,
    } state = SEND_REQ;

    // authentication state machine
    while(true) {
        switch(state) {
            // send the auth request
            case SEND_REQ:
                this->authSendRequest(tag);
                state = READ_REQ_ACK;
                break;
            // receive the request ack
            case READ_REQ_ACK: {
                memset(&head, 0, sizeof(head));
                payload.clear();

                if(!this->readMessage(head, payload)) {
                    Logging::error("Failed to read auth message (state {})", state);
                    return false;
                } else if(head.tag != tag ||
                   head.endpoint != MessageEndpoint::Authentication || 
                   head.messageType != AuthMessageType::AUTH_REQUEST_ACK) {
                    Logging::error("Received unexpected message: tag {}, type {:x}:{:x}", head.tag, 
                                   head.endpoint, head.messageType);
                    break;
                }

                // decode the acknowledgement
                AuthRequestAck ack;
                auto ds = bitsery::quickDeserialization(InputAdapter{payload.begin(), payload.size()}, ack);
                if(ds.first != bitsery::ReaderError::NoError || !ds.second) {
                    throw std::runtime_error("failed to deserialize AuthRequestAck");
                }
                if(ack.status != AUTH_SUCCESS) {
                    Logging::error("Authentication failure: {}", ack.status);
                    return false;
                }

                // if success, get the desired method and send response
                method = ack.method;

                Logging::trace("Negotiated auth method: {}", method);
                state = SEND_RESPONSE;
                break;
            }

            // send an authentication response
            case SEND_RESPONSE:
                this->authSendResponse(tag);
                state = READ_AUTH_STATE;
                break;
            // receive the response ack
            case READ_AUTH_STATE: {
                memset(&head, 0, sizeof(head));
                payload.clear();

                if(!this->readMessage(head, payload)) {
                    Logging::error("Failed to read auth message (state {})", state);
                    return false;
                } else if(head.tag != tag ||
                   head.endpoint != MessageEndpoint::Authentication || 
                   head.messageType != AuthMessageType::AUTH_RESPONSE_ACK) {
                    Logging::error("Received unexpected message: tag {}, type {:x}:{:x}", head.tag, 
                                   head.endpoint, head.messageType);
                    break;
                }

                // decode the acknowledgement
                AuthResponseAck ack;
                auto ds = bitsery::quickDeserialization(InputAdapter{payload.begin(), payload.size()}, ack);
                if(ds.first != bitsery::ReaderError::NoError || !ds.second) {
                    throw std::runtime_error("failed to deserialize AuthResponseAck");
                }

                if(ack.status != AUTH_SUCCESS) {
                    Logging::error("Authentication failure: {}", ack.status);
                    return false;
                }

                // if we get here, authentication succeeded
                return true;
            }

            // unexpected state
            default:
                Logging::error("Invalid auth state: {}", state);
                return false;
        }
    }

    // we shouldn't fall down here
    return false;
}

/**
 * Sends the authentication request to the server.
 *
 * @param sentTag Tag of the message sent
 *
 * @throws If an error occurs, an exception is thrown.
 */
void Client::authSendRequest(uint8_t &sentTag) {
    using namespace Lichtenstein::Proto::MessageTypes;

    // build the auth message
    AuthRequest msg;
    memset(&msg, 0, sizeof(msg));

    msg.nodeId = uuids::to_string(this->uuid);
    msg.methods.push_back("me.tseifert.lichtenstein.auth.null");

    // serialize and send
    Buffer payload;
    auto writtenSize = bitsery::quickSerialization(OutputAdapter{payload}, msg);
    payload.resize(writtenSize);

    uint8_t tag = this->nextTag++;
    this->send(MessageEndpoint::Authentication, AuthMessageType::AUTH_REQUEST, tag, payload);
    sentTag = tag;
}

/**
 * Sends to the server the authentication response.
 *
 * This performs the needed computations based on the original data sent by the server to
 * complete authentication.
 *
 * @param sentTag Tag of the message sent
 *
 * @throws If an error occurs, an exception is thrown.
 */
void Client::authSendResponse(uint8_t &sentTag) {
    using namespace Lichtenstein::Proto::MessageTypes;

    // build the auth message
    AuthResponse msg;
    memset(&msg, 0, sizeof(msg));

    // serialize and send
    Buffer payload;
    auto writtenSize = bitsery::quickSerialization(OutputAdapter{payload}, msg);
    payload.resize(writtenSize);

    uint8_t tag = this->nextTag++;
    this->send(MessageEndpoint::Authentication, AuthMessageType::AUTH_RESPONSE, tag, payload);
    sentTag = tag;
}



/**
 * Subscribes for pixel data updates for all loaded channels.
 *
 * If a channel could not complete registration, the entire process fails.
 */
void Client::subscribeChannels() {
    using namespace Lichtenstein::Proto::MessageTypes;
    
    uint8_t tag;
    PixelSubscribe msg;
    memset(&msg, 0, sizeof(msg));

    Header head;
    PayloadType payload;

    auto plugin = Output::PluginManager::shared;
    for(const auto& channel : plugin->channels) {
        // build the subscribe request
        msg.channel = channel->getChannelIndex();
        msg.format = (PixelFormat) channel->getPixelFormat();
        msg.length = channel->getNumPixels();

        switch(channel->getPixelFormat()) {
            case 0:
                msg.format = PIX_FORMAT_RGB;
                break;

            case 1:
                msg.format = PIX_FORMAT_RGBW;
                break;

            default:
                Logging::error("Invalid pixel format for channel {}: {}", msg.channel, 
                        channel->getPixelFormat());
                throw std::runtime_error("Invalid pixel format");
        }

        // send subscription request
        Buffer reqData;
        auto writtenSize = bitsery::quickSerialization(OutputAdapter{reqData}, msg);
        reqData.resize(writtenSize);

        tag = this->nextTag++;
        this->send(MessageEndpoint::PixelData, PixelMessageType::PIX_SUBSCRIBE, tag, reqData);

        // try to receive a response
        if(!this->readMessage(head, payload)) {
            Logging::error("Failed to read subscribe ack message (expected tag {})", tag);
            throw std::runtime_error("Failed to read subscribtion ack message");
        } else if(head.tag != tag ||
           head.endpoint != MessageEndpoint::PixelData || 
           head.messageType != PixelMessageType::PIX_SUBSCRIBE_ACK) {
            Logging::error("Received unexpected message: tag {}, type {:x}:{:x}", head.tag, 
                           head.endpoint, head.messageType);
            throw std::runtime_error("Received unexpected message");
        }

        // decode the acknowledgement
        PixelSubscribeAck ack;
        auto ds = bitsery::quickDeserialization(InputAdapter{payload.begin(), payload.size()}, ack);
        if(ds.first != bitsery::ReaderError::NoError || !ds.second) {
            throw std::runtime_error("failed to deserialize PixelSubscribeAck");
        }

        if(ack.status != PIX_SUCCESS) {
            Logging::error("Subscription failure: {} (for channel {}, length {}, offset {}, format {:x})",
                    ack.status, msg.channel, msg.length, msg.start, msg.format);
            throw std::runtime_error("Failed to subscribe for pixel data");
        }

        // store the identifier if succes
        Logging::trace("Subscription for channel {}: {}", channel->getChannelIndex(),
                ack.subscriptionId);
        this->activeSubscriptions.push_back(std::make_pair(channel->getChannelIndex(),
                    ack.subscriptionId));
    }
}

/**
 * Removes all existing subscriptions.
 *
 * Note that even if an unsubscribe fails, we'll keep going and try to unsubscribe for all the
 * remaining items.
 */
void Client::removeSubscriptions() {
    using namespace Lichtenstein::Proto::MessageTypes;

    uint8_t tag;
    PixelUnsubscribe msg;
    memset(&msg, 0, sizeof(msg));

    Header head;
    PayloadType payload;

    // send an unsubscribe message for each subscription
    for(const auto &info : this->activeSubscriptions) {
        const auto &[channel, token] = info;

        // build unsubscribe request
        msg.channel = channel;
        msg.subscriptionId = token;

        // send unsubscribe request
        Buffer reqData;
        auto writtenSize = bitsery::quickSerialization(OutputAdapter{reqData}, msg);
        reqData.resize(writtenSize);

        tag = this->nextTag++;
        this->send(MessageEndpoint::PixelData, PixelMessageType::PIX_UNSUBSCRIBE, tag, reqData);

        // try to receive a response
        if(!this->readMessage(head, payload)) {
            Logging::error("Failed to read unsubscribe ack message (expected tag {})", tag);
            continue;
        } else if(head.tag != tag ||
           head.endpoint != MessageEndpoint::PixelData || 
           head.messageType != PixelMessageType::PIX_UNSUBSCRIBE_ACK) {
            Logging::error("Received unexpected message: tag {}, type {:x}:{:x}", head.tag, 
                           head.endpoint, head.messageType);
            continue;
        }

        // decode the acknowledgement
        PixelUnsubscribeAck ack;
        auto ds = bitsery::quickDeserialization(InputAdapter{payload.begin(), payload.size()}, ack);
        if(ds.first != bitsery::ReaderError::NoError || !ds.second) {
            throw std::runtime_error("failed to deserialize PixelUnsubscribeAck");
        }

        if(ack.status != PIX_SUCCESS) {
            Logging::error("Failed to unsubscribe: {} (for channel {}, id {:x})", ack.status,
                    msg.channel, msg.subscriptionId);
            continue;
        }

        Logging::trace("Removed {} subscriptions for channel {}", ack.subscriptionsRemoved,
                msg.channel);
    }

    this->activeSubscriptions.clear();
}



/**
 * Handles received pixel data.
 *
 * This will send an acknowledgement back and forward it to the appropriate channel.
 */
void Client::handlePixelData(const Header &hdr, const PayloadType &payload) {
    using namespace Lichtenstein::Proto::MessageTypes;

    PixelDataMessageAck msg;
    memset(&msg, 0, sizeof(msg));

    // decode message
    PixelDataMessage data;
    auto ds = bitsery::quickDeserialization(InputAdapter{payload.begin(), payload.size()}, data);
    if(ds.first != bitsery::ReaderError::NoError || !ds.second) {
        throw std::runtime_error("failed to deserialize PixelDataMessage");
    }

    // send to output channel
    auto plugin = Output::PluginManager::shared;
    if(data.channel >= plugin->channels.size()) {
        throw std::runtime_error("invalid channel number");
    }

    auto outChannel = plugin->channels[data.channel];
    outChannel->updatePixelData(data.offset, data.pixels.data(), data.pixels.size());

    // acknowledge
    msg.channel = data.channel;

    Buffer ackData;
    auto writtenSize = bitsery::quickSerialization(OutputAdapter{ackData}, msg);
    ackData.resize(writtenSize);

    this->reply(hdr, PixelMessageType::PIX_DATA_ACK, ackData);
}



/**
 * Queries the server for the current multicast information.
 */
void Client::getMulticastInfo() {
    using namespace Lichtenstein::Proto::MessageTypes;

    // send the "multicast get info" message
    McastCtrlGetInfo msg;
    memset(&msg, 0, sizeof(msg));

    Buffer reqData;
    auto writtenSize = bitsery::quickSerialization(OutputAdapter{reqData}, msg);
    reqData.resize(writtenSize);

    uint8_t tag = this->nextTag++;
    this->send(MessageEndpoint::MulticastControl, McastCtrlMessageType::MCC_GET_INFO, tag, reqData);

    // wait to receive the multicast info
    while(true) {
        struct MessageHeader header;
        std::vector<unsigned char> payload;

        if(!this->readMessage(header, payload)) {
            continue;
        }

        if(header.endpoint != MessageEndpoint::MulticastControl ||
                header.messageType != McastCtrlMessageType::MCC_GET_INFO_ACK) {
            Logging::error("Unexpected message {:x}:{:x}", header.endpoint, header.messageType);
            continue;
        }

        // parse the response
        McastCtrlGetInfoAck info;
        auto ds = bitsery::quickDeserialization(InputAdapter{payload.begin(), payload.size()}, info);
        if(ds.first != bitsery::ReaderError::NoError || !ds.second) {
            throw std::runtime_error("failed to deserialize McastCtrlGetInfoAck");
        }

        if(info.status != MCC_SUCCESS) {
            Logging::error("Failed to get mcast info: {}", info.status);
            throw std::runtime_error("Failed to get multicast info");
        }

        Logging::debug("Multicast info: group address {} port {} (key id {:x})", info.address,
                info.port, info.keyId);

        // set the address/port number and fire up the multicast receiving
        this->mcastReceiver->setGroupInfo(info.address, info.port, info.keyId);

        // done!
        break;
    }
}
