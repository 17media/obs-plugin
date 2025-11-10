#pragma once

#include <string>
#include <vector>
#include <regex>

namespace seventeen {
namespace diag {

class PrivacyFilter {
public:
    PrivacyFilter();
    ~PrivacyFilter() = default;
    
    std::string filterSensitiveData(const std::string& input);
    
    void addCustomPattern(const std::string& pattern, const std::string& replacement);
    
    void setMaskCharacter(char mask) { m_maskCharacter = mask; }
    
    enum class FilterLevel {
        STRICT,    // Remove all potentially sensitive data
        MODERATE,  // Remove obvious sensitive data
        MINIMAL    // Remove only critical data like passwords
    };
    
    void setFilterLevel(FilterLevel level) { m_filterLevel = level; }

private:
    struct FilterPattern {
        std::regex pattern;
        std::string replacement;
        bool enabled;
    };
    
    std::vector<FilterPattern> m_patterns;
    FilterLevel m_filterLevel;
    char m_maskCharacter;
    
    void initializeDefaultPatterns();
    std::string maskString(const std::string& input, size_t start, size_t length);
    bool shouldApplyPattern(const FilterPattern& pattern) const;
};

} // namespace diag
} // namespace seventeen
