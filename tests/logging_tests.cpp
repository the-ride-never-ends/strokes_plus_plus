#include "logging/structured_logger.h"
#include "test_support.h"
#include <filesystem>
#include <fstream>
#include <string>

namespace strokes::tests {
void run_logging_tests() {
    const auto directory=std::filesystem::temp_directory_path()/"strokes-plus-plus-logger-tests";
    std::error_code error;std::filesystem::remove_all(directory,error);
    {
        logging::StructuredLogger logger(directory/"strokes.log");
        check(logger.ready(),"structured logger creates its directory and file");
        check(logger.log("gesture_start",{{"process","notepad.exe"},{"x",-120.0}}),"structured event is written");
        check(logger.log("recognition_result",{{"gesture","left"},{"score",0.91}}),"second structured event is written");
    }
    std::ifstream input(directory/"strokes.log");std::string line;int records=0;
    while(std::getline(input,line)){auto parsed=config::json::parse(line);check(static_cast<bool>(parsed),"each log line is valid JSON");
        auto* object=parsed.value->get_if<config::json::Object>();check(object&&object->contains("event")&&object->contains("timestamp_ms"),"log record has required fields");++records;}
    check(records==2,"logger appends one JSON object per event");
    std::filesystem::remove_all(directory,error);
}
}  // namespace strokes::tests
