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


#ifndef ARCH_INFO_H
#define ARCH_INFO_H


#include "storage/XmlFile.h"


namespace storage
{

    class ArchInfo
    {
    public:

	ArchInfo();

	void readData(const xmlNode* node);
	void saveData(xmlNode* node) const;

	void detect(bool instsys);

	string arch;
	bool is_ppc_mac;
	bool is_ppc_pegasos;
	bool is_efiboot;

    };


    std::ostream& operator<<(std::ostream& s, const ArchInfo& archinfo);

}


#endif
