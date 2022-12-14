#include <string>
#include <vector>
#include <map>
#include <optional>
#include <algorithm>

#include <edjx/logger.hpp>
#include <edjx/http.hpp>
#include <edjx/request.hpp>
#include <edjx/response.hpp>

using edjx::logger::info;
using edjx::http::HttpMethod;
using edjx::http::HttpHeaders;
using edjx::http::HttpStatusCode;
using edjx::request::HttpRequest;
using edjx::response::HttpResponse;
using edjx::error::HttpError;

static const HttpStatusCode HTTP_STATUS_OK = 200;
static const HttpStatusCode HTTP_STATUS_BAD_REQUEST = 400;
static const HttpStatusCode HTTP_STATUS_METHOD_NOT_ALLOWED = 405;

// This helper function returns false if JSON is empty.
// No further validation is performed.
bool json_is_valid(const std::string & json) {
    bool empty = true;

    for (auto c : json) {
        if (!isspace(c)) {
            empty = false;
        }
    }

    return !empty;
}

// This helper function inserts a "key":"value" pair at the beginning of a JSON.
// It looks for an opening brace '{' and inserts the key:value pair after it.
// It will append a comma ',' after the key:value pair if needed.
// This function does not attempt to validate the JSON.
std::string insert_into_json(
    const std::string & json,
    const std::string & name,
    const std::string & value
) {
    bool inserted = false;
    bool comma_inserted = false;
    bool comma_not_needed = false;

    std::string result;

    for (char c : json) {
        if (c == '{' && !inserted) {
            result += "{\"" + name + "\":\"" + value + "\"";
            inserted = true;
        } else if (inserted && !comma_inserted && !comma_not_needed) {
            if (isspace(c)) {
                continue;
            } else if (c != '}') {
                result += ',';
                comma_inserted = true;
            } else {
                comma_not_needed = true;
            }
            result += c;
        } else {
            result += c;
        }
    }

    return result;
}

// This helper function appends a Name=Value pair at the beginning of
// a URL-encoded form. The appended Name=Value pair must already
// be in an encoded form.
std::string insert_into_form(
    const std::string & form,
    const std::string & name,
    const std::string & value
) {
    std::string result = name + "=" + value;

    bool form_empty = true;
    for (char c : form) {
        if (!isspace(c)) {
            form_empty = false;
            break;
        }
    }

    if (!form_empty) {
        result += "&" + form;
    }

    return result;
}

bool char_equal_nocase(char c1, char c2) {
    return tolower(c1) == tolower(c2);
}

bool string_equal_nocase(const std::string & str1, const std::string & str2) {
    return str1.length() == str2.length() && std::equal(str1.begin(), str1.end(), str2.begin(), char_equal_nocase);
}

// This helper function gets values of an HTTP header.
std::optional<std::string> header_value(const HttpHeaders & headers, const std::string & name) {
    std::optional<std::string> result = std::nullopt;
    bool first_entry = true;

    // Create a comma-separated list of all header values.
    // Header name is case-insensitive.
    for (const auto & header : headers) {
        if (string_equal_nocase(header.first, name)) {
            for (const std::string & value : header.second) {
                if (first_entry) {
                    result = "";
                    first_entry = false;
                } else {
                    *result += ',';
                }
                *result += value;
            }
        }
    }

    return result;
}

HttpResponse serverless(HttpRequest & req) {
    info("**Incoming HTTP with diff content type function**");

    switch (req.get_method()) {
        case HttpMethod::POST: {
            std::string content_type_header = header_value(req.get_headers(), "Content-Type").value_or("");
            std::vector<uint8_t> incoming_req_body;
            HttpError err = req.read_body(incoming_req_body);
            if (err != HttpError::Success) {
                return HttpResponse(to_string(err))
                    .set_status(HTTP_STATUS_BAD_REQUEST)
                    .set_header("Serverless", "EDJX")
                    .set_header("Content-Type", "text/plain");
            }

            if (content_type_header == "application/json") {
                std::string body_str = edjx::utils::to_string(incoming_req_body);

                if (!json_is_valid(body_str)) {
                    return HttpResponse("Request body must be a valid JSON")
                        .set_status(HTTP_STATUS_BAD_REQUEST);
                }

                std::string outgoing_body = insert_into_json(body_str, "Modified By", "Example function");

                return HttpResponse(outgoing_body)
                    .set_status(HTTP_STATUS_OK)
                    .set_header("Serverless", "EDJX")
                    .set_header("Content-Type", "application/json");
            }
            else if (content_type_header == "text/plain") {
                std::string body_str = edjx::utils::to_string(incoming_req_body);

                std::string outgoing_body = "Modified By : Example Function " + body_str;

                return HttpResponse(outgoing_body)
                    .set_status(HTTP_STATUS_OK)
                    .set_header("Serverless", "EDJX")
                    .set_header("Content-Type", "text/plain");
            }
            else if (content_type_header == "application/x-www-form-urlencoded") {
                std::string body_str = edjx::utils::to_string(incoming_req_body);

                std::string outgoing_body = insert_into_form(body_str, "Modified+By", "Example+Function");

                return HttpResponse(outgoing_body)
                    .set_status(HTTP_STATUS_OK)
                    .set_header("Serverless", "EDJX")
                    .set_header(
                        "Content-Type",
                        "application/x-www-form-urlencoded"
                    );
            }
            else {
                return HttpResponse().set_status(HTTP_STATUS_BAD_REQUEST);
            }
            break;
        }
        default: {
            return HttpResponse().set_status(HTTP_STATUS_METHOD_NOT_ALLOWED);
        }
    }
}