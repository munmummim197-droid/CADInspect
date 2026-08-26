#pragma once

#include <iosfwd>
#include <string>

#include <stepcompare/reporting/report.hpp>

namespace stepcompare::reporting {

void writeJson(const Report& report, std::ostream& output);
[[nodiscard]] std::string toJson(const Report& report);

// Emits RFC 4180 records with CRLF line endings. Report metadata uses key/value
// records and component results use component records in the same stable schema.
void writeCsv(const Report& report, std::ostream& output);
[[nodiscard]] std::string toCsv(const Report& report);

}  // namespace stepcompare::reporting
