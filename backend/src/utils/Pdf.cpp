#include "utils/Pdf.hpp"

#include <iomanip>
#include <sstream>

namespace app::pdf {
namespace {

/// Decode l'UTF-8 et replie vers Latin-1 (WinAnsi) ; remplace l'inconnu par '?'.
std::string utf8ToLatin1(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size();) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        unsigned int codepoint = 0;
        size_t length = 1;
        if (c < 0x80) {
            codepoint = c;
        } else if ((c & 0xE0) == 0xC0) {
            codepoint = c & 0x1Fu;
            length = 2;
        } else if ((c & 0xF0) == 0xE0) {
            codepoint = c & 0x0Fu;
            length = 3;
        } else if ((c & 0xF8) == 0xF0) {
            codepoint = c & 0x07u;
            length = 4;
        } else {
            out.push_back('?');
            ++i;
            continue;
        }
        if (i + length > value.size()) {
            out.push_back('?');
            break;
        }
        for (size_t k = 1; k < length; ++k) {
            codepoint = (codepoint << 6) |
                        (static_cast<unsigned char>(value[i + k]) & 0x3Fu);
        }
        out.push_back(codepoint <= 0xFF ? static_cast<char>(codepoint) : '?');
        i += length;
    }
    return out;
}

std::string number(double value) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(2) << value;
    return os.str();
}

}  // namespace

std::string escapeText(const std::string& value) {
    const std::string latin1 = utf8ToLatin1(value);
    std::string out;
    out.reserve(latin1.size() + 8);
    for (char c : latin1) {
        if (c == '(' || c == ')' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

Document::Document() { addPage(); }

void Document::addPage() {
    pages_.emplace_back();
    cursorY_ = kPageHeight - kMargin;
}

void Document::ensurePage() {
    if (pages_.empty()) addPage();
    if (cursorY_ < kBottomMargin) addPage();
}

double Document::remaining() const { return cursorY_ - kBottomMargin; }

void Document::text(const std::string& value, double size, bool bold, double indent) {
    ensurePage();
    textAt(kMargin + indent, cursorY_, value, size, bold);
    cursorY_ -= size * 1.45;
}

void Document::textAt(double x, double y, const std::string& value, double size, bool bold) {
    ensurePage();
    std::ostringstream stream;
    stream << "BT /" << (bold ? "F2" : "F1") << ' ' << number(size) << " Tf " << number(x)
           << ' ' << number(y) << " Td (" << escapeText(value) << ") Tj ET\n";
    pages_.back() += stream.str();
}

void Document::horizontalRule(double thickness) {
    ensurePage();
    std::ostringstream stream;
    stream << number(thickness) << " w " << number(kMargin) << ' ' << number(cursorY_)
           << " m " << number(kPageWidth - kMargin) << ' ' << number(cursorY_) << " l S\n";
    pages_.back() += stream.str();
    cursorY_ -= 8;
}

void Document::space(double points) {
    ensurePage();
    cursorY_ -= points;
}

std::string Document::render() const {
    // Objets : 1 catalogue, 2 arbre de pages, 3-4 polices, puis page+contenu.
    std::vector<std::string> objects;
    const size_t pageCount = pages_.empty() ? 1 : pages_.size();
    const int firstPageObject = 5;

    std::ostringstream kids;
    for (size_t i = 0; i < pageCount; ++i) {
        kids << (firstPageObject + static_cast<int>(i) * 2) << " 0 R ";
    }

    objects.push_back("<< /Type /Catalog /Pages 2 0 R >>");
    objects.push_back("<< /Type /Pages /Kids [" + kids.str() + "] /Count " +
                      std::to_string(pageCount) + " >>");
    objects.push_back(
        "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>");
    objects.push_back(
        "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold /Encoding /WinAnsiEncoding >>");

    for (size_t i = 0; i < pageCount; ++i) {
        const int contentObject = firstPageObject + static_cast<int>(i) * 2 + 1;
        std::ostringstream page;
        page << "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " << number(kPageWidth) << ' '
             << number(kPageHeight) << "] /Resources << /Font << /F1 3 0 R /F2 4 0 R >> >> "
             << "/Contents " << contentObject << " 0 R >>";
        objects.push_back(page.str());

        const std::string& content = i < pages_.size() ? pages_[i] : std::string{};
        std::ostringstream stream;
        stream << "<< /Length " << content.size() << " >>\nstream\n" << content << "endstream";
        objects.push_back(stream.str());
    }

    std::ostringstream out;
    out << "%PDF-1.4\n";
    std::vector<size_t> offsets;
    offsets.reserve(objects.size());
    for (size_t i = 0; i < objects.size(); ++i) {
        offsets.push_back(static_cast<size_t>(out.tellp()));
        out << (i + 1) << " 0 obj\n" << objects[i] << "\nendobj\n";
    }

    const size_t xrefOffset = static_cast<size_t>(out.tellp());
    out << "xref\n0 " << (objects.size() + 1) << "\n";
    out << "0000000000 65535 f \n";
    for (size_t offset : offsets) {
        out << std::setw(10) << std::setfill('0') << offset << " 00000 n \n";
    }
    out << "trailer\n<< /Size " << (objects.size() + 1) << " /Root 1 0 R >>\nstartxref\n"
        << xrefOffset << "\n%%EOF\n";
    return out.str();
}

}  // namespace app::pdf
