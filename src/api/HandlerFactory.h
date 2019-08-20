//
// Created by Tristan Seifert on 2019-08-20.
//

#ifndef LICHTENSTEIN_SERVER_HANDLERFACTORY_H
#define LICHTENSTEIN_SERVER_HANDLERFACTORY_H

#include <memory>
#include <string>
#include <map>

namespace api {
  class IRequestHandler;

  class ClientHandler;

  /**
   * The handler factory contains a registry of all API handlers
   */
  class HandlerFactory {
    public:
      using createMethod = std::unique_ptr<IRequestHandler>(*)(ClientHandler *);

    public:
      HandlerFactory() = delete;

    public:
      static bool
      registerClass(const std::string type, createMethod funcCreate);

      static std::unique_ptr<IRequestHandler>
      create(const std::string &type, ClientHandler *client);

      static void dump();

    private:
      static std::map<std::string, createMethod> *registrations;
  };
}


#endif //LICHTENSTEIN_SERVER_HANDLERFACTORY_H
