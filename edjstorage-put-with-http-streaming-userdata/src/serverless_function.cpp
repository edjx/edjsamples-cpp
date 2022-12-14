#include <cstdint>
#include <string>
#include <vector>
#include <optional>

#include <edjx/storage.hpp>
#include <edjx/logger.hpp>
#include <edjx/error.hpp>
#include <edjx/request.hpp>
#include <edjx/response.hpp>
#include <edjx/utils.hpp>
#include <edjx/http.hpp>
#include <edjx/stream.hpp>

using edjx::request::HttpRequest;
using edjx::response::HttpResponse;
using edjx::error::HttpError;
using edjx::error::StorageError;
using edjx::error::StreamError;
using edjx::storage::StorageResponsePending;
using edjx::storage::StorageResponse;
using edjx::stream::ReadStream;
using edjx::stream::WriteStream;
using edjx::logger::info;
using edjx::logger::error;
using edjx::http::HttpStatusCode;

static const HttpStatusCode HTTP_STATUS_OK = 200;
static const HttpStatusCode HTTP_STATUS_BAD_REQUEST = 400;
static const HttpStatusCode HTTP_STATUS_INTERNAL_SERVER_ERROR = 500;

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
    info("** Storage put - Streaming version with custom data **");

    // 1. param (required): "file_name" -> name that will be given to the uploaded content
    std::optional<std::string> file_name = query_param_by_name(req, "file_name");
    if (!file_name.has_value()) {
        error("No file name found in query params of the request");
        HttpResponse("No file name found in query params of the request")
            .set_status(HTTP_STATUS_BAD_REQUEST)
            .set_header("Serverless", "EDJX")
            .send();
        return false;
    }

    // 2. param (required): "bucket_id" -> in which bucket content will be uploaded
    std::optional<std::string> bucket_id = query_param_by_name(req, "bucket_id");
    if (!bucket_id.has_value()) {
        error("No bucket id found in query params of the request");
        HttpResponse("No bucket id found in query params of the request")
            .set_status(HTTP_STATUS_BAD_REQUEST)
            .set_header("Serverless", "EDJX")
            .send();
        return false;
    }

    // 3. param (optional): "properties" -> e.g., cache-control=true,a=b
    std::optional<std::string> properties = query_param_by_name(req, "properties");

    // Open a read stream from the HTTP request from the client
    ReadStream client_read_stream;
    HttpError client_read_open_err = req.open_read_stream(client_read_stream);
    if (client_read_open_err != HttpError::Success) {
        error("Could not open read stream from the request: " + to_string(client_read_open_err));
        HttpResponse("Could not open read stream from the request: " + to_string(client_read_open_err))
            .set_status(HTTP_STATUS_INTERNAL_SERVER_ERROR)
            .set_header("Serverless", "EDJX")
            .send();
        return false;
    }

    // Open a write stream to the storage
    StorageResponsePending storage_resp_pending;
    WriteStream storage_write_stream;
    StorageError storage_write_open_err = edjx::storage::put_streaming(
        storage_resp_pending,
        storage_write_stream,
        bucket_id.value(),
        file_name.value(),
        properties.value_or("")
    );
    if (storage_write_open_err != StorageError::Success) {
        error("Error when creating a storage write stream: " + to_string(storage_write_open_err));
        client_read_stream.close();
        HttpResponse("Error when creating a storage write stream: " + to_string(storage_write_open_err))
            .set_status(edjx::error::to_http_status_code(storage_write_open_err))
            .set_header("Serverless", "EDJX")
            .send();
        return false;
    }

    // Prepare an HTTP response for the client
    HttpResponse res;
    res.set_status(HTTP_STATUS_OK);
    res.set_header("Serverless", "EDJX");

    // Open a write stream for the HTTP response
    WriteStream client_write_stream;
    HttpError client_write_open_err = res.send_streaming(client_write_stream);
    if (client_write_open_err != HttpError::Success) {
        error("Error when creating a write stream for the HTTP response: " + to_string(client_write_open_err));
        client_read_stream.close();
        storage_write_stream.abort();
        HttpResponse("Error when creating a write stream for the HTTP response: " + to_string(client_write_open_err))
            .set_status(HTTP_STATUS_INTERNAL_SERVER_ERROR)
            .set_header("Serverless", "EDJX")
            .send();
        return false;
    }

    // Read the request chunk by chunk
    int count = 0;
    std::vector<uint8_t> bytes;
    StreamError client_read_err;
    while ((client_read_err = client_read_stream.read_chunk(bytes)) == StreamError::Success) {
        count++;
        // Send the chunk to the storage
        StreamError storage_write_err = storage_write_stream.write_chunk(bytes);
        if (storage_write_err != StreamError::Success) {
            error("Error when writing a chunk to the storage: " + to_string(storage_write_err));

            StreamError client_write_err = client_write_stream.write_chunk(
                "Error when writing a chunk to the storage: " + to_string(storage_write_err) + "\r\n"
            );
            if (client_write_err != StreamError::Success) {
                error("Error when writing a response chunk: " + to_string(client_write_err));
            }
            client_read_stream.close();
            storage_write_stream.abort();
            client_write_stream.close();
            return false;
        }

        // Stream the chunk upload progress back to the client
        StreamError client_write_err = client_write_stream.write_chunk(
            "Sent chunk #" + std::to_string(count) + " of size " + std::to_string(bytes.size()) + "\r\n"
        );
        if (client_write_err != StreamError::Success) {
            error("Error when writing a response chunk: " + to_string(client_write_err));
            client_read_stream.close();
            storage_write_stream.abort();
            client_write_stream.close();
            return false;
        }
    }
    if (client_read_err != StreamError::EndOfStream) {
        error("Error when reading a chunk from the HTTP request: " + to_string(client_read_err));

        StreamError client_write_err = client_write_stream.write_chunk(
            "Error when reading a chunk from the HTTP request: " + to_string(client_read_err) + "\r\n"
        );
        if (client_write_err != StreamError::Success) {
            error("Error when writing a response chunk: " + to_string(client_write_err));
        }
        client_read_stream.close();
        storage_write_stream.abort();
        client_write_stream.close();
        return false;
    }

    // Close the HTTP request read stream
    StreamError client_read_close_err = client_read_stream.close();
    if (client_read_close_err != StreamError::Success) {
        error("Error when closing the read stream: " + to_string(client_read_close_err));
    }

    // Report the end of the stream to the client
    StreamError client_write_err = client_write_stream.write_chunk(
        "End of stream\r\n"
    );
    if (client_write_err != StreamError::Success) {
        error("Error when writing a response chunk: " + to_string(client_write_err));
        storage_write_stream.abort();
        client_write_stream.close();
        return false;
    }

    // Close the storage write stream
    StreamError storage_write_close_err = storage_write_stream.close();
    if (storage_write_close_err != StreamError::Success) {
        error("Error when closing the storage write stream: " + to_string(storage_write_close_err));

        StreamError client_write_err = client_write_stream.write_chunk(
            "Error when closing the storage write stream: " + to_string(storage_write_close_err) + "\r\n"
        );
        if (client_write_err != StreamError::Success) {
            error("Error when writing a chunk: " + to_string(client_write_err));
            client_write_stream.close();
            return false;
        }
        return false;
    }

    // Get a response from the storage
    StorageResponse storage_resp;
    StorageError storage_resp_err = storage_resp_pending.get_storage_response(storage_resp);
    if (storage_resp_err != StorageError::Success) {
        error("Storage response error: " + to_string(storage_resp_err));

        StreamError client_write_err = client_write_stream.write_chunk(
            "Storage response error: " + to_string(storage_resp_err) + "\r\n"
        );
        if (client_write_err != StreamError::Success) {
            error("Error when writing a chunk: " + to_string(client_write_err));
        }
        client_write_stream.close();
        return false;
    }

    std::vector<uint8_t> body;
    StreamError storage_resp_body_err = storage_resp.read_body(body);
    if (storage_resp_body_err != StreamError::Success) {
        error("Error when reading storage response body: " + to_string(storage_resp_body_err));

        StreamError client_write_err = client_write_stream.write_chunk(
            "Error when reading storage response body: " + to_string(storage_resp_body_err) + "\r\n"
        );
        if (client_write_err != StreamError::Success) {
            error("Error when writing a chunk: " + to_string(client_write_err));
        }
        client_write_stream.close();
        return false;
    }

    info("Streaming to storage was successful");

    client_write_err = client_write_stream.write_chunk(
        "Storage put successful: " + edjx::utils::to_string(body) + "\r\n"
    );
    if (client_write_err != StreamError::Success) {
        error("Error when writing a chunk: " + to_string(client_write_err));
        client_write_stream.close();
        return false;
    }

    // Close the HTTP response stream
    StreamError client_write_close_err = client_write_stream.close();
    if (client_write_close_err != StreamError::Success) {
        error("Error when closing the write stream: " + to_string(client_write_close_err));
        return false;
    }

    return true;
}