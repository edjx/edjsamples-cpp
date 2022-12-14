#include <cstdint>
#include <string>
#include <vector>
#include <map>

#include <edjx/storage.hpp>
#include <edjx/logger.hpp>
#include <edjx/error.hpp>
#include <edjx/request.hpp>
#include <edjx/response.hpp>
#include <edjx/http.hpp>

using edjx::request::HttpRequest;
using edjx::response::HttpResponse;
using edjx::error::StorageError;
using edjx::error::StreamError;
using edjx::storage::StorageResponse;
using edjx::storage::FileAttributes;
using edjx::logger::info;
using edjx::logger::error;
using edjx::http::HttpStatusCode;

static const HttpStatusCode HTTP_STATUS_OK = 200;
static const HttpStatusCode HTTP_STATUS_BAD_REQUEST = 400;

std::optional<std::string> query_param_by_name(const HttpRequest & req, const std::string & param_name) {
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
    info("**Storage get with http function**");

    // 1. param (required): "file_name" -> name that will be given to the uploaded content
    std::optional<std::string> file_name = query_param_by_name(req, "file_name");
    if (!file_name.has_value()) {
        error("No file_name found in query params of request");
        return HttpResponse("No file name found in query params of request")
            .set_status(HTTP_STATUS_BAD_REQUEST);
    }

    // 2. param (required): "bucket_id" -> in which bucket content will be uploaded
    std::optional<std::string> bucket_id = query_param_by_name(req, "bucket_id");
    if (!bucket_id.has_value()) {
        error("No bucket id found in query params of request");
        return HttpResponse("No bucket id found in query params of request")
            .set_status(HTTP_STATUS_BAD_REQUEST);
    }

    StorageResponse res_bytes;
    StorageError err = edjx::storage::get(res_bytes, bucket_id.value(), file_name.value());
    if (err != StorageError::Success) {
        return HttpResponse(to_string(err)).set_status(edjx::error::to_http_status_code(err));
    }
    info("Get Content Successful");

    std::vector<uint8_t> body;
    StreamError s_err = res_bytes.read_body(body);
    if (s_err != StreamError::Success) {
        error(to_string(s_err));
        return HttpResponse("failure in get_fetch_response: " + to_string(s_err))
            .set_status(HTTP_STATUS_BAD_REQUEST);
    }

    HttpResponse res(body);
    for (const auto & header : res_bytes.get_headers()) {
        res.append_header(header.first, header.second);
    }

    return res
        .set_status(HTTP_STATUS_OK)
        .set_header("Content-Type", "text/plain");
}