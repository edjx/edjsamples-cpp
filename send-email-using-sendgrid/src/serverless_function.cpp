#include <string>
#include <vector>
#include <optional>

#include <edjx/logger.hpp>
#include <edjx/http.hpp>
#include <edjx/request.hpp>
#include <edjx/response.hpp>
#include <edjx/fetch.hpp>
#include <edjx/error.hpp>
#include <edjx/utils.hpp>

using edjx::logger::info;
using edjx::logger::error;
using edjx::http::HttpMethod;
using edjx::http::Uri;
using edjx::http::HttpStatusCode;
using edjx::request::HttpRequest;
using edjx::response::HttpResponse;
using edjx::fetch::HttpFetch;
using edjx::fetch::FetchResponse;
using edjx::error::HttpError;
using edjx::error::StreamError;

static const HttpStatusCode HTTP_STATUS_BAD_REQUEST = 400;

static const std::string SENDGRID_API_KEY = "SG.XXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXX";

HttpError send_email(FetchResponse & response, const std::string & subject, const std::string & message) {
    Uri send_uri("https://api.sendgrid.com/v3/mail/send");

    std::string email_json = "{\"personalizations\": [{\"to\": [{\"email\": \"edjx@edjx.io\"}]}],\"from\": {\"email\": \"edjx@edjx.io\"},\"subject\": subject,\"content\": [{\"type\": \"text/plain\", \"value\": message}]}";
    std::vector<uint8_t> email_vec = edjx::utils::to_bytes(email_json);

    return HttpFetch(send_uri, HttpMethod::POST)
        .set_header(
            "Authorization",
            "Bearer " + SENDGRID_API_KEY
        )
        .set_header("Content-Type", "application/json")
        .set_body(email_vec)
        .send(response);
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
    info("**Send email using sendgrid function**");

    std::string message = query_param_by_name(req, "message").value_or("Default Message");

    std::string subject = query_param_by_name(req, "subject").value_or("Default Subject");

    FetchResponse fetch_response;
    HttpError err = send_email(fetch_response, subject, message);
    if (err != HttpError::Success) {
        error(to_string(err));
        return HttpResponse().set_status(HTTP_STATUS_BAD_REQUEST);
    }

    std::vector<uint8_t> body;
    StreamError s_err = fetch_response.read_body(body);
    if (s_err != StreamError::Success) {
        error(to_string(s_err));
        return HttpResponse("failure in get_fetch_response: " + to_string(s_err))
            .set_status(HTTP_STATUS_BAD_REQUEST);
    }

    return HttpResponse(body)
        .set_status(fetch_response.get_status_code())
        .set_header("Serverless", "EDJX");
}