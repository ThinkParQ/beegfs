#ifndef CURL_WRAPPER_H_
#define CURL_WRAPPER_H_

#include <common/threading/Mutex.h>

#include <curl/curl.h>

#include <chrono>
#include <mutex>
#include <unordered_map>


class CurlWrapper
{
   public:
      CurlWrapper(std::chrono::milliseconds timeout, bool checkSSLCertificates);

      CurlWrapper(const CurlWrapper&) = delete;
      CurlWrapper& operator=(const CurlWrapper&) = delete;
      CurlWrapper(CurlWrapper&&) = delete;
      CurlWrapper& operator=(CurlWrapper&&) = delete;

      ~CurlWrapper() = default;

      void enableHttpAuth(const std::string& user, const std::string& password);

      typedef std::unordered_map<std::string, std::string> ParameterMap;

      unsigned short sendGetRequest(const std::string& url,
            const ParameterMap& parameters);
      unsigned short sendPostRequest(const std::string& url, const char* data,
            const ParameterMap& parameters, const std::vector<std::string>& headers);

      static size_t writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata);

   protected:
      std::unique_ptr<CURL, void(*)(void*)> curlHandle;
      std::string response;

      char errorBuffer[CURL_ERROR_SIZE];

      std::string makeParameterStr(const ParameterMap& parameters) const;

      void setResponse(const std::string& response)
      {
         this->response = response;
      }

   public:
      const std::string& getResponse() const
      {
         return response;
      }

};

#endif
