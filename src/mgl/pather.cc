/**
   MiracleGrue - Model Generator for toolpathing. <http://www.grue.makerbot.com>
   Copyright (C) 2011 Far McKon <Far@makerbot.com>, Hugo Boyer (hugo@makerbot.com)

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Affero General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.
 
    MiracleGrue - Toolpath generator for 3D printing.
    Copyright (C) 2012 Joseph Sadusk <jsadusk@makerbot.com>, Filipp Gelman <filipp@makerbot.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

 */

#include <list>
#include <vector>

#include "pather.h"
#include "limits.h"
#include "pather_optimizer_graph.h"
#include "pather_optimizer_fastgraph.h"
#include "dump_restore.h"

namespace mgl {
using namespace std;

Pather::Pather(const PatherConfig& pCfg, ProgressBar* progress) 
		: Progressive(progress), patherCfg(pCfg) {}
Pather::Pather(const GrueConfig& grueConf, ProgressBar* progress)
        : Progressive(progress) {
    patherCfg.doGraphOptimization = grueConf.get_doGraphOptimization();
    patherCfg.coarseness = grueConf.get_coarseness();
    patherCfg.directionWeight = grueConf.get_directionWeight();
}

void Pather::generatePaths(const GrueConfig& grueCfg,
		const RegionList &skeleton,
		const LayerMeasure &layerMeasure,
		const Grid &grid,
		LayerPaths &layerpaths,
		int sfirstSliceIdx, // =-1
		int slastSliceIdx) //
{
	size_t firstSliceIdx = 0;
	size_t lastSliceIdx = INT_MAX;

	if (sfirstSliceIdx > 0) {
		firstSliceIdx = (size_t) sfirstSliceIdx;
	}

	if (slastSliceIdx > 0) {
		lastSliceIdx = (size_t) slastSliceIdx;
	}

	bool direction = false;
	unsigned int currentSlice = 0;

	initProgress("Path generation", skeleton.size());
    
    abstract_optimizer* optimizer = NULL;
    if(grueCfg.get_doGraphOptimization()) {
        optimizer = new pather_optimizer_fastgraph(grueCfg);
    } else {
        optimizer = new pather_optimizer();
    }

	for (RegionList::const_iterator layerRegions = skeleton.begin();
			layerRegions != skeleton.end(); ++layerRegions) {
		tick();
        try {
		if (currentSlice < firstSliceIdx) continue;
		if (currentSlice > lastSliceIdx) break;
        if(grueCfg.get_doRaft() && currentSlice > 1 && 
                currentSlice < grueCfg.get_raftLayers() && 
                grueCfg.get_raftAligned()) {
            //don't flip direction
        } else {
            direction = !direction;
        }
		const layer_measure_index_t layerMeasureId =
				layerRegions->layerMeasureId;

		//adding these should be handled in gcoder
		const Scalar z = layerMeasure.getLayerPosition(layerMeasureId);
		const Scalar h = layerMeasure.getLayerThickness(layerMeasureId);
		const Scalar w = layerMeasure.getLayerWidth(layerMeasureId);

		layerpaths.push_back(LayerPaths::Layer(z, h, w, layerMeasureId));

		LayerPaths::Layer& lp_layer = layerpaths.back();

		//TODO: this only handles the case where the user specifies the extruder
		// it does not handle a dualstrusion print
		lp_layer.extruders.push_back(
				LayerPaths::Layer::ExtruderLayer(grueCfg.get_defaultExtruder()));
		LayerPaths::Layer::ExtruderLayer& extruderlayer =
				lp_layer.extruders.back();
		
//        Json::Value spurLoops;
//        for(std::list<LoopList>::const_iterator depthIter = 
//                layerRegions->spurLoops.begin(); 
//                depthIter != layerRegions->spurLoops.end(); 
//                ++depthIter) {
//            dumpLoopList(*depthIter, spurLoops);
//        }
//        std::cerr << Json::FastWriter().write(spurLoops);
		
		optimizer->clearBoundaries();
        optimizer->clearPaths();

		const std::list<LoopList>& insetLoops = layerRegions->insetLoops;
		const std::list<OpenPathList>& spurPaths = layerRegions->spurs;
		
        if(grueCfg.get_doOutlines()) {
            for(LoopList::const_iterator iter = layerRegions->outlines.begin(); 
                    iter != layerRegions->outlines.end(); 
                    ++iter) {
                const LoopPath outlinePath(*iter, iter->clockwise(), 
                        iter->counterClockwise());
                extruderlayer.paths.push_back(PathLabel(PathLabel::TYP_OUTLINE, 
                        PathLabel::OWN_MODEL));
                OpenPath& path = extruderlayer.paths.back().myPath;
                for(LoopPath::const_iterator pointIter = outlinePath.fromStart(); 
                        pointIter != outlinePath.end(); 
                        ++pointIter) {
                    path.appendPoint(*pointIter);
                }
            }
            for(LoopList::const_iterator iter = layerRegions->supportLoops.begin(); 
                    iter != layerRegions->supportLoops.end(); 
                    ++iter) {
                const LoopPath outlinePath(*iter, iter->clockwise(), 
                        iter->counterClockwise());
                extruderlayer.paths.push_back(PathLabel(PathLabel::TYP_OUTLINE, 
                        PathLabel::OWN_SUPPORT));
                OpenPath& path = extruderlayer.paths.back().myPath;
                for(LoopPath::const_iterator pointIter = outlinePath.fromStart(); 
                        pointIter != outlinePath.end(); 
                        ++pointIter) {
                    path.appendPoint(*pointIter);
                }
            }
        }
		
		optimizer->addBoundaries(layerRegions->outlines);	
        
        bool hasInfill = grueCfg.get_doInfills() && 
                grueCfg.get_infillDensity() > 0;
        bool hasSolidLayers = grueCfg.get_roofLayerCount() > 0 || 
                grueCfg.get_floorLayerCount() > 0;
        
        if(!hasInfill && !hasSolidLayers) {
            optimizer->addBoundaries(layerRegions->interiorLoops);
        }
        
        const GridRanges& infillRanges = layerRegions->infill;

		const std::vector<Scalar>& values = 
				!direction ? grid.getXValues() : grid.getYValues();
		axis_e axis = direction ? X_AXIS : Y_AXIS;
        
        if(grueCfg.get_doRaft() || grueCfg.get_doSupport()) {
            LoopList outsetSupportLoops;
            loopsOffset(outsetSupportLoops, layerRegions->supportLoops, 
                    0.01);
            optimizer->addBoundaries(outsetSupportLoops);
            
            const GridRanges& supportRanges = layerRegions->support;
            OpenPathList supportPaths;
            grid.gridRangesToOpenPaths(
                    direction ? supportRanges.xRays : supportRanges.yRays, 
                    values, 
                    axis, 
                    supportPaths);
            optimizer->addPaths(supportPaths, PathLabel(PathLabel::TYP_INFILL, 
                    PathLabel::OWN_SUPPORT, 0));
        }
		if(grueCfg.get_doInsets()) {
            int currentShell = LayerPaths::Layer::ExtruderLayer::INSET_LABEL_VALUE;
            for(std::list<LoopList>::const_iterator listIter = insetLoops.begin(); 
                    listIter != insetLoops.end(); 
                    ++listIter) {
                int shellVal = currentShell;
                optimizer->addPaths(*listIter, 
                        PathLabel(PathLabel::TYP_INSET, 
                        PathLabel::OWN_MODEL, shellVal));
                ++currentShell;
            }

            currentShell = LayerPaths::Layer::ExtruderLayer::INSET_LABEL_VALUE;
            for(std::list<OpenPathList>::const_iterator spurIter = spurPaths.begin(); 
                spurIter != spurPaths.end(); 
                    ++spurIter) {
                int shellVal = currentShell;
                optimizer->addPaths(*spurIter, 
                        PathLabel(PathLabel::TYP_INSET, 
                        PathLabel::OWN_MODEL, shellVal));
                ++currentShell;
            }
        }

		OpenPathList infillPaths;
		grid.gridRangesToOpenPaths(
				direction ? infillRanges.xRays : infillRanges.yRays,  
				values, 
				axis, 
				infillPaths);
		
		LabeledOpenPaths preoptimized;
		
        if(grueCfg.get_doInfills()) {
            optimizer->addPaths(infillPaths, PathLabel(PathLabel::TYP_INFILL, 
                    PathLabel::OWN_MODEL, 
                    LayerPaths::Layer::ExtruderLayer::INFILL_LABEL_VALUE));
        }
        optimizer->optimize(preoptimized);
//        smoothCollection(preoptimized, grueCfg.get_coarseness(), 
//                grueCfg.get_directionWeight());
        cleanPaths(preoptimized);
        smoothCollection(preoptimized, grueCfg.get_coarseness(), 
                grueCfg.get_directionWeight());
        
        extruderlayer.paths.insert(extruderlayer.paths.end(), 
                preoptimized.begin(), preoptimized.end());
        } catch (const std::exception& our) {
            std::cout << "Error " << our.what() << " on layer " << 
                    currentSlice << std::endl;
        }
		++currentSlice;
	}
    delete optimizer;
}

void Pather::cleanPaths(LabeledOpenPaths& result) {
    std::vector<LabeledOpenPaths::iterator> eraseMe;
    typedef LabeledOpenPaths::iterator iterator;
    if(result.empty())
        return;
    iterator current = result.begin();
    iterator next = current;
    for(++next; 
            next != result.end(); 
            ++current, ++next) {
        if(false) {
            //paths below dropThreshold are too short to be valid. 
            //even connections must be longer, so we drop them
            const Scalar dropThreshold = patherCfg.coarseness * 0.5;
            //keep dropping current until length above threshold
            while(current != result.end() && current->myPath.distance() < 
                    dropThreshold) {
                current = result.erase(current);
                next = current;
                ++next;
            }
            //keep dropping next until length above threshold
            while(next != result.end() && next->myPath.distance() < 
                    dropThreshold) {
                next = result.erase(next);
            }
            if(current == result.end() || next == result.end())
                break;
        }
        Point2Type currentStart = *(current->myPath.fromStart());
        Point2Type currentEnd = *(current->myPath.fromEnd());
        Point2Type nextStart = *(next->myPath.fromStart());
        Point2Type nextEnd = *(next->myPath.fromEnd());
        if((currentEnd - nextStart).squaredMagnitude() > 
                (patherCfg.coarseness * patherCfg.coarseness)) { //separate paths
            continue;
        }
        //here we only join spurs and connections
        if((current->myLabel.isConnection() || 
                current->myLabel.isInset()) && 
                (next->myLabel.isConnection() || 
                next->myLabel.isInset())) {
            //we have adjacent paths of the correct types
            if((currentStart == currentEnd && current->myPath.size() > 2) || 
                    (nextStart == nextEnd && next->myPath.size() > 2)) //one is an inset, don't join
                continue;
            OpenPath::iterator nextPoint = next->myPath.fromStart();
            ++nextPoint;
            current->myPath.appendPoints(nextPoint, next->myPath.end());
            if(current->myLabel.isInset()) {
                next->myLabel = current->myLabel;
            }
            next->myPath = current->myPath;
            eraseMe.push_back(current);
        }
    }
    while(!eraseMe.empty()) {
        result.erase(eraseMe.back());
        eraseMe.pop_back();
    }
}

}
