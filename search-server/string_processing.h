#pragma once
#include <string>
#include <vector>
#include <set>
#include <stdexcept>

using namespace std::string_literals;

std::vector<std::string> SplitIntoWords(const std::string& text);

bool IsValidWord(const std::string& word);

template <typename StringContainer>
std::set<std::string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    std::set<std::string> non_empty_strings;
    for (const std::string& str : strings) {
        if (!IsValidWord(str)) {
            throw std::invalid_argument("Стоп-слово \""s + str + "\" содержит спецсимволы"s);
        }
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}