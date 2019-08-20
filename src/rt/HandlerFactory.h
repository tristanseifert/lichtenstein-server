//
// Created by Tristan Seifert on 2019-08-20.
//

#ifndef LICHTENSTEIN_SERVER_RT_HANDLERFACTORY_H
#define LICHTENSTEIN_SERVER_RT_HANDLERFACTORY_H

#include <string>
#include <memory>
#include <map>


namespace rt {
  class IRequestHandler;

  class ClientHandler;

  /**
   * The handler factory contains a registry of all API handlers for the
   * realtime API
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


#endif //LICHTENSTEIN_SERVER_RT_HANDLERFACTORY_H
