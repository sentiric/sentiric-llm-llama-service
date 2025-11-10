#include "model_handler.h"
#include "spdlog/spdlog.h"

void ModelHandler::initialize() {
    spdlog::info("Model handler initialized");
}

bool ModelHandler::is_ready() const {
    return true;
}