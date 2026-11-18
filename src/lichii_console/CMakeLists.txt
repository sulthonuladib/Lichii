# SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

target_sources(lichii PRIVATE
    console_app.cpp
    console_app.h
    console_settings.cpp
    console_settings.h
    library_model.cpp
    library_model.h
    appearance.cpp
    appearance.h
)
target_compile_definitions(lichii PRIVATE LICHII_CONSOLE)
target_link_libraries(lichii PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Multimedia Qt6::Network Qt6::Svg)

file(GLOB console_qml CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/qml/*.qml")
file(GLOB_RECURSE console_assets CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.wav"
)
qt_add_resources(lichii horizon_resources
    PREFIX "/horizon"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES ${console_qml} ${console_assets}
)

if (LICHII_TESTS AND LINUX)
    target_sources(lichii PRIVATE console_smoke.cpp library_smoke.cpp library_smoke.h)
    target_link_libraries(lichii PRIVATE Qt6::Test)
    target_compile_definitions(lichii PRIVATE LICHII_CONSOLE_TESTS)
    add_test(NAME horizon_console_smoke COMMAND lichii --console-smoke-test)
    set_tests_properties(horizon_console_smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
        TIMEOUT 90
    )
endif()
