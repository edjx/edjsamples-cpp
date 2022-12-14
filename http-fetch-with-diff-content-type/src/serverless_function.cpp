#include <cstdint>
#include <string>
#include <vector>
#include <optional>

#include <edjx/logger.hpp>
#include <edjx/error.hpp>
#include <edjx/http.hpp>
#include <edjx/request.hpp>
#include <edjx/response.hpp>
#include <edjx/fetch.hpp>
#include <edjx/utils.hpp>

using edjx::logger::info;
using edjx::logger::error;
using edjx::error::HttpError;
using edjx::error::StreamError;
using edjx::http::Uri;
using edjx::http::HttpMethod;
using edjx::http::HttpStatusCode;
using edjx::request::HttpRequest;
using edjx::response::HttpResponse;
using edjx::fetch::HttpFetch;
using edjx::fetch::FetchResponse;

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
    info("**HTTP Fetch with diff content type function**");

    std::string body_type = query_param_by_name(req, "body_type")
        .value_or("application/json");

    std::vector<uint8_t> body;

    if (body_type == "application/json") {
        body = edjx::utils::to_bytes("{\"Requested By\":\"Example Function\"}");
    } else if (body_type == "text/plain") {
        body = edjx::utils::to_bytes("Requested By : Example Function");
    } else if (body_type == "application/x-www-form-urlencoded") {
        body = edjx::utils::to_bytes("Requested%20By=Example%20Function");
    } else {
        return HttpResponse().set_status(HTTP_STATUS_BAD_REQUEST);
    }

    Uri fetch_uri("https://httpbin.org/post");
    FetchResponse fetch_res;
    HttpError err = HttpFetch(fetch_uri, HttpMethod::POST)
        .set_body(body)
        .set_header("Content-Type", body_type)
        .send(fetch_res);
    if (err != HttpError::Success) {
        error(to_string(err));
        return HttpResponse("failure in fetch req for given body type : " + to_string(err))
            .set_status(HTTP_STATUS_BAD_REQUEST);
    }

    std::vector<uint8_t> res_body;
    StreamError s_err = fetch_res.read_body(res_body);
    if (s_err != StreamError::Success) {
        error(to_string(s_err));
        return HttpResponse("failure in get_fetch_response: " + to_string(s_err))
            .set_status(HTTP_STATUS_BAD_REQUEST);
    }

    HttpResponse res(res_body);
    res.set_status(HTTP_STATUS_OK);
    res.set_headers(fetch_res.get_headers());
    res.set_header("Serverless", "EDJX");

    return res;
}