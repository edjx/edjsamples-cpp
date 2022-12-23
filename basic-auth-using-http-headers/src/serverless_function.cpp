#include <cstdint>
#include <string>
#include <vector>
#include <map>

#include <edjx/error.hpp>
#include <edjx/request.hpp>
#include <edjx/response.hpp>
#include <edjx/logger.hpp>
#include <edjx/http.hpp>

using edjx::http::HttpHeaders;
using edjx::http::HttpStatusCode;
using edjx::request::HttpRequest;
using edjx::response::HttpResponse;
using edjx::error::HttpError;
using edjx::logger::error;
using edjx::logger::info;

static const HttpStatusCode HTTP_STATUS_OK = 200;
static const HttpStatusCode HTTP_STATUS_UNAUTHORIZED = 401;

bool verify_auth_token(const std::string & _token) {
    // TODO
    return true;
}

HttpResponse serverless(const HttpRequest & req) {
    info("**Basic Auth using http headers function**");

    // Get "Authorization" header value (Note: case-sensitive!)
    const HttpHeaders & headers = req.get_headers();
    HttpHeaders::const_iterator auth_header_iterator = headers.find("authorization");
    if (auth_header_iterator == headers.end()) {
        return HttpResponse("Authentication Error : No token present")
            .set_status(HTTP_STATUS_UNAUTHORIZED);
    }

    // Create a single comma-separated string of values from std::vector<std::string>
    std::string auth_header_value;
    bool auth_header_first_value = true;
    for (const auto & value : auth_header_iterator->second) {
        if (!auth_header_first_value) {
            auth_header_value += ",";
        }
        auth_header_value += value;
        auth_header_first_value = false;
    }

    // Verify authorization token
    if (!verify_auth_token(auth_header_value)) {
        return HttpResponse("Authentication Error : Invalid auth token")
            .set_status(HTTP_STATUS_UNAUTHORIZED);
    }

    // Return success
    return HttpResponse()
        .set_status(HTTP_STATUS_OK)
        .set_header("Serverless", "EDJX");
}