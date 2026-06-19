#pragma once
#include "osm_model.h"
#include <string>

OSMData parse_osm_xml(const std::string& filepath);
