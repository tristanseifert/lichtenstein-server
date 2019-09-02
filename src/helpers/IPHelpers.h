//
// Created by Tristan Seifert on 2019-09-02.
//

#ifndef LICHTENSTEIN_SERVER_HELPERS_IPHELPERS_H
#define LICHTENSTEIN_SERVER_HELPERS_IPHELPERS_H

#include <string>

#include <netdb.h>

namespace helpers {
  /**
   * Provides some helper functions for dealing with IP addresses
   */
  class IPHelpers {
    public:
      IPHelpers() = delete;

      ~IPHelpers() = delete;

    public:
      static bool isWildcardAddress(const std::string &address);


      static struct addrinfo *resolveHost(const std::string &host);
  };
}


#endif //LICHTENSTEIN_SERVER_HELPERS_IPHELPERS_H
