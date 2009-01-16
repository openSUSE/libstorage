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
    enum Rank { RANK_DISK, RANK_PARTITION, RANK_LVMVG, RANK_LVMLV,
		RANK_MOUNTPOINT, RANK_NONE };

    struct Node
    {
	Node(const string& id, Rank rank, const string& label)
	    : id(id), rank(rank), label(label)
	    {}

	const string id;
	const Rank rank;
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

		Node disk_node("device:" + i1->device, RANK_DISK, i1->device);
		nodes.push_back(disk_node);

		if (!i1->usedByDevice.empty())
		{
		    edges.push_back(Edge(disk_node.id, "device:" + i1->usedByDevice));
		}

		deque<PartitionInfo> partitions;
		getPartitionInfo(i1->name, partitions);
		for (deque<PartitionInfo>::iterator i2 = partitions.begin();
		     i2 != partitions.end(); ++i2)
		{
		    if (i2->partitionType == EXTENDED)
			continue;

		    Node partition_node("device:" + i2->v.device, RANK_PARTITION, i2->v.device);
		    nodes.push_back(partition_node);

		    edges.push_back(Edge(disk_node.id, partition_node.id));

		    if (!i2->v.usedByDevice.empty())
		    {
			edges.push_back(Edge(partition_node.id, "device:" + i2->v.usedByDevice));
		    }

		    if (!i2->v.mount.empty())
		    {
			Node mountpoint_node("mountpoint:" + i2->v.mount, RANK_MOUNTPOINT, i2->v.mount);
			nodes.push_back(mountpoint_node);

			edges.push_back(Edge(partition_node.id, mountpoint_node.id));
		    }
		}

	    } break;

	    case LVM: {

		Node vg_node("device:" + i1->device, RANK_LVMVG, i1->device);
		nodes.push_back(vg_node);

		deque<LvmLvInfo> lvs;
		getLvmLvInfo(i1->name, lvs);
		for (deque<LvmLvInfo>::iterator i2 = lvs.begin();
		     i2 != lvs.end(); ++i2)
		{
		    Node lv_node("device:" + i2->v.device, RANK_LVMLV, i2->v.device);
		    nodes.push_back(lv_node);

		    edges.push_back(Edge(vg_node.id, lv_node.id));

		    if (!i2->v.mount.empty())
		    {
			Node mountpoint_node("mountpoint:" + i2->v.mount, RANK_MOUNTPOINT, i2->v.mount);
			nodes.push_back(mountpoint_node);

			edges.push_back(Edge(lv_node.id, mountpoint_node.id));
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

    // TODO
    for (Rank rank = RANK_DISK; rank != RANK_NONE; rank = (storage::Rank)(rank + 1))
    {
	list<string> ids;
	for (list<Node>::const_iterator node = nodes.begin(); node != nodes.end(); ++node)
	    if (node->rank == rank)
		ids.push_back(dotQuote(node->id) + ";");

	if (!ids.empty())
	    out << "    { rank=same; " << boost::join(ids, " ") << " };" << endl;
    }
    out << endl;


    for (list<Edge>::const_iterator edge = edges.begin(); edge != edges.end(); ++edge)
	out << "    " << (*edge) << endl;

    out << "}" << endl;

    out.close();


    return 0;
}
