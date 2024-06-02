#pragma once

// NOLINTBEGIN(readability-identifier-naming)
template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
// NOLINTEND(readability-identifier-naming)
