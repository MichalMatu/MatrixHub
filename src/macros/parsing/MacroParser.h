#pragma once

#include <Arduino.h>

#include <FS.h>
#include <LittleFS.h>

#include <memory>
#include <macros/model/MacroDefinitions.h>

namespace MACROS {

class MacroParser {
public:
    MacroParser();
    ~MacroParser();

    // Initialize parser with a file path
    bool begin(const char* filename);
    
    // Initialize parser from in-memory content (no file handle kept open)
    bool beginFromContent(const uint8_t* data, size_t len);
    
    // Cleanup resources
    void end();

    // Fetch the next parsed command. Returns false on EOF.
    bool next(MacroCommand& cmd);

    uint32_t getLineNumber() const { return _lineNumber; }
    const char* getError() const { return _error.c_str(); }

private:
    File _file;
    bool _fileOpen;
    bool _memoryMode;  // true when parsing from in-memory content
    const uint8_t* _memoryData; // Direct pointer to content in memory mode
    uint32_t _lineNumber;
    PsramString _error;

    // --- Optimization: PSRAM Buffer for Block Reads ---
    // 512 bytes matches common flash page sizes, minimizing overhead.
    static const size_t BUFFER_SIZE = 512; 
    PsramVector<uint8_t> _buffer; 
    size_t _bufferIndex;
    size_t _bufferLength;

    // Internal helpers
    bool readLine(PsramString& line);
    void fillBuffer();
    void parseLine(const PsramString& line, MacroCommand& cmd);
    uint8_t parseKey(const PsramString& keyStr);
    uint8_t getModifier(const PsramString& modName);
};

} // namespace MACROS
