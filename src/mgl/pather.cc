/**
   MiracleGrue - Model Generator for toolpathing. <http://www.grue.makerbot.com>
   Copyright (C) 2011 Far McKon <Far@makerbot.com>, Hugo Boyer (hugo@makerbot.com)

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Affero General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

*/

#include <list>

#include "pather.h"
#include "limits.h"

using namespace mgl;
using namespace std;

Pather::Pather(ProgressBar * progress)
		:Progressive(progress)
{
}


void Pather::generatePaths(const ExtruderConfig &extruderCfg,
						   const LayerLoops &layerloops,
						   const RegionList &skeleton,
						   LayerPaths &layerpaths,
						   int sfirstSliceIdx,  // =-1
						   int slastSliceIdx )  //
{
	size_t firstSliceIdx = 0;
	size_t lastSliceIdx = INT_MAX;

	if(sfirstSliceIdx > 0 )
	{
		firstSliceIdx = (size_t)sfirstSliceIdx;
	}

	if(slastSliceIdx  > 0 )
	{
		lastSliceIdx = (size_t)slastSliceIdx;
	}

	bool direction = false;
	unsigned int currentSlice = 0;

	initProgress("Path generation", layerloops.readLayers().size());

	// TODO: incrementing regions and loops in lockstep will break with raft
	RegionList::const_iterator layer_regions = skeleton.begin();
	for(LayerLoops::const_layer_iterator loops = layerloops.begin();
		loops != layerloops.end(); ++loops)
	{
		tick();

		if(loops->getIndex() < firstSliceIdx) continue;
		if(loops->getIndex() > lastSliceIdx) break;

		direction = !direction;
		const LoopList& outline_loops = loops->readLoops();

		Scalar z = layerloops.layerMeasure.getLayerPosition(loops->getIndex());
		Scalar h = layerloops.layerMeasure.getLayerThickness(loops->getIndex());

		layerpaths.push_back(LayerPaths::Layer(z, h, loops->getIndex()));

		LayerPaths::Layer& lp_layer = layerpaths.back();
		
		//TODO: this only handles the case where the user specifies the extruder
		// it does not handle a dualstrusion print
		lp_layer.extruders.push_back(
				 LayerPaths::Layer::ExtruderLayer(extruderCfg.defaultExtruder));
		LayerPaths::Layer::ExtruderLayer& extruderlayer =
			lp_layer.extruders.back();

		const std::list<LoopList>& insetLoops = layer_regions->insetLoops;

		outlines(outline_loops, extruderlayer.outlinePaths);

		insets(insetLoops, extruderlayer.insetPaths);

		const GridRanges &infillRanges = layer_regions->infill;

		this->infills(infillRanges, layerloops.grid, outline_loops,
					  direction, extruderlayer.infillPaths);

		++currentSlice;
		++layer_regions;
	}
}


void Pather::outlines(const LoopList& outline_loops,
					  LoopPathList &boundary_paths)
{
	//using a indeterminate start point for the beginning of the LoopPathList
	//as that's what the old Polygon logic did

	for (LoopList::const_iterator i = outline_loops.begin();
		 i != outline_loops.end(); ++i) {
		boundary_paths.push_back(LoopPath(*i, i->clockwise(),
										  i->counterClockwise()));
	}
}

void Pather::insets(const list<LoopList>& inset_loops,
					list<LoopPathList> &inset_paths)
{
	for (list<LoopList>::const_iterator i = inset_loops.begin();
		 i != inset_loops.end(); ++i) {

		inset_paths.push_back(LoopPathList());
		LoopPathList& lp_list = inset_paths.back();

		for (LoopList::const_iterator j = i->begin(); j != i->end(); ++j) {
			lp_list.push_back(LoopPath(*j, j->clockwise(),
									   j->counterClockwise()));
		}
	}
}


void Pather::infills(const GridRanges &infillRanges,
					 const Grid &grid,
					 const LoopList &outlines,
					 const bool direction,
					 OpenPathList &infills)
{
	grid.pathsFromRanges(infillRanges, outlines, direction, infills);
}
