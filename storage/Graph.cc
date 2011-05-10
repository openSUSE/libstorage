/*
 * Copyright (c) [2004-2010] Novell, Inc.
 *
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, contact Novell, Inc.
 *
 * To contact Novell about this file by physical or electronic mail, you may
 * find current contact information at www.novell.com.
 */


#include <string>
#include <fstream>
#include <boost/algorithm/string.hpp>

#include "config.h"
#include "storage/Graph.h"
#include "storage/AppUtil.h"
#include "storage/HumanString.h"
#include "storage/StorageInterface.h"


namespace storage
{
    using namespace std;


    class Graph
    {

    public:

	bool save(const string& filename) const;

    protected:

	enum NodeType { NODE_DISK, NODE_DMMULTIPATH, NODE_DMRAID, NODE_PARTITION, NODE_MD, NODE_MDPART,
			NODE_LVMVG, NODE_LVMLV, NODE_DM, NODE_BTRFS, NODE_MOUNTPOINT };

	struct Node
	{
	    Node(NodeType type, const string& device, const string& label, unsigned long long sizeK)
		: type(type), device(device), label(label), sizeK(sizeK) {}

	    string id() const { return (type == NODE_MOUNTPOINT ? "mountpoint:" : "device:") + device; }

	    NodeType type;
	    string device;
	    string label;
	    unsigned long long sizeK;
	};

	enum EdgeType { EDGE_SUBDEVICE, EDGE_MOUNT, EDGE_USED };

	struct Edge
	{
	    Edge(EdgeType type, const string& id1, const string& id2)
		: type(type), id1(id1), id2(id2) {}

	    EdgeType type;
	    string id1;
	    string id2;
	};

	enum RankType { RANK_SOURCE, RANK_SAME, RANK_SINK };

	struct Rank
	{
	    Rank(RankType type = RANK_SAME)
		: type(type) {}

	    RankType type;
	    list<string> ids;
	};

	friend ostream& operator<<(ostream& s, const Node& node);
	friend ostream& operator<<(ostream& s, const Edge& edge);
	friend ostream& operator<<(ostream& s, const Rank& rank);

	list<Node> nodes;
	list<Edge> edges;
	list<Rank> ranks;

    private:

	static string quote(const string& str);
	static string makeTooltip(const Text& text, const string& label, unsigned long long sizeK);

    };


    class DeviceGraph : public Graph
    {

    public:

	DeviceGraph(StorageInterface* s);

    private:

	Rank disks_rank;
	Rank partitions_rank;
	Rank mounts_rank;

	void processUsedby(const Node& node, const list<UsedByInfo>& usedBy);
	void processMount(const Node& node, const VolumeInfo& volume);

    };


    class MountGraph : public Graph
    {

    public:

	MountGraph(StorageInterface* s);

    private:

	map<int, Rank> mount_ranks;

	typedef map<string, string> Entries;

	Entries entries;

	Entries::const_iterator findParent(const Entries::const_iterator& child) const;

    };


    DeviceGraph::DeviceGraph(StorageInterface* s)
	: disks_rank(RANK_SOURCE), partitions_rank(), mounts_rank(RANK_SINK)
    {
	deque<ContainerInfo> containers;
	s->getContainers(containers);
	for (deque<ContainerInfo>::const_iterator i1 = containers.begin(); i1 != containers.end(); ++i1)
	{
	    switch (i1->type)
	    {
		case DISK: {

		    DiskInfo diskinfo;
		    s->getDiskInfo(i1->device, diskinfo);

		    Node disk_node(NODE_DISK, i1->device, i1->name, diskinfo.sizeK);
		    nodes.push_back(disk_node);
		    disks_rank.ids.push_back(disk_node.id());

		    processUsedby(disk_node, i1->usedBy);

		    deque<PartitionInfo> partitions;
		    s->getPartitionInfo(i1->name, partitions);
		    for (deque<PartitionInfo>::const_iterator i2 = partitions.begin(); i2 != partitions.end(); ++i2)
		    {
			if (i2->partitionType == EXTENDED)
			    continue;

			Node partition_node(NODE_PARTITION, i2->v.device, i2->v.name, i2->v.sizeK);
			nodes.push_back(partition_node);
			partitions_rank.ids.push_back(partition_node.id());

			edges.push_back(Edge(EDGE_SUBDEVICE, disk_node.id(), partition_node.id()));

			processUsedby(partition_node, i2->v.usedBy);
			processMount(partition_node, i2->v);
		    }

		} break;

		case LVM: {

		    LvmVgInfo lvmvginfo;
		    s->getLvmVgInfo(i1->name, lvmvginfo);

		    Node vg_node(NODE_LVMVG, i1->device, i1->name, lvmvginfo.sizeK);
		    nodes.push_back(vg_node);

		    processUsedby(vg_node, i1->usedBy);

		    Rank rank;

		    deque<LvmLvInfo> lvs;
		    s->getLvmLvInfo(i1->name, lvs);
		    for (deque<LvmLvInfo>::const_iterator i2 = lvs.begin(); i2 != lvs.end(); ++i2)
		    {
			Node lv_node(NODE_LVMLV, i2->v.device, i2->v.name, i2->v.sizeK);
			nodes.push_back(lv_node);
			rank.ids.push_back(lv_node.id());

			edges.push_back(Edge(EDGE_SUBDEVICE, vg_node.id(), lv_node.id()));

			processUsedby(lv_node, i2->v.usedBy);
			processMount(lv_node, i2->v);
		    }

		    if (!rank.ids.empty())
			ranks.push_back(rank);

		} break;

		case MD: {

		    deque<MdInfo> mds;
		    s->getMdInfo(mds);

		    for (deque<MdInfo>::const_iterator i2 = mds.begin(); i2 != mds.end(); ++i2)
		    {
			Node md_node(NODE_MD, i2->v.device, i2->v.name, i2->v.sizeK);
			nodes.push_back(md_node);

			processUsedby(md_node, i2->v.usedBy);
			processMount(md_node, i2->v);
		    }

		} break;

		case MDPART: {

		    MdPartCoInfo mdpartcoinfo;
		    s->getMdPartCoInfo(i1->device, mdpartcoinfo);

		    Node mdpart_node(NODE_MDPART, i1->device, i1->name, mdpartcoinfo.d.sizeK);
		    nodes.push_back(mdpart_node);

		    processUsedby(mdpart_node, i1->usedBy);

		    Rank rank;

		    deque<MdPartInfo> partitions;
		    s->getMdPartInfo(i1->name, partitions);
		    for (deque<MdPartInfo>::const_iterator i2 = partitions.begin(); i2 != partitions.end(); ++i2)
		    {
			if (i2->p.partitionType == EXTENDED)
			    continue;

			Node partition_node(NODE_PARTITION, i2->v.device, i2->v.name, i2->v.sizeK);
			nodes.push_back(partition_node);
			rank.ids.push_back(partition_node.id());

			edges.push_back(Edge(EDGE_SUBDEVICE, mdpart_node.id(), partition_node.id()));

			processUsedby(partition_node, i2->v.usedBy);
			processMount(partition_node, i2->v);
		    }

		    if (!rank.ids.empty())
			ranks.push_back(rank);

		} break;

		case DM: {

		    deque<DmInfo> dms;
		    s->getDmInfo(dms);

		    for (deque<DmInfo>::const_iterator i2 = dms.begin(); i2 != dms.end(); ++i2)
		    {
			Node dm_node(NODE_DM, i2->v.device, i2->v.name, i2->v.sizeK);
			nodes.push_back(dm_node);

			processUsedby(dm_node, i2->v.usedBy);
			processMount(dm_node, i2->v);
		    }

		} break;

		case DMRAID: {

		    DmraidCoInfo dmraidcoinfo;
		    s->getDmraidCoInfo(i1->device, dmraidcoinfo);

		    Node dmraid_node(NODE_DMRAID, i1->device, i1->name, dmraidcoinfo.p.d.sizeK);
		    nodes.push_back(dmraid_node);

		    processUsedby(dmraid_node, i1->usedBy);

		    Rank rank;

		    deque<DmraidInfo> partitions;
		    s->getDmraidInfo(i1->name, partitions);
		    for (deque<DmraidInfo>::const_iterator i2 = partitions.begin(); i2 != partitions.end(); ++i2)
		    {
			if (i2->p.p.partitionType == EXTENDED)
			    continue;

			Node partition_node(NODE_PARTITION, i2->p.v.device, i2->p.v.name, i2->p.v.sizeK);
			nodes.push_back(partition_node);
			rank.ids.push_back(partition_node.id());

			edges.push_back(Edge(EDGE_SUBDEVICE, dmraid_node.id(), partition_node.id()));

			processUsedby(partition_node, i2->p.v.usedBy);
			processMount(partition_node, i2->p.v);
		    }

		    if (!rank.ids.empty())
			ranks.push_back(rank);

		} break;

		case DMMULTIPATH: {

		    DmmultipathCoInfo dmmultipathcoinfo;
		    s->getDmmultipathCoInfo(i1->device, dmmultipathcoinfo);

		    Node dmmultipath_node(NODE_DMMULTIPATH, i1->device, i1->name, dmmultipathcoinfo.p.d.sizeK);
		    nodes.push_back(dmmultipath_node);

		    processUsedby(dmmultipath_node, i1->usedBy);

		    Rank rank;

		    deque<DmmultipathInfo> partitions;
		    s->getDmmultipathInfo(i1->name, partitions);
		    for (deque<DmmultipathInfo>::const_iterator i2 = partitions.begin(); i2 != partitions.end(); ++i2)
		    {
			if (i2->p.p.partitionType == EXTENDED)
			    continue;

			Node partition_node(NODE_PARTITION, i2->p.v.device, i2->p.v.name, i2->p.v.sizeK);
			nodes.push_back(partition_node);
			rank.ids.push_back(partition_node.id());

			edges.push_back(Edge(EDGE_SUBDEVICE, dmmultipath_node.id(), partition_node.id()));

			processUsedby(partition_node, i2->p.v.usedBy);
			processMount(partition_node, i2->p.v);
		    }

		    if (!rank.ids.empty())
			ranks.push_back(rank);

		} break;

		case BTRFSC: {

		    deque<BtrfsInfo> btrfss;
		    s->getBtrfsInfo(btrfss);

		    for (deque<BtrfsInfo>::const_iterator i2 = btrfss.begin(); i2 != btrfss.end(); ++i2)
		    {
			Node btrfs_node(NODE_BTRFS, i2->v.uuid, i2->v.name, i2->v.sizeK);
			nodes.push_back(btrfs_node);

			processUsedby(btrfs_node, i2->v.usedBy);
			processMount(btrfs_node, i2->v);
		    }

		} break;

		case LOOP:
		case NFSC:
		case TMPFSC:
		case CUNKNOWN:
		    break;
	    }
	}

	if (!disks_rank.ids.empty())
	    ranks.push_back(disks_rank);

	if (!partitions_rank.ids.empty())
	    ranks.push_back(partitions_rank);

	if (!mounts_rank.ids.empty())
	    ranks.push_back(mounts_rank);
    }


    void
    DeviceGraph::processUsedby(const Node& node, const list<UsedByInfo>& usedBy)
    {
	for (list<UsedByInfo>::const_iterator i = usedBy.begin(); i != usedBy.end(); ++i)
	{
	    edges.push_back(Edge(EDGE_USED, node.id(), "device:" + i->device));
	}
    }


    void
    DeviceGraph::processMount(const Node& node, const VolumeInfo& volume)
    {
	if (!volume.mount.empty())
	{
	    Node mountpoint_node(NODE_MOUNTPOINT, volume.device, volume.mount, volume.sizeK);
	    nodes.push_back(mountpoint_node);
	    mounts_rank.ids.push_back(mountpoint_node.id());

	    edges.push_back(Edge(EDGE_MOUNT, node.id(), mountpoint_node.id()));
	}
    }


    MountGraph::MountGraph(StorageInterface* s)
    {
	mount_ranks[0].type = RANK_SOURCE;

	deque<VolumeInfo> volumes;
	s->getVolumes(volumes);
	for (deque<VolumeInfo>::const_iterator i1 = volumes.begin(); i1 != volumes.end(); ++i1)
	{
	    if (!i1->mount.empty() && i1->mount != "swap")
	    {
		Node mountpoint_node(NODE_MOUNTPOINT, i1->device, i1->mount, i1->sizeK);
		nodes.push_back(mountpoint_node);

		int r = i1->mount == "/" ? 0 : count(i1->mount.begin(), i1->mount.end(), '/');
		mount_ranks[r].ids.push_back(mountpoint_node.id());

		entries[i1->mount] = i1->device;
	    }
	}

	for (map<int, Rank>::const_iterator i1 = mount_ranks.begin(); i1 != mount_ranks.end(); ++i1)
	    if (!i1->second.ids.empty())
		ranks.push_back(i1->second);

	for (Entries::const_iterator i1 = entries.begin(); i1 != entries.end(); ++i1)
	{
	    Entries::const_iterator i2 = findParent(i1);
	    if (i2 != entries.end())
		edges.push_back(Edge(EDGE_MOUNT, "mountpoint:" + i2->second, "mountpoint:" + i1->second));
	}
    }


    MountGraph::Entries::const_iterator
    MountGraph::findParent(const Entries::const_iterator& child) const
    {
	string mount = child->first;

	while (mount != "/")
	{
	    string::size_type pos = mount.rfind("/");
	    if (pos == string::npos)
		return entries.end();

	    mount.erase(pos);
	    if (mount.empty())
		mount = "/";

	    Entries::const_iterator i1 = entries.find(mount);
	    if (i1 != entries.end())
		return i1;
	}

	return entries.end();
    }


    string
    Graph::quote(const string& str)
    {
	return '"' + boost::replace_all_copy(str, "\"", "\\\"") + '"';
    }


    string
    Graph::makeTooltip(const Text& text, const string& label, unsigned long long sizeK)
    {
	return quote(text.text + "\\n" + label + "\\n" + byteToHumanString(1024 * sizeK, false, 2, false));
    }


    ostream& operator<<(ostream& s, const Graph::Node& node)
    {
	s << Graph::quote(node.id()) << " [label=" << Graph::quote(node.label);
	switch (node.type)
	{
	    case Graph::NODE_DISK:
		s << ", color=\"#ff0000\", fillcolor=\"#ffaaaa\"" << ", tooltip="
		  << Graph::makeTooltip(_("Hard Disk"), node.device, node.sizeK);
		break;
	    case Graph::NODE_DMRAID:
		s << ", color=\"#ff0000\", fillcolor=\"#ffaaaa\"" << ", tooltip="
		  << Graph::makeTooltip(_("DM RAID"), node.device, node.sizeK);
		break;
	    case Graph::NODE_DMMULTIPATH:
		s << ", color=\"#ff0000\", fillcolor=\"#ffaaaa\"" << ", tooltip="
		  << Graph::makeTooltip(_("DM Multipath"), node.device, node.sizeK);
		break;
	    case Graph::NODE_PARTITION:
		s << ", color=\"#cc33cc\", fillcolor=\"#eeaaee\"" << ", tooltip="
		  << Graph::makeTooltip(_("Partition"), node.device, node.sizeK);
		break;
	    case Graph::NODE_MD:
		s << ", color=\"#aaaa00\", fillcolor=\"#ffffaa\"" << ", tooltip="
		  << Graph::makeTooltip(_("MD RAID"), node.device, node.sizeK);
		break;
	    case Graph::NODE_MDPART:
		s << ", color=\"#aaaa00\", fillcolor=\"#ffffaa\"" << ", tooltip="
		  << Graph::makeTooltip(_("MD RAID"), node.device, node.sizeK);
		break;
	    case Graph::NODE_LVMVG:
		s << ", color=\"#0000ff\", fillcolor=\"#aaaaff\"" << ", tooltip="
		  << Graph::makeTooltip(_("Volume Group"), node.device, node.sizeK);
		break;
	    case Graph::NODE_LVMLV:
		s << ", color=\"#6622dd\", fillcolor=\"#bb99ff\"" << ", tooltip="
		  << Graph::makeTooltip(_("Logical Volume"), node.device, node.sizeK);
		break;
	    case Graph::NODE_BTRFS:
		s << ", color=\"#17534f\", fillcolor=\"#28e3d8\"" << ", tooltip="
		  << Graph::makeTooltip(_("Btrfs Volume"), node.device, node.sizeK);
		break;
	    case Graph::NODE_DM:
		s << ", color=\"#885511\", fillcolor=\"#ddbb99\"" << ", tooltip="
		  << Graph::makeTooltip(_("DM Device"), node.device, node.sizeK);
		break;
	    case Graph::NODE_MOUNTPOINT:
		s << ", color=\"#008800\", fillcolor=\"#99ee99\"" << ", tooltip="
		  << Graph::makeTooltip(_("Mount Point"), node.device, node.sizeK);
		break;
	}
	return s << "];";
    }


    ostream& operator<<(ostream& s, const Graph::Edge& edge)
    {
	s << Graph::quote(edge.id1) << " -> " << Graph::quote(edge.id2);
	switch (edge.type)
	{
	    case Graph::EDGE_SUBDEVICE:
		s << " [style=solid]";
		break;
	    case Graph::EDGE_MOUNT:
		s << " [style=dashed]";
		break;
	    case Graph::EDGE_USED:
		s << " [style=dotted]";
		break;
	}
	return s << ";";
    }


    ostream& operator<<(ostream& s, const Graph::Rank& rank)
    {
	static const char* names[] = { "source", "same", "sink" };

	s << "{ rank=" << names[rank.type] << "; ";
	for (list<string>::const_iterator id = rank.ids.begin(); id != rank.ids.end(); ++id)
	    s << Graph::quote(*id) << " ";
	return s << "};";
    }


    bool
    Graph::save(const string& filename) const
    {
	ofstream out(filename.c_str());
	classic(out);

	out << "// generated by libstorage version " VERSION << endl;
	out << "// " << hostname() << ", " << datetime() << endl;
	out << endl;

	out << "digraph storage" << endl;
	out << "{" << endl;
	out << "    node [shape=rectangle, style=filled, fontname=\"Arial\"];" << endl;
	out << "    edge [color=\"#444444\"];" << endl;
	out << endl;

	for (list<Node>::const_iterator node = nodes.begin(); node != nodes.end(); ++node)
	    out << "    " << (*node) << endl;

	out << endl;

	for (list<Rank>::const_iterator rank = ranks.begin(); rank != ranks.end(); ++rank)
	    out << "    " << (*rank) << endl;

	out << endl;

	for (list<Edge>::const_iterator edge = edges.begin(); edge != edges.end(); ++edge)
	    out << "    " << (*edge) << endl;

	out << "}" << endl;

	out.close();

	return !out.bad();
    }


    bool
    saveDeviceGraph(StorageInterface* s, const string& filename)
    {
	return DeviceGraph(s).save(filename);
    }


    bool
    saveMountGraph(StorageInterface* s, const string& filename)
    {
	return MountGraph(s).save(filename);
    }
}
