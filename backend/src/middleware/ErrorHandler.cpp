#include "middleware/ErrorHandler.hpp"

#include <nlohmann/json.hpp>

#include "core/Error.hpp"
#include "core/Logger.hpp"
#include "utils/Http.hpp"

namespace app::middleware {

std::string errorBody(const std::string& code, const std::string& message) {
    const nlohmann::json body{{"error", {{"code", code}, {"message", message}}}};
    return body.dump();
}

Handler withErrorHandling(std::string route, Handler handler) {
    return [route = std::move(route), handler = std::move(handler)](
               const httplib::Request& req, httplib::Response& res) {
        const std::string context = req.method + " " + req.path;
        try {
            handler(req, res);
        } catch (const AppException& e) {
            nlohmann::json error{{"code", e.code()}, {"message", e.what()}};
            if (!e.details().empty()) error["details"] = e.details();

            const std::string message = std::string(e.what()) + " [" + context + "]";
            if (e.httpStatus() >= 500) {
                LOG_ERROR(route, message);
            } else if (e.httpStatus() == 401 || e.httpStatus() == 403) {
                LOG_WARN(route, message);
            } else {
                LOG_INFO(route, message);
            }
            http::sendJson(res, e.httpStatus(), nlohmann::json{{"error", error}});
        } catch (const nlohmann::json::exception& e) {
            LOG_WARN(route, std::string("JSON invalide: ") + e.what() + " [" + context + "]");
            http::sendJson(res, 400,
                           nlohmann::json{{"error",
                                           {{"code", "INVALID_JSON"},
                                            {"message", "Corps de requete JSON invalide"}}}});
        } catch (const std::exception& e) {
            // Filet de securite : le detail technique reste dans les logs,
            // le client ne recoit jamais d'information interne.
            LOG_CRITICAL(route, std::string("Exception non geree: ") + e.what() + " [" + context + "]");
            http::sendJson(res, 500,
                           nlohmann::json{{"error",
                                           {{"code", "INTERNAL_ERROR"},
                                            {"message", "Erreur interne du serveur"}}}});
        } catch (...) {
            LOG_CRITICAL(route, "Exception inconnue [" + context + "]");
            http::sendJson(res, 500,
                           nlohmann::json{{"error",
                                           {{"code", "INTERNAL_ERROR"},
                                            {"message", "Erreur interne du serveur"}}}});
        }
    };
}

}  // namespace app::middleware
