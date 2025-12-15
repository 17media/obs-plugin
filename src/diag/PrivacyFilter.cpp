#include "PrivacyFilter.hpp"

#include <algorithm>
#include <sstream>

namespace seventeen {
    namespace diag {

        PrivacyFilter::PrivacyFilter()
            : m_filterLevel(FilterLevel::MODERATE), m_maskCharacter('*') {
            initializeDefaultPatterns();
        }

        std::string PrivacyFilter::filterSensitiveData(const std::string& input) {
            std::string result = input;

            for (const auto& pattern : m_patterns) {
                if (!pattern.enabled || !shouldApplyPattern(pattern)) {
                    continue;
                }

                try {
                    result = std::regex_replace(result, pattern.pattern, pattern.replacement);
                } catch (const std::regex_error&) {
                    // Skip invalid patterns
                    continue;
                }
            }

            return result;
        }

        void PrivacyFilter::addCustomPattern(const std::string& pattern,
                                             const std::string& replacement) {
            try {
                m_patterns.push_back(
                    {std::regex(pattern, std::regex_constants::icase), replacement, true});
            } catch (const std::regex_error&) {
                // Invalid regex pattern, ignore
            }
        }

        void PrivacyFilter::initializeDefaultPatterns() {
            // API Keys and Tokens
            m_patterns.push_back({std::regex(R"(api[_-]?key["\s]*[:=]["\s]*([a-zA-Z0-9_\-]{20,}))",
                                             std::regex_constants::icase),
                                  "api_key=***REDACTED***", true});

            m_patterns.push_back(
                {std::regex(R"(bearer["\s]+([a-zA-Z0-9_\-\.]{20,}))", std::regex_constants::icase),
                 "Bearer ***REDACTED***", true});

            m_patterns.push_back({std::regex(R"(authorization["\s]*:["\s]*([a-zA-Z0-9_\-\s]{10,}))",
                                             std::regex_constants::icase),
                                  "Authorization: ***REDACTED***", true});

            // Passwords
            m_patterns.push_back({std::regex(R"(password["\s]*[:=]["\s]*([^"\s\n]{3,}))",
                                             std::regex_constants::icase),
                                  "password=***REDACTED***", true});

            m_patterns.push_back(
                {std::regex(R"(pwd["\s]*[:=]["\s]*([^"\s\n]{3,}))", std::regex_constants::icase),
                 "pwd=***REDACTED***", true});

            // Personal Information (Moderate and Strict levels)
            m_patterns.push_back({
                std::regex(R"(\b\d{3}-\d{2}-\d{4}\b)", std::regex_constants::icase),
                "***SSN_REDACTED***",
                false  // Disabled by default, enabled in moderate/strict
            });

            m_patterns.push_back(
                {std::regex(R"(\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}\b)",
                            std::regex_constants::icase),
                 "***EMAIL_REDACTED***", false});

            // IP Addresses (Strict level only)
            m_patterns.push_back(
                {std::regex(R"(\b(?:[0-9]{1,3}\.){3}[0-9]{1,3}\b)", std::regex_constants::icase),
                 "***IP_REDACTED***", false});

            // URLs with credentials
            m_patterns.push_back(
                {std::regex(
                     R"((https?://)[a-zA-Z0-9._%+-]+:[^@\s]+@([a-zA-Z0-9.-]+\.[a-zA-Z]{2,}))",
                     std::regex_constants::icase),
                 "$1***CREDENTIALS_REDACTED***@$2", true});

            // Session IDs and similar
            m_patterns.push_back(
                {std::regex(R"(session[_-]?id["\s]*[:=]["\s]*([a-zA-Z0-9_\-]{10,}))",
                            std::regex_constants::icase),
                 "session_id=***REDACTED***", true});

            // Private keys
            m_patterns.push_back(
                {std::regex(
                     R"(-----BEGIN [A-Z ]+ PRIVATE KEY-----[\s\S]+?-----END [A-Z ]+ PRIVATE KEY-----)",
                     std::regex_constants::icase),
                 "-----BEGIN PRIVATE KEY-----\n***REDACTED***\n-----END PRIVATE KEY-----", true});

            // Database connection strings
            m_patterns.push_back(
                {std::regex(R"(Server=[^;]+;Database=[^;]+;User [A-Za-z]+=[^;]+;Password=[^;]+;)",
                            std::regex_constants::icase),
                 "***CONNECTION_STRING_REDACTED***", true});

            // File paths (user directories)
            m_patterns.push_back(
                {std::regex(R"([A-Za-z]:\\Users\\[^\\]+\\)", std::regex_constants::icase),
                 "C:\\Users\\***USERNAME_REDACTED***\\", false});

            m_patterns.push_back({std::regex(R"(/Users/[^/]+/)", std::regex_constants::icase),
                                  "/Users/***USERNAME_REDACTED***/", false});
        }

        std::string PrivacyFilter::maskString(const std::string& input, size_t start,
                                              size_t length) {
            if (start >= input.length() || length == 0) {
                return input;
            }

            size_t end = std::min(start + length, input.length());
            std::string result = input;

            for (size_t i = start; i < end; ++i) {
                result[i] = m_maskCharacter;
            }

            return result;
        }

        bool PrivacyFilter::shouldApplyPattern(const FilterPattern& pattern) const {
            switch (m_filterLevel) {
            case FilterLevel::STRICT:
                return true;  // Apply all patterns
            case FilterLevel::MODERATE:
                // Apply patterns that are enabled by default or marked for moderate
                return pattern.enabled || pattern.replacement.find("EMAIL") != std::string::npos ||
                       pattern.replacement.find("SSN") != std::string::npos;
            case FilterLevel::MINIMAL:
                // Only apply critical patterns (passwords, API keys, etc.)
                return pattern.enabled &&
                       (pattern.replacement.find("REDACTED") != std::string::npos ||
                        pattern.replacement.find("password") != std::string::npos);
            }
            return false;
        }

    }  // namespace diag
}  // namespace seventeen
