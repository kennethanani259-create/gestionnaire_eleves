#pragma once
/// Endpoints du tableau de bord, des rapports et des imports/exports.
#include "api/Router.hpp"
#include "services/ReportService.hpp"

namespace app {

class ReportController {
public:
    explicit ReportController(ReportService& reports) : reports_(reports) {}
    void registerRoutes(api::Router& router);

private:
    ReportService& reports_;
};

}  // namespace app
