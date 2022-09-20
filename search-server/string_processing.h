#pragma once
#include <string>
#include <vector>
#include <set>
#include <stdexcept>

using namespace std::string_literals;

std::vector<std::string_view> SplitIntoWords(std::string_view text);

bool IsValidWord(const std::string_view word);

template <typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    std::set<std::string, std::less<>> non_empty_strings;
    for (const std::string_view str : strings) {
        if (!IsValidWord(str)) {
            throw std::invalid_argument("Стоп-слово \""s + std::string(str) + "\" содержит спецсимволы"s);
        }
        if (!str.empty()) {
            non_empty_strings.insert(std::string(str));
        }
    }
    return non_empty_strings;
}