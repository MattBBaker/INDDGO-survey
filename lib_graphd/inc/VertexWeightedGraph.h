/*
   This file is part of INDDGO.

   Copyright (C) 2012, Oak Ridge National Laboratory

   This product includes software produced by UT-Battelle, LLC under Contract No.
   DE-AC05-00OR22725 with the Department of Energy.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the New BSD 3-clause software license (LICENSE).

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   LICENSE for more details.

   For more information please contact the INDDGO developers at:
   inddgo-info@googlegroups.com

 */

#ifndef WEIGHTEDMUTABLEGRAPH_H_
#define WEIGHTEDMUTABLEGRAPH_H_

#include "Graph.h"
#include "WeightedGraph.h"
#include "Graph.h"

namespace Graph {
    class VertexWeightedGraph : public WeightedGraph
    {
public:
    VertexWeightedGraph();
    VertexWeightedGraph(int n);
    virtual ~VertexWeightedGraph();
    VertexWeightedGraph& operator=(const VertexWeightedGraph& wmg);
    void write_largest_component(std::string filetype, std::string filename);

    friend class GraphUtil;
    friend class GraphProperties;
    friend class GraphDisplay;
    friend class GraphEOUtil;
    friend class GraphCreator;
    };
}

#endif /* WEIGHTEDMUTABLEGRAPH_H_ */
