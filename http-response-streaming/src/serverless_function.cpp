#include <cstdint>
#include <string>
#include <vector>

#include <edjx/logger.hpp>
#include <edjx/request.hpp>
#include <edjx/response.hpp>
#include <edjx/http.hpp>
#include <edjx/stream.hpp>

using edjx::logger::info;
using edjx::logger::error;
using edjx::request::HttpRequest;
using edjx::response::HttpResponse;
using edjx::http::HttpStatusCode;
using edjx::stream::WriteStream;
using edjx::error::HttpError;
using edjx::error::StreamError;

static const HttpStatusCode HTTP_STATUS_OK = 200;

bool serverless_streaming(HttpRequest & req) {
    info("** Streamed HTTP Response **");

    // Prepare an HTTP response
    HttpResponse res;
    res.set_status(HTTP_STATUS_OK);
    res.set_header("Content-Type", "text/plain");
    res.set_header("Serverless", "EDJX");

    // Open a write stream for the response body
    WriteStream write_stream;
    HttpError err = res.send_streaming(write_stream);
    if (err != HttpError::Success) {
        error("Error in send_streaming: " + to_string(err));
        return false;
    }

    // Stream the HTTP response body
    info("** Streaming HTTP response **");

    uint32_t iterations = 10000;
    bool success = true;

    for (uint32_t i = 0; i < iterations; i++) {
        // Send a chunk
        StreamError err;
        err = write_stream.write_chunk("Chunk " + std::to_string(i) + "/" + std::to_string(iterations) + "\r\n");
        // Check if the chunk was successfully sent
        if (err != StreamError::Success) {
            error("Error when writing a chunk: " + edjx::error::to_string(err));
            success = false;
            break;
        }
    }

    // Close the stream
    if (success) {
        info("** Closing the write stream **");
        StreamError close_err = write_stream.close();
        if (close_err != StreamError::Success) {
            error("Error when closing the stream: " + to_string(close_err));
            success = false;
        }
    } else {
        info("** Aborting the write stream **");
        StreamError close_err = write_stream.abort();
        if (close_err != StreamError::Success) {
            error("Error when aborting the stream: " + to_string(close_err));
            success = false;
        }
    }

    return success;
}