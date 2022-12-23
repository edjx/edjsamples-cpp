#include <cstdint>
#include <string>
#include <vector>

#include <edjx/logger.hpp>
#include <edjx/error.hpp>
#include <edjx/http.hpp>
#include <edjx/request.hpp>
#include <edjx/response.hpp>
#include <edjx/fetch.hpp>
#include <edjx/utils.hpp>
#include <edjx/stream.hpp>

using edjx::logger::info;
using edjx::logger::error;
using edjx::error::HttpError;
using edjx::error::StreamError;
using edjx::http::Uri;
using edjx::http::HttpMethod;
using edjx::http::HttpStatusCode;
using edjx::request::HttpRequest;
using edjx::response::HttpResponse;
using edjx::fetch::HttpFetch;
using edjx::fetch::FetchResponse;
using edjx::stream::ReadStream;
using edjx::stream::WriteStream;

static const HttpStatusCode HTTP_STATUS_OK = 200;
static const HttpStatusCode HTTP_STATUS_INTERNAL_SERVER_ERROR = 500;

inline std::vector<uint8_t> to_bytes(const std::string & str) {
    std::vector<uint8_t> bytes;
    for (auto c : str) {
        bytes.push_back(static_cast<uint8_t>(c));
    }
    return bytes;
}

bool serverless_streaming(HttpRequest & req) {
    // URL to fetch from
    Uri fetch_uri("https://norvig.com/big.txt");

    // Send an HTTP fetch request
    FetchResponse fetch_res;
    HttpError err = HttpFetch(fetch_uri, HttpMethod::GET).send(fetch_res);
    if (err != HttpError::Success) {
        error("Error in HTTP fetch: " + to_string(err));
        HttpResponse("Error in HTTP fetch: {}" + to_string(err))
            .set_status(HTTP_STATUS_INTERNAL_SERVER_ERROR)
            .set_header("Serverless", "EDJX")
            .send();
        return false;
    }

    // Get a read stream from the fetch response
    ReadStream read_stream = fetch_res.get_read_stream();

    // Prepare an HTTP response
    HttpResponse res;
    res.set_status(HTTP_STATUS_OK);
    res.set_header("Serverless", "EDJX");

    // Open a write stream for the HTTP response
    WriteStream write_stream;
    HttpError http_err = res.send_streaming(write_stream);
    if (http_err != HttpError::Success) {
        error("Error when opening a write stream: " + to_string(http_err));
        read_stream.close();
        return false;
    }

    // Pipe the fetch response stream into the HTTP response stream
    StreamError stream_err = read_stream.pipe_to(write_stream);
    if (stream_err == StreamError::Success) {
        info("Successfully executed pipe");
    } else {
        error("Error when piping: " + to_string(stream_err));
        write_stream.abort();
        return false;
    }

    // No need to call close() on streams because the pipe_to() method
    // automatically closes both the read stream and the write stream.

    return true;
}