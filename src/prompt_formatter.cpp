#include "prompt_formatter.h"
#include <sstream>

std::string PromptFormatter::format(const std::string& user_prompt) {
    // Phi-3 için standart sohbet şablonu.
    // Bu, modelin bir komut aldığını anlamasını ve doğru cevap vermesini sağlar.
    std::ostringstream oss;
    oss << "<|user|>\n"
        << user_prompt
        << "<|end|>\n"
        << "<|assistant|>\n";
    return oss.str();
}