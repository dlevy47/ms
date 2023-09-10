#pragma once

#include "util/convert.hh"
#include "util/error.hh"

// Visitor is a helper type to be used with std::visit. Usage:
// 
// ```
// std::variant<std::string, double> variant;
// 
// std::visit(Visitor {
//   [&](const std::string& s) {
//     std::cout << "string: " << s;
//   },
//   [&](const double& d) {
//     std::cout << "double: " << d;
//   },
// }, variant);
// ```
//
// source: https://en.cppreference.com/w/cpp/utility/variant/visit
template<class... Ts>
struct Visitor: Ts... { using Ts::operator()...; };

// Deduction guide for Visitor.
template<class... Ts>
Visitor(Ts...) -> Visitor<Ts...>;
