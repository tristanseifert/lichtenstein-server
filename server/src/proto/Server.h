/**
 * Implements the Lichtenstein protocol server, which network nodes would
 * connect to in order to receive pixel data.
 *
 * The protocol is fully defined by Cap'n Proto messages that are serialized
 * and sent over a DTLS secured connection. Nodes additionally must
 * authenticate themselves at the start of the connection.
 */
#ifndef PROTO_SERVER_H
#define PROTO_SERVER_H

#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>

#include <sys/socket.h>

#include <openssl/ssl.h>

namespace Lichtenstein::Server::Proto {
    class ServerWorker;

    class Server {
        using WorkerPtr = std::shared_ptr<ServerWorker>;

        public:
            static void start();
            static void stop();

            // ya shouldn't be calling these ya hear!!!
            Server();
            ~Server();

        public:
            /**
             * Returns the global protocol server instance.
             */
            static std::shared_ptr<Server> shared() {
                return sharedInstance;
            }

        private:
            void terminate();

            void initWorker();
            void initDtls();
            void openSocket();
            void loadCert();

            void main();
            void garbageCollectClients();

        // client accept and authentication
        private:
            WorkerPtr acceptClient();
            void connect(int, const struct sockaddr_storage &);
            WorkerPtr newClient(int, const struct sockaddr_storage &, SSL *);

        // certificate reading and verification callbacks
        private:
            int getKeyPasswd(char *, int, int, void *);
            static void setEchoEnable(bool);

        // DTLS cookie callbacks
        private:
            int dtlsCookieNewCb(SSL *, unsigned char *, unsigned int *);
            int dtlsCookieVerifyCb(SSL *, const unsigned char *, unsigned int);

            void dtlsCookieMake(SSL *, unsigned char *, unsigned int *);

        private:
            static std::shared_ptr<Server> sharedInstance;

            // listening thread
            std::atomic_bool shouldTerminate;
            std::unique_ptr<std::thread> worker = nullptr;

            // all accepted clients
            std::mutex clientsLock;
            std::vector<WorkerPtr> clients;
            // clients that have terminated that need to be deallocated
            std::mutex finishedClientsLock;
            std::vector<WorkerPtr> finishedClients;

            // listening socket 
            int socket = -1;

            // accept timeout
            double acceptTimeout = 2.5;
            // read timeout for clients
            double clientReadTimeout = 0.3;

            // SSL context
            SSL_CTX *ctx = nullptr;

            // random data used to generate DTLS cookies
            static const size_t kCookieSecretLen = 16;

            std::atomic_bool cookieSecretValid = false;
            unsigned char cookieSecret[kCookieSecretLen];

            // name/path of the file we're currently reading in
            std::string currentInFile;

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
};

#endif
