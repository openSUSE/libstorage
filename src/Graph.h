/*
 *  Author:	Arvin Schnell <aschnell@suse.de>
 */


#ifndef GRAPH_H
#define GRAPH_H


#include <string>

using std::string;


namespace storage
{
    class StorageInterface;


    /**
     * Saves a graph of the storage devices as a DOT file for graphviz.
     *
     * @param s StorageInterface
     * @param filename filename of graph
     * @return true on successful writing of graph
     */
    bool saveGraph(StorageInterface* s, const string& filename);

}


#endif
