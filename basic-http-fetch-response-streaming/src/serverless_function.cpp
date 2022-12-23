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

static const HttpStatusCode HTTP_STATUS_OK = 200;
static const HttpStatusCode HTTP_STATUS_INTERNAL_SERVER_ERROR = 500;

inline std::vector<uint8_t> to_bytes(const std::string & str) {
    std::vector<uint8_t> bytes;
    for (auto c : str) {
        bytes.push_back(static_cast<uint8_t>(c));
    }
    return bytes;
}

HttpResponse serverless(const HttpRequest & req) {
    info("** Read Stream Example **");

    std::vector<uint8_t> received_data;
    int chunk_count = 0;

    // URL to fetch from
    Uri fetch_uri("https://norvig.com/big.txt");

    // Send an HTTP fetch request
    FetchResponse fetch_res;
    HttpError err = HttpFetch(fetch_uri, HttpMethod::GET).send(fetch_res);
    if (err != HttpError::Success) {
        return HttpResponse("Failure in HTTP Fetch send(): " + to_string(err))
            .set_status(HTTP_STATUS_INTERNAL_SERVER_ERROR)
            .set_header("Serverless", "EDJX");
    }

    // Open a read stream
    ReadStream read_stream = fetch_res.get_read_stream();

    // Read the fetch response body chunk by chunk
    std::vector<uint8_t> chunk;
    StreamError stream_err;
    while ((stream_err = read_stream.read_chunk(chunk)) == StreamError::Success) {
        // Record the chunks
        received_data.insert(received_data.end(), chunk.begin(), chunk.end());
        chunk_count++;
    }
    if (stream_err != StreamError::EndOfStream) {
        return HttpResponse("Failure in fetch request read_chunk(): " + to_string(stream_err))
            .set_status(HTTP_STATUS_INTERNAL_SERVER_ERROR)
            .set_header("Serverless", "EDJX");
    }

    // Close the read stream
    StreamError close_err = read_stream.close();
    if (close_err != StreamError::Success) {
        return HttpResponse("Failure in read stream close(): " + to_string(close_err))
            .set_status(HTTP_STATUS_INTERNAL_SERVER_ERROR)
            .set_header("Serverless", "EDJX");
    }

    std::vector<uint8_t> count_text = to_bytes("Received " + std::to_string(chunk_count) + " chunks");
    received_data.insert(received_data.end(), count_text.begin(), count_text.end());

    HttpResponse res(received_data);
    res.set_status(HTTP_STATUS_OK);
    res.set_header("Serverless", "EDJX");

    return res;
}