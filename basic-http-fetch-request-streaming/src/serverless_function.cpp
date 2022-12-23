#include <cstdint>
#include <string>
#include <vector>

#include <edjx/logger.hpp>
#include <edjx/error.hpp>
#include <edjx/http.hpp>
#include <edjx/request.hpp>
#include <edjx/response.hpp>
#include <edjx/fetch.hpp>
#include <edjx/utils.hpp>
#include <edjx/stream.hpp>

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
using edjx::fetch::FetchResponsePending;
using edjx::stream::WriteStream;

static const HttpStatusCode HTTP_STATUS_OK = 200;
static const HttpStatusCode HTTP_STATUS_INTERNAL_SERVER_ERROR = 500;

HttpResponse serverless(const HttpRequest & req) {
    info("** HTTP Fetch Streaming Example **");

    // An HTTP request will be sent to this address
    Uri fetch_uri("https://httpbin.org/post");

    // Open a write stream
    FetchResponsePending fetch_resp_pending;
    WriteStream write_stream;
    HttpError err = HttpFetch(fetch_uri, HttpMethod::POST)
        .set_header("Content-Type", "text/plain")
        .send_streaming(fetch_resp_pending, write_stream);
    if (err != HttpError::Success) {
        error("Error when opening a write stream: " + to_string(err));
        return HttpResponse("Error when opening a write stream: " + to_string(err))
            .set_status(HTTP_STATUS_INTERNAL_SERVER_ERROR)
            .set_header("Serverless", "EDJX");
    }

    // Prepare data that will be streamed
    std::string content = "WHERE IS THE EDGE, ANYWAY?\nIf you ask a cloud company, they tell you the edge is their multi-billion dollar collection of server farms. A content delivery network (CDN) provider says it's their hundreds of points of presence. Wireless carriers will try to convince you it's their tens of thousands of macrocell and picocell sites.\n\nAt EDJX, we say the edge is anywhere and everywhere, a thousand feet away from you at all times. We believe computing needs to become ubiquitous, like electricity, to power billions of connected devices.\nThe edge will go so far out into the woods, you can hear the sasquatch scream.";
    bool stream_success = true;

    // Send chunks of increasing sizes (1, 2, 3, ...)
    for (int i = 0, len = 1; i < content.length(); i += len, len++) {
        // Alternate between sending chunks as text and as bytes
        std::string chunk = content.substr(i, len);
        StreamError err = write_stream.write_chunk(chunk);
        if (err != StreamError::Success) {
            error("Error when writing a text chunk: " + to_string(err));
            stream_success = false;
            break;
        }
    }

    // Close the stream
    if (stream_success) {
        // No error encountered - cleanly close the stream
        StreamError close_err = write_stream.close();
        if (close_err != StreamError::Success) {
            error("Error when closing a stream: " + to_string(close_err));
            stream_success = false;
        }
    } else {
        // There was an error - abort the stream
        StreamError close_err = write_stream.abort();
        if (close_err != StreamError::Success) {
            error("Error when aborting a stream: " + to_string(close_err));
            stream_success = false;
        }
    }

    // Exit if streaming failed
    if (!stream_success) {
        return HttpResponse("Write stream failed. See error log.")
            .set_status(HTTP_STATUS_INTERNAL_SERVER_ERROR)
            .set_header("Serverless", "EDJX");
    }

    // Get response from the server
    FetchResponse fetch_resp;
    err = fetch_resp_pending.get_fetch_response(fetch_resp);
    if (err != HttpError::Success) {
        error("Could not obtain fetch response: " + to_string(err));
        return HttpResponse("Could not obtain fetch response: " + to_string(err))
            .set_status(HTTP_STATUS_INTERNAL_SERVER_ERROR)
            .set_header("Serverless", "EDJX");
    }

    // Get body from the response
    std::vector<uint8_t> body;
    StreamError s_err = fetch_resp.read_body(body);
    if (s_err != StreamError::Success) {
        error("Could not read fetch response body: " + to_string(s_err));
        return HttpResponse("Could not read fetch response body: " + to_string(s_err))
            .set_status(HTTP_STATUS_INTERNAL_SERVER_ERROR)
            .set_header("Serverless", "EDJX");
    }

    // Send the fetch response body as a response to the client, all at once
    HttpResponse res(body);
    res.set_status(HTTP_STATUS_OK);
    res.set_header("Serverless", "EDJX");

    return res;
}