#include "string_processing.h"
#include <iostream>
#include <algorithm>

std::vector<std::string_view> SplitIntoWords(std::string_view text) {
    std::vector<std::string_view> words;
    text.remove_prefix(std::min(text.size(), static_cast<size_t>(text.find_first_not_of(" "))));

    while (!text.empty()) {
        size_t space = text.find(' ');
        size_t amount = (space == text.npos ? text.size() : space);
        auto str_plus = text.substr(0, amount);
        words.push_back(str_plus);

        text.remove_prefix(std::min(text.size(), amount));
        text.remove_prefix(std::min(text.size(), static_cast<size_t>(text.find_first_not_of(" "))));
    }

    return words;
}

bool IsValidWord(const std::string_view word) {
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}