# M3-PROMPT-2 Source Analysis B — ModeController + Test Pattern

## ModeController BatchMode instantiation
- Where created: line 45
- Constructor args passed: `BatchMode(this)` — single parent QWidget pointer (ModeController instance)
- Enter/exit method: `setScreen()` method (lines 27–58); lazy initialization at line 39–51; mode swap via `setCurrentWidget(target)` at line 56

## AppContext.h services
- ConversionManager: not present; `conversion` field is `std::shared_ptr<IConversionEngine>` (line 25)
- Document session API: `document` field is `std::shared_ptr<DocumentSession>` (line 28); active document accessed via DocumentSession interface

## CMakeLists.txt test pattern
Line 387–402 shows the standard pattern:
```cmake
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/UnitTests.cpp")
    add_executable(UnitTests
        tests/UnitTests.cpp
    )
    target_include_directories(UnitTests PRIVATE src)
    target_link_libraries(UnitTests PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Widgets
    )
    add_test(NAME UnitTests COMMAND UnitTests)
    set_tests_properties(UnitTests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;qt;headless"
    )
endif()
```

For TestBatchMode.cpp pattern (not yet present), would follow same structure:
- Check: `if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestBatchMode.cpp")`
- Executable name: `TestBatchMode`
- Include dirs: `src tests`
- Link libs: `pdfws_ui pdfws_engines pdfws_commands pdfws_core Qt6::Test Qt6::Widgets`
- Labels: `"unit;modes;batch;ui;qt;headless"`

## TestBatchMode.cpp exists?
No — file does not exist at `C:\Users\User\Projects\pdf\tests\TestBatchMode.cpp`

## Additional observations
- ModeController uses lazy initialization: modes are created on first `setScreen(id)` call, not at constructor time
- BatchMode is instantiated with `this` (parent ModeController) as its sole parameter — parent is expected to provide UI context
- AppContext provides `IConversionEngine` (not ConversionManager); DocumentSession is the document context holder
- All test blocks follow identical CMake pattern (lines 387–694): `if(EXISTS ...)`, include `src tests`, link Qt6::Test + core libs, set `QT_QPA_PLATFORM=offscreen` env var, timeout property, and labels
- No TestBatchMode currently registered in CMakeLists.txt
