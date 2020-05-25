#include "Server.h"
#include "ServerWorker.h"
#include "SocketTypes+fmt.h"

#include "Logging.h"
#include "ConfigManager.h"
#include "db/DataStore.h"

#include <cstring>
#include <stdexcept>
#include <functional>
#include <algorithm>
#include <iostream>
#include <cmath>

#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <fmt/format.h>
#include <spdlog/fmt/bin_to_hex.h>

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/opensslv.h>

using namespace Lichtenstein::Server::Proto;

std::shared_ptr<Server> Server::sharedInstance = nullptr;

/**
 * Starts the server by allocating the shared instance.
 */
void Server::start() {
    XASSERT(!sharedInstance, "Protocol server already running? ({})", 
            (void*)sharedInstance.get());

    // initialize SSL
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    // now, allocate the server
    sharedInstance = std::make_shared<Server>();
}

/**
 * Terminates the server.
 */
void Server::stop() {
    XASSERT(sharedInstance, "Expected protocol server to be running");

    sharedInstance->terminate();
    sharedInstance = nullptr;
}



/**
 * Initializes the protocol server. This will set up the DTLS context and
 * listening thread.
 */
Server::Server() {
    // perform initialization
    this->initDtls();
    this->loadCert();
    this->openSocket();

    // now that most initialization is done, we can launch the worker thread
    this->initWorker();
}

/**
 * Cleans up any resources allocated by the server, including its listening
 * thread and all client connections.
 */
Server::~Server() {
    // ensure the worker gets the terminate request
    if(!this->shouldTerminate) {
        Logging::error("You should call Proto::Server::terminate() before dealloc");
        this->terminate();
    }

    // signal the clients to terminate
    Logging::debug("Signaling {} client handlers to terminate",
            this->clients.size());
    
    {
        std::lock_guard lg(this->clientsLock);
        for(auto client : this->clients) {
            client->signalShutdown();
        }
    }

    // wait on the worker thread to exit
    this->worker->join();
}



/**
 * Terminates the server.
 */
void Server::terminate() {
    if(this->shouldTerminate) {
        Logging::error("Ignoring repeated call to Proto::Server::terminate()");
        return;
    }

    // set the termination flag
    Logging::debug("Requesting protocol server termination");
    this->shouldTerminate = true;
}



/**
 * Sets up the server's worker thread.
 */
void Server::initWorker() {
    this->shouldTerminate = false;
    this->worker = std::make_unique<std::thread>(&Server::main, this);
}

/**
 * Creates the DTLS context.
 */
void Server::initDtls() {
    // attempt to create a DTLS socket
    const SSL_METHOD *method;
    method = DTLS_server_method();

    this->ctx = SSL_CTX_new(method);
    if(!this->ctx) {
        throw std::runtime_error("SSL_CTX_new() failed");
    }

    // generate DTLS cookie secret
    XASSERT(!this->cookieSecretValid, "DTLS cookie secret is unexpectedly set");

    if(!RAND_bytes(this->cookieSecret, kCookieSecretLen)) {
        throw std::runtime_error("Failed to generate DTLS cookie secret");
    }

    this->cookieSecretValid = true;

    // allow readahead
    SSL_CTX_set_read_ahead(this->ctx, 1);

    // configure the DTLS cookie callbacks
    SSL_CTX_set_cookie_generate_cb(this->ctx, [](auto ssl, auto out, auto outLen) {
        return sharedInstance->dtlsCookieNewCb(ssl, out, outLen);
    });
    
    SSL_CTX_set_cookie_verify_cb(this->ctx, [](auto ssl, auto in, auto inLen) {
        return sharedInstance->dtlsCookieVerifyCb(ssl, in, inLen);
    });

    // configure cipher suites list if config is specified
    auto ciphers = ConfigManager::get("server.tls.cipher_list", "");

    if(ciphers.size()) {
        Logging::info("Using custom cipher suites: {}", ciphers);
        int err = SSL_CTX_set_cipher_list(this->ctx, ciphers.c_str());

        if(err == 0) {
            auto what = fmt::format("Failed to set cipher list '{}'", ciphers);
            throw SSLError(what);
        }
    }

    // configure the passphrase callback for encrypted keys
    SSL_CTX_set_default_passwd_cb(this->ctx, [](auto buf, auto size, auto rwflag, auto ud) {
        return sharedInstance->getKeyPasswd(buf, size, rwflag, ud);
    });
}

/**
 * Opens the listening socket.
 */
void Server::openSocket() {
    int err;
    const int on = 1;
    const int off = 0;
    bool wantDualStack = false;

    // load the config
    auto port = ConfigManager::getUnsigned("server.listen.port", 7420);
    auto listenAddr = ConfigManager::get("server.listen.address", "");

    if(port > 65535) {
        auto what = fmt::format("Invalid protocol server port: {}", port);
        throw std::runtime_error(what);
    }

    // try to parse the listen address
    struct sockaddr_storage servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    if(listenAddr.size()) {
        auto *addr4 = reinterpret_cast<struct sockaddr_in *>(&servaddr);
        auto *addr6 = reinterpret_cast<struct sockaddr_in6 *>(&servaddr);

        // can we parse it as an IPv4 address?
        err = inet_pton(AF_INET, listenAddr.c_str(), &addr4->sin_addr);

        if(err == 1) {
            addr4->sin_family = AF_INET;
            addr4->sin_port = htons(port);

            goto open;
        } else if(err == -1) {
            throw std::system_error(errno, std::generic_category(), 
                    "inet_pton(AF_INET) failed");
        }

        // if not, try to parse as an IPv6 address
        err = inet_pton(AF_INET6, listenAddr.c_str(), &addr6->sin6_addr);

        if(err == 1) {
            addr6->sin6_family = AF_INET6;
            addr6->sin6_port = htons(port);

            goto open;
        } else if(err == -1) {
            throw std::system_error(errno, std::generic_category(), 
                    "inet_pton(AF_INET6) failed");
        } else {
            auto what = fmt::format("Failed to parse listen address '{}'", 
                    listenAddr);
            throw std::runtime_error(what);
        }
    } else {
        auto *addr = reinterpret_cast<struct sockaddr_in6 *>(&servaddr);
        // listen on any interface
        addr->sin6_family = AF_INET6;
        addr->sin6_addr = in6addr_any;
        addr->sin6_port = htons(port);

        // we want a dual stack socket
        wantDualStack = true;
    }

    // allocate a socket
open: ;
    this->socket = ::socket(servaddr.ss_family, SOCK_DGRAM, 0);
    if(this->socket == -1) {
        throw std::system_error(errno, std::generic_category(), 
                "Failed to create protocol listening socket");
    }

    err = setsockopt(this->socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if(err < 0) {
        throw std::system_error(errno, std::generic_category(), 
                "Failed to set SO_REUSEADDR on protocol listening socket");
    }

#if defined(SO_REUSEPORT) && !defined(__linux__)
    err = setsockopt(this->socket, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
    if(err < 0) {
        throw std::system_error(errno, std::generic_category(), 
                "Failed to set SO_REUSEPORT on protocol listening socket");
    }
#endif

    // for an IPv6 "any" socket, disable the IPv6 only flag
    if(wantDualStack) {
        err = setsockopt(this->socket, IPPROTO_IPV6, IPV6_V6ONLY, &off, 
                sizeof(off));

        if(err < 0) {
            throw std::system_error(errno, std::generic_category(), 
                    "Failed to clear IPV6_V6ONLY on protocol listening socket");
        }
    }

    // set the port number and bind to the address
    if(servaddr.ss_family == AF_INET) {
        err = bind(this->socket, 
                reinterpret_cast<const struct sockaddr *>(&servaddr), 
                sizeof(sockaddr_in));
    } else if(servaddr.ss_family == AF_INET6) {
        err = bind(this->socket, 
                reinterpret_cast<const struct sockaddr *>(&servaddr), 
                sizeof(sockaddr_in6));
    }

    if(err != 0) {
        throw std::system_error(errno, std::generic_category(), 
                "Failed to bind protocol socket");
    }

    Logging::info("Protocol server is listening on {}", servaddr);

    // read some more config info
    this->acceptTimeout = ConfigManager::getDouble("server.accept_timeout", 2.5);
    Logging::trace("Accept timeout is {} seconds", this->acceptTimeout);
    
    this->clientReadTimeout = ConfigManager::getDouble("server.read_timeout", 0.3);
    Logging::trace("Client read timeout is {} seconds", this->clientReadTimeout);
}

/**
 * Initializes the server certificate. This must be done on the main thread, as
 * it will block if the key is protected with a passphrase.
 */
void Server::loadCert() {
    int err;

    // get the cert paths
    auto certPath = ConfigManager::get("server.tls.cert_path", "");
    auto keyPath = ConfigManager::get("server.tls.key_path", "");

    if(certPath == "" || keyPath == "") {
        throw std::runtime_error("server.tls.cert_path and server.tls.key_path must be specified");
    }

    // load the certificate
    this->currentInFile = certPath;
    SSL_CTX_set_default_passwd_cb_userdata(this->ctx, 
            (void*)this->currentInFile.c_str());

    err = SSL_CTX_use_certificate_file(this->ctx, certPath.c_str(), 
            SSL_FILETYPE_PEM);
    if(err <= 0) {
        auto what = fmt::format("Failed to read cert from '{}'", certPath);
        throw SSLError(what);
    }

    // try to load the private key
    this->currentInFile = keyPath;
    SSL_CTX_set_default_passwd_cb_userdata(this->ctx, 
            (void*)this->currentInFile.c_str());

    err = SSL_CTX_use_PrivateKey_file(this->ctx, keyPath.c_str(), 
            SSL_FILETYPE_PEM);
    if(err <= 0) {
        auto what = fmt::format("Failed to read key from '{}'", keyPath);
        throw SSLError(what);
    }

    // verify the private key
    err = SSL_CTX_check_private_key(this->ctx);

    if(err != 1) {
        auto what = fmt::format("Key '{}' does not jive with cert '{}'", 
                keyPath, certPath);
        throw SSLError(what);
    }

    // clear out the default password userdata pointer (name)
    SSL_CTX_set_default_passwd_cb_userdata(this->ctx, nullptr);
}



/*
 * Listening thread entry point; this will continuously accept new clients for
 * until termination is requested. When the thread is launched, most of the
 * initialization should have been completed.
 */
void Server::main() {
    Logging::debug("Waiting for protocol client connections");

    // accept clients until termination is requested
    while(!this->shouldTerminate) {
        try {
            auto client = this->acceptClient();
            if(!client) {
                // ignore null return values
                goto beach;
            }

            // add the clients to our internal bookkeeping
            this->clientsLock.lock();
            this->clients.push_back(client);
            this->clientsLock.unlock();
        } catch(std::exception &e) {
            Logging::error("Exception while handling client: {}", e.what());
        }

beach: ;
        // garbage collect old clients
        this->garbageCollectClients();
    }

    Logging::debug("Protocol handler is shutting down");

    // close out any clients
    this->clientsLock.lock();
    Logging::debug("Closing {} client handlers", this->clients.size());
    this->clients.clear();
    this->clientsLock.unlock();

    // close the listening socket and TLS context
    SSL_CTX_free(this->ctx);
    this->ctx = nullptr;
   
    if(this->socket > 0) {
        int err = close(this->socket);
        XASSERT(!err, "Failed to close protocol socket: {}", errno);
        this->socket = -1;
    }
}
/**
 * Attempts to accept a client on the DTLS socket.
 */
Server::WorkerPtr Server::acceptClient() {
    WorkerPtr outWorker = nullptr;
    int err;
    int clientSock = -1;

    const int on = 1;

    struct timeval timeout;
    memset(&timeout, 0, sizeof(timeout));

    struct sockaddr_storage clientAddr;
    memset(&clientAddr, 0, sizeof(clientAddr));

    // get a BIO with a read timeout to use for this accept call
    BIO *bio = BIO_new_dgram(this->socket, BIO_NOCLOSE);
    if(!bio) {
        throw SSLError("BIO_new_dgram() failed");
    }

    double fraction, whole;
    fraction = modf(this->acceptTimeout, &whole);

    timeout.tv_sec = whole;
    timeout.tv_usec = (fraction * 1000 * 1000);
    BIO_ctrl(bio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);

    // create a new SSL context to accept the connection into
    SSL *ssl = SSL_new(this->ctx);
    if(!ssl) {
        throw SSLError("SSL_new() failed");
    }

    SSL_set_bio(ssl, bio, bio);
    SSL_set_options(ssl, SSL_OP_COOKIE_EXCHANGE);

    // listen for incoming requests and create a socket for new connections
    do {
        err = DTLSv1_listen(ssl, &clientAddr);

        // exit immediately if termination is requested
        if(this->shouldTerminate) {
            goto cleanup;
        }

        // for non-fatal errors, log the cause
        if(err == 0) {
            std::string errStr;
            SSLError::getSslErrors(errStr);

            Logging::warn("DTLSv1_listen() non-fatal error: {}", errStr);
        } else if(err < 0) {
            // ignore "fatal" errors
            // the accept handler is idle so garbage collect old workers
            this->garbageCollectClients();
        }
    } while(err != 1);

    clientSock = ::socket(clientAddr.ss_family, SOCK_DGRAM, 0);
    if(clientSock < 0) {
        throw std::system_error(errno, std::generic_category(),
                "Failed to create client socket");
    }

    err = setsockopt(clientSock, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
    if(err < 0) {
        throw std::system_error(errno, std::generic_category(), 
                "Failed to set SO_REUSEPORT");
    }

    // connect socket and attempt to complete the SSL handshake
    try {
        // connect the socket…
        this->connect(clientSock, clientAddr);
        BIO_set_fd(bio, clientSock, BIO_CLOSE);
        BIO_ctrl(bio, BIO_CTRL_DGRAM_SET_CONNECTED, 0, &clientAddr);

        // …then attempt the SSL handshake
        do {
            err = SSL_accept(ssl);

            // if success, create a client
            if(err == 1) {
                return this->newClient(clientSock, clientAddr, ssl);
            } 
            // log the cause for errors
            else if(err != 0) {
                std::string errStr;
                SSLError::getSslErrors(errStr);

                Logging::error("SSL_accept() = {}: {}", err, errStr);
            }
        } while(err == 0);
    } catch(std::exception &e) {
        // close client socket, ignoring errors
        close(clientSock);

        auto what = fmt::format("Failed to accept client (from {})", clientAddr);
        std::throw_with_nested(std::runtime_error(what));
    }

cleanup: ;
    if(clientSock > 0) {
        close(clientSock);
    }

    // this clears the BIO as well
    SSL_free(ssl);
    ssl = nullptr;

    return outWorker;
}
/**
 * Connects the client socket. This will bind it to the interface on which the
 * connection was received, as well as connecting it to the remote address so
 * that datagrams are associated properly.
 */
void Server::connect(int clientSock, const struct sockaddr_storage &clientAddr) {
    int err;

    const int off = 0;

    // get the server's address
    struct sockaddr_storage serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));

    socklen_t serverAddrLen = sizeof(serverAddr);

    err = getsockname(this->socket, 
            reinterpret_cast<struct sockaddr *>(&serverAddr), &serverAddrLen);
    if(err == -1) {
        throw std::system_error(errno, std::system_category(),
                "Failed to get server socket address");
    }

    // bind the addresses based on address family
    switch(clientAddr.ss_family) {
        // IPv4 address
        case AF_INET: {
            auto server = reinterpret_cast<const struct sockaddr *>(&serverAddr);
            auto client = reinterpret_cast<const struct sockaddr *>(&clientAddr);

            err = bind(clientSock, server, sizeof(struct sockaddr_in));
            if(err == -1) {
                throw std::system_error(errno, std::system_category(),
                        "Failed to bind IPv4 address");
            }
            
            err = ::connect(clientSock, client, sizeof(struct sockaddr_in));
            if(err == -1) {
                throw std::system_error(errno, std::system_category(),
                        "Failed to connect to IPv4 address");
            }
            break;
        }

        // IPv6 address
        case AF_INET6: {
            auto server = reinterpret_cast<const struct sockaddr *>(&serverAddr);
            auto client = reinterpret_cast<const struct sockaddr *>(&clientAddr);

            // disable the "IPv6 only" option
            err = setsockopt(clientSock, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
            if(err < 0) {
                throw std::system_error(errno, std::generic_category(), 
                        "Failed to clear IPV6_V6ONLY on client socket");

            }

            err = bind(clientSock, server, sizeof(struct sockaddr_in6));
            if(err == -1) {
                throw std::system_error(errno, std::system_category(),
                        "Failed to bind IPv6 address");
            }
            
            err = ::connect(clientSock, client, sizeof(struct sockaddr_in6));
            if(err == -1) {
                throw std::system_error(errno, std::system_category(),
                        "Failed to connect to IPv6 address");
            }
            break;
        }
        default: {
            auto what = fmt::format("Unknown address family {} for client socket {}", 
                    clientAddr.ss_family, clientSock);
            throw std::runtime_error(what);
        }
    }
}
/**
 * Creates a new client worker. The provided client has successfully
 * established the DTLS connection at this point.
 */
Server::WorkerPtr Server::newClient(int clientSock, 
        const struct sockaddr_storage &clientAddr, SSL *ssl) {
    // configure the BIO read timeout
    struct timeval timeout;
    memset(&timeout, 0, sizeof(timeout));

    double fraction, whole;
    fraction = modf(this->clientReadTimeout, &whole);

    timeout.tv_sec = whole;
    timeout.tv_usec = (fraction * 1000 * 1000);

    BIO *bio = SSL_get_rbio(ssl);
    if(!bio) {
        throw std::runtime_error("Failed to get client read BIO");
    }

    BIO_ctrl(bio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);

    // create the client
    auto client = std::make_shared<ServerWorker>(clientSock, clientAddr, ssl);

    client->addShutdownHandler([&](int cause) {
        Logging::trace("Client {} exiting: {}", (void*)client.get(), cause);

        // on terminations not initiated by us, garbage collect it
        if((cause & 0x8000) == 0x8000) {
            std::lock_guard lg(this->finishedClientsLock);
            this->finishedClients.push_back(client);
        }
    });

    return client;
}



/**
 * Queries for the key password on the standard output.
 */
int Server::getKeyPasswd(char *buf, int bufLen, int writing, void *ud) {
    auto name = reinterpret_cast<const char *>(ud);
    std::cout << fmt::format("Enter passphrase for '{}': ", name);

    std::string pass;

    // collect passphrase with echo disabled
    setEchoEnable(false);
    std::cin >> pass;
    setEchoEnable(true);

    // this is necessary since the enter after the key isn't echoed
    std::cout << std::endl;

    if(bufLen < pass.size()) {
        Logging::error("Key passphrase truncated! (buf = {}, in = {})", 
                bufLen, pass.size());
    }

    strncpy(buf, pass.c_str(), bufLen);
    return pass.size();
}
/**
 * Controls whether echoing to stdout is enabled by the terminal. This is used
 * to suppress echo when the password is entered.
 */
void Server::setEchoEnable(bool enable) {
    int err;

    struct termios tty;
    err = tcgetattr(STDIN_FILENO, &tty);
    XASSERT(err == 0, "Failed to get terminal attributes: {}", errno);

    if(enable) {
        tty.c_lflag |= ECHO;
    } else {
        tty.c_lflag &= ~ECHO;
    }

    err = tcsetattr(STDIN_FILENO, TCSAFLUSH, &tty);
    XASSERT(err == 0, "Failed to set echo state ({}): {}", enable, errno);
}



/**
 * Wraps a call around our internal DTLS cookie calculation function; this is
 * called for every new DTLS connection.
 */
int Server::dtlsCookieNewCb(SSL *ssl, unsigned char *cookie,
        unsigned int *cookieLen) {
    // calculate the expected cookie
    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int resultLen;

    this->dtlsCookieMake(ssl, result, &resultLen);

    // then copy it out
    memcpy(cookie, result, resultLen);
    *cookieLen = resultLen;

    return resultLen;
}

/**
 * Verifies a DTLS connection cookie. Currently, this just re-generates the
 * expected cookie and compares it.
 */
int Server::dtlsCookieVerifyCb(SSL *ssl, const unsigned char *cookie,
        unsigned int cookieLen) {
    // calculate the expected cookie
    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int resultLen;

    this->dtlsCookieMake(ssl, result, &resultLen);

    // compare it to what we got (TODO: timing-independent compare?)
    if(cookieLen == resultLen && memcmp(result, cookie, resultLen) == 0) {
        return 1;
    }

    // we got an invalid cooke :O
    auto dump = spdlog::to_hex(cookie, cookie+cookieLen);
    Logging::error("DTLS cookie failed HMAC ({})", dump);

    return 0;
}

/**
 * Generates a DTLS cookie for the given client. The cookie contains some
 * random bytes, as well as the client's address.
 */
void Server::dtlsCookieMake(SSL *ssl, unsigned char *result, 
        unsigned int *resultLen) {
    size_t length = 0;
    unsigned char *buffer = nullptr;

    // get the client address
    union {
        struct sockaddr_storage ss;
        struct sockaddr_in6 s6;
        struct sockaddr_in s4;
    } peer;
    memset(&peer, 0, sizeof(peer));

    BIO_dgram_get_peer(SSL_get_rbio(ssl), &peer);

    // calculate how big the buffer needs to be
    length = 0;
    switch(peer.ss.ss_family) {
        case AF_INET:
            length += sizeof(struct in_addr);
            break;

        case AF_INET6:
            length += sizeof(struct in6_addr);
            break;

        default:
            auto what = fmt::format("Unexpected address family {}", peer.ss.ss_family);
            throw std::runtime_error(what);
    }

    length += sizeof(in_port_t);

    buffer = static_cast<unsigned char *>(OPENSSL_malloc(length));
    if(!buffer) {
        throw std::runtime_error("Failed to allocate DTLS cookie buffer");
    }

    // copy the IP address and port into the buffer
    switch (peer.ss.ss_family) {
        case AF_INET: {
            auto ptr = buffer;

            memcpy(ptr, &peer.s4.sin_port, sizeof(in_port_t));
            ptr += sizeof(in_port_t);

            memcpy(ptr, &peer.s4.sin_addr, sizeof(struct in_addr));
            break;
        }

        case AF_INET6: {
            auto ptr = buffer;

            memcpy(ptr, &peer.s6.sin6_port, sizeof(in_port_t));
            ptr += sizeof(in_port_t);

            memcpy(ptr, &peer.s6.sin6_addr, sizeof(struct in6_addr));
            break;
        }
    }

    // then, using our secret random bytes, calculate a HMAC
    auto res = HMAC(EVP_sha1(), static_cast<const void *>(this->cookieSecret), 
            kCookieSecretLen, static_cast<const unsigned char *>(buffer), 
            length, result, resultLen);
    OPENSSL_free(buffer);

    if(!res) {
        throw std::runtime_error("Failed to calculate DTLS cookie HMAC");
    }
}

/**
 * Removes any clients that have recently terminated from the clients list.
 */
void Server::garbageCollectClients() {
    // acquire the lock on the recently flinished clients and iterate over it
    std::lock_guard finishedLg(this->finishedClientsLock);

    if(this->finishedClients.empty()) {
        return;
    }

    Logging::debug("Garbage collecting {} clients", 
            this->finishedClients.size());

    for(auto it = this->finishedClients.begin(); 
            it != this->finishedClients.end();) {
        // skip the client if it is not yet done executing
        if(!((*it)->isDone())) {
            Logging::warn("Skipping client handler {}, not yet done", (void*) it->get());
            ++it;
            continue;
        }

        // try to remove this one from the clients list
        {
            std::lock_guard clientsLg(this->clientsLock);
            this->clients.erase(std::remove(this->clients.begin(), 
                        this->clients.end(), *it), this->clients.end());
        }

        // remove it from the "to remove" list
        it = this->finishedClients.erase(it);
    }
}



/**
 * Initializes a new SSL error object. This includes the ssl library error
 * string alongside the `what` description.
 */
Server::SSLError::SSLError(const std::string what) : 
    std::runtime_error("SSL error") {
    // format our internal string
    std::string libErr;
    getSslErrors(libErr);

    this->whatStr = fmt::format("{}: {}", what, libErr);
}

/**
 * Gets the error string from the SSL library
 */
void Server::SSLError::getSslErrors(std::string &out) {
    // print errors into a memory BIO and get a buffer ptr
    BIO *bio = BIO_new(BIO_s_mem());
    ERR_print_errors(bio);

    char *buf = nullptr;
    size_t len = BIO_get_mem_data(bio, &buf);

    // strip the last newline from the string
    out = std::string(buf, len);
    out.erase(std::find_if(out.rbegin(), out.rend(), [](int ch) {
        return !std::iscntrl(ch);
    }).base(), out.end());

    // clean up
    BIO_free(bio);
}
