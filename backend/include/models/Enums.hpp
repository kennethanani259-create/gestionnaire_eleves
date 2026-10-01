#pragma once
/**
 * @file Enums.hpp
 * @brief Enumerations metier typees + conversions chaine <-> enum.
 *
 * Les fonctions parse*() levent ValidationError si la valeur est inconnue :
 * la validation des valeurs d'enumeration est donc centralisee ici.
 */
#include <string>
#include <vector>

namespace app {

enum class Gender { Male, Female };
enum class StudentStatus { Active, Inactive, Transferred, Expelled };
enum class EvalType { Homework, Quiz, Exam, Lab, Project, Continuous };
enum class AttendanceStatus { Present, Absent, Excused, Late };
enum class UserRole { Admin, Teacher, Viewer };

std::string toString(Gender value);
std::string toString(StudentStatus value);
std::string toString(EvalType value);
std::string toString(AttendanceStatus value);
std::string toString(UserRole value);

Gender parseGender(const std::string& value);
StudentStatus parseStudentStatus(const std::string& value);
EvalType parseEvalType(const std::string& value);
AttendanceStatus parseAttendanceStatus(const std::string& value);
UserRole parseUserRole(const std::string& value);

/// Libelles francais destines a l'affichage et aux exports PDF.
std::string label(Gender value);
std::string label(StudentStatus value);
std::string label(EvalType value);
std::string label(AttendanceStatus value);
std::string label(UserRole value);

/// Niveau de privilege croissant : Viewer(0) < Teacher(1) < Admin(2).
int privilegeLevel(UserRole role);

}  // namespace app
