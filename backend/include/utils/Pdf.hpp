#pragma once
/**
 * @file Pdf.hpp
 * @brief Generateur PDF 1.4 minimal mais conforme (aucune dependance externe).
 *
 * Suffisant pour les bulletins et rapports : texte positionne, polices
 * Helvetica / Helvetica-Bold, filets horizontaux, pagination.
 * Les caracteres UTF-8 sont convertis vers WinAnsi (Latin-1).
 */
#include <string>
#include <vector>

namespace app::pdf {

/// Format A4 en points PostScript (1/72 pouce).
constexpr double kPageWidth = 595.28;
constexpr double kPageHeight = 841.89;

class Document {
public:
    Document();

    /// Demarre une nouvelle page ; le curseur revient en haut.
    void addPage();
    /// Ecrit une ligne de texte a la position courante et descend d'une ligne.
    void text(const std::string& value, double size = 11, bool bold = false,
              double indent = 0);
    /// Ecrit du texte a une position absolue (origine en bas a gauche).
    void textAt(double x, double y, const std::string& value, double size = 11,
                bool bold = false);
    /// Ligne horizontale sur toute la largeur utile.
    void horizontalRule(double thickness = 0.8);
    /// Saut vertical.
    void space(double points = 10);
    /// Position verticale courante (en points depuis le bas).
    double cursorY() const { return cursorY_; }
    void setCursorY(double y) { cursorY_ = y; }
    /// Hauteur restante avant le bas de page.
    double remaining() const;

    /// Serialise le document complet au format PDF.
    std::string render() const;

private:
    void ensurePage();

    std::vector<std::string> pages_;  ///< flux de contenu de chaque page
    double cursorY_ = 0;
    static constexpr double kMargin = 50.0;
    static constexpr double kBottomMargin = 50.0;
};

/// Convertit une chaine UTF-8 en Latin-1 et echappe les caracteres PDF speciaux.
std::string escapeText(const std::string& value);

}  // namespace app::pdf
