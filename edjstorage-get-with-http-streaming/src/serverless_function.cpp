#include <cstdint>
#include <string>
#include <vector>

#include <edjx/storage.hpp>
#include <edjx/logger.hpp>
#include <edjx/error.hpp>
#include <edjx/request.hpp>
#include <edjx/response.hpp>
#include <edjx/http.hpp>
#include <edjx/stream.hpp>

using edjx::request::HttpRequest;
using edjx::response::HttpResponse;
using edjx::error::HttpError;
using edjx::error::StorageError;
using edjx::error::StreamError;
using edjx::storage::StorageResponse;
using edjx::storage::FileAttributes;
using edjx::logger::info;
using edjx::logger::error;
using edjx::http::HttpStatusCode;
using edjx::stream::ReadStream;
using edjx::stream::WriteStream;

static const HttpStatusCode HTTP_STATUS_OK = 200;
static const HttpStatusCode HTTP_STATUS_BAD_REQUEST = 400;

static std::optional<std::string> query_param_by_name(const HttpRequest & req, const std::string & param_name) {
    std::string uri = req.get_uri().as_string();
    std::vector<std::pair<std::string, std::string>> query_parsed;

    // e.g., https://example.com/path/to/page?name=ferret&color=purple

    size_t query_start = uri.find('?');

    if (query_start != std::string::npos) {
        // Query is present
        std::string name;
        std::string value;
        bool parsing_name = true;
        for (std::string::iterator it = uri.begin() + query_start + 1; it != uri.end(); ++it) {
            char c = *it;
            switch (c) {
                case '?':
                    break; // Invalid URI
                case '=':
                    parsing_name = false;
                    break;
                case '&':
                    query_parsed.push_back(make_pair(name, value));
                    name.clear();
                    value.clear();
                    parsing_name = true;
                    break;
                default:
                    if (parsing_name) {
                        name += c;
                    } else {
                        value += c;
                    }
                    break;
            }
        }
        if (!name.empty() || !value.empty()) {
            query_parsed.push_back(make_pair(name, value));
        }

        for (const auto & parameter : query_parsed) {
            if (parameter.first == param_name) {
                return parameter.second;
            }
        }
    }

    return {};
}

bool serverless_streaming(HttpRequest & req) {
    info("** Storage get with http function - Streaming version **");

    // 1. param (required): "file_name" -> name that will be given to the uploaded content
    std::optional<std::string> file_name = query_param_by_name(req, "file_name");
    if (!file_name.has_value()) {
        error("No file_name found in query params of request");
        HttpResponse("No file name found in query params of request")
            .set_status(HTTP_STATUS_BAD_REQUEST)
            .send();
        return false;
    }

    // 2. param (required): "bucket_id" -> in which bucket content will be uploaded
    std::optional<std::string> bucket_id = query_param_by_name(req, "bucket_id");
    if (!bucket_id.has_value()) {
        error("No bucket id found in query params of request");
        HttpResponse("No bucket id found in query params of request")
            .set_status(HTTP_STATUS_BAD_REQUEST)
            .send();
        return false;
    }

    // Get the file from the storage
    StorageResponse storage_res;
    StorageError storage_err = edjx::storage::get(storage_res, bucket_id.value(), file_name.value());
    if (storage_err != StorageError::Success) {
        error("Error in storage::get(): " + to_string(storage_err));
        HttpResponse(to_string(storage_err))
            .set_status(to_http_status_code(storage_err))
            .send();
        return false;
    }
    info("Get Content Successful");

    // Open a read stream from the file
    ReadStream read_stream = storage_res.get_read_stream();

    // Prepare a response
    HttpResponse res;
    res.set_status(HTTP_STATUS_OK);
    res.set_header("Serverless", "EDJX");

    // Open a write stream for the response
    WriteStream write_stream;
    HttpError http_err = res.send_streaming(write_stream);
    if (http_err != HttpError::Success) {
        error("Could not open write stream: " + to_string(http_err));
        read_stream.close();
        return false;
    }

    // Read the file chunk by chunk
    int count = 0;
    StreamError read_err;
    std::vector<uint8_t> chunk;
    while ((read_err = read_stream.read_chunk(chunk)) == StreamError::Success) {
        count++;
        // Send the chunk in the HTTP response
        StreamError write_err = write_stream.write_chunk(chunk);
        if (write_err != StreamError::Success) {
            error("Error when writing a chunk: " + to_string(write_err));
            read_stream.close();
            write_stream.abort();
            return false;
        }
    }

    if (read_err != StreamError::EndOfStream) {
        error("Error in read_chunk: " + to_string(read_err));
        read_stream.close();
        write_stream.abort();
        return false;
    }

    // Send some statistics at the end
    StreamError write_err = write_stream.write_chunk(
        "\nEnd of stream. Received " + std::to_string(count) + " chunks."
    );
    if (write_err != StreamError::Success) {
        error("Error when writing a text chunk: " + to_string(write_err));
        read_stream.close();
        write_stream.abort();
        return false;
    }

    bool retval = true;

    StreamError close_err = read_stream.close();
    if (close_err != StreamError::Success) {
        error("Error when closing the read stream: " + to_string(close_err));
        retval = false;
    }

    close_err = write_stream.close();
    if (close_err != StreamError::Success) {
        error("Error when closing the write stream: " + to_string(close_err));
        retval = false;
    }

    return retval;
}