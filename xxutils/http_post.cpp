// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "http_post.h"
#include "app_class.h"
#include "tcp_socket.h"
#include <util/string_utils.h>
#include <curl/curl.h>
#include <curl/multi.h>

#define LOG_DEBUG(s) do{ if (logger) logger->debug(s); }while(0)
#define LOG_INFO(s) do{ if (logger) logger->info(s); }while(0)
#define LOG_ERROR(s) do{ if (logger) logger->error(s); }while(0)

const HttpResponse mk_http_resp(int http_code, const std::string &http_desc,
                                HttpHeaders &headers,
                                const std::string &body)
{
    HttpResponse response(HTTP_1_0, http_code, http_desc);
    response.set_headers(headers);
    response.set_body(body);
    return response;
}


HttpClientError::HttpClientError(const std::string &msg):
    runtime_error(msg)
{}

static size_t store_body(char *data, size_t size, size_t nmemb,
                         HttpResponse *writerData)
{
    if (writerData == NULL)
        return 0;
    writerData->put_body_piece(std::string(data, size * nmemb));
    return size * nmemb;
}

static size_t store_header(char *data, size_t size, size_t nmemb,
                           HttpResponse *writerData)
{
    if (writerData == NULL)
        return 0;
    writerData->put_header_line(std::string(data, size * nmemb));
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

static curl_slist *fill_headers(Yb::ILogger *logger,
                                CURL *curl, const HttpHeaders &headers,
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
            LOG_DEBUG("send header: " + i->first + ": " + i->second);
            curl_slist *new_hlist = curl_slist_append(
                hlist,
                (i->first + ": " + i->second).c_str()
            );
            if (!new_hlist)
                throw HttpClientError("curl_slist_append() failed!");
            hlist = new_hlist;
        }
        if (content_length) {
            LOG_DEBUG("pass header: Content-Length: " + Yb::to_string(content_length));
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
    Yb::ILogger *outer_logger,
    double timeout,
    const std::string &method,
    const HttpHeaders &headers,
    const HttpParams &params,
    const std::string &body,
    bool ssl_validate,
    const std::string &client_cer,
    const std::string &client_key,
    bool dump_headers,
    const FiltersMap &filters)
{
    CURL *curl = NULL;
    CURLM *mcurl = NULL;
    curl_slist *hlist = NULL;
    CURLcode res;
    CURLMcode mres;
    long http_code = 0;
    HttpResponse response(HTTP_X, 0, "");

    Yb::ILogger::Ptr logger_holder(
            outer_logger?
            (HTTP_POST_NO_LOGGER == outer_logger? NULL:
             outer_logger->new_logger("http_post").release()):
            theApp::instance().new_logger("http_post").release());
    Yb::ILogger *logger = logger_holder.get();

    try {
        curl = curl_easy_init();
        if (!curl)
            throw HttpClientError("curl_easy_init() failed");
        mcurl = curl_multi_init();
        if (!mcurl)
            throw HttpClientError("curl_multi_init() failed");
        curl_multi_add_handle(mcurl, curl);

        LOG_INFO("method: " + method + ", uri: " + uri);

        // calculate and set the URI
        std::string params_dump = dict2str(params, filters);
        std::string params_str = serialize_params(curl, params);
        LOG_DEBUG("sending parameters: " + params_dump);
        std::string fixed_uri = uri;
        if (!params_str.empty() && method == "GET") {
            if (fixed_uri.find('?') == std::string::npos)
                fixed_uri += "?" + params_str;
            else
                fixed_uri += "&" + params_str;
            params_str.clear();
        }
        res = curl_easy_setopt(curl, CURLOPT_URL, fixed_uri.c_str());
        if (res != CURLE_OK)
            throw HttpClientError(
                "curl_easy_setopt(curl, CURLOPT_URL, uri) failed: " +
                std::string(curl_easy_strerror(res)));

        // set the body if necessary
        std::string request_body = body;
        if (!params_str.empty() && method == "POST") {
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
            hlist = fill_headers(dump_headers? logger: NULL,
                                 curl, headers, content_length);
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
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, store_header);

        // set body extractor function
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, store_body);

#if 0
        // perform the request, res will get the return code
        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
            throw HttpClientError(
                "curl_easy_perform(\"" + uri + "\") failed: " +
                std::string(curl_easy_strerror(res)));
#else
        Yb::MilliSec start_ts = Yb::get_cur_time_millisec();
        int num_handles = 1;

        while (1) {
            mres = curl_multi_perform(mcurl, &num_handles);
            if (mres != CURLM_CALL_MULTI_PERFORM)
                break;
        }

        int iter_count = 1, no_data_count = 0;
        while (num_handles) {
            /* get file descriptors from the transfers */
            int maxfd = -1;
            fd_set fdread, fdwrite, fdexcep;
            FD_ZERO(&fdread);
            FD_ZERO(&fdwrite);
            FD_ZERO(&fdexcep);
            curl_multi_fdset(mcurl, &fdread, &fdwrite, &fdexcep, &maxfd);

            /* increasing timeouts */
            int e = iter_count - 1;
            if (e > 8)
                e = 8;
            Yb::MilliSec select_timeout = 1 << (e * 2);
            if (timeout > 0) {
                Yb::MilliSec time_left =
                    start_ts + (long)timeout - Yb::get_cur_time_millisec();
                if (select_timeout > time_left && time_left > 10)
                    select_timeout = time_left;
            }
            if (select_timeout > 1000)
                select_timeout = 1000;
            else if (select_timeout < 1)
                select_timeout = 1;
            //LOG_DEBUG("select_timeout=" + Yb::to_string(select_timeout));

            struct timeval timeout_struct;
            timeout_struct.tv_sec = select_timeout / 1000;
            timeout_struct.tv_usec = 1000 * (select_timeout % 1000);

            int rc = select(
                maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout_struct);
            if (rc == -1) {
                throw HttpClientError("select() failed");
            }
            if (rc == 0)
                no_data_count++;
            else
                no_data_count = 0;
            if (no_data_count > 1) {
                int delay = 20;
                //LOG_DEBUG("sleeping for " + Yb::to_string(delay) + " millisec");
                sleep_msec(delay);
            }

            while (1) {
                mres = curl_multi_perform(mcurl, &num_handles);
                if (mres != CURLM_CALL_MULTI_PERFORM)
                    break;
            }

            ++iter_count;
        }

        //LOG_DEBUG("iter_count=" + Yb::to_string(iter_count));
#endif
        /* call curl_multi_perform or curl_multi_socket_action first, then loop
           through and check if there are any transfers that have completed */

        struct CURLMsg *m;
        do {
            int msgq = 0;
            m = curl_multi_info_read(mcurl, &msgq);
            if (m && (m->msg == CURLMSG_DONE)) {
                if (m->data.result != CURLE_OK)
                    throw HttpClientError(
                        "curl failed: " + Yb::to_string(m->data.result) +
                        ", " + std::string(
                            curl_easy_strerror(m->data.result)));
                break;
            }
        } while (m);

        // extract HTTP code
        res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (res != CURLE_OK)
            throw HttpClientError(
                "curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, ...) failed: " +
                std::string(curl_easy_strerror(res)));

        // clean up
        if (hlist)
            curl_slist_free_all(hlist);
        hlist = NULL;
        curl_multi_remove_handle(mcurl, curl);
        curl_multi_cleanup(mcurl);
        mcurl = NULL;
        curl_easy_cleanup(curl);
        curl = NULL;
    }
    catch (...) {
        // clean up on exception
        if (hlist)
            curl_slist_free_all(hlist);
        hlist = NULL;
        if (mcurl) {
            curl_multi_remove_handle(mcurl, curl);
            curl_multi_cleanup(mcurl);
        }
        mcurl = NULL;
        if (curl)
            curl_easy_cleanup(curl);
        curl = NULL;
        throw;
    }

    LOG_INFO("HTTP " + Yb::to_string(response.resp_code()) +
             " " + response.resp_desc() + " (body size: " +
             Yb::to_string(response.body().size()) +
             ")");
    if (dump_headers) {
        const Yb::StringDict &out_headers = response.headers();
        for (auto i = out_headers.begin(); i != out_headers.end(); ++i)
            LOG_DEBUG("recv header: " + i->first + ": " + i->second);
    }
    LOG_DEBUG("response body: " + response.body());
    return response;
}

// vim:ts=4:sts=4:sw=4:et:

