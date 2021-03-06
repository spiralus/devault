# Copyright (c) 2018 The Bitcoin developers

project(devault-qt)
cmake_minimum_required(VERSION 3.5)


# Project Info
SET(APP_NAME DeVault-Qt)
SET(PROJECT_NAME_SHORT   "DeVault-Qt")
SET(PROJECT_NAME_LONG    "CMake DeVault Qt project")
SET(PROJECT_DESCRIPTION  "CMake build system template for Qt based projects")
SET(PROJECT_COPYRIGHT    "Copyright (C) 2019 DeVault Developers")
SET(PROJECT_CONTACT      "jonspock@protonmail.com")
SET(PROJECT_VENDOR       "Jon Spock")
SET(ORG_WEBSITE          "https://devault.cc")

# For projects using Git, use git version patch
IF("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    SET(PATCH_OUT "0")
ELSE("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    # Find Git Version Patch
    FIND_PROGRAM(GIT git)
    IF(GIT)
        EXECUTE_PROCESS(
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMAND ${GIT} rev-parse --short HEAD 
            OUTPUT_VARIABLE GIT_OUT OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        SET(PATCH_OUT "0-${GIT_OUT}")
    ELSE(GIT)
        SET(PATCH_OUT "0")
    ENDIF(GIT)
ENDIF("${CMAKE_BUILD_TYPE}" STREQUAL "Release")

SET(PROJECT_VERSION_PATCH ${PATCH_OUT})

# Project Version
SET(CPACK_PACKAGE_VERSION_MAJOR "1")
SET(CPACK_PACKAGE_VERSION_MINOR "0")
SET(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
SET(CPACK_PACKAGE_VERSION ${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH})
SET(PROJECT_VERSION_MAJOR ${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR})
SET(PROJECT_VERSION ${CPACK_PACKAGE_VERSION})

MESSAGE(STATUS "Version: ${PROJECT_VERSION}")

STRING(TOLOWER ${PROJECT_NAME_SHORT} PROJECT_NAME_SHORT_LOWER)
PROJECT(${PROJECT_NAME_SHORT_LOWER}-${PROJECT_VERSION})

# System on which project is built
SITE_NAME(HOSTNAME)
SET(SYSNAME ${CMAKE_SYSTEM}-${CMAKE_SYSTEM_PROCESSOR})

# CMake Settings
SET(CMAKE_COLOR_MAKEFILE ON)

# This ensure that AUTOMOC doesn't run on generated files.
if (CMAKE_VERSION VERSION_LESS 3.10)
  message("Not setting CMP0071")
else()
  cmake_policy(SET CMP0071 OLD)
endif()

include(BrewHelper)
find_brew_prefix(QT5_PREFIX qt5)

set(QT_REQUIRED_COMPONENTS Core Widgets Network Test DBus)
find_package(Qt5 COMPONENTS ${QT_REQUIRED_COMPONENTS} REQUIRED HINTS "${QT5_PREFIX}")

# Find out more about Qt. This is similar to
# http://code.qt.io/cgit/qt/qtwebkit.git/tree/Source/cmake/OptionsQt.cmake
get_target_property(QT_CORE_TYPE Qt5::Core TYPE)
if(QT_CORE_TYPE MATCHES STATIC)
	set(QT_STATIC_BUILD ON)
endif()


find_package(Qt5DBus)
if (Qt5DBus_FOUND)
  add_compile_options("-DUSE_DBUS")
endif()

find_package(QREncode)
if (QREncode_FOUND)
  add_definitions(-DUSE_QRCODE)
  message(STATUS "Found QREncode")
  set(QR_LIB ${QREncode_LIBRARY})
else()
  message(STATUS " QREncode NOT FOUND ")
endif()
 


set(STATIC_DEPENDENCIES_CMAKE_FILE "${CMAKE_BINARY_DIR}/QtStaticDependencies.cmake")
if(EXISTS ${STATIC_DEPENDENCIES_CMAKE_FILE})
	file(REMOVE ${STATIC_DEPENDENCIES_CMAKE_FILE})
endif()

set(CONVERT_PRL_PATH "${CONTRIB_PATH}/qt/convert-prl-libs-to-cmake.pl")
macro(CONVERT_PRL_LIBS_TO_CMAKE _qt_component)
	if(TARGET Qt5::${_qt_component})
		get_target_property(_lib_location Qt5::${_qt_component} LOCATION)
		execute_process(COMMAND ${PERL_EXECUTABLE} "${CONVERT_PRL_PATH}"
			--lib ${_lib_location}
			--out ${STATIC_DEPENDENCIES_CMAKE_FILE}
			--component ${_qt_component}
			--compiler ${CMAKE_CXX_COMPILER_ID}
		)
	endif()
endmacro()

if(QT_STATIC_BUILD)
	foreach(qt_module ${QT_REQUIRED_COMPONENTS})
		CONVERT_PRL_LIBS_TO_CMAKE(${qt_module})
	endforeach()
    # HACK: We must explicitly add LIB path of the Qt installation
    # to correctly find qtpcre
    link_directories(${_qt5_install_prefix}/../)

	# Now that we generated the dependencies, import them.
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CONVERT_PRL_PATH}")
	if(NOT EXISTS ${STATIC_DEPENDENCIES_CMAKE_FILE})
		message(FATAL_ERROR "Unable to find ${STATIC_DEPENDENCIES_CMAKE_FILE}")
	endif()
	include(${STATIC_DEPENDENCIES_CMAKE_FILE})
	list(REMOVE_DUPLICATES STATIC_LIB_DEPENDENCIES)
endif()

# Localisation
add_subdirectory(locale)

add_custom_command(OUTPUT temp_bitcoin_locale.qrc
	COMMAND cmake
	ARGS
		-E copy
		"${CMAKE_CURRENT_SOURCE_DIR}/bitcoin_locale.qrc"
		temp_bitcoin_locale.qrc
	MAIN_DEPENDENCY bitcoin_locale.qrc
	VERBATIM
)

add_custom_command(OUTPUT qrc_bitcoin_locale.cpp
	COMMAND Qt5::rcc
	ARGS
		temp_bitcoin_locale.qrc
		-name bitcoin_locale
		-o qrc_bitcoin_locale.cpp
	MAIN_DEPENDENCY temp_bitcoin_locale.qrc
	DEPENDS locales
	VERBATIM
)

# UI elements
qt5_wrap_ui(UI_GENERATED_HEADERS
	forms/addressbookpage.ui
	forms/askpassphrasedialog.ui
	forms/setpassphrasedialog.ui
	forms/coincontroldialog.ui
	forms/editaddressdialog.ui
	forms/helpmessagedialog.ui
	forms/intro.ui
	forms/modaloverlay.ui
	forms/openuridialog.ui
	forms/optionsdialog.ui
	forms/overviewpage.ui
	forms/receivecoinsdialog.ui
	forms/receiverequestdialog.ui
	forms/debugwindow.ui
	forms/sendcoinsdialog.ui
	forms/sendcoinsentry.ui
	forms/signverifymessagedialog.ui
	forms/transactiondescdialog.ui
)

# Qt MOC
set(CMAKE_AUTOMOC ON)

# Handle qrc resources
qt5_add_resources(QRC_BITCOIN_CPP bitcoin.qrc)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(mac_stuff
	  macdockiconhandler.mm
	  macnotificationhandler.mm
	  )

if(BUILD_WALLET)
  file(GLOB QT_BASE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
  source_group("BaseHeaders" FILES ${QT_BASE_HEADERS})
endif()
endif()


add_library(bitcoin-qt-base
  ${QT_BASE_HEADERS}
	bantablemodel.cpp
	bitcoinaddressvalidator.cpp
	bitcoinamountfield.cpp
	bitcoingui.cpp
	bitcoinunits.cpp
	clientmodel.cpp
	csvmodelwriter.cpp
  dvtui.cpp
	guiutil.cpp
	intro.cpp
	modaloverlay.cpp
	networkstyle.cpp
	notificator.cpp
	optionsdialog.cpp
	optionsmodel.cpp
	peertablemodel.cpp
	platformstyle.cpp
	qvalidatedlineedit.cpp
	qvaluecombobox.cpp
	rpcconsole.cpp
	splashscreen.cpp
	trafficgraphwidget.cpp
	utilitydialog.cpp
  dvtui.cpp

	# Handle ui files
	${UI_GENERATED_HEADERS}

	# Translations
	${BITCOIN_QM_FILES}

	# Handle qrc files
	${QRC_BITCOIN_CPP}
	qrc_bitcoin_locale.cpp

  ${mac_stuff}

)

# Windows support
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	target_sources(bitcoin-qt-base PRIVATE winshutdownmonitor.cpp)
endif()

target_include_directories(bitcoin-qt-base
	PUBLIC
		.
		${CMAKE_CURRENT_BINARY_DIR}
		${CMAKE_CURRENT_BINARY_DIR}/forms
)

target_link_libraries(bitcoin-qt-base
	server
	rpcclient
	Qt5::Widgets
	Qt5::Network
  ${QR_LIB}
)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set_property(TARGET bitcoin-qt-base PROPERTY AUTOMOC_MOC_OPTIONS "-DQ_OS_MAC")
endif()

# Wallet
if(BUILD_WALLET)
	# Automoc option.
	set(AUTOMOC_MOC_OPTIONS -DENABLE_WALLET=1)

	# Add wallet functionality to bitcoin-qt
	target_sources(bitcoin-qt-base
		PRIVATE
			addressbookpage.cpp
			addresstablemodel.cpp
			askpassphrasedialog.cpp
			setpassphrasedialog.cpp
			coincontroldialog.cpp
			coincontroltreewidget.cpp
			editaddressdialog.cpp
			openuridialog.cpp
			overviewpage.cpp
			receivecoinsdialog.cpp
			receiverequestdialog.cpp
			recentrequeststablemodel.cpp
			sendcoinsdialog.cpp
			sendcoinsentry.cpp
			signverifymessagedialog.cpp
			transactiondesc.cpp
			transactiondescdialog.cpp
			transactionfilterproxy.cpp
			transactionrecord.cpp
			transactiontablemodel.cpp
			transactionview.cpp
			walletframe.cpp
			walletmodel.cpp
			walletmodeltransaction.cpp
			walletview.cpp
	)

	target_link_libraries(bitcoin-qt-base wallet)
endif()

# The executable
if (DO_PACKAGING)
  add_executable(${APP_NAME} MACOSX_BUNDLE bitcoin.cpp)
else()
  add_executable(${APP_NAME} bitcoin.cpp)
endif()
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  target_link_libraries(${APP_NAME} bitcoin-qt-base "-framework Foundation" "-framework CoreServices" "-framework AppKit")
else()
  target_link_libraries(${APP_NAME} bitcoin-qt-base)
endif()

QT5_USE_Modules(${APP_NAME} Gui)
QT5_USE_Modules(${APP_NAME} Core)
QT5_USE_Modules(${APP_NAME} Widgets)
QT5_USE_Modules(${APP_NAME} Test)
QT5_USE_Modules(${APP_NAME} PrintSupport)
QT5_USE_Modules(${APP_NAME} Network)
if (Qt5DBus_FOUND)
  QT5_USE_Modules(${APP_NAME} DBus)
endif()

set_target_properties(${APP_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/../..")


if (DO_PACKAGING)
  #############################################################
  # Enable packaging
  INCLUDE(InstallRequiredSystemLibraries)
  

  if (APPLE)
    SET_PROPERTY(TARGET ${APP_NAME} PROPERTY MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in")
    SET_PROPERTY(TARGET ${APP_NAME} PROPERTY MACOSX_BUNDLE TRUE)
  endif()
  
  set(APPS "\${CMAKE_INSTALL_PREFIX}/${APP_NAME}.app")
  set(DIRS ${CMAKE_BINARY_DIR})


  SET(CPACK_SYSTEM_NAME ${CMAKE_SYSTEM_NAME})
  SET(CPACK_PACKAGE_NAME ${PROJECT_NAME_SHORT})
  SET(CPACK_PACKAGE_DESCRIPTION "${PROJECT_NAME_LONG}")
  SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_NAME_SHORT} - ${PROJECT_DESCRIPTION}")
  SET(CPACK_PACKAGE_VENDOR ${PROJECT_VENDOR})
  SET(CPACK_PACKAGE_CONTACT ${PROJECT_CONTACT})
  SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/README.md")
  SET(CPACK_RESOURCE_FILE_README ${CMAKE_SOURCE_DIR}/README.md)
  SET(CPACK_PACKAGE_INSTALL_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
  
  SET(CPACK_PACKAGE_FILE_NAME ${PROJECT_NAME}-${CPACK_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR})


  if (APPLE)
    SET(CPACK_GENERATOR "DragNDrop")
    # Libraries are bundled directly
    SET(CPACK_COMPONENT_LIBRARIES_HIDDEN TRUE)
    SET(MACOSX_BUNDLE_BUNDLE_NAME ${APP_NAME})
    SET(MACOSX_BUNDLE_INFO_STRING "DeVault-Qt")
    SET(MACOSX_BUNDLE_GUI_IDENTIFIER "com.apple.DeVault-Qt")
    SET(MACOSX_BUNDLE_COPYRIGHT "(c) DeVault 2018")
    SET(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
    SET(MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
    SET(MACOSX_BUNDLE_LONG_VERSION_STRING "Version ${PROJECT_VERSION}")
  endif()
  # Bundle Properties

  SET(CPACK_PACKAGE_EXECUTABLES "DeVault-Qt" "devault-qt")
  INCLUDE(CPack)
  INSTALL(TARGETS ${APP_NAME} DESTINATION .)
endif()


# Test tests
add_subdirectory(test)
