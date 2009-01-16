/*
  Textdomain    "storage"
*/


#include <string>
#include <fstream>

#include "y2storage/Storage.h"


using namespace std;
using namespace storage;


namespace storage
{
    struct Node
    {
	Node(const string& id, const string& label)
	    : id(id), label(label)
	    {}

	const string id;
	const string label;
    };

    struct Edge
    {
	Edge(const string& node1, const string& node2)
	    : node1(node1), node2(node2)
	    {}

	const string node1;
	const string node2;
    };

    string dotQuote(const string& s)
    {
	return '"' + s + '"';
    }

    std::ostream& operator<<(std::ostream& s, const Node& node)
    {
	return s << dotQuote(node.id) << " [label=" << dotQuote(node.label) << "];";
    }

    std::ostream& operator<<(std::ostream& s, const Edge& edge)
    {
	return s << dotQuote(edge.node1) << " -> " << dotQuote(edge.node2) << ";";
    }
}


int
Storage::saveGraph(const string& filename)
{
    list<Node> nodes;
    list<Edge> edges;


    deque<ContainerInfo> containers;
    getContainers(containers);
    for (deque<ContainerInfo>::iterator i1 = containers.begin();
	 i1 != containers.end(); ++i1)
    {
	switch (i1->type)
	{
	    case DISK: {

		Node disk_node("device:" + i1->device, i1->device);
		nodes.push_back(disk_node);

		deque<PartitionInfo> partitions;
		getPartitionInfo (i1->name, partitions);
		for (deque<PartitionInfo>::iterator i2 = partitions.begin();
		     i2 != partitions.end(); ++i2)
		{
		    Node partition_node("device:" + i2->v.device, i2->v.device);
		    nodes.push_back(partition_node);

		    edges.push_back(Edge(disk_node.id, partition_node.id));

		    if (!i2->v.mount.empty())
		    {
			Node mountpoint_node("mountpoint:" + i2->v.mount, i2->v.mount);
			nodes.push_back(mountpoint_node);

			edges.push_back(Edge(partition_node.id, mountpoint_node.id));
		    }
		}

	    } break;
	}
    }


    ofstream out(filename.c_str());

    out << "digraph storage" << endl;
    out << "{" << endl;
    out << "    node [ shape=rectangle ];" << endl;
    out << endl;

    for (list<Node>::const_iterator node = nodes.begin(); node != nodes.end(); ++node)
	out << "    " << (*node) << endl;

    out << endl;

    for (list<Edge>::const_iterator edge = edges.begin(); edge != edges.end(); ++edge)
	out << "    " << (*edge) << endl;

    out << "}" << endl;

    out.close();

    return 0;
}
