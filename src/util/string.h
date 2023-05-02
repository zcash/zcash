// Copyright (c) 2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_UTIL_STRING_H
#define ZCASH_UTIL_STRING_H

#include <functional>
#include <sstream>

/// Turns an arbitrary container into an Oxford-comma formatted list.
template <template<class...> class C, class T, class Func>
std::string FormatList(
        const C<T>& container,
        /// the text to use before the last element of the container (std::nullopt will leave a
        /// single space before the final element, while `""` will leave two spaces).
        const std::optional<std::string>& conjunction,
        /// how to format an individual element from the container (std::function<std::string(const T&)>)
        Func formatElement) {
    std::ostringstream list;
    bool initialElement = true;
    for (auto it = container.cbegin(); it != container.cend(); std::advance(it, 1)) {
        if (!initialElement) {
            // separator after the previous element
            list << (container.size() == 2 ? " " : ", ");

            // insert the correlating conjunction if we’re at the last element
            if (std::distance(it, container.cend()) == 1 && conjunction.has_value()) {
                list << conjunction.value() << " ";
            }
        }
        list << formatElement(*it);
        initialElement = false;
    }
    return list.str();
}

#endif // ZCASH_UTIL_STRING_H
