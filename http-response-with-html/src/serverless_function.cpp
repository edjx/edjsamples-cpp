#include <cstdint>
#include <string>
#include <vector>

#include <edjx/logger.hpp>
#include <edjx/request.hpp>
#include <edjx/response.hpp>
#include <edjx/http.hpp>

using edjx::logger::info;
using edjx::request::HttpRequest;
using edjx::response::HttpResponse;
using edjx::http::HttpStatusCode;

static const HttpStatusCode HTTP_STATUS_OK = 200;

static const char * const WEBSITE_HTML_HOME =
"<html lang=\"en\" dir=\"ltr\">\n\
  <head>\n\
    <meta charset=\"UTF-8\">\n\
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n\
    <title> Home | Serverless Function | EDJX</title>\n\
   </head>\n\
<body>\n\
  <nav>\n\
    <div class=\"menu\">\n\
      <div class=\"logo\">\n\
        <a href=\"#\">EDJX</a>\n\
      </div>\n\
      <ul>\n\
        <li><a href=\"?page=home\">Home</a></li>\n\
        <li><a href=\"?page=about\">About</a></li>\n\
        <li><a href=\"?page=services\">Services</a></li>\n\
        <li><a href=\"?page=contact\">Contact</a></li>\n\
      </ul>\n\
    </div>\n\
  </nav>\n\
  <div class=\"img\"></div>\n\
  <div class=\"center\">\n\
    <div class=\"title\">Deploy Serverless  Functions @Edge</div>\n\
    <div class=\"btns\">\n\
      <button>Learn More</button>\n\
      <button>Subscribe</button>\n\
    </div>\n\
  </div>\n\
</body>\n\
</html>\n\
";

static const char * const ABOUT_HTML =
"<html lang=\"en\" dir=\"ltr\">\n\
  <head>\n\
    <meta charset=\"UTF-8\">\n\
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n\
    <title> About | Serverless Function | EDJX</title>\n\
   </head>\n\
<body>\n\
<h1>About</h1>\n\
<p>If you ask a cloud company, they tell you the edge is their multi-billion dollar collection of server farms. A content delivery network (CDN) provider says it's their hundreds of points of presence. Wireless carriers will try to convince you it's their tens of thousands of macrocell and picocell sites.<p>\n\
<p>At EDJX, we say the edge is anywhere and everywhere, a thousand feet away from you at all times. We believe computing needs to become ubiquitous, like electricity, to power billions of connected devices.</p>\n\
</body>\n\
</html>\n\
";

static const char * const SERVICES_HTML =
"<html lang=\"en\" dir=\"ltr\">\n\
  <head>\n\
    <meta charset=\"UTF-8\">\n\
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n\
    <title> Services | Serverless Function | EDJX</title>\n\
   </head>\n\
<body>\n\
  <nav>\n\
    <div class=\"menu\">\n\
      <div class=\"logo\">\n\
        <a href=\"#\">EDJX Services</a>\n\
      </div>\n\
      <ul>\n\
        <li><a href=\"#\">CDN</a></li>\n\
        <li><a href=\"#\">Serverless Computing</a></li>\n\
        <li><a href=\"#\">Edge Systems</a></li>\n\
        <li><a href=\"#\">Serverless DB</a></li>\n\
      </ul>\n\
    </div>\n\
  </nav>\n\
</body>\n\
</html>\n\
";

static const char * const CONTACT_HTML = 
"<html lang=\"en\" dir=\"ltr\">\n\
  <head>\n\
    <meta charset=\"UTF-8\">\n\
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n\
    <title> Services | Serverless Function | EDJX</title>\n\
   </head>\n\
<body>\n\
<div class=\"tn-atom\" field=\"tn_text_1571256120374\">\n\
We're here answer all your questions, provide you with a live demo, or talk about your specific requirements. \n\
Drop us a note or give us a call.<br><br>\n\
<strong>SALES</strong><br><strong> </strong>hello@edjx.io<br><br><strong>SUPPORT</strong>\n\
<br>support@edjx.io<br><br><strong>HEADQUARTERS</strong><br><strong> </strong>\n\
EDJX, Inc.<br>8601 Six Forks Road, Suite 400<br>Raleigh, NC 27615\n\
</div>\n\
</body>\n\
</html>";

std::string query_param_by_name(const HttpRequest & req, const std::string & param_name) {
    std::string uri = req.get_uri().as_string();
    std::vector<std::pair<std::string, std::string>> uri_parsed;

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
                    uri_parsed.push_back(make_pair(name, value));
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
            uri_parsed.push_back(make_pair(name, value));
        }

        for (const auto & parameter : uri_parsed) {
            if (parameter.first == param_name) {
                return parameter.second;
            }
        }
    }

    return "";
}

HttpResponse serverless(const HttpRequest & req) {
    info("**HTTP response with HTML function**");

    std::string page = query_param_by_name(req, "page");
    if (page.empty()) {
        page = "home";
    }

    HttpResponse res;

    if (page == "about") {
        res = HttpResponse(ABOUT_HTML);
    } else if (page == "services") {
        res = HttpResponse(SERVICES_HTML);
    } else if (page == "contact") {
        res = HttpResponse(CONTACT_HTML);
    } else /* "home" */ {
        res = HttpResponse(WEBSITE_HTML_HOME);
    }

    res.set_status(HTTP_STATUS_OK)
        .set_header("Content-Type", "text/html")
        .set_header("Serverless", "EDJX");

    return res;
}