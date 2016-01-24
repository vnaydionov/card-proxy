// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "http_post.h"
#include <curl/curl.h>

HttpClientError::HttpClientError(const std::string &msg):
    runtime_error(msg)
{}

static int writer(char *data, size_t size, size_t nmemb,
                  std::string *writerData)
{
    if (writerData == NULL)
        return 0;
    writerData->append(data, size*nmemb);
    return size * nmemb;
}

const std::string http_post(const std::string &uri,
    const HttpParams &params,
    double timeout,
    const std::string &method)
{
    CURL *curl = NULL;
    std::string buffer;
    try {
        curl = curl_easy_init();
        if (!curl)
            throw HttpClientError("curl_easy_init() failed");
        std::string fixed_uri = uri;
        std::string body;
        auto i = params.begin(), iend = params.end();
        for (bool item0 = true; i != iend; ++i, item0 = false) {
            if (!item0)
                body += "&";
            char *output = curl_easy_escape(curl, i->first.c_str(), i->first.size());
            if (output) {
                body += output;
                curl_free(output);
            }
            body += "=";
            output = curl_easy_escape(curl, i->second.c_str(), i->second.size());
            if (output) {
                body += output;
                curl_free(output);
            }
        }
        if (method != "POST") {
            if (body.find('?') == std::string::npos)
                fixed_uri += "?" + body;
            else
                fixed_uri += "&" + body;
            body = "";
        }
        CURLcode res = curl_easy_setopt(curl, CURLOPT_URL, fixed_uri.c_str());
        if (res != CURLE_OK)
            throw HttpClientError(
                "curl_easy_setopt(curl, CURLOPT_URL, uri) failed: " +
                std::string(curl_easy_strerror(res)));
        if (method == "POST") {
            res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            if (res != CURLE_OK)
                throw HttpClientError(
                    "curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body) failed: " +
                    std::string(curl_easy_strerror(res)));
        }
        // curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        if (timeout > 0)
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)timeout);
        /* Set body extractor */
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        /* Check for errors */
        if (res != CURLE_OK)
            throw HttpClientError(
                "curl_easy_perform(\"" + uri + "\") failed: " +
                std::string(curl_easy_strerror(res)));
        curl_easy_cleanup(curl);
    }
    catch (...) {
        if (curl)
            curl_easy_cleanup(curl);
        throw;
    }
    return buffer;
}

// vim:ts=4:sts=4:sw=4:et:

