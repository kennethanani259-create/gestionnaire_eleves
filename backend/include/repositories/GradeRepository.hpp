#pragma once
#include <optional>
#include <vector>

#include "database/Database.hpp"
#include "models/Grade.hpp"

namespace app {

/// Agregat SQL : moyenne d'un eleve dans une matiere (notes ramenees sur 20).
struct SubjectAverageRow {
    long long studentId = 0;
    long long subjectId = 0;
    std::string subjectName;
    std::string subjectCode;
    double coefficient = 1.0;
    long long gradeCount = 0;
    double average20 = 0.0;
    double best20 = 0.0;
    double worst20 = 0.0;
};

struct GradeFilter {
    std::optional<long long> studentId;
    std::optional<long long> subjectId;
    std::optional<long long> classId;
    std::optional<int> term;
    std::optional<EvalType> evalType;
    int limit = 0;   ///< 0 = pas de limite
    int offset = 0;
};

class IGradeRepository {
public:
    virtual ~IGradeRepository() = default;
    virtual long long create(const Grade& value) = 0;
    virtual void update(const Grade& value) = 0;
    virtual bool remove(long long id) = 0;
    virtual std::optional<Grade> findById(long long id) = 0;
    virtual std::vector<Grade> find(const GradeFilter& filter) = 0;
    /// Moyennes par matiere d'un eleve (agregation SQL).
    virtual std::vector<SubjectAverageRow> subjectAverages(long long studentId,
                                                           std::optional<int> term) = 0;
    /// Moyennes par matiere de toute une classe (une ligne par eleve et matiere).
    virtual std::vector<SubjectAverageRow> classSubjectAverages(long long classId,
                                                                std::optional<int> term) = 0;
};

class GradeRepository : public IGradeRepository {
public:
    explicit GradeRepository(Database& db) : db_(db) {}
    long long create(const Grade& value) override;
    void update(const Grade& value) override;
    bool remove(long long id) override;
    std::optional<Grade> findById(long long id) override;
    std::vector<Grade> find(const GradeFilter& filter) override;
    std::vector<SubjectAverageRow> subjectAverages(long long studentId,
                                                   std::optional<int> term) override;
    std::vector<SubjectAverageRow> classSubjectAverages(long long classId,
                                                        std::optional<int> term) override;

private:
    Database& db_;
};

}  // namespace app
