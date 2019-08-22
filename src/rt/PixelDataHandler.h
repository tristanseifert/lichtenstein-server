//
// Created by Tristan Seifert on 2019-08-21.
//

#ifndef LICHTENSTEIN_SERVER_RT_PIXELDATAHANDLER_H
#define LICHTENSTEIN_SERVER_RT_PIXELDATAHANDLER_H

#include <functional>
#include <mutex>
#include <tuple>
#include <unordered_map>
#include <map>
#include <stdexcept>
#include <string>

class DbChannel;

namespace rt {
  /**
   * Clients may register through this class to receive callbacks when new pixel
   * data is available for a given channel.
   *
   * Upon registration, a token is returned that can be used to later remove
   * that callback.
   */
  class PixelDataHandler {
    public:
      class InvalidTokenError : public std::runtime_error {
        public:
          explicit InvalidTokenError(const char *what) : std::runtime_error(
                  what) {}
      };

    public:
      typedef enum {
        RGB,
        RGBW
      } PixelFormat;

    public:
      using dataCallback = std::function<void(const DbChannel &,
                                              const std::vector<std::byte> &data,
                                              PixelFormat format)>;

    public:
      PixelDataHandler() = delete;

      ~PixelDataHandler() = delete;

    public:
      static int
      registerHandler(DbChannel &channel, const dataCallback &callback);

      static void updateHandler(int token, const dataCallback &callback);

      static void removeHandler(int token);

      static void
      receivedData(DbChannel &channel, const void *data, const size_t len,
                   PixelFormat format);

    private:
      static void updateCallbackMap();

      static std::string dumpCallbackMap();

    private:
      // type of an entry in the callback directory
      using callbackDirValType = std::tuple<DbChannel, dataCallback>;
      /// protects access to the callback directory
      static std::mutex callbackDirLock;
      /// callback directory
      static std::unordered_map<int, callbackDirValType> callbackDir;

      /// type of an entry in the callback list
      using callbackListValType = dataCallback;
      /// protects accesses to the callback list
      static std::mutex callbackListLock;
      /// callback list
      static std::multimap<DbChannel, callbackListValType> callbackList;
  };
}

#endif //LICHTENSTEIN_SERVER_RT_PIXELDATAHANDLER_H
