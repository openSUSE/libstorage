/*
 * Copyright (c) [2004-2014] Novell, Inc.
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


#ifndef CMD_BTRFS_H
#define CMD_BTRFS_H


#include <vector>
#include <list>
#include <map>


namespace storage
{
    using std::vector;
    using std::list;
    using std::map;


    /**
     * Class to probe for btrfs filesystems: Call "btrfs filesystem show"
     * (globally, not restricted to any disk or partition) and parse its
     * output.
     */
    class CmdBtrfsShow
    {
    public:

	/**
	 * Constructor. If 'do_probe' is 'true', call "btrfs filesystem show"
	 * as an external command. If 'do_probe' is 'false', do nothing for now
	 * (this is mostly useful for testing).
	 *
	 * This may throw a SystemCmdException or a ParseException.
	 */
	CmdBtrfsShow( bool do_probe = true );

	/**
	 * Probe for btrfs filesystems with the "btrfs filesystem show" command
	 * and parse its output.
	 *
	 * This may throw a SystemCmdException or a ParseException.
	 */
	void probe();

	/**
	 * Entry for one btrfs filesystem. Since btrfs includes a volume
	 * manager (independent of LVM or the device mapper), this may be
	 * multiple devices for a single btrfs filesystem.
	 */
	struct Entry
	{
	    list<string> devices;
	};

	friend std::ostream& operator<<(std::ostream& s, const CmdBtrfsShow& cmdbtrfsshow);
	friend std::ostream& operator<<(std::ostream& s, const Entry& entry);

	/**
	 * Find the btrfs filesystem with UUID 'uuid' and return the
	 * corresponding entry in 'entry'. Return 'true' upon success, 'false'
	 * if there is no btrfs filesystem with that UUID.
	 */
	bool getEntry( const string& uuid, Entry& entry ) const;

	/**
	 * Return a list of all filesystem UUIDs with btrfs.
	 */
	list<string> getUuids() const;

	/**
	 * Parse the output of "btrfs filesystem show" passed in 'lines'.
	 *
	 * This may throw a ParseException.
	 */
	void parse( const vector<string>& lines );

    private:

	typedef map<string, Entry>::const_iterator const_iterator;

	map<string, Entry> data;

    };

}

#endif
