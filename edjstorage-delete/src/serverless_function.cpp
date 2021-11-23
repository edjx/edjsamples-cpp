#include <cstdint>
#include <string>
#include <vector>

#include <edjx/storage.hpp>
#include <edjx/logger.hpp>
#include <edjx/error.hpp>
#include <edjx/request.hpp>
#include <edjx/response.hpp>
#include <edjx/http.hpp>

using edjx::request::HttpRequest;
using edjx::response::HttpResponse;
using edjx::error::StorageError;
using edjx::storage::StorageResponse;
using edjx::logger::info;
using edjx::logger::error;
using edjx::http::HttpStatusCode;

static const HttpStatusCode HTTP_STATUS_OK = 200;
static const HttpStatusCode HTTP_STATUS_BAD_REQUEST = 400;

HttpStatusCode to_http_status_code(StorageError e) {
    switch (e) {
        case StorageError::Success:
            return 200;
        case StorageError::EmptyContent:
        case StorageError::MissingFileName:
        case StorageError::MissingBucketID:
        case StorageError::MissingAttributes:
        case StorageError::InvalidAttributes:
            return 400; //HTTP_STATUS_BAD_REQUEST;
        case StorageError::ContentNotFound:
        case StorageError::ContentDeleted:
            return 404; //HTTP_STATUS_NOT_FOUND;
        case StorageError::UnAuthorized:
            return 403; //HTTP_STATUS_FORBIDDEN;
        case StorageError::DeletedBucketID:
        case StorageError::InternalError:
        case StorageError::SystemError:
            return 500; //HTTP_STATUS_INTERNAL_SERVER_ERROR;
    }
}

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
        if (! name.empty() || ! value.empty()) {
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
    info("Content Delete Flow");

    // 1.Param(Required) : "file_name" -> name that will be given to uploading content
    std::optional<std::string> file_name = query_param_by_name(req, "file_name");
    if (! file_name.has_value()) {
        error("No file_name found in query params of request");
        return HttpResponse("No file name found in query params of request")
            .set_status(HTTP_STATUS_BAD_REQUEST);
    };

    // 2.Param(Required) : "bucket_id" ->  in which content will be uploaded
    std::optional<std::string> bucket_id = query_param_by_name(req, "bucket_id");
    if (! bucket_id.has_value()) {
        error("No bucket id found in query params of request");
        return HttpResponse("No bucket id found in query params of request")
            .set_status(HTTP_STATUS_BAD_REQUEST);
    };

    StorageResponse res_bytes;
    StorageError err = edjx::storage::remove(res_bytes, bucket_id.value(), file_name.value());
    if (err != StorageError::Success) {
        error("Content Deletion Failed");
        return HttpResponse(to_string(err))
            .set_status(to_http_status_code(err));
    };

    HttpResponse res = HttpResponse("Success").set_status(HTTP_STATUS_OK);
    info("Content Deletion Successful");

    return res;
}