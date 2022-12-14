#include <cstdlib>
#include <cstdint>

#include <edjx/logger.hpp>
#include <edjx/request.hpp>
#include <edjx/response.hpp>
#include <edjx/error.hpp>
#include <edjx/http.hpp>

using edjx::logger::error;
using edjx::request::HttpRequest;
using edjx::response::HttpResponse;
using edjx::error::HttpError;
using edjx::http::HttpStatusCode;

static const HttpStatusCode HTTP_STATUS_BAD_REQUEST = 400;

extern HttpResponse serverless(const HttpRequest & req);

int main(void) {
    HttpRequest req;
    HttpError err = HttpRequest::from_client(req);
    if (err != HttpError::Success) {
        error(edjx::error::to_string(err));
        HttpResponse().set_status(HTTP_STATUS_BAD_REQUEST).send();
        return EXIT_FAILURE;
    }

    HttpResponse res = serverless(req);
    err = res.send();
    if (err != HttpError::Success) {
        error(edjx::error::to_string(err));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

//
// edjExecutor calls init() instead of _start()
// (constructors of global objects are not called if _start() is not executed)
//
extern "C" void _start(void);

__attribute__((export_name("init")))
extern "C" void init(void) {
    _start();
}