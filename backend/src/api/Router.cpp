#include "api/Router.hpp"

#include <utility>

namespace app::api {

Handler Router::wrap(const std::string& method, const std::string& pattern, Access access,
                     Handler handler) {
    Guard guard = guard_;
    Handler guarded = [guard, access, handler = std::move(handler)](
                          const httplib::Request& req, httplib::Response& res) {
        if (guard) guard(req, access);  // leve une exception si l'acces est refuse
        handler(req, res);
    };
    return middleware::withErrorHandling(method + " " + pattern, std::move(guarded));
}

void Router::get(const std::string& pattern, Access access, Handler handler) {
    server_.Get(pattern, wrap("GET", pattern, access, std::move(handler)));
}
void Router::post(const std::string& pattern, Access access, Handler handler) {
    server_.Post(pattern, wrap("POST", pattern, access, std::move(handler)));
}
void Router::put(const std::string& pattern, Access access, Handler handler) {
    server_.Put(pattern, wrap("PUT", pattern, access, std::move(handler)));
}
void Router::del(const std::string& pattern, Access access, Handler handler) {
    server_.Delete(pattern, wrap("DELETE", pattern, access, std::move(handler)));
}

}  // namespace app::api
