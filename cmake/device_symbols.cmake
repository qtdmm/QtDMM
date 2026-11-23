# Copyright (c) 2026 The QtDMM developers
# SPDX-License-Identifier: GPL-3.0-or-later

## The pictures of the kinds of instrument in the manufacturers' colours
## (DeviceSymbol), made when CMake runs - no Python in the build.
##
## In:  assets/devices/<type>.svg, the templates: #FF00FF primary, #00FFFF
##      secondary, #FFFF00 the outline;
##      assets/devices/manufacturer-colors.json, per manufacturer the colours,
##      the addConfig vendors it stands for ("match"), the kinds its models are
##      ("types") and other colours for a kind ("device_classes");
##      assets/devices/override/<key>_<type>.svg, a picture of its own for one
##      manufacturer and kind instead of the template.
## Out: <build>/devices/<type>.svg in the neutral colours ("default"), and
##      <build>/devices/<key>/<type>.svg for each manufacturer and kind it
##      has, each with <type>_dark.svg, the outline light for a dark design;
##      <build>/devices/vendors.json, addConfig vendor -> key;
##      <build>/devices/websites.json, addConfig vendor -> the maker's "url".
##
## qtdmm_device_symbols(<var>) sets <var> to the <qresource prefix="/devices">
## block of resources.qrc. A file is written only when it changed, so rcc
## runs again only then.

set(QTDMM_SYMBOL_OUTLINE_LIGHT "#3A3D41")   # the outline in a light design: dark grey
set(QTDMM_SYMBOL_OUTLINE_DARK "#D0D4D8")    # and in a dark one: light grey

# <out> from <template> with the three colours
function(qtdmm_symbol_write template out primary secondary outline)
	file(READ "${template}" svg)
	foreach(pair "FF00FF;${primary}" "ff00ff;${primary}" "00FFFF;${secondary}" "00ffff;${secondary}"
	             "FFFF00;${outline}" "ffff00;${outline}")
		list(GET pair 0 from)
		list(GET pair 1 to)
		string(REPLACE "#${from}" "${to}" svg "${svg}")
	endforeach()
	file(WRITE "${out}.new" "${svg}")
	configure_file("${out}.new" "${out}" COPYONLY)
	file(REMOVE "${out}.new")
endfunction()

function(qtdmm_device_symbols result)
	set(src "${CMAKE_SOURCE_DIR}/assets/devices")
	set(out "${CMAKE_BINARY_DIR}/devices")
	set(json_file "${src}/manufacturer-colors.json")
	file(GLOB templates "${src}/*.svg")
	file(GLOB overrides "${src}/override/*.svg")
	list(SORT templates)
	set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
	             "${json_file}" ${templates} ${overrides})
	file(READ "${json_file}" json)
	string(JSON primary GET "${json}" manufacturers default colors primary)
	string(JSON secondary GET "${json}" manufacturers default colors secondary)

	set(qrc "    <qresource prefix=\"/devices\">\n")
	# neutral, every kind: for the manufacturers without colours of their own
	foreach(template IN LISTS templates)
		get_filename_component(type "${template}" NAME_WE)
		foreach(variant "" "_dark")
			set(outline "${QTDMM_SYMBOL_OUTLINE_LIGHT}")
			if(variant)
				set(outline "${QTDMM_SYMBOL_OUTLINE_DARK}")
			endif()
			qtdmm_symbol_write("${template}" "${out}/${type}${variant}.svg" "${primary}" "${secondary}" "${outline}")
			string(APPEND qrc "        <file alias=\"${type}${variant}.svg\">${out}/${type}${variant}.svg</file>\n")
		endforeach()
	endforeach()

	# each manufacturer, the kinds it has
	set(vendors "")
	set(websites "")
	string(JSON count LENGTH "${json}" manufacturers)
	math(EXPR last "${count} - 1")
	foreach(i RANGE ${last})
		string(JSON key MEMBER "${json}" manufacturers ${i})
		if(key STREQUAL "default")
			continue()
		endif()
		if(NOT key MATCHES "^[a-z0-9_]+$")
			message(FATAL_ERROR "manufacturer-colors.json: key \"${key}\" - lower case, [a-z0-9_] only")
		endif()
		string(JSON url ERROR_VARIABLE nourl GET "${json}" manufacturers ${key} url)
		string(JSON matches LENGTH "${json}" manufacturers ${key} match)
		if(matches GREATER 0)
			math(EXPR mlast "${matches} - 1")
			foreach(m RANGE ${mlast})
				string(JSON vendor GET "${json}" manufacturers ${key} match ${m})
				string(APPEND vendors "  \"${vendor}\": \"${key}\",\n")
				if(NOT nourl AND url MATCHES "^https?://")
					string(APPEND websites "  \"${vendor}\": \"${url}\",\n")
				endif()
			endforeach()
		endif()
		string(JSON types ERROR_VARIABLE none LENGTH "${json}" manufacturers ${key} types)
		if(none OR types EQUAL 0)
			continue()
		endif()
		math(EXPR tlast "${types} - 1")
		foreach(t RANGE ${tlast})
			string(JSON type GET "${json}" manufacturers ${key} types ${t})
			# colours of its own for this kind, else the manufacturer's
			string(JSON primary ERROR_VARIABLE none GET "${json}" manufacturers ${key} device_classes ${type} primary)
			if(none)
				string(JSON primary GET "${json}" manufacturers ${key} colors primary)
			endif()
			string(JSON secondary ERROR_VARIABLE none GET "${json}" manufacturers ${key} device_classes ${type} secondary)
			if(none)
				string(JSON secondary GET "${json}" manufacturers ${key} colors secondary)
			endif()
			set(template "${src}/override/${key}_${type}.svg")
			if(NOT EXISTS "${template}")
				set(template "${src}/${type}.svg")
			endif()
			if(NOT EXISTS "${template}")
				message(FATAL_ERROR "manufacturer-colors.json: ${key} has the kind \"${type}\", no template ${type}.svg")
			endif()
			foreach(variant "" "_dark")
				set(outline "${QTDMM_SYMBOL_OUTLINE_LIGHT}")
				if(variant)
					set(outline "${QTDMM_SYMBOL_OUTLINE_DARK}")
				endif()
				set(file "${out}/${key}/${type}${variant}.svg")
				qtdmm_symbol_write("${template}" "${file}" "${primary}" "${secondary}" "${outline}")
				string(APPEND qrc "        <file alias=\"${key}/${type}${variant}.svg\">${file}</file>\n")
			endforeach()
		endforeach()
	endforeach()

	# which folder a vendor's pictures are in (DeviceSymbol)
	string(REGEX REPLACE ",\n$" "\n" vendors "${vendors}")
	file(WRITE "${out}/vendors.json.new" "{\n${vendors}}\n")
	configure_file("${out}/vendors.json.new" "${out}/vendors.json" COPYONLY)
	file(REMOVE "${out}/vendors.json.new")
	string(APPEND qrc "        <file alias=\"vendors.json\">${out}/vendors.json</file>\n")
	string(REGEX REPLACE ",\n$" "\n" websites "${websites}")
	file(WRITE "${out}/websites.json.new" "{\n${websites}}\n")
	configure_file("${out}/websites.json.new" "${out}/websites.json" COPYONLY)
	file(REMOVE "${out}/websites.json.new")
	string(APPEND qrc "        <file alias=\"websites.json\">${out}/websites.json</file>\n")

	string(APPEND qrc "    </qresource>\n")
	set(${result} "${qrc}" PARENT_SCOPE)
endfunction()
