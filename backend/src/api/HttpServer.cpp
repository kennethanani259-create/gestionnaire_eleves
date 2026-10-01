#include "api/HttpServer.hpp"

#include <chrono>
#include <filesystem>

#include <nlohmann/json.hpp>

#include "controllers/AuthController.hpp"
#include "controllers/ClassController.hpp"
#include "controllers/GradeController.hpp"
#include "controllers/ReportController.hpp"
#include "controllers/StudentController.hpp"
#include "core/Error.hpp"
#include "core/Logger.hpp"
#include "middleware/AuthMiddleware.hpp"
#include "middleware/ErrorHandler.hpp"
#include "repositories/AttendanceRepository.hpp"
#include "repositories/ClassRepository.hpp"
#include "repositories/GradeRepository.hpp"
#include "repositories/SchoolYearRepository.hpp"
#include "repositories/StudentRepository.hpp"
#include "repositories/SubjectRepository.hpp"
#include "repositories/TeacherRepository.hpp"
#include "repositories/UserRepository.hpp"
#include "services/AttendanceService.hpp"
#include "services/AuthService.hpp"
#include "services/ClassService.hpp"
#include "services/GradeService.hpp"
#include "services/ReportService.hpp"
#include "services/StudentService.hpp"
#include "utils/Http.hpp"

namespace app::api {

/// Toutes les dependances de l'application, construites une seule fois.
struct HttpServer::Impl {
    Config config;
    Database& db;
    httplib::Server server;

    // Repositories
    StudentRepository studentRepo;
    ClassRepository classRepo;
    SubjectRepository subjectRepo;
    TeacherRepository teacherRepo;
    GradeRepository gradeRepo;
    AttendanceRepository attendanceRepo;
    UserRepository userRepo;
    SchoolYearRepository yearRepo;

    // Services
    StudentService studentService;
    ClassService classService;
    GradeService gradeService;
    AttendanceService attendanceService;
    AuthService authService;
    ReportService reportService;

    // Middleware d'authentification (doit preceder le routeur)
    middleware::AuthMiddleware authMiddleware;

    // Controleurs
    StudentController studentController;
    ClassController classController;
    GradeController gradeController;
    AuthController authController;
    ReportController reportController;

    Router router;

    Impl(Config cfg, Database& database)
        : config(std::move(cfg)),
          db(database),
          studentRepo(db),
          classRepo(db),
          subjectRepo(db),
          teacherRepo(db),
          gradeRepo(db),
          attendanceRepo(db),
          userRepo(db),
          yearRepo(db),
          studentService(studentRepo, classRepo),
          classService(classRepo, subjectRepo, studentRepo, teacherRepo, yearRepo),
          gradeService(gradeRepo, studentRepo, subjectRepo, classRepo, attendanceRepo),
          attendanceService(attendanceRepo, studentRepo, subjectRepo, classRepo),
          authService(userRepo, config.jwtSecret, config.jwtTtlMinutes),
          reportService(studentService, classService, gradeService, attendanceService,
                        studentRepo, gradeRepo),
          authMiddleware(authService),
          studentController(studentService, gradeService, attendanceService),
          classController(classService, studentService, gradeService),
          gradeController(gradeService, attendanceService),
          authController(authService),
          reportController(reportService),
          // Le garde du routeur delegue au middleware d'authentification :
          // toute route non publique exige un jeton valide et le role requis.
          router(server, [this](const httplib::Request& req, Access access) {
              authMiddleware(req, access);
          }) {}
};

HttpServer::HttpServer(Config config, Database& db) {
    impl_ = std::make_unique<Impl>(std::move(config), db);

    // Cree le compte administrateur initial si la base ne contient aucun compte.
    impl_->authService.ensureInitialAdmin();

    registerMiddlewares();
    registerRoutes();
    registerStaticFiles();
}

HttpServer::~HttpServer() = default;

httplib::Server& HttpServer::server() { return impl_->server; }

void HttpServer::registerMiddlewares() {
    auto& server = impl_->server;

    server.set_payload_max_length(8ULL * 1024 * 1024);  // 8 Mo : protege la memoire

    // Journalisation de chaque requete avec sa duree.
    server.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        const std::string message = req.method + " " + req.path + " -> " +
                                    std::to_string(res.status) + " (" +
                                    std::to_string(res.body.size()) + " o)";
        if (res.status >= 500) {
            LOG_ERROR("http", message);
        } else if (res.status >= 400) {
            LOG_WARN("http", message);
        } else {
            LOG_DEBUG("http", message);
        }
    });

    // En-tetes communs (securite + CORS pour un frontend servi separement).
    server.set_post_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("X-Content-Type-Options", "nosniff");
        res.set_header("X-Frame-Options", "SAMEORIGIN");
        res.set_header("Referrer-Policy", "same-origin");
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    });

    // Pre-vol CORS.
    server.Options(R"(/.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    // Filet de securite ultime : aucune exception ne doit tuer le serveur.
    server.set_exception_handler(
        [](const httplib::Request& req, httplib::Response& res, std::exception_ptr ep) {
            std::string detail = "exception inconnue";
            try {
                if (ep) std::rethrow_exception(ep);
            } catch (const std::exception& e) {
                detail = e.what();
            }
            LOG_CRITICAL("http", "Exception non capturee sur " + req.path + ": " + detail);
            res.status = 500;
            res.set_content(
                middleware::errorBody("INTERNAL_ERROR", "Erreur interne du serveur"),
                "application/json; charset=utf-8");
        });

    // 404 JSON pour l'API. Important : ne jamais ecraser un corps deja produit
    // par un controleur (httplib appelle ce handler pour tout statut >= 400).
    server.set_error_handler([](const httplib::Request& req, httplib::Response& res) {
        if (!res.body.empty()) return;
        if (res.status == 404 && req.path.rfind("/api/", 0) == 0) {
            res.set_content(middleware::errorBody("NOT_FOUND", "Endpoint inexistant: " + req.path),
                            "application/json; charset=utf-8");
        }
    });
}

void HttpServer::registerRoutes() {
    auto& router = impl_->router;

    // Sante du service (public, utile pour la supervision et les tests).
    router.get(R"(/api/health)", Access::Public,
               [this](const httplib::Request&, httplib::Response& res) {
                   http::sendJson(res, 200,
                                  {{"status", "ok"},
                                   {"version", "1.0.0"},
                                   {"database", impl_->db.path()}});
               });

    impl_->authController.registerRoutes(router);
    impl_->studentController.registerRoutes(router);
    impl_->classController.registerRoutes(router);
    impl_->gradeController.registerRoutes(router);
    impl_->reportController.registerRoutes(router);
}

void HttpServer::registerStaticFiles() {
    const std::string& dir = impl_->config.frontendDir;
    std::error_code ec;
    if (dir.empty() || !std::filesystem::is_directory(dir, ec)) {
        LOG_WARN("http", "Repertoire frontend introuvable (" + dir +
                             "): seule l'API est disponible");
        return;
    }
    if (!impl_->server.set_mount_point("/", dir)) {
        LOG_WARN("http", "Montage du frontend impossible: " + dir);
        return;
    }
    LOG_INFO("http", "Frontend servi depuis " + dir);
}

bool HttpServer::run() {
    const auto& config = impl_->config;
    LOG_INFO("http", "Demarrage du serveur sur http://" + config.host + ":" +
                         std::to_string(config.port));
    if (!impl_->server.bind_to_port(config.host.c_str(), config.port)) {
        LOG_CRITICAL("http", "Impossible d'ecouter sur le port " + std::to_string(config.port));
        return false;
    }
    return impl_->server.listen_after_bind();
}

void HttpServer::stop() { impl_->server.stop(); }

}  // namespace app::api
