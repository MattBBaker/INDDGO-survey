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

#ifndef METISGRAPHREADER_H_
#define METISGRAPHREADER_H_

#include "GraphReader.h"
#include "Node.h"

#include <vector>
#include <string>
using namespace std;

namespace Graph {
    class MetisGraphReader : public GraphReader
    {
private:
    vector<int> weights;
    vector<int> degree;
    vector<Node> nodes;
    int num_edges;
    int capacity;
    string filename;
    void split(const string& s, char c, vector<int>& v);

public:
    MetisGraphReader();
    MetisGraphReader(string f);
    virtual ~MetisGraphReader();
    virtual void read_graph(const char *filename);
    virtual void read_graph();
    virtual vector<int> get_degree();
    virtual vector<Node> get_nodes();
    virtual int get_num_edges() const;
    virtual vector<int> get_weights();
    virtual int get_capacity() const;
    virtual void set_capacity(int capacity);
    };
}

#endif /* METISGRAPHREADER_H_ */
