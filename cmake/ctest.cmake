if (BUILD_TESTING)

	enable_testing()

	## All sources of the program (except main.cpp) are compiled once, in the
	## object library qtdmm_core (CMakeLists.txt). A test is just its own
	## tests/test_*.cpp linked against it.
	##
	## qtdmm_test_exe(<target> <source>)
	##     executable <target> from <source>, linked with qtdmm_core and QtTest;
	##     QTDMM_WERROR: warnings in the tests fail the build, too
	## qtdmm_add_test(<test name> <target> <source> [args...])
	##     the same, plus one ctest test running it with the args
	##
	## A test that needs the compiled resources (:/Help, :/icons, :/devices,
	## the .qm files) adds: target_link_libraries(<target> PRIVATE qtdmm_resources)

	function(qtdmm_test_exe target source)
		add_executable(${target} MACOSX_BUNDLE ${source})
		target_link_libraries(${target} PRIVATE qtdmm_core Qt6::Test)
		if (QTDMM_WARNING_FLAGS)
			target_compile_options(${target} PRIVATE ${QTDMM_WARNING_FLAGS})
		endif()
	endfunction()

	function(qtdmm_add_test name target source)
		qtdmm_test_exe(${target} ${source})
		add_test(NAME ${name} COMMAND ${target} ${ARGN})
	endfunction()

	set(TEST_DATA "${CMAKE_SOURCE_DIR}/tests/data")

	## decoders: protocol table, meter keys, device types, then one test per fixture
	qtdmm_test_exe(test_decoder tests/test_decoder.cpp)
	add_test(NAME protocol_table COMMAND test_decoder --table)
	add_test(NAME meter_keys     COMMAND test_decoder --keys)
	add_test(NAME device_types   COMMAND test_decoder --types)

	file(GLOB TEST_FILES CONFIGURE_DEPENDS "${TEST_DATA}/decoder/*.json")
	foreach(test_file ${TEST_FILES})
		get_filename_component(name ${test_file} NAME_WE)
		add_test(NAME ${name} COMMAND test_decoder ${test_file})
	endforeach()

	## the core's base types and ReadingAdapter (the decoder fixtures check the ports too)
	qtdmm_add_test(reading_adapter test_adapter tests/test_adapter.cpp)

	qtdmm_add_test(dmmgraph test_graph tests/test_graph.cpp "${TEST_DATA}/graph")

	## the recorder's store on its own
	qtdmm_add_test(recording_store test_recordingstore tests/test_recordingstore.cpp "${TEST_DATA}/graph")

	## the recorder against readings with given times: the grid export, the
	## integral and the triggers, as recorded with the 100 ms sample clock
	qtdmm_add_test(recording_golden test_recording_golden tests/test_recording_golden.cpp "${TEST_DATA}/recording")

	## the handbook: same compiled resources as the application, so the test
	## sees exactly the pages the user gets under :/Help/
	qtdmm_add_test(handbook test_help tests/test_help.cpp)
	target_link_libraries(test_help PRIVATE qtdmm_resources)

	## the analog meter: angle mapping, full-scale derivation, ballistics and
	## a headless render check
	qtdmm_add_test(analog_meter test_meter tests/test_meter.cpp)

	## the MDI window arrangement: automatic layout, order, title bars
	qtdmm_add_test(mdi_arranger test_mdiarranger tests/test_mdiarranger.cpp)

	## the digital display: glyph table and a headless render check
	qtdmm_add_test(digital_display test_display tests/test_display.cpp)

	## the meter's settings widget (DeviceSettings): keys in and out, "Advanced", groups per kind of meter
	qtdmm_add_test(device_settings test_devicesettings tests/test_devicesettings.cpp)

	## how to open a meter from its keys alone (MeterSetup), for every model as the settings page has it
	qtdmm_add_test(meter_setup test_metersetup tests/test_metersetup.cpp)

	## the meters of one process (DeviceSession): opened from My devices without a window, SCPI QTDMM:DEVice
	qtdmm_add_test(device_session test_devicesession tests/test_devicesession.cpp)

	## the program itself, offscreen: a meter in the window and two with --also, read over SCPI TCP
	## (Unix only: the console and the end of a GUI process on Windows)
	if(NOT WIN32)
		qtdmm_add_test(process_e2e test_process_e2e tests/test_process_e2e.cpp $<TARGET_FILE:${PROJECT_NAME}>)
		target_link_libraries(test_process_e2e PRIVATE Qt6::Network)
		add_dependencies(test_process_e2e ${PROJECT_NAME})
		set_tests_properties(process_e2e PROPERTIES TIMEOUT 60)
	endif()

	## the assistant "Add device": pages per connection, models, name, Next, the
	## buttons that finish it; draws the device symbols (:/devices)
	qtdmm_add_test(add_device test_adddevice tests/test_adddevice.cpp)
	target_link_libraries(test_adddevice PRIVATE qtdmm_resources)

	## the pictures made from the templates and manufacturer-colors.json
	qtdmm_add_test(device_symbols test_devicesymbol tests/test_devicesymbol.cpp
	               "${CMAKE_SOURCE_DIR}/assets/devices/manufacturer-colors.json")
	target_link_libraries(test_devicesymbol PRIVATE qtdmm_resources)

	## the sidebar "Devices": My devices in order, the one in use, click, rename, when it is shown; Settings of an entry; the instances
	qtdmm_add_test(device_sidebar test_devicesidebar tests/test_devicesidebar.cpp)
	target_link_libraries(test_devicesidebar PRIVATE qtdmm_resources)   # the pictures, the makers' websites

	## the meter's keys under the display: which a meter has, what a click sends, the states
	qtdmm_add_test(control_bar test_controlbar tests/test_controlbar.cpp)

	## one meter's front: its LCD follows the meter of its value, keys only where the meter has them
	qtdmm_add_test(front_panel test_frontpanel tests/test_frontpanel.cpp)

	## the menu of a value: the LCD place on the meter's front panel, graph axis, analog meter, table
	qtdmm_add_test(value_menu test_valuemenu tests/test_valuemenu.cpp)

	## "Sources..." of a view: the open meters' values, several or one;
	## "Values..." of a front panel
	qtdmm_add_test(sources_dialog test_sourcesdlg tests/test_sourcesdlg.cpp)

	## the application's log and its window
	qtdmm_add_test(app_log test_applog tests/test_applog.cpp)

	## the settings pages: keys through the pages, fields with their choice,
	## the recording length asked by Record
	qtdmm_add_test(settings_pages test_settingspages tests/test_settingspages.cpp)

	## the keys of the main window's commands and the page Keyboard
	qtdmm_add_test(shortcuts test_shortcuts tests/test_shortcuts.cpp)

	## the formula evaluator of calculated instances
	qtdmm_add_test(calc_expression test_calc tests/test_calc.cpp)

	## the calculated-value source: formula over the other instances' readings,
	## checked through the real ASCII decoder
	qtdmm_add_test(calc_device test_calc_device tests/test_calc_device.cpp)

	## HID cable chips: report layouts and chip detection, no hardware needed
	qtdmm_add_test(hid_cable test_hid tests/test_hid.cpp "${TEST_DATA}/hid_cables.json")

	## DMM connection state machine (Connecting/Connected/Timeout/Error/reconnect)
	## against a fake RFC 2217 server
	qtdmm_add_test(dmm_link_state test_dmm tests/test_dmm.cpp)

	## MeterController behind its SCPI server: what a command asks of the UI
	## runs after it, once (review R10-05)
	qtdmm_add_test(meter_controller test_metercontroller tests/test_metercontroller.cpp)

	## RFC 2217 client against a fake server: negotiation, telnet filtering, IAC escaping
	qtdmm_add_test(rfc2217_client test_rfc2217 tests/test_rfc2217.cpp)

	## the recorder's CSV formats, without widgets
	qtdmm_add_test(recording_file test_recording tests/test_recording.cpp "${TEST_DATA}/graph")
	target_link_libraries(test_recording PRIVATE qtdmm_resources)   # the LabPlot templates

	## the export dialog: formats, where to, the last choice
	qtdmm_add_test(export_dialog test_exportdlg tests/test_exportdlg.cpp)

	## the project basket: its folder, copies under free names
	qtdmm_add_test(basket test_basket tests/test_basket.cpp)

	## the readings table model, without its widget
	qtdmm_add_test(reading_log test_readinglog tests/test_readinglog.cpp)

	## Victron Instant Readout: advertisement parsing, AES-CTR, the decoder
	qtdmm_add_test(victron_instant_readout test_victronble tests/test_victronble.cpp)

	## mDNS browsing for qtdmm-bridge announcements
	qtdmm_add_test(mdns_browse test_mdns tests/test_mdns.cpp
		"${TEST_DATA}/mdns" "${CMAKE_SOURCE_DIR}/tools/qtdmm-bridge")

	## SCPI server: command interpreter, TCP round trip, mDNS announcement
	qtdmm_add_test(scpi_server test_scpi tests/test_scpi.cpp)

	## HiSLIP (VISA over LAN) in front of the SCPI interpreter, over TCP on localhost
	qtdmm_add_test(scpi_hislip test_hislip tests/test_hislip.cpp)

	## VXI-11 over TCP/UDP on localhost: own portmapper on a high port, links, locks,
	## limits, abort channel, and the registration with a (fake) system rpcbind
	qtdmm_add_test(scpi_vxi11 test_vxi11 tests/test_vxi11.cpp)

	## alarms: conditions, duration, hysteresis, acknowledge, JSON
	qtdmm_add_test(alarms test_alarm tests/test_alarm.cpp)
	## limit test (PASS/FAIL): limits, verdict, deviation, JSON
	qtdmm_add_test(limit_test test_limittest tests/test_limittest.cpp)
	## alarm sounds: once, N times, while raised, stopping; the built-in WAVs
	qtdmm_add_test(alarm_sound test_alarmsound tests/test_alarmsound.cpp)
	target_link_libraries(test_alarmsound PRIVATE qtdmm_resources)
	## the rules of every open meter: sounds, one banner, Acknowledge for all, the
	## front panel's colour, the window's rules moving into its entry
	qtdmm_add_test(alarm_service test_alarmservice tests/test_alarmservice.cpp)
	target_link_libraries(test_alarmservice PRIVATE qtdmm_resources)
	## a window switching between two of My devices takes over exactly the entry's keys
	qtdmm_add_test(switch_device test_switchdevice tests/test_switchdevice.cpp)
	target_link_libraries(test_switchdevice PRIVATE qtdmm_resources)

	## XLSX/ODS writer, read back with LibreOffice when installed
	qtdmm_add_test(spreadsheet test_spreadsheet tests/test_spreadsheet.cpp)

	## the min/max memory: kept across range changes, afresh on another port
	## (header only: nothing to link but QtCore)
	add_executable(test_minmax MACOSX_BUNDLE tests/test_minmax.cpp)
	target_include_directories(test_minmax PRIVATE src)
	target_link_libraries(test_minmax PRIVATE Qt6::Core)
	if (QTDMM_WARNING_FLAGS)
		target_compile_options(test_minmax PRIVATE ${QTDMM_WARNING_FLAGS})
	endif()
	add_test(NAME minmax_memory COMMAND test_minmax)

	## which value of which meter a view follows: text and JSON forms, matching
	## (header only: nothing to link but QtCore)
	add_executable(test_valueref MACOSX_BUNDLE tests/test_valueref.cpp)
	target_include_directories(test_valueref PRIVATE src)
	target_link_libraries(test_valueref PRIVATE Qt6::Core)
	if (QTDMM_WARNING_FLAGS)
		target_compile_options(test_valueref PRIVATE ${QTDMM_WARNING_FLAGS})
	endif()
	add_test(NAME value_ref COMMAND test_valueref)

	## the Poincaré plot's values: pairs, SD1/SD2, gaps, port changes
	qtdmm_add_test(poincare_series test_poincare tests/test_poincare.cpp)

	## the drawing clock of the views: wanted, off, slower when ticks cost too much
	qtdmm_add_test(frame_clock test_frameclock tests/test_frameclock.cpp)

	## "My devices": the meter keys, devices.conf, the stable port name
	qtdmm_add_test(device_library test_devicelibrary tests/test_devicelibrary.cpp)

	## workspace files: version 2 (JSON) and 1 (ini), refused ones, unknown keys kept
	qtdmm_add_test(workspace_file test_workspacefile tests/test_workspacefile.cpp)

	## the workspaces used last: newest first, once each, shared through a file
	qtdmm_add_test(recent_files test_recentfiles tests/test_recentfiles.cpp)

	## "Find device": families, adapters, access check, udev rules; nothing is opened
	qtdmm_add_test(device_discovery test_discovery tests/test_discovery.cpp "${CMAKE_SOURCE_DIR}")

	## symbol sets: under "System" QtDMM's own symbols from the plain set
	qtdmm_add_test(icon_sets test_icons tests/test_icons.cpp)
	target_link_libraries(test_icons PRIVATE qtdmm_resources)

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
			"QT_FORCE_STDERR_LOGGING=1;QT_LOGGING_TO_CONSOLE=1;QT_QPA_PLATFORM=offscreen;LANG=${lang}.UTF-8;LC_ALL=${lang}.UTF-8;LANGUAGE=")
	endforeach()

	## the tests report through qWarning()/qInfo(); on Windows Qt sends those
	## to the debugger instead of stderr unless told otherwise, and ctest's
	## --output-on-failure would show nothing. Last, so it reaches every test
	## (the ones added after it said nothing on Windows); a test with an
	## environment of its own keeps it
	get_property(ALL_TESTS DIRECTORY PROPERTY TESTS)
	foreach(_test ${ALL_TESTS})
		get_test_property(${_test} ENVIRONMENT _env)
		if (NOT _env)
			set_tests_properties(${_test} PROPERTIES ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1;QT_LOGGING_TO_CONSOLE=1;PYTHONDONTWRITEBYTECODE=1")
		endif()
	endforeach()

endif()
