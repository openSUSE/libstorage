#ifndef GRAPH_H
#define GRAPH_H


#include <string>


#include "StorageInterface.h"


namespace storage
{

    /**
     * Saves a graph of the storage devices as a DOT file for graphviz.
     *
     * @param filename filename of graph
     * @return zero if all is ok, negative number to indicate an error
     */
    int saveGraph(StorageInterface* s, const std::string& filename);

}


#endif
