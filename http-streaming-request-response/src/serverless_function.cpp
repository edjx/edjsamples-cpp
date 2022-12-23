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
    info("** Streamed HTTP request and response **");

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
    int count = 0;
    std::vector<uint8_t> chunk;
    StreamError read_err;
    while ((read_err = read_stream.read_chunk(chunk)) == StreamError::Success) {
        write_err = write_stream.write_chunk(chunk);
        if (write_err != StreamError::Success) {
            error("Error when writing a chunk: " + to_string(write_err));
            read_stream.close();
            write_stream.abort();
            return false;
        }
        count++;
    }
    if (read_err != StreamError::EndOfStream) {
        error("Error when reading a chunk: " + to_string(read_err));
        read_stream.close();
        write_stream.abort();
        return false;
    }

    // Write some statistics at the end
    write_err = write_stream.write_chunk(
        "\r\nTransmitted " + std::to_string(count) + " chunks.\r\n"
    );
    if (write_err != StreamError::Success) {
        error("Error when writing the summary info: " + to_string(write_err));
        read_stream.close();
        write_stream.abort();
        return false;
    }

    bool close_success = true;

    // Close the write stream
    StreamError close_err = write_stream.close();
    if (close_err != StreamError::Success) {
        error("Error when closing the write stream: " + to_string(close_err));
        close_success = false;
    }

    // Close the read stream
    close_err = read_stream.close();
    if (close_err != StreamError::Success) {
        error("Error when closing the read stream: " + to_string(close_err));
        close_success = false;
    }

    // Return true if the serverless_streaming function finished successfully
    return close_success;
}