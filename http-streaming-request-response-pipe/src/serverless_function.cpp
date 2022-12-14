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
using edjx::stream::ReadStream;
using edjx::stream::WriteStream;
using edjx::error::HttpError;
using edjx::error::StreamError;

static const HttpStatusCode HTTP_STATUS_OK = 200;

bool serverless_streaming(HttpRequest & req) {
    info("** Streamed HTTP request and response with pipe **");

    // Open a read stream from the request
    ReadStream read_stream;
    HttpError http_err = req.open_read_stream(read_stream);
    if (http_err != HttpError::Success) {
        error("Could not open read stream: " + to_string(http_err));
        return false;
    }

    // Prepare a response
    HttpResponse res;
    res.set_status(HTTP_STATUS_OK);
    res.set_header("Serverless", "EDJX");

    // Open a write stream for the response
    WriteStream write_stream;
    http_err = res.send_streaming(write_stream);
    if (http_err != HttpError::Success) {
        error("Could not open write stream: " + to_string(http_err));
        read_stream.close();
        return false;
    }

    // Stream the message
    StreamError write_err = write_stream.write_chunk(
        "Welcome to EDJX! Streamed data will be echoed back.\r\n"
    );
    if (write_err != StreamError::Success) {
        error("Error when writing a chunk: " + to_string(write_err));
        read_stream.close();
        write_stream.abort();
        return false;
    }

    // Read all chunks from the read stream and send them to the write stream
    StreamError stream_err = read_stream.pipe_to(write_stream);
    if (stream_err != StreamError::Success) {
        error("Error when piping streams: " + to_string(stream_err));
        return false;
    }

    // No need to call close() on streams because the pipe_to() method
    // automatically closes both the read stream and the write stream.

    // Return true if the serverless_streaming function finished successfully
    return true;
}