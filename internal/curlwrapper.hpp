// snapbox
// HTTP Client Library for Jule
// https://github.com/adamperkowski/snapbox
// Copyright (c) 2025, Adam Perkowski
// BSD 3-Clause License

#ifndef CURLWRAPPER_HPP
#define CURLWRAPPER_HPP

#include <memory>
#include <string>
#include <curl/curl.h>

using FilePtr = std::unique_ptr<FILE, decltype(&fclose)>;
using CurlPtr = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
using CurlSlist = std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>;

static FilePtr openFile(const char *path, const char *mode) {
    return FilePtr{fopen(path, mode), &fclose};
}

static CurlPtr initCurl() {
    return CurlPtr{curl_easy_init(), &curl_easy_cleanup};
}

static struct curl_slist* sliceToSlist(const __jule_Slice<__jule_Str> &headersSlice) {
    struct curl_slist* headers = NULL;
    const int headersLen = headersSlice.len();
    for (size_t i = 0; i < headersLen; i += 2) {
        if (i + 1 < headersLen) {
            const std::string header{headersSlice[i] + ": " + headersSlice[i + 1]};
            // header.c_str() is cloned, so you don't have to worry about the lifetime here.
            headers = curl_slist_append(headers, header.c_str());
        }
    }
    return headers;
}

static CurlSlist initHeadersList(const __jule_Slice<__jule_Str> &headers) {
    return CurlSlist{sliceToSlist(headers), &curl_slist_free_all};
}

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

static size_t WriteFileCallback(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    return fwrite(ptr, size, nmemb, stream);
}

struct Response {
    __jule_Str body;
    __jule_Int status;
};

static Response request(const char *url, const __jule_Slice<__jule_Str> &headers, const __jule_Int method) {
    CURLcode res;
    std::string readBuffer;

    Response response;
    CurlSlist headersList = initHeadersList(headers);

    CurlPtr curl = initCurl();
    if (!curl) {
        response.status = 418;
        return response;
    }

    curl_easy_setopt(curl.get(), CURLOPT_URL, url);
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headersList.get());

    if (method == 0) { // GET
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &readBuffer);
    } else if (method == 1) { // HEAD
        curl_easy_setopt(curl.get(), CURLOPT_NOBODY, 1L);
    }

    res = curl_easy_perform(curl.get());
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response.status);

    if (method == 0) { // GET
        response.body = readBuffer;
    }

    return response;
}

static Response post(const char *url, const char *data, const __jule_Slice<__jule_Str> &headers, const __jule_Int method) {
    CURLcode res;
    std::string readBuffer;

    Response response;
    CurlSlist headersList = initHeadersList(headers);

    CurlPtr curl = initCurl();
    if (!curl) {
        response.status = 418;
        return response;
    }

    curl_easy_setopt(curl.get(), CURLOPT_URL, url);
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headersList.get());

    if (method == 0) { // POST
        curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
    } else if (method == 1) { // PUT
        curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, "PUT");
    } else if (method == 2) { // DELETE
        curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, "DELETE");
    }

    if (data) {
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, data);
    }
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &readBuffer);

    res = curl_easy_perform(curl.get());
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response.status);

    response.body = readBuffer;

    return response;
}

static bool download(__jule_Str url, __jule_Str filename) {
    CurlPtr curl = initCurl();
    if (!curl) {
        return false;
    }

    FilePtr file{openFile(filename, "wb")};
    if (!file) {
        return false;
    }

    // curl_easy_setopt is a macro, so implicit conversion don't happen.
    curl_easy_setopt(curl.get(), CURLOPT_URL, (const char*)url);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, WriteFileCallback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, file.get());

    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);

    curl_easy_perform(curl.get());

    return true;
}

#endif // CURLWRAPPER_HPP
