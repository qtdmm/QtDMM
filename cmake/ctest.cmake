if (BUILD_TESTING)
	## QTDMM_WERROR: warnings in the tests fail the build, too
	add_compile_options(${QTDMM_WARNING_FLAGS})

	set( TEST_DECODER test_decoder)
	enable_testing()

	file(GLOB DECODER_FILES CONFIGURE_DEPENDS src/device/decoders/*.h  src/device/decoders/*.cpp )
	## the Victron decoder reads bit fields through src/device/victronble.cpp (which also carries the AES)
	list(APPEND DECODER_FILES src/device/victronble.cpp src/3rdparty/tiny-aes/aes.c)
	add_executable(${TEST_DECODER} MACOSX_BUNDLE tests/test_decoder.cpp src/device/dmmdecoder.cpp src/device/protocols.cpp src/core/siprefix.cpp src/core/readingadapter.cpp ${DECODER_FILES})
	target_link_libraries(${TEST_DECODER} PRIVATE Qt::Core Qt::Test)
	add_test(NAME protocol_table COMMAND ${TEST_DECODER} --table)

	## the core's base types and ReadingAdapter (the decoder fixtures check the ports too)
	add_executable(test_adapter tests/test_adapter.cpp src/core/readingadapter.cpp src/core/siprefix.cpp)
	target_include_directories(test_adapter PRIVATE src)
	target_link_libraries(test_adapter PRIVATE Qt::Core)
	add_test(NAME reading_adapter COMMAND test_adapter)

	file(GLOB TEST_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tests/data/decoder/*.json")
	foreach(test_file ${TEST_FILES})
		get_filename_component(name ${test_file} NAME_WE)
		add_test(NAME ${name} COMMAND ${TEST_DECODER} ${test_file})
	endforeach()

	set( TEST_GRAPH test_graph)
	add_executable(${TEST_GRAPH} MACOSX_BUNDLE tests/test_graph.cpp src/ui/views/graphwidget.cpp src/ui/views/gapline.cpp src/recording/recordingstore.cpp src/recording/recordingfile.cpp src/recording/spreadsheet.cpp src/3rdparty/miniz/miniz.c src/core/settings.cpp src/core/siprefix.cpp src/ui/engnumbervalidator.cpp)
	target_link_libraries(${TEST_GRAPH} PRIVATE Qt6::Widgets Qt6::PrintSupport Qt6::Charts Qt6::Svg Qt::Core Qt::Test)
	add_test(NAME dmmgraph COMMAND ${TEST_GRAPH} "${CMAKE_SOURCE_DIR}/tests/data/graph")

	## the recorder's store on its own: QtCore only, no widgets
	add_executable(test_recordingstore tests/test_recordingstore.cpp src/recording/recordingstore.cpp src/recording/recordingfile.cpp src/recording/spreadsheet.cpp src/3rdparty/miniz/miniz.c src/core/siprefix.cpp src/core/readingadapter.cpp)
	target_include_directories(test_recordingstore PRIVATE src)
	target_link_libraries(test_recordingstore PRIVATE Qt::Core Qt::Test)
	add_test(NAME recording_store COMMAND test_recordingstore)

	## the handbook: same compiled resources as the application, so the test
	## sees exactly the pages the user gets under :/Help/
	set( TEST_HELP test_help)
	## links the same compiled resources as the application (qtdmm_resources
	## also carries the generated ui header helpdlg.cpp needs)
	add_executable(${TEST_HELP} MACOSX_BUNDLE tests/test_help.cpp src/ui/dialogs/helpdlg.cpp src/core/settings.cpp)
	target_include_directories(${TEST_HELP} PRIVATE src)
	target_link_libraries(${TEST_HELP} PRIVATE qtdmm_resources Qt6::Widgets Qt::Core)
	add_test(NAME handbook COMMAND ${TEST_HELP})

	## the analog meter: angle mapping, full-scale derivation, ballistics and
	## a headless render check
	set( TEST_METER test_meter)
	add_executable(${TEST_METER} MACOSX_BUNDLE tests/test_meter.cpp src/ui/views/analogmeter.cpp src/ui/panelframe.cpp src/core/siprefix.cpp src/core/readingadapter.cpp)
	target_include_directories(${TEST_METER} PRIVATE src)
	target_link_libraries(${TEST_METER} PRIVATE Qt6::Widgets Qt::Core Qt::Test)
	add_test(NAME analog_meter COMMAND ${TEST_METER})

	## the MDI window arrangement: automatic layout, order, title bars
	set( TEST_MDI test_mdiarranger)
	add_executable(${TEST_MDI} MACOSX_BUNDLE tests/test_mdiarranger.cpp src/ui/mdiarranger.cpp)
	target_include_directories(${TEST_MDI} PRIVATE src)
	target_link_libraries(${TEST_MDI} PRIVATE Qt6::Widgets Qt::Core Qt::Test)
	add_test(NAME mdi_arranger COMMAND ${TEST_MDI})

	## the digital display: glyph table and a headless render check
	set( TEST_DISPLAY test_display)
	add_executable(${TEST_DISPLAY} MACOSX_BUNDLE tests/test_display.cpp src/ui/views/lcdwidget.cpp src/ui/panelframe.cpp src/core/siprefix.cpp)
	target_include_directories(${TEST_DISPLAY} PRIVATE src)
	target_link_libraries(${TEST_DISPLAY} PRIVATE Qt6::Widgets Qt::Core)
	add_test(NAME digital_display COMMAND ${TEST_DISPLAY})

	## instance coordination over shared memory: registration, state channel
	## and published readings
	set( TEST_SHAREDSTATE test_sharedstate)
	add_executable(${TEST_SHAREDSTATE} MACOSX_BUNDLE tests/test_sharedstate.cpp src/service/sharedstatemanager.cpp)
	target_include_directories(${TEST_SHAREDSTATE} PRIVATE src)
	target_link_libraries(${TEST_SHAREDSTATE} PRIVATE Qt::Core)
	add_test(NAME shared_state COMMAND ${TEST_SHAREDSTATE})

	## instances dialog: list from config files, delete mode, calculated instance
	set( TEST_INSTANCES test_instances)
	add_executable(${TEST_INSTANCES} MACOSX_BUNDLE tests/test_instances.cpp src/ui/dialogs/instancesdlg.cpp src/core/settings.cpp src/device/protocols.cpp src/device/dmmdecoder.cpp src/core/siprefix.cpp ${DECODER_FILES}
		src/service/sharedstatemanager.cpp src/core/calcexpr.cpp src/core/siprefix.cpp src/ui/forms/uiinstancesdlg.ui)
	target_include_directories(${TEST_INSTANCES} PRIVATE src)
	target_link_libraries(${TEST_INSTANCES} PRIVATE Qt6::Widgets Qt::Core)
	add_test(NAME instances_dialog COMMAND ${TEST_INSTANCES})
	## the formula evaluator of calculated instances
	set( TEST_CALC test_calc)
	add_executable(${TEST_CALC} MACOSX_BUNDLE tests/test_calc.cpp src/core/calcexpr.cpp src/core/siprefix.cpp)
	target_include_directories(${TEST_CALC} PRIVATE src)
	target_link_libraries(${TEST_CALC} PRIVATE Qt::Core)
	add_test(NAME calc_expression COMMAND ${TEST_CALC})

	## the calculated-value source: formula over the other instances' readings,
	## checked through the real ASCII decoder
	set( TEST_CALC_DEVICE test_calc_device)
	add_executable(${TEST_CALC_DEVICE} MACOSX_BUNDLE tests/test_calc_device.cpp src/device/transports/calc.cpp src/core/calcexpr.cpp
		src/service/sharedstatemanager.cpp src/device/dmmdecoder.cpp src/device/protocols.cpp src/core/siprefix.cpp ${DECODER_FILES})
	target_include_directories(${TEST_CALC_DEVICE} PRIVATE src)
	target_link_libraries(${TEST_CALC_DEVICE} PRIVATE Qt::Core)
	add_test(NAME calc_device COMMAND ${TEST_CALC_DEVICE})

	## HID cable chips: report layouts and chip detection, no hardware needed
	set( TEST_HID test_hid)
	add_executable(${TEST_HID} MACOSX_BUNDLE tests/test_hid.cpp src/device/transports/hidserial.cpp src/device/transports/hidreader.cpp src/device/transports/hidholtek.cpp src/device/dmmdecoder.cpp src/device/protocols.cpp src/core/siprefix.cpp ${DECODER_FILES})
	target_include_directories(${TEST_HID} PRIVATE src)
	target_link_libraries(${TEST_HID} PRIVATE Qt::Core ${HIDAPI_TARGET})
	add_test(NAME hid_cable COMMAND ${TEST_HID} "${CMAKE_SOURCE_DIR}/tests/data/hid_cables.json")

	## DMM connection state machine (Connecting/Connected/Timeout/Error/reconnect)
	## against a fake RFC 2217 server; needs the whole port stack
	set( TEST_DMM test_dmm)
	add_executable(${TEST_DMM} MACOSX_BUNDLE tests/test_dmm.cpp src/device/meterconnection.cpp src/device/framereader.cpp src/device/transport.cpp
		src/device/transports/serial.cpp src/device/transports/hidserial.cpp src/device/transports/hidreader.cpp src/device/transports/hidholtek.cpp src/device/transports/rfc2217serial.cpp src/device/transports/sigrok.cpp
		src/device/transports/calc.cpp src/core/calcexpr.cpp src/service/sharedstatemanager.cpp src/device/dmmdecoder.cpp src/device/protocols.cpp src/core/siprefix.cpp ${DECODER_FILES})
	target_include_directories(${TEST_DMM} PRIVATE src)
	target_link_libraries(${TEST_DMM} PRIVATE Qt6::Widgets Qt6::SerialPort Qt::Network Qt::Core ${HIDAPI_TARGET})
	add_test(NAME dmm_link_state COMMAND ${TEST_DMM})

	## RFC 2217 client against a fake server: negotiation, telnet filtering, IAC escaping
	set( TEST_RFC2217 test_rfc2217)
	add_executable(${TEST_RFC2217} MACOSX_BUNDLE tests/test_rfc2217.cpp src/device/transports/rfc2217serial.cpp src/device/dmmdecoder.cpp src/device/protocols.cpp src/core/siprefix.cpp ${DECODER_FILES})
	target_include_directories(${TEST_RFC2217} PRIVATE src)
	target_link_libraries(${TEST_RFC2217} PRIVATE Qt::Core Qt::Network)
	add_test(NAME rfc2217_client COMMAND ${TEST_RFC2217})

	## the recorder's CSV formats, without widgets
	set( TEST_RECORDING test_recording)
	add_executable(${TEST_RECORDING} MACOSX_BUNDLE tests/test_recording.cpp src/recording/recordingfile.cpp src/recording/spreadsheet.cpp src/3rdparty/miniz/miniz.c src/core/siprefix.cpp)
	target_include_directories(${TEST_RECORDING} PRIVATE src)
	target_link_libraries(${TEST_RECORDING} PRIVATE Qt::Core)
	add_test(NAME recording_file COMMAND ${TEST_RECORDING} "${CMAKE_SOURCE_DIR}/tests/data/graph")

	## the readings table model, without its widget
	set( TEST_READINGLOG test_readinglog)
	add_executable(${TEST_READINGLOG} MACOSX_BUNDLE tests/test_readinglog.cpp src/recording/readingsmodel.cpp src/recording/recordingstore.cpp src/recording/recordingfile.cpp src/recording/spreadsheet.cpp src/3rdparty/miniz/miniz.c src/core/siprefix.cpp)
	target_include_directories(${TEST_READINGLOG} PRIVATE src)
	target_link_libraries(${TEST_READINGLOG} PRIVATE Qt6::Gui Qt::Core Qt::Test)
	add_test(NAME reading_log COMMAND ${TEST_READINGLOG})

	## Victron Instant Readout: advertisement parsing, AES-CTR, the decoder
	set( TEST_VICTRON test_victronble)
	add_executable(${TEST_VICTRON} MACOSX_BUNDLE tests/test_victronble.cpp src/device/dmmdecoder.cpp src/device/protocols.cpp src/core/siprefix.cpp ${DECODER_FILES})
	target_include_directories(${TEST_VICTRON} PRIVATE src ${CMAKE_BINARY_DIR})
	target_link_libraries(${TEST_VICTRON} PRIVATE Qt::Core Qt::Test)
	add_test(NAME victron_instant_readout COMMAND ${TEST_VICTRON})

	## mDNS browsing for qtdmm-bridge announcements
	set( TEST_MDNS test_mdns)
	add_executable(${TEST_MDNS} MACOSX_BUNDLE tests/test_mdns.cpp src/service/mdnsbrowser.cpp)
	target_include_directories(${TEST_MDNS} PRIVATE src)
	target_link_libraries(${TEST_MDNS} PRIVATE Qt::Core Qt6::Network Qt::Test)
	add_test(NAME mdns_browse COMMAND ${TEST_MDNS} "${CMAKE_SOURCE_DIR}/tests/data/mdns" "${CMAKE_SOURCE_DIR}/tools/qtdmm-bridge")

	## SCPI server: command interpreter, TCP round trip, mDNS announcement
	set( TEST_SCPI test_scpi)
	add_executable(${TEST_SCPI} MACOSX_BUNDLE tests/test_scpi.cpp src/service/scpiserver.cpp src/service/mdnsresponder.cpp src/service/mdnsbrowser.cpp)
	target_include_directories(${TEST_SCPI} PRIVATE src)
	target_link_libraries(${TEST_SCPI} PRIVATE Qt::Core Qt6::Network Qt::Test)
	add_test(NAME scpi_server COMMAND ${TEST_SCPI})

	## alarms: conditions, duration, hysteresis, acknowledge, JSON
	set( TEST_ALARM test_alarm)
	add_executable(${TEST_ALARM} MACOSX_BUNDLE tests/test_alarm.cpp src/core/alarm.cpp src/ui/engnumbervalidator.cpp src/core/siprefix.cpp)
	target_include_directories(${TEST_ALARM} PRIVATE src)
	target_link_libraries(${TEST_ALARM} PRIVATE Qt6::Gui Qt::Core Qt::Test)
	add_test(NAME alarms COMMAND ${TEST_ALARM})

	## XLSX/ODS writer, read back with LibreOffice when installed
	set( TEST_SPREADSHEET test_spreadsheet)
	add_executable(${TEST_SPREADSHEET} MACOSX_BUNDLE tests/test_spreadsheet.cpp src/recording/spreadsheet.cpp src/3rdparty/miniz/miniz.c)
	target_include_directories(${TEST_SPREADSHEET} PRIVATE src)
	target_link_libraries(${TEST_SPREADSHEET} PRIVATE Qt::Core Qt::Test)
	add_test(NAME spreadsheet COMMAND ${TEST_SPREADSHEET})

	## generated documents must match their sources (device table from the
	## decoders, README from docs/)
	find_package(Python3 COMPONENTS Interpreter)
	if (Python3_Interpreter_FOUND)
		add_test(NAME docs_generated COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/generate_docs.py" --check)
		## the network bridge (tools/qtdmm-bridge) has its own unittest suite, no pyserial needed
		if (Python3_VERSION VERSION_GREATER_EQUAL 3.11)
			add_test(NAME qtdmm_bridge COMMAND ${Python3_EXECUTABLE} -m unittest discover -s tests WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/tools/qtdmm-bridge")
		endif()
	endif()

	## the version scheme YY.N[.P][-rcK]: tags and dev builds -> version strings
	add_test(NAME version_scheme COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/tests/test_version.cmake")

	## desktop entry and AppStream metadata, when the validators are installed
	## (desktop-file-utils, appstream); no network: screenshot URLs aren't fetched
	find_program(DESKTOP_FILE_VALIDATE desktop-file-validate)
	if (DESKTOP_FILE_VALIDATE)
		add_test(NAME desktop_entry COMMAND ${DESKTOP_FILE_VALIDATE} "${CMAKE_SOURCE_DIR}/assets/io.github.qtdmm.qtdmm.desktop")
	endif()
	find_program(APPSTREAMCLI appstreamcli)
	if (APPSTREAMCLI)
		add_test(NAME appstream_metadata COMMAND ${APPSTREAMCLI} validate --no-net "${CMAKE_SOURCE_DIR}/assets/appimage/qtdmm.appdata.xml")
	endif()

	## the tests report through qWarning()/qInfo(); on Windows Qt sends those
	## to the debugger instead of stderr unless told otherwise, and ctest's
	## --output-on-failure would show nothing
	get_property(ALL_TESTS DIRECTORY PROPERTY TESTS)
	set_tests_properties(${ALL_TESTS} PROPERTIES ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1;QT_LOGGING_TO_CONSOLE=1;PYTHONDONTWRITEBYTECODE=1")

	## symbol sets: under "System" QtDMM's own symbols from the plain set
	set( TEST_ICONS test_icons)
	add_executable(${TEST_ICONS} MACOSX_BUNDLE tests/test_icons.cpp src/ui/designs.cpp)
	target_include_directories(${TEST_ICONS} PRIVATE src)
	target_link_libraries(${TEST_ICONS} PRIVATE qtdmm_resources Qt6::Widgets Qt::Core)
	add_test(NAME icon_sets COMMAND ${TEST_ICONS})

	## Alt letters used twice in a menu or among the widgets shown together
	## (and doubled keys), in the real program in every language: it starts
	## offscreen with a config of its own (it writes one, a missing one would
	## greet with a dialog), checks and quits. The locale comes from LANG only
	## on Linux and the BSDs; elsewhere this checks the system language. A
	## dialog waiting for a click must not hold CI: a timeout.
	foreach(lang en_US de_DE es_ES fr_FR pl_PL)
		set(_dir "${CMAKE_BINARY_DIR}/mnemonics/${lang}")
		file(MAKE_DIRECTORY "${_dir}")
		add_test(NAME mnemonics_${lang} COMMAND ${PROJECT_NAME} --check-mnemonics --config-dir "${_dir}" --config-id check)
		set_tests_properties(mnemonics_${lang} PROPERTIES TIMEOUT 120 ENVIRONMENT
			"QT_FORCE_STDERR_LOGGING=1;QT_LOGGING_TO_CONSOLE=1;QT_QPA_PLATFORM=offscreen;LANG=${lang}.UTF-8;LC_ALL=${lang}.UTF-8;LANGUAGE=;QTDMM_IPC_KEY=qtdmm_mnemonics_${lang}")
	endforeach()
endif()
