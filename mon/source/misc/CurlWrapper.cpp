#include "CurlWrapper.h"

#include <exception/CurlException.h>

CurlWrapper::CurlWrapper(std::chrono::milliseconds timeout, bool checkSSLCertificates) :
   curlHandle(curl_easy_init(), &curl_easy_cleanup)
{
   if (curlHandle.get() == NULL)
      throw CurlException("Curl init failed.");

   if (curl_easy_setopt(curlHandle.get(), CURLOPT_ERRORBUFFER, &errorBuffer) != CURLE_OK)
      throw CurlException("Setting Curl error buffer failed.");

   if (curl_easy_setopt(curlHandle.get(), CURLOPT_NOSIGNAL, 1L) != CURLE_OK)
      throw CurlException(errorBuffer);

   if (curl_easy_setopt(curlHandle.get(), CURLOPT_TIMEOUT_MS,
         std::chrono::milliseconds(timeout).count()) != CURLE_OK)
      throw CurlException(errorBuffer);

   if (curl_easy_setopt(curlHandle.get(), CURLOPT_WRITEFUNCTION, writeCallback) != CURLE_OK)
      throw CurlException(errorBuffer);

   if (curl_easy_setopt(curlHandle.get(), CURLOPT_WRITEDATA, static_cast<void*>(this)) != CURLE_OK)
      throw CurlException(errorBuffer);

   if (curl_easy_setopt(curlHandle.get(), CURLOPT_CONNECTTIMEOUT_MS,
         timeout.count()) != CURLE_OK)
      throw CurlException(errorBuffer);

   if (!checkSSLCertificates)
   {
      if (curl_easy_setopt(curlHandle.get(), CURLOPT_SSL_VERIFYPEER, 0) != CURLE_OK)
         throw CurlException(errorBuffer);

      if (curl_easy_setopt(curlHandle.get(), CURLOPT_SSL_VERIFYHOST, 0) != CURLE_OK)
         throw CurlException(errorBuffer);
   }
}

void CurlWrapper::enableHttpAuth(const std::string& user, const std::string& password)
{
   if (curl_easy_setopt(curlHandle.get(), CURLOPT_HTTPAUTH, CURLAUTH_ANY))
      throw CurlException(errorBuffer);

   if (curl_easy_setopt(curlHandle.get(), CURLOPT_USERNAME, user.c_str()))
      throw CurlException(errorBuffer);

   if (curl_easy_setopt(curlHandle.get(), CURLOPT_PASSWORD, password.c_str()))
      throw CurlException(errorBuffer);
}


unsigned short CurlWrapper::sendGetRequest(const std::string& url, const ParameterMap& parameters)
{
   std::string parameterStr = makeParameterStr(parameters);

   if (curl_easy_setopt(curlHandle.get(), CURLOPT_URL, (url + parameterStr).c_str()) != CURLE_OK)
      throw CurlException(errorBuffer);

   if (curl_easy_setopt(curlHandle.get(), CURLOPT_HTTPGET, 1L) != CURLE_OK)
      throw CurlException(errorBuffer);

   // replace with curl_multi_perform?
   if (curl_easy_perform(curlHandle.get()) != CURLE_OK)
      throw CurlException(errorBuffer);

   long responseCode;
   if (curl_easy_getinfo(curlHandle.get(), CURLINFO_RESPONSE_CODE, &responseCode) != CURLE_OK)
      throw CurlException(errorBuffer);

   return responseCode;
}

unsigned short CurlWrapper::sendPostRequest(const std::string& url, const char* data,
      const ParameterMap& parameters, const std::vector<std::string>& headers)
{
   std::string parameterStr = makeParameterStr(parameters);

   if (curl_easy_setopt(curlHandle.get(), CURLOPT_URL, (url + parameterStr).c_str()) != CURLE_OK)
      throw CurlException(errorBuffer);

   if (curl_easy_setopt(curlHandle.get(), CURLOPT_POSTFIELDS, data) != CURLE_OK)
      throw CurlException(errorBuffer);

   struct curl_slist* headerList = nullptr;
   for (const auto& header : headers) {
      headerList = curl_slist_append(headerList, header.c_str());
   }

   if (curl_easy_setopt(curlHandle.get(), CURLOPT_HTTPHEADER, headerList) != CURLE_OK)
      throw CurlException(errorBuffer);

   // replace with curl_multi_perform?
   if (curl_easy_perform(curlHandle.get()) != CURLE_OK)
      throw CurlException(errorBuffer);

   long responseCode;
   if (curl_easy_getinfo(curlHandle.get(), CURLINFO_RESPONSE_CODE, &responseCode) != CURLE_OK)
      throw CurlException(errorBuffer);

   return responseCode;
}

std::string CurlWrapper::makeParameterStr(const ParameterMap& parameters) const
{
   if (!parameters.empty())
   {
      std::string parameterStr = "?";

      for (auto iter = parameters.begin(); iter != parameters.end(); iter++)
      {
         {
            auto escaped = std::unique_ptr<char, void(*)(void*)> (
                  curl_easy_escape(curlHandle.get(), (iter->first).c_str(),0),
                  &curl_free);

            if (!escaped)
               throw CurlException(errorBuffer);

            parameterStr += escaped.get();
         }

         {
            auto escaped = std::unique_ptr<char, void(*)(void*)> (
                  curl_easy_escape(curlHandle.get(), (iter->second).c_str(),0),
                  &curl_free);

            if (!escaped)
               throw CurlException(errorBuffer);

            parameterStr += "=";
            parameterStr += escaped.get();
            parameterStr += "&";
         }
      }

      parameterStr.resize(parameterStr.size() - 1);

      return parameterStr;
   }

   return {};
}

size_t CurlWrapper::writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
   auto instance = static_cast<CurlWrapper*>(userdata);
   instance->setResponse(std::string(ptr, size*nmemb));

   // Always signal success
   return size*nmemb;
}
