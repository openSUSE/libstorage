#ifndef GRAPH_H
#define GRAPH_H


#include <string>

#include "StorageInterface.h"


namespace storage
{

    /**
     * Saves a graph of the storage devices as a DOT file for graphviz.
     *
     * @param s StorageInterface
     * @param filename filename of graph
     * @return true on successful writing of graph
     */
    bool saveGraph(StorageInterface* s, const std::string& filename);

}


#endif
