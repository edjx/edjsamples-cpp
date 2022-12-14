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
using edjx::error::StorageError;
using edjx::error::StreamError;
using edjx::storage::StorageResponsePending;
using edjx::storage::StorageResponse;
using edjx::storage::FileAttributes;
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

HttpResponse serverless(const HttpRequest & req) {
    info("** Storage put with http function - Streaming version **");

    // 1. param (required): "file_name" -> name that will be given to the uploaded content
    std::optional<std::string> file_name = query_param_by_name(req, "file_name");
    if (!file_name.has_value()) {
        error("No file_name found in query params of request");
        return HttpResponse("No file name found in query params of request")
            .set_status(HTTP_STATUS_BAD_REQUEST)
            .set_header("Serverless", "EDJX");
    }

    // 2. param (required): "bucket_id" -> in which bucket content will be uploaded
    std::optional<std::string> bucket_id = query_param_by_name(req, "bucket_id");
    if (!bucket_id.has_value()) {
        error("No bucket id found in query params of request");
        return HttpResponse("No bucket id found in query params of request")
            .set_status(HTTP_STATUS_BAD_REQUEST)
            .set_header("Serverless", "EDJX");
    }

    // 3. param (optional): "properties" -> e.g., cache-control=true,a=b
    std::optional<std::string> properties = query_param_by_name(req, "properties");

    // Create a write stream to the storage
    StorageResponsePending storage_resp_pending;
    WriteStream write_stream;
    StorageError err = edjx::storage::put_streaming(
        storage_resp_pending,
        write_stream,
        bucket_id.value(),
        file_name.value(),
        properties.value_or("")
    );
    if (err != StorageError::Success) {
        error("Error when creating a stream: " + to_string(err));
        return HttpResponse("Error when creating a stream: " + to_string(err))
            .set_status(edjx::error::to_http_status_code(err))
            .set_header("Serverless", "EDJX");
    }

    // Prepare the content that will be uploaded to the storage
    std::string content = "WHERE IS THE EDGE, ANYWAY?\nIf you ask a cloud company, they tell you the edge is their multi-billion dollar collection of server farms. A content delivery network (CDN) provider says it's their hundreds of points of presence. Wireless carriers will try to convince you it's their tens of thousands of macrocell and picocell sites.\n\nAt EDJX, we say the edge is anywhere and everywhere, a thousand feet away from you at all times. We believe computing needs to become ubiquitous, like electricity, to power billions of connected devices.\nThe edge will go so far out into the woods, you can hear the sasquatch scream.";

    bool success = true;

    // Send chunks of increasing sizes (1, 2, 3, ...)
    for (int i = 0, len = 1; i < content.length(); i += len, len++) {
        std::string chunk = content.substr(i, len);
        StreamError err = write_stream.write_chunk(chunk);
        if (err != StreamError::Success) {
            error("Error when writing a text chunk: " + to_string(err));
            success = false;
            break;
        }
    }

    // Close the stream
    if (success) {
        // No error encountered - cleanly close the stream
        StreamError close_err = write_stream.close();
        if (close_err != StreamError::Success) {
            error("Error when closing a stream: " + to_string(close_err));
            success = false;
        }
    } else {
        // There was an error - abort the stream
        StreamError close_err = write_stream.abort();
        if (close_err != StreamError::Success) {
            error("Error when aborting a stream: " + to_string(close_err));
            success = false;
        }
    }

    // Exit if streaming failed
    if (!success) {
        error("Streaming to storage failed.");
        return HttpResponse("Streaming to storage failed. See error log.")
            .set_status(HTTP_STATUS_INTERNAL_SERVER_ERROR)
            .set_header("Serverless", "EDJX");
    }

    // Get a response from the storage
    StorageResponse storage_resp;
    err = storage_resp_pending.get_storage_response(storage_resp);
    if (err != StorageError::Success) {
        error("Error when receiving a storage response: " + to_string(err));
        return HttpResponse("Error when receiving a storage response: " + to_string(err))
            .set_status(edjx::error::to_http_status_code(err))
            .set_header("Serverless", "EDJX");
    }

    std::vector<uint8_t> body;
    StreamError resp_err = storage_resp.read_body(body);
    if (resp_err != StreamError::Success) {
        error("Error when reading storage response body: " + to_string(resp_err));
        return HttpResponse("Error when reading storage response body: " + to_string(resp_err))
            .set_status(HTTP_STATUS_INTERNAL_SERVER_ERROR)
            .set_header("Serverless", "EDJX");
    }

    info("Streaming to storage was successful");

    return HttpResponse(body)
        .set_status(HTTP_STATUS_OK)
        .set_header("Serverless", "EDJX");
}