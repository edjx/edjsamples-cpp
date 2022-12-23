#include <edjx/logger.hpp>
#include <edjx/request.hpp>
#include <edjx/response.hpp>
#include <edjx/fetch.hpp>
#include <edjx/http.hpp>
#include <edjx/error.hpp>

using edjx::logger::info;
using edjx::logger::error;
using edjx::request::HttpRequest;
using edjx::response::HttpResponse;
using edjx::fetch::HttpFetch;
using edjx::fetch::FetchResponse;
using edjx::http::HttpMethod;
using edjx::http::Uri;
using edjx::http::HttpStatusCode;
using edjx::error::HttpError;
using edjx::error::StreamError;

static const HttpStatusCode HTTP_STATUS_OK = 200;
static const HttpStatusCode HTTP_STATUS_BAD_REQUEST = 400;
static const HttpStatusCode HTTP_STATUS_METHOD_NOT_ALLOWED = 405;


HttpResponse serverless(const HttpRequest & req) {
    info("**Incoming HTTP request method function**");

    HttpMethod fetch_method;
    Uri fetch_uri;

    switch (req.get_method()) {
        case HttpMethod::GET:
            fetch_method = HttpMethod::GET;
            fetch_uri = Uri("https://httpbin.org/get");
            break;
        case HttpMethod::POST:
            fetch_method = HttpMethod::POST;
            fetch_uri = Uri("https://httpbin.org/post");
            break;
        case HttpMethod::PUT:
            fetch_method = HttpMethod::PUT;
            fetch_uri = Uri("https://httpbin.org/put");
            break;
        case HttpMethod::DELETE:
            fetch_method = HttpMethod::DELETE;
            fetch_uri = Uri("https://httpbin.org/delete");
            break;
        case HttpMethod::PATCH:
            fetch_method = HttpMethod::PATCH;
            fetch_uri = Uri("https://httpbin.org/patch");
            break;
        default:
            return HttpResponse().set_status(HTTP_STATUS_METHOD_NOT_ALLOWED);
    }

    FetchResponse fetch_res;
    HttpError err = HttpFetch(fetch_uri, fetch_method)
        .set_header("accept", "application/json")
        .send(fetch_res);
    if (err != HttpError::Success) {
        error(to_string(err));
        return HttpResponse("failure in fetch req for given method : " + to_string(err))
            .set_status(HTTP_STATUS_BAD_REQUEST);
    }

    std::vector<uint8_t> body;
    StreamError s_err = fetch_res.read_body(body);
    if (s_err != StreamError::Success) {
        error(to_string(s_err));
        return HttpResponse("failure in get_fetch_response: " + to_string(s_err))
            .set_status(HTTP_STATUS_BAD_REQUEST);
    }

    HttpResponse res(body);
    res.set_status(HTTP_STATUS_OK);
    res.set_headers(fetch_res.get_headers());
    res.set_header("Serverless", "EDJX");

    return res;
}