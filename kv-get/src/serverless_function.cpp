#include <cstdint>

#include <edjx/logger.hpp>
#include <edjx/request.hpp>
#include <edjx/response.hpp>
#include <edjx/error.hpp>
#include <edjx/kv.hpp>
#include <edjx/http.hpp>

using edjx::logger::info;
using edjx::logger::error;
using edjx::request::HttpRequest;
using edjx::response::HttpResponse;
using edjx::error::KVError;
using edjx::http::HttpStatusCode;

static const HttpStatusCode HTTP_STATUS_OK = 200;
static const HttpStatusCode HTTP_STATUS_BAD_REQUEST = 400;
static const HttpStatusCode HTTP_STATUS_UNAUTHORIZED = 401;
static const HttpStatusCode HTTP_STATUS_NOT_FOUND = 404;

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
    info("Inside KV get example function");

    std::optional<std::string> key = query_param_by_name(req, "key");

    if (key.has_value()) {
        std::vector<uint8_t> val;
        KVError err = edjx::kv::get(val, key.value());

        switch (err) {
            case KVError::Success:
                return HttpResponse(val)
                    .set_status(HTTP_STATUS_OK);
            case KVError::Unknown:
                return HttpResponse(edjx::error::to_string(err))
                    .set_status(HTTP_STATUS_BAD_REQUEST);
            case KVError::UnAuthorized:
                return HttpResponse(edjx::error::to_string(err))
                    .set_status(HTTP_STATUS_UNAUTHORIZED);
            case KVError::NotFound:
                return HttpResponse(edjx::error::to_string(err))
                    .set_status(HTTP_STATUS_NOT_FOUND);
        }
    } else {
        error("No key provided in user request");
        return HttpResponse("No key provided in user request")
            .set_status(HTTP_STATUS_BAD_REQUEST);
    }
}