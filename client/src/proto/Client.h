/**
 * Lichtenstein protocol client; this handles the persistent DTLS connection to
 * the server, authenticating the connection, and acting as a mux for messages
 * between different endpoints.
 */
#ifndef PROTO_CLIENT_H
#define PROTO_CLIENT_H

#include <proto/WireMessage.h>

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <atomic>
#include <memory>
#include <thread>

#include <sys/socket.h>
#include <sys/time.h>

#include <openssl/ssl.h>
#include <uuid.h>

#include "proto/WireMessage.h"
#include "proto/lichtenstein_v1.capnp.h"

namespace Lichtenstein::Client::Proto {
    class Client {
        using Header = struct Lichtenstein::Proto::MessageHeader;
        using PayloadType = std::vector<std::byte>;
        using MessageEndpoint = Lichtenstein::Proto::MessageEndpoint;
        using MsgReader = Lichtenstein::Proto::WireTypes::Message::Reader;

        public:
            // don't call these, it is a Shared Instance(tm)
            Client();
            virtual ~Client();

        public:
            void terminate();

        public:
            static void start();
            static void stop();

            static std::shared_ptr<Client> get() {
                return shared;
            }


        private:
            void resolve();

            void workerMain();

            void establishConnection();
            bool setUpSocket();
            bool setUpSsl();
            bool authenticate();

            void close();
            size_t bytesAvailable();
            size_t write(const PayloadType &);
            size_t read(void *, size_t);

            bool readHeader(Header &);
            void readPayload(const Header &, PayloadType &);

        public:
            bool readMessage(Header &, PayloadType &);

            MsgReader decode(const PayloadType &);
            void send(MessageEndpoint, uint8_t, const PayloadType &);

            /**
             * Replies to an incoming message. This copies the type and tag
             * values from the provided header, but sends the bytes unmodified.
             */
            void reply(const Header &hdr,
                    const PayloadType &data) {
                this->send(hdr.type, hdr.tag, data);
            }

        private:
            uuids::uuid uuid;
            std::vector<std::byte> secret;

            bool serverV4Only = false;
            std::string serverHost;
            unsigned int serverPort;

            size_t serverAddrLen;
            struct sockaddr_storage serverAddr;

            struct timeval readTimeout;

            std::atomic_bool run;
            std::unique_ptr<std::thread> worker = nullptr;

            // connection to server 
            int sock = -1;
            // SSL context for connection
            SSL_CTX *ctx = nullptr;
            // Current SSL instance
            SSL *ssl = nullptr;
            // IO abstraction around socket for OpenSSL
            BIO *bio = nullptr;

            // whether we should try to cleanly shut down SSL connection
            bool sslShutdown = true;

        private:
            static std::shared_ptr<Client> shared;

        private:
            // number of times we retry connecting before giving up
            static const size_t kConnectionAttempts = 10;
            // minmum length of node secret
            static const size_t kSecretMinLength = 16;

        public:
            class SSLError: public std::runtime_error {
                public:
                    SSLError() = delete;
                    SSLError(const std::string what);

                    virtual const char* what() const noexcept {
                        return this->whatStr.c_str();
                    }

                    static void getSslErrors(std::string &);

                private:
                    std::string whatStr;
            };
    };
}

#endif