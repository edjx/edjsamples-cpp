#include <edjx/logger.hpp>
#include <edjx/error.hpp>
#include <edjx/request.hpp>
#include <edjx/response.hpp>
#include <edjx/fetch.hpp>
#include <edjx/http.hpp>

using edjx::logger::info;
using edjx::logger::error;
using edjx::error::HttpError;
using edjx::request::HttpRequest;
using edjx::response::HttpResponse;
using edjx::fetch::HttpFetch;
using edjx::fetch::FetchResponse;
using edjx::http::Uri;
using edjx::http::HttpMethod;
using edjx::http::HttpStatusCode;

static const HttpStatusCode HTTP_STATUS_OK = 200;
static const HttpStatusCode HTTP_STATUS_BAD_REQUEST = 400;


HttpResponse serverless(const HttpRequest & req) {
    info("**Basic HTTP request and response function**");

    Uri fetch_uri("https://httpbin.org/get");

    FetchResponse fetch_res;
    HttpError err = HttpFetch(fetch_uri, HttpMethod::GET).send(fetch_res);
    if (err != HttpError::Success) {
        error(to_string(err));
        return HttpResponse("failure in fetch req : " + to_string(err))
            .set_status(HTTP_STATUS_BAD_REQUEST);
    }

    return HttpResponse(fetch_res.get_body())
        .set_status(HTTP_STATUS_OK)
        .set_header("Serverless", "EDJX");
}