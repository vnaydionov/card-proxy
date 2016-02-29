// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "http_post.h"
#include <util/string_utils.h>
#include <curl/curl.h>

#define LOG_DEBUG(s) do{ if (logger) logger->debug(s); }while(0)
#define LOG_INFO(s) do{ if (logger) logger->info(s); }while(0)
#define LOG_ERROR(s) do{ if (logger) logger->error(s); }while(0)

HttpClientError::HttpClientError(const std::string &msg):
    runtime_error(msg)
{}

static size_t writer(char *data, size_t size, size_t nmemb,
                     std::string *writerData)
{
    if (writerData == NULL)
        return 0;
    writerData->append(data, size*nmemb);
    return size * nmemb;
}

static size_t header_callback(char *data, size_t size, size_t nmemb,
                              HttpHeaders *headersData)
{
    using Yb::StrUtils::trim_trailing_space;
    using Yb::StrUtils::split_str_by_chars;

    if (headersData == NULL)
        return 0;
    std::string header(data, size*nmemb);
    size_t colon_pos = header.find(':');
    if (colon_pos == std::string::npos) {
        std::vector<std::string> parts;
        split_str_by_chars(header, " ", parts, 3);
        if (parts.size() == 3)
            (*headersData)["X-ReasonPhrase"] = trim_trailing_space(parts[2]);
        return size * nmemb;
    }
    std::string header_name = header.substr(0, colon_pos);
    std::string header_value = header.substr(colon_pos + 1);
    (*headersData)[trim_trailing_space(header_name)]
            = trim_trailing_space(header_value);
    return size * nmemb;
}

static const std::string escape(CURL *curl, const std::string &s)
{
    std::string result;
    char *output = curl_easy_escape(curl, s.c_str(), s.size());
    if (output) {
        result = output;
        curl_free(output);
    }
    return result;
}

static const std::string serialize_params(CURL *curl, const HttpParams &params)
{
    std::string result;
    auto i = params.begin(), iend = params.end();
    for (bool first = true; i != iend; ++i, first = false) {
        if (!first)
            result += "&";
        result += escape(curl, i->first) + "=" + escape(curl, i->second);
    }
    return result;
}

static curl_slist *fill_headers(CURL *curl, const HttpHeaders &headers,
                                int content_length)
{
    using Yb::StrUtils::str_to_lower;

    if (!headers.size())
        return NULL;
    curl_slist *hlist = NULL;
    try {
        auto i = headers.begin(), iend = headers.end();
        for (; i != iend; ++i) {
            if (str_to_lower(i->first) == "content-length")
                continue;
            curl_slist *new_hlist = curl_slist_append(
                hlist,
                (i->first + ": " + i->second).c_str()
            );
            if (!new_hlist)
                throw HttpClientError("curl_slist_append() failed!");
            hlist = new_hlist;
        }
        if (content_length) {
            curl_slist *new_hlist = curl_slist_append(
                hlist,
                ("Content-Length: " + Yb::to_string(content_length)).c_str()
            );
            if (!new_hlist)
                throw HttpClientError("curl_slist_append() failed!");
            hlist = new_hlist;
        }
        return hlist;
    }
    catch (...) {
        if (hlist)
            curl_slist_free_all(hlist);
        throw;
    }
}

const HttpResponse http_post(const std::string &uri,
    Yb::ILogger *logger,
    double timeout,
    const std::string &method,
    const HttpHeaders &headers,
    const HttpParams &params,
    const std::string &body,
    bool ssl_validate,
    const std::string &client_cer,
    const std::string &client_key)
{
    CURL *curl = NULL;
    curl_slist *hlist = NULL;
    long http_code = 0;
    HttpHeaders out_headers;
    std::string result_buffer;

    try {
        curl = curl_easy_init();
        if (!curl)
            throw HttpClientError("curl_easy_init() failed");
        LOG_INFO("method: " + method + ", uri: " + uri);

        // calculate and set the URI
        std::string params_str = serialize_params(curl, params);
        LOG_DEBUG("sending parameters: " + params_str);
        std::string fixed_uri = uri;
        if (!params_str.empty() && method == "GET") {
            if (fixed_uri.find('?') == std::string::npos)
                fixed_uri += "?" + params_str;
            else
                fixed_uri += "&" + params_str;
        }
        CURLcode res = curl_easy_setopt(curl, CURLOPT_URL, fixed_uri.c_str());
        if (res != CURLE_OK)
            throw HttpClientError(
                "curl_easy_setopt(curl, CURLOPT_URL, uri) failed: " +
                std::string(curl_easy_strerror(res)));

        // set the body if necessary
        std::string request_body = body;
        if (request_body.empty() && !params_str.empty() && method == "POST") {
            request_body = params_str;
        }
        int content_length = request_body.size();
        if (content_length) {
            res = curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, request_body.c_str());
            if (res != CURLE_OK)
                throw HttpClientError(
                    "curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, body) failed: " +
                    std::string(curl_easy_strerror(res)));
        }

        // set custom headers if necessary
        if (headers.size() || content_length) {
            hlist = fill_headers(curl, headers, content_length);
            res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hlist);
            if (res != CURLE_OK)
                throw HttpClientError(
                    "curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hlist) failed: " +
                    std::string(curl_easy_strerror(res)));
        }

        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);

        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        if (timeout > 0)
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)timeout);

        // set client certificate
        if (!client_cer.empty()) {
            curl_easy_setopt(curl, CURLOPT_SSLCERT, client_cer.c_str());
            curl_easy_setopt(curl, CURLOPT_SSLKEY, client_key.c_str());
        }

        if (!ssl_validate) {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        }

        // set headers extractor function
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &out_headers);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);

        // set body extractor function
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result_buffer);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);

        // perform the request, res will get the return code
        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
            throw HttpClientError(
                "curl_easy_perform(\"" + uri + "\") failed: " +
                std::string(curl_easy_strerror(res)));

        // extract HTTP code
        res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (res != CURLE_OK)
            throw HttpClientError(
                "curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, ...) failed: " +
                std::string(curl_easy_strerror(res)));

        // clean up
        if (hlist)
            curl_slist_free_all(hlist);
        curl_easy_cleanup(curl);
    }
    catch (...) {
        // clean up on exception
        if (hlist)
            curl_slist_free_all(hlist);
        if (curl)
            curl_easy_cleanup(curl);
        throw;
    }
    std::string reason_phrase = "SomeDesc";
    auto rp = out_headers.find("X-ReasonPhrase");
    if (out_headers.end() != rp) {
        reason_phrase = rp->second;
        out_headers.erase(rp);
    }
    LOG_INFO("HTTP " + Yb::to_string(http_code) +
             " " + reason_phrase + " (body size: " +
             Yb::to_string(result_buffer.size()) +
             ")");
    LOG_DEBUG("response body: " + result_buffer);
    return boost::make_tuple(http_code, reason_phrase, result_buffer, out_headers);
}

// vim:ts=4:sts=4:sw=4:et:

