//
// Created by Tristan Seifert on 2019-08-21.
//
#include "PixelDataHandler.h"

#include "db/Node.h"
#include "db/Channel.h"

#include <glog/logging.h>

#include <random>
#include <sstream>
#include <iomanip>


namespace rt {
  std::mutex PixelDataHandler::callbackDirLock;
  std::unordered_map<int, PixelDataHandler::callbackDirValType> PixelDataHandler::callbackDir;

  std::mutex PixelDataHandler::callbackListLock;
  std::multimap<DbChannel, PixelDataHandler::callbackListValType> PixelDataHandler::callbackList;

  /**
   * Registers a new pixel data handler.
   *
   * @param channel Channel to watch for output data
   * @param callback Callback to execute
   * @return Registration token for this callback, to remove it later
   */
  int PixelDataHandler::registerHandler(DbChannel &channel,
                                        const dataCallback &callback) {
    int token = 0;

    std::random_device dev;
    std::mt19937 rng(dev());

    // generate a random value to use as the token
    bool tokenUnique = false;

    do {
      // generate a token
      token = rng();

      // check whether it exists in the callback directory
      {
        std::lock_guard lock(callbackDirLock);
        tokenUnique = (callbackDir.count(token) == 0);
      }
    } while(!tokenUnique);

    // insert it into our callback map, then update callbacks and return
    {
      std::lock_guard lock(callbackDirLock);
      callbackDir.insert({token, {channel, callback}});
    }

    updateCallbackMap();

    return token;
  }

  /**
   * Changes the callback for the given handler.
   *
   * @param token Callback token returned by registerHandler()
   * @param callback New callback to use
   */
  void
  PixelDataHandler::updateHandler(int token, const dataCallback &callback) {
    // take lock and try to find the callback info
    std::lock_guard lock(callbackDirLock);
    auto callbackInfo = callbackDir.find(token);

    if(callbackInfo == callbackDir.end()) {
      // it could not be found
      std::stringstream error;

      error << "Failed updating handler, the token was not found. ";
      error << "Registered handlers:" << std::endl;
      error << dumpCallbackMap();

      throw InvalidTokenError(error.str().c_str());
    }

    // change its value
    std::get<1>(callbackInfo->second) = callback;
  }

  /**
   * Removes an existing callback.
   *
   * @param token Callback token
   */
  void PixelDataHandler::removeHandler(int token) {
    // remove the handler if we have it
    {
      std::lock_guard lock(callbackDirLock);

      if(callbackDir.count(token) == 0) {
        std::stringstream error;

        error << "There is no handler with that token registered. ";
        error << "Registered handlers:" << std::endl;
        error << dumpCallbackMap();

        throw InvalidTokenError(error.str().c_str());
      }

      callbackDir.erase(token);
    }

    // update callback map
    updateCallbackMap();
  }


  /**
   * Converts the token -> tuple<channel, callback> into the channel ->
   * vector<callback> format used by the ingestion function.
   *
   * @note It's important neither of the two locks are taken when entering this
   * function, or it will wait forever.
   */
  void PixelDataHandler::updateCallbackMap() {
    // acquire both locks
    std::lock_guard dirLock(callbackDirLock);
    std::lock_guard listLock(callbackListLock);

    // empty the callback list
    callbackList.clear();

    // iterate over the callback directory
    for(auto const &[token, value] : callbackDir) {
      auto[channel, callback] = value;
      callbackList.insert({channel, callback});
    }
  }

  /**
   * Called by external code when new data for a particular channel is available.
   *
   * @param channel Channel for which this data is
   * @param data Data pointer
   * @param len Number of bytes
   * @param format Pixel data format
   */
  void PixelDataHandler::receivedData(DbChannel &channel, const void *data,
                                      const size_t len, PixelFormat format) {
    // create a vector of data
    auto *charBuffer = reinterpret_cast<const std::byte *>(data);
    std::vector<std::byte> dataVect(charBuffer, charBuffer + len);

    // invoke all the callbacks
    auto range = callbackList.equal_range(channel);

    for(auto callback = range.first; callback != range.second; ++callback) {
      callback->second(channel, dataVect, format);
    }
  }


  /**
   * As a debugging aide, this dumps the callback map.
   */
  std::string PixelDataHandler::dumpCallbackMap() {
    std::stringstream stream;

    stream << std::setw(8) << "Token";
    stream << std::setw(8) << "ChanId";
    stream << std::setw(8) << "Callback" << std::endl;

    // write all
    for(auto const &[token, value] : callbackDir) {
      auto[channel, callback] = value;

      stream << std::setw(8) << std::hex << token;
      stream << std::setw(8) << std::dec << channel.getId();
      stream << std::setw(8) << std::hex << &callback << std::endl;
    }

    // done
    return stream.str();
  }
}