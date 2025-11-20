#pragma once

#include "llama.h"
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <cstdint>
#include <sstream>
#include <algorithm>
#include <cstring>

// --- EKSİK TİP TANIMLARI (MANUEL ENJEKSİYON) ---
// llama.h içinde bu versiyonda görünmeyen tanımları buraya ekliyoruz.
// Bu tanımlar llama.cpp'nin iç yapısıyla birebir eşleşmelidir.

enum llama_gretype {
    LLAMA_GRETYPE_END            = 0,
    LLAMA_GRETYPE_ALT            = 1,
    LLAMA_GRETYPE_RULE_REF       = 2,
    LLAMA_GRETYPE_CHAR           = 3,
    LLAMA_GRETYPE_CHAR_NOT       = 4,
    LLAMA_GRETYPE_CHAR_ALT       = 5,
    LLAMA_GRETYPE_CHAR_RNG_UPPER = 6,
};

typedef struct llama_grammar_element {
    enum llama_gretype type;
    uint32_t           value; // Unicode code point or rule ID
} llama_grammar_element;

// Fonksiyon İmzaları (llama.cpp içinde var ama header'da gizli olabilir)
extern "C" {
    // Grammar oluşturucu
    struct llama_grammar * llama_grammar_init(
            const llama_grammar_element ** rules,
            size_t n_rules,
            size_t start_rule_index);

    // Grammar temizleyici
    void llama_grammar_free(struct llama_grammar * grammar);
}

// --- PARSER IMPLEMENTASYONU ---

namespace grammar_parser {

// GBNF Parse State
struct parse_state {
    std::map<std::string, uint32_t> symbol_ids;
    std::vector<std::vector<llama_grammar_element>> rules;

    uint32_t get_symbol_id(const std::string & name) {
        if (symbol_ids.find(name) == symbol_ids.end()) {
            symbol_ids[name] = rules.size();
            rules.push_back({});
        }
        return symbol_ids[name];
    }

    void add_rule(uint32_t rule_id, const std::vector<llama_grammar_element> & rule) {
        if (rule_id >= rules.size()) {
            rules.resize(rule_id + 1);
        }
        rules[rule_id] = rule;
    }
    
    // API'ye uygun formatta kuralları döndürür
    std::vector<const llama_grammar_element *> c_rules() const {
        std::vector<const llama_grammar_element *> out;
        for (const auto & rule : rules) {
            out.push_back(rule.data());
        }
        return out;
    }
};

// Basit GBNF Parser Yardımcıları
inline bool is_word_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
}

inline std::pair<uint32_t, const char *> parse_hex(const char * src, int size) {
    uint32_t val = 0;
    const char * p = src;
    for (int i = 0; i < size; i++) {
        uint32_t d = 0;
        if (*p >= '0' && *p <= '9') d = *p - '0';
        else if (*p >= 'a' && *p <= 'f') d = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'F') d = *p - 'A' + 10;
        else return {0, src}; 
        val = (val << 4) | d;
        p++;
    }
    return {val, p};
}

inline const char * parse_space(const char * src, bool newline_ok) {
    const char * pos = src;
    while (*pos) {
        if (*pos == ' ' || *pos == '\t' || *pos == '\r' || (newline_ok && *pos == '\n')) {
            pos++;
        } else if (*pos == '#') {
            while (*pos && *pos != '\n') pos++;
        } else {
            break;
        }
    }
    return pos;
}

inline parse_state parse(const char * src) {
    parse_state state;
    const char * pos = src;
    
    while (*pos) {
        pos = parse_space(pos, true);
        if (!*pos) break;

        // Kural Adı
        const char * name_start = pos;
        while (is_word_char(*pos)) pos++;
        std::string name(name_start, pos - name_start);
        uint32_t rule_id = state.get_symbol_id(name);

        pos = parse_space(pos, false);
        if (*pos != ':' || *(pos+1) != ':' || *(pos+2) != '=') {
             throw std::runtime_error("Expected '::=' after rule name: " + name);
        }
        pos += 3;
        pos = parse_space(pos, true);

        // Alternatifleri Parse Et
        std::vector<llama_grammar_element> current_rule;
        
        while (true) {
            // sequence
            while (true) {
                pos = parse_space(pos, true);
                if (!*pos || *pos == '|') break; // End of alternate
                
                if (*pos == '"') { // String literal
                    pos++;
                    while (*pos && *pos != '"') {
                        uint32_t char_val = (uint8_t)*pos;
                        if (*pos == '\\') {
                            pos++;
                            if (*pos == 'x') { pos++; auto res = parse_hex(pos, 2); char_val = res.first; pos = res.second; }
                            else if (*pos == 'u') { pos++; auto res = parse_hex(pos, 4); char_val = res.first; pos = res.second; }
                            else if (*pos == 'U') { pos++; auto res = parse_hex(pos, 8); char_val = res.first; pos = res.second; }
                            else if (*pos == 't') char_val = '\t';
                            else if (*pos == 'r') char_val = '\r';
                            else if (*pos == 'n') char_val = '\n';
                            else char_val = *pos; 
                        }
                        current_rule.push_back({LLAMA_GRETYPE_CHAR, char_val});
                        pos++;
                    }
                    pos++; // skip quote
                } else if (*pos == '[') { // Char range
                    pos++;
                    bool negate = false;
                    if (*pos == '^') { negate = true; pos++; }
                    
                    while (*pos && *pos != ']') {
                        uint32_t start = (uint8_t)*pos;
                         if (*pos == '\\') {
                            pos++;
                            start = (uint8_t)*pos;
                        }
                        pos++;
                        uint32_t end = start;
                        if (*pos == '-') {
                            pos++;
                            end = (uint8_t)*pos;
                             if (*pos == '\\') { pos++; end = (uint8_t)*pos; }
                            pos++;
                        }
                        if (negate) {
                             current_rule.push_back({LLAMA_GRETYPE_CHAR_NOT, start});
                        } else {
                            current_rule.push_back({LLAMA_GRETYPE_CHAR_ALT, start});
                        }
                    }
                    pos++;
                } else if (is_word_char(*pos)) { // Rule reference
                    const char * ref_start = pos;
                    while (is_word_char(*pos)) pos++;
                    std::string ref_name(ref_start, pos - ref_start);
                    current_rule.push_back({LLAMA_GRETYPE_RULE_REF, state.get_symbol_id(ref_name)});
                } else if (*pos == '(') {
                    throw std::runtime_error("Grouping (...) not supported in micro-parser yet.");
                } else if (*pos == '*' || *pos == '+' || *pos == '?') {
                     throw std::runtime_error("Repetition operators (*,+,?) not supported directly. Use recursive rules.");
                } else {
                    break;
                }
            }
            
            if (*pos == '|') {
                current_rule.push_back({LLAMA_GRETYPE_ALT, 0});
                pos++;
            } else {
                break;
            }
        }
        
        current_rule.push_back({LLAMA_GRETYPE_END, 0});
        state.add_rule(rule_id, current_rule);
        
        if (!*pos) break;
    }
    
    return state;
}

} // namespace grammar_parser