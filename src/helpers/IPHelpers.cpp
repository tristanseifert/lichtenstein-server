//
// Created by Tristan Seifert on 2019-09-02.
//
#include "IPHelpers.h"

#include <glog/logging.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <cstring>


namespace helpers {
  /**
   * Determines whether the address is the "any interface" address (e.g. whether
   * it is 0.0.0.0 or the IPv6 equivalent thereof)
   *
   * @param address Address string to parse
   * @return If it's the wildcard address or nah
   */
  bool IPHelpers::isWildcardAddress(const std::string &address) {
    bool isWildcard = false;

    // resolve it
    struct addrinfo *resolved = resolveHost(address);
    struct addrinfo *i;

    // there were no addresses
    if(resolved == nullptr) {
      return false;
    }

    // check each one of them
    for(i = resolved; i != nullptr; i = i->ai_next) {
      // is this the IN_ANY address?
      switch(i->ai_family) {
        case AF_INET: {
          sockaddr_in *addr = reinterpret_cast<sockaddr_in *>(i->ai_addr);

          if(addr->sin_addr.s_addr == INADDR_ANY) {
            isWildcard = true;
            goto done;
          }

          break;
        }

        case AF_INET6: {
          sockaddr_in6 *addr = reinterpret_cast<sockaddr_in6 *>(i->ai_addr);
          struct in6_addr any = IN6ADDR_ANY_INIT;

          if(memcmp(&any, &addr->sin6_addr, sizeof(any)) == 0) {
            isWildcard = true;
            goto done;
          }

          break;
        }

        default:
          VLOG(1) << "Ignoring unknown address family " << i->ai_family;
      }
    }

    // clean up
    done:;
    freeaddrinfo(resolved);
    return isWildcard;
  }


  /**
   * Resolves the given hostname and port
   */
  struct addrinfo *IPHelpers::resolveHost(const std::string &host) {
    int err;

    struct addrinfo hints{}, *out;
    memset(&hints, 0, sizeof(hints));

    // configure hints when resolving
    hints.ai_family = AF_UNSPEC;
//    hints.ai_socktype = SOCK_DGRAM;

    // try to resolve hostname
    const char *hostname = host.c_str();

    err = getaddrinfo(hostname, nullptr, &hints, &out);

    if(err != 0) {
      throw std::system_error(errno, std::system_category(),
                              "error resolving hostname");
    }

    return out;
  }
}