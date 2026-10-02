#include "utils/Csv.hpp"

#include <algorithm>
#include <sstream>

namespace app::csv {
namespace {

char detectDelimiter(const std::string& content) {
    // Compare le nombre de ',' et ';' sur la premiere ligne.
    const auto lineEnd = content.find('\n');
    const std::string header = content.substr(0, lineEnd);
    const auto commas = std::count(header.begin(), header.end(), ',');
    const auto semicolons = std::count(header.begin(), header.end(), ';');
    return semicolons > commas ? ';' : ',';
}

}  // namespace

std::vector<Row> parse(const std::string& content) {
    std::vector<Row> rows;
    if (content.empty()) return rows;

    const char delimiter = detectDelimiter(content);
    Row row;
    std::string field;
    bool inQuotes = false;

    for (size_t i = 0; i < content.size(); ++i) {
        const char c = content[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < content.size() && content[i + 1] == '"') {
                    field.push_back('"');  // guillemet echappe
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                field.push_back(c);
            }
            continue;
        }
        if (c == '"') {
            inQuotes = true;
        } else if (c == delimiter) {
            row.push_back(field);
            field.clear();
        } else if (c == '\n') {
            row.push_back(field);
            field.clear();
            rows.push_back(row);
            row.clear();
        } else if (c != '\r') {
            field.push_back(c);
        }
    }
    if (!field.empty() || !row.empty()) {
        row.push_back(field);
        rows.push_back(row);
    }

    // Supprime les lignes totalement vides.
    rows.erase(std::remove_if(rows.begin(), rows.end(),
                              [](const Row& r) {
                                  return r.empty() ||
                                         std::all_of(r.begin(), r.end(), [](const std::string& v) {
                                             return v.empty();
                                         });
                              }),
               rows.end());
    return rows;
}

std::string escape(const std::string& value) {
    const bool needsQuotes =
        value.find_first_of(",;\"\n\r") != std::string::npos;
    if (!needsQuotes) return value;
    std::string out = "\"";
    for (char c : value) {
        if (c == '"') out.push_back('"');
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

std::string write(const std::vector<Row>& rows, char delimiter) {
    std::ostringstream out;
    for (const auto& row : rows) {
        for (size_t i = 0; i < row.size(); ++i) {
            if (i > 0) out << delimiter;
            out << escape(row[i]);
        }
        out << "\r\n";
    }
    return out.str();
}

}  // namespace app::csv
