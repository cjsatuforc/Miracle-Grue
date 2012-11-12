/**
   MiracleGrue - Model Generator for toolpathing. <http://www.grue.makerbot.com>
   Copyright (C) 2011 Far McKon <Far@makerbot.com>, Hugo Boyer (hugo@makerbot.com)

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Affero General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

 */

#include "gcoder.h"

#include "log.h"
#include <math.h>
#include <string>
#include <list>
#include <map>
#include <vector>

namespace mgl {

using namespace std;



// function that adds an s to a noun if count is more than 1

std::string plural(const char*noun, int count, const char* ending = "s") {
    string s(noun);
    if (count > 1) {
        return s + ending;
    }
    return s;
}

//
// computes 2 positions (one before and one at the end of) the polygon and stores them in start and stop.
// These positions are aligned with the fisrt line and last line of the polygon.
// LeadIn is the distance between start and the first point of the polygon (along the first polygon line).
// LeadOut is the distance between the last point of the Polygon and stop (along the last polygon line).

void polygonLeadInAndLeadOut(const mgl::Polygon &polygon, const Extruder &extruder,
        double leadIn, double leadOut,
        Point2Type &start, Point2Type &end) {
    size_t count = polygon.size();

    const Point2Type &a = polygon[0]; // first element
    const Point2Type &b = polygon[1];

    const Point2Type &c = polygon[count - 2];
    const Point2Type &d = polygon[count - 1]; // last element

    if (extruder.isVolumetric()) {
        start = a;
        end = d;
        return;
    }

    Point2Type ab = b - a;
    ab.normalise();
    Point2Type cd = d - c;
    cd.normalise();

    start.x = a.x - ab.x * leadIn;
    start.y = a.y - ab.y * leadIn;
    end.x = d.x + cd.x * leadOut;
    end.y = d.y + cd.y * leadOut;

}

GCoder::GCoder(const GrueConfig& grueConf, ProgressBar* progress)
        : Progressive(progress), grueCfg(grueConf), gantry(grueCfg), 
        progressTotal(0), progressCurrent(0), 
        progressPercent(0) {
    gantry.init_to_start();
}

/**
 * Writes intial gcode data to start of the gcode file, including setup & startup info
 * @param gout - output stream for the gcode text
 * @param sourceName - source of this gcode (usually the origional stl file)
 */
void GCoder::writeStartDotGCode(std::ostream &gout, const char* sourceName) {
    gout.precision(3);
    gout.setf(ios::fixed);

    writeGCodeConfig(gout, sourceName);

    const string &header_file = grueCfg.get_header();

    if (header_file.length() > 0) {
        ifstream header_in(header_file.c_str(), ifstream::in);

        if (header_in.fail())
            throw GcoderException((string("Unable to open gcode header file [") +
                header_file + "]").c_str());

        gout << "(header [" << header_file << "] begin)" << endl;

        while (header_in.good()) {
            char buf[1024];

            header_in.read(buf, sizeof (buf));
            gout.write(buf, header_in.gcount());
        }

        if (header_in.fail() && !header_in.eof())
            throw GcoderException((string("Error reading gcode header file [") +
                header_file + "]").c_str());

        gout << "(header [" << header_file << "] end)" << endl << endl;
    }
}

void GCoder::writeEndDotGCode(std::ostream &ss) const {
    const string &footer_file = grueCfg.get_footer();


    if (footer_file.length() > 0) {
        ifstream footer_in(footer_file.c_str(), ifstream::in);

        if (footer_in.fail())
            throw GcoderException((string("Unable to open footer file [") +
                footer_file + "]").c_str());

        ss << "(footer [" << footer_file << "] begin)" << endl;

        while (footer_in.good()) {
            char buf[1024];

            footer_in.read(buf, sizeof (buf));
            ss.write(buf, footer_in.gcount());
        }

        if (footer_in.fail() && !footer_in.eof())
            throw GcoderException((string("Error reading footer file [") +
                footer_file + "]").c_str());

        ss << "(footer [" << footer_file << "] end)" << endl << endl;
    }
}

void GCoder::writeProgressPercent(std::ostream& ss, unsigned int current, 
        unsigned int total) {
    if(!grueCfg.get_doPrintProgress())
        return;
    unsigned int curPercent = (current--*100)/total;
    if(curPercent != progressPercent) {
        ss << "M73 P" << curPercent << " (progress (" << 
                curPercent << "%): " << current 
                << "/" << total << ")" << std::endl;
        progressPercent = curPercent;
    }
}

void GCoder::writeInfills(std::ostream& ss,
        Scalar z, Scalar h, Scalar w,
        size_t sliceId,
        const Extruder& extruder,
        const LayerPaths::Layer::ExtruderLayer& paths) {
    try {
        ss << "(infills: " << paths.infillPaths.size() << ")" << endl;
        Extrusion extrusion;
        calcInfillExtrusion(extruder.id, sliceId, extrusion);
        gantry.snort(ss, extruder, extrusion);
        for (LayerPaths::Layer::ExtruderLayer::const_infill_iterator iter =
                paths.infillPaths.begin();
                iter != paths.infillPaths.end();
                ++iter) {
            writePath(ss, z, h, w, extruder, extrusion, *iter);
        }
        gantry.snort(ss, extruder, extrusion);
    } catch (GcoderException& mixup) {
        stringstream errormsg;
        errormsg << "\nERROR writing infills in slice " <<
                sliceId << " for extruder " <<
                extruder.id << " : " << mixup.error << endl;
        Log::info() << errormsg.str();
        Log::severe() << errormsg.str();
    }
}

void GCoder::writeSupport(std::ostream &ss,
        Scalar z, Scalar h, Scalar w,
        size_t sliceId,
        const Extruder &extruder,
        const LayerPaths::Layer::ExtruderLayer& paths) {
    try {
        ss << "(support: " << paths.supportPaths.size() << ")" << endl;
        Extrusion extrusion;
        calcInfillExtrusion(extruder.id, sliceId, extrusion);
        gantry.snort(ss, extruder, extrusion);
        for (LayerPaths::Layer::ExtruderLayer::const_infill_iterator iter =
                paths.supportPaths.begin();
                iter != paths.supportPaths.end();
                ++iter) {
            writePath(ss, z, h, w, extruder, extrusion, *iter);
        }
        gantry.snort(ss, extruder, extrusion);
    } catch (GcoderException& mixup) {
        Log::info() << "ERROR writing support in slice " <<
                sliceId << " for extruder " <<
                extruder.id << " : " << mixup.error << endl;
        Log::severe() << "ERROR writing support in slice " <<
                sliceId << " for extruder " <<
                extruder.id << " : " << mixup.error << endl;
    }
}

void GCoder::writeInsets(std::ostream& ss,
        Scalar z, Scalar h, Scalar w,
        size_t sliceId,
        const Extruder& extruder,
        const LayerPaths& layerpaths,
        LayerPaths::layer_iterator layerId,
        const LayerPaths::Layer::ExtruderLayer& paths) {
    try {
        ss << "(insets: " << paths.insetPaths.size() << ")" << endl;
        Extrusion extrusion;
        calcInSetExtrusion(layerpaths, extruder.id, layerId,
                paths.insetPaths.end(), extrusion);
        gantry.snort(ss, extruder, extrusion);
        for (LayerPaths::Layer::ExtruderLayer::const_inset_iterator i =
                paths.insetPaths.begin();
                i != paths.insetPaths.end();
                ++i) {
            calcInSetExtrusion(layerpaths, extruder.id, layerId, i,
                    extrusion);
            for (OpenPathList::const_iterator j = i->begin();
                    j != i->end(); ++j) {
                writePath(ss, z, h, w, extruder, extrusion, *j);
            }
        }
        calcInSetExtrusion(layerpaths, extruder.id, layerId,
                paths.insetPaths.end(), extrusion);
        gantry.snort(ss, extruder, extrusion);
    } catch (GcoderException& mixup) {
        stringstream errormsg;
        errormsg << "\nERROR writing insets in slice " <<
                sliceId << " for extruder " <<
                extruder.id << " : " << mixup.error << endl;
        Log::info() << errormsg.str();
        Log::severe() << errormsg.str();
    }
}

void GCoder::writeOutlines(std::ostream& ss,
        Scalar z, Scalar h, Scalar w,
        size_t sliceId,
        const Extruder& extruder,
        const LayerPaths::Layer::ExtruderLayer& paths) {
    try {
        ss << "(outlines: " << paths.outlinePaths.size() << ")" << endl;
        Extrusion extrusion;
        calcInfillExtrusion(extruder.id, sliceId, extrusion);
        gantry.snort(ss, extruder, extrusion);
        for (LayerPaths::Layer::ExtruderLayer::const_outline_iterator iter =
                paths.outlinePaths.begin();
                iter != paths.outlinePaths.end();
                ++iter) {
            writePath(ss, z, h, w, extruder, extrusion, *iter);
        }
    } catch (GcoderException& mixup) {
        stringstream errormsg;
        errormsg << "\nERROR writing outlines in slice " <<
                sliceId << " for extruder " <<
                extruder.id << " : " << mixup.error << endl;
        Log::info() << errormsg.str();
        Log::severe() << errormsg.str();
    }
}

void GCoder::moveZ(ostream & ss, Scalar z, unsigned int, Scalar zFeedrate) {
    bool doX = false;
    bool doY = false;
    bool doZ = true;
    bool doE = false;
    bool doFeed = true;


    gantry.g1Motion(ss, 0, 0, z, 0, zFeedrate, 0, 0,
            "move Z", doX, doY, doZ, doE, doFeed);

}

void GCoder::calcOutlineExtrusion(unsigned int extruderId,
        unsigned int sliceId,
        Extrusion& extrusionParams) const {
    string profileName;
    if (sliceId == 0) {
        profileName = grueCfg.get_extruders()[extruderId].firstLayerExtrusionProfile;
    } else {
        profileName = grueCfg.get_extruders()[extruderId].outlinesExtrusionProfile;
    }
    const GrueConfig::profileNameMap::const_iterator it =
            grueCfg.get_extrusionProfiles().find(profileName);
    if (it == grueCfg.get_extrusionProfiles().end()) {
        //		Log::severe() << "Failed to find extrusion profile <name>" << 
        //		profileName  << "</name>" << endl;
        GcoderException mixup((string("Failed to find extrusion profile ") +
                profileName).c_str());
        throw mixup;
    } else {
        extrusionParams = it->second;
    }
    extrusionParams.feedrate *= grueCfg.get_scalingFactor();
}

void GCoder::calcInfillExtrusion(unsigned int extruderId, unsigned int sliceId, Extrusion &extrusion) const {
    string profileName;
    if (sliceId == 0) {
        profileName = grueCfg.get_extruders()[extruderId].firstLayerExtrusionProfile;
    } else {
        profileName = grueCfg.get_extruders()[extruderId].infillsExtrusionProfile;
    }

    const std::map<std::string, Extrusion>::const_iterator it =
            grueCfg.get_extrusionProfiles().find(profileName);
    if (it == grueCfg.get_extrusionProfiles().end()) {
        //		Log::severe() << "Failed to find extrusion profile <name>" << 
        //				profileName  << "</name>" << endl;
        GcoderException mixup((string("Failed to find extrusion profile ") +
                profileName).c_str());
        throw mixup;
    } else {
        extrusion = it->second;
    }
    extrusion.feedrate *= grueCfg.get_scalingFactor();
}

void GCoder::calcInfillExtrusion(const LayerPaths& layerpaths,
        unsigned int extruderId,
        LayerPaths::const_layer_iterator layerId,
        Extrusion& extrusionParams) const {
    string profileName = layerId == layerpaths.begin() ?
            grueCfg.get_extruders()[extruderId].firstLayerExtrusionProfile :
            grueCfg.get_extruders()[extruderId].infillsExtrusionProfile;

    const std::map<std::string, Extrusion>::const_iterator it =
            grueCfg.get_extrusionProfiles().find(profileName);
    if (it == grueCfg.get_extrusionProfiles().end()) {
        //		Log::severe() << "Failed to find extrusion profile <name>" << 
        //				profileName  << "</name>" << endl;
        GcoderException mixup((string("Failed to find extrusion profile ") +
                profileName).c_str());
        throw mixup;
    } else {
        extrusionParams = it->second;
    }
    extrusionParams.feedrate *= grueCfg.get_scalingFactor();
}

void GCoder::calcInSetExtrusion(unsigned int extruderId,
        unsigned int sliceId,
        unsigned int, // insetId,
        unsigned int, // insetCount,
        Extrusion &extrusion) const {
    string profileName;
    if (sliceId == 0) {
        profileName = grueCfg.get_extruders()[extruderId].firstLayerExtrusionProfile;
    } else {
        profileName = grueCfg.get_extruders()[extruderId].insetsExtrusionProfile;
    }

    const std::map<std::string, Extrusion>::const_iterator &it =
            grueCfg.get_extrusionProfiles().find(profileName);
    if (it == grueCfg.get_extrusionProfiles().end()) {
        //		Log::severe() << "Failed to find extrusion profile <name>" << 
        //				profileName  << "</name>" << endl;
        GcoderException mixup((string("Failed to find extrusion profile ") +
                profileName).c_str());
        throw mixup;
    } else {
        extrusion = it->second;
    }
    extrusion = it->second;
    extrusion.feedrate *= grueCfg.get_scalingFactor();
}

void GCoder::calcInSetExtrusion(const LayerPaths& layerpaths,
        unsigned int extruderId,
        LayerPaths::const_layer_iterator layerId,
        LayerPaths::Layer::ExtruderLayer::const_inset_iterator, // insetId, 
        Extrusion& extrusionParams) const {
    string profileName = layerId == layerpaths.begin() ?
            grueCfg.get_extruders()[extruderId].firstLayerExtrusionProfile :
            grueCfg.get_extruders()[extruderId].infillsExtrusionProfile;

    const std::map<std::string, Extrusion>::const_iterator it =
            grueCfg.get_extrusionProfiles().find(profileName);
    if (it == grueCfg.get_extrusionProfiles().end()) {
        //		Log::severe() << "Failed to find extrusion profile <name>" << 
        //				profileName  << "</name>" << endl;
        GcoderException mixup((string("Failed to find extrusion profile ") +
                profileName).c_str());
        throw mixup;
    } else {
        extrusionParams = it->second;
    }
    extrusionParams.feedrate *= grueCfg.get_scalingFactor();
}

void GCoder::writeGcodeFile(LayerPaths& layerpaths,
        const LayerMeasure& layerMeasure,
        std::ostream& gout,
        const std::string& title) {
    writeGcodeFile(layerpaths,
            layerMeasure,
            gout,
            title,
            layerpaths.begin(),
            layerpaths.end());
}

void GCoder::writeGcodeFile(LayerPaths& layerpaths,
        const LayerMeasure&, // layerMeasure, 
        std::ostream& gout,
        const std::string& title,
        LayerPaths::layer_iterator begin,
        LayerPaths::layer_iterator end) {
    writeStartDotGCode(gout, title.c_str());
    size_t sliceCount = 0;
    progressTotal = 1;
    progressCurrent = 0;
    progressPercent = 0;
    for (LayerPaths::const_layer_iterator it = begin;
            it != end;
            ++it, ++sliceCount){
        for(LayerPaths::Layer::const_extruder_iterator exit = 
                it->extruders.begin(); 
                exit != it->extruders.end(); 
                ++exit) {
            for(LayerPaths::Layer::ExtruderLayer::const_path_iterator pathiter = 
                    exit->paths.begin(); 
                    pathiter != exit->paths.end(); 
                    ++pathiter) {
                progressTotal += pathiter->myPath.size();
            }
        }
    }
    initProgress("gcode", sliceCount);
    size_t layerSequence = 0;
    for (LayerPaths::layer_iterator it = begin;
            it != end; ++it, ++layerSequence) {
        tick();
        //Scalar z = layerMeasure.sliceIndexToHeight(codeSlice);
        if(grueCfg.get_doAnchor() && layerSequence == 0) {
            Extrusion strusion;
            const Extruder& struder = grueCfg.get_extruders()[
                    it->extruders.front().extruderId];
            calcInfillExtrusion(struder.id, 0, strusion);
            gantry.set_current_extruder_index(struder.code);
            Point2Type startPoint;
            if(!it->extruders.empty() && 
                    !it->extruders.front().paths.empty() && 
                    !it->extruders.front().paths.front().myPath.empty()) {
                startPoint = *(it->extruders.front().paths.front().myPath.fromStart());
            }
            gantry.snort(gout, struder, 
                    strusion);
            const Scalar currentZ = it->layerZ + it->layerHeight;
            const Scalar currentH = it->layerHeight;
            const Scalar currentW = it->layerW * 2.0;
            gantry.g1(gout, struder, 
                    strusion, grueCfg.get_startingX(), 
                    grueCfg.get_startingY(), currentZ, 
                    strusion.feedrate, 
                    currentH, currentW, "(Anchor Start)");
            gantry.squirt(gout, struder, 
                    strusion);
            gantry.g1(gout, struder, 
                    strusion, grueCfg.get_startingX(), 
                    grueCfg.get_startingY(), currentZ, 
                    strusion.feedrate, 
                    currentH, currentW, "(Anchor Start)");
            gantry.g1(gout, struder, 
                    strusion, startPoint.x, startPoint.y, currentZ, 
                    strusion.feedrate, 
                    currentH, currentW, "(Anchor End)");
        }
        writeSlice(gout, layerpaths, it, layerSequence);
    }
    if(grueCfg.get_doFanCommand()) {
        //print command to disable fan
        gout << "M127 T" << 
                grueCfg.get_defaultExtruder() 
                << " (Turn off the fan)" << endl;
    }
    writeEndDotGCode(gout);
}

Point2Type GCoder::startPoint(const SliceData& sliceData) {
    if (grueCfg.get_doOutlines()) {
        return sliceData.extruderSlices[0].boundary[0][0];
    } else if (grueCfg.get_doInsets()) {
        if (sliceData.extruderSlices.size() < 1)
            throw Exception("zero extruder slices for finding start point");

        if (sliceData.extruderSlices[0].insetLoopsList.size() < 1)
            throw Exception("zero inset loops for finding start point");

        if (sliceData.extruderSlices[0].insetLoopsList[0].size() < 1)
            throw Exception("zero loops for finding start point");

        return sliceData.extruderSlices[0].insetLoopsList[0][0][0];
    } else {
        return sliceData.extruderSlices[0].infills[0][0];
    }
}

void GCoder::writeSlice(std::ostream& ss,
        LayerPaths& layerpaths,
        LayerPaths::layer_iterator layerIter,
        size_t layerSequence) {
    LayerPaths::Layer& currentLayer = *layerIter;
    unsigned int extruderCount = currentLayer.extruders.size();
    ss << "(Slice " << layerSequence << ", " << extruderCount <<
            " " << plural("Extruder", extruderCount) << ") " << endl;
    ss << "(Layer Height: \t" << layerIter->layerHeight << ")" << endl;
    ss << "(Layer Width: \t" << layerIter->layerW << ")" << endl;
    if (grueCfg.get_doPrintLayerMessages()) {
        //print layer message to printer screen if config enabled
        ss << "M70 P20 (Layer: " << layerSequence << ")" << endl;
    }
    if (grueCfg.get_doFanCommand()&& layerSequence == grueCfg.get_fanLayer()) {
        //print command to enable fan
        ss << "M126 T" << 
                grueCfg.get_defaultExtruder()
                << " (Turn on the fan)" << endl;
    }
    //iterate over all extruders invoked in this layer
    for (LayerPaths::Layer::const_extruder_iterator it =
            currentLayer.extruders.begin();
            it != currentLayer.extruders.end();
            ++it) {
        //this is the current extruder
        const Extruder& currentExtruder = grueCfg.get_extruders()[it->extruderId];
        gantry.set_current_extruder_index(currentExtruder.code);
        //this is the current extruder's zFeedrate
        Scalar zFeedrate = grueCfg.get_scalingFactor() *
                grueCfg.get_rapidMoveFeedRateZ();
        const Scalar currentZ = currentLayer.layerZ + currentLayer.layerHeight;
        const Scalar currentH = currentLayer.layerHeight;
        const Scalar currentW = currentLayer.layerW;
        try {
            moveZ(ss, currentZ, currentExtruder.id, zFeedrate);
        } catch (GcoderException& mixup) {
            Log::info() << "ERROR writing Z move in slice " <<
                    layerSequence << " for extruder " << currentExtruder.id <<
                    " : " << mixup.error << endl;
        }

        if (grueCfg.get_doOutlines()) {
            writeOutlines(ss, currentZ, currentH, currentW, layerSequence,
                    currentExtruder, *it);
        }
        if (grueCfg.get_doInsets()) {
            writeInsets(ss, currentZ, currentH, currentW, layerSequence,
                    currentExtruder, layerpaths, layerIter, *it);
        }
        if (grueCfg.get_doInfills()) {
            writeInfills(ss, currentZ, currentH, currentW, layerSequence,
                    currentExtruder, *it);
        }
        if (grueCfg.get_doSupport()) {
            writeSupport(ss, currentZ, currentH, currentW, layerSequence,
                    currentExtruder, *it);
        }

        writePaths(ss, currentZ, currentH, currentW, layerSequence,
                currentExtruder, it->paths);
    }
}

Scalar Extrusion::crossSectionArea(Scalar height, Scalar width) const {


    //two semicircles joined by a rectangle
    Scalar radius = height / 2;
    return (M_TAU / 2) * (radius * radius) + height * (width - height);
    //LONG LIVE TAU!
}

Scalar Extruder::feedCrossSectionArea() const {
    Scalar radius = feedDiameter / 2;
    //feedstock should be a cylinder
    return (M_TAU / 2) * radius * radius;
    //LONG LIVE TAU!
}

/**
 * Writes config header metadata into a gcode file
 * @param ss Stream to write config data to
 * @param sourceName - Name of source of this model. Usually the original .stl filename
 */
void GCoder::writeGCodeConfig(std::ostream &ss, const char* title = "unknown source") const {
    std::string indent = "* ";
    ss << endl;
    ss << "(Makerbot Industries)" << endl;
    ss << "(This file contains digital fabrication directives in gcode format)"
            << endl;
    ss << "(For your 3D printer)" << endl;
    ss << "(http://wiki.makerbot.com/gcode)" << endl;

    MyComputer hal9000;

    ss << "(" << indent << "Generated by " <<
            getMiracleGrueProgramName() << " " <<
            getMiracleGrueVersionStr() << ")" << endl;
    ss << "(" << indent << hal9000.clock.now() << ")" << endl;
    ss << "(" << indent << title << ")" << endl;

    std::string plurial = grueCfg.get_extruders().size() ? "" : "s";
    ss << "(" << indent << grueCfg.get_extruders().size() << " extruder" << plurial << ")" << endl;

    ss << "(" << indent << "Extrude infills: " << grueCfg.get_doInfills() << ")" << endl;
    ss << "(" << indent << "Extrude insets: " << grueCfg.get_doInsets() << ")" << endl;
    ss << "(" << indent << "Extrude outlines: " << grueCfg.get_doOutlines() << ")" << endl;
    ss << endl;
}

}



